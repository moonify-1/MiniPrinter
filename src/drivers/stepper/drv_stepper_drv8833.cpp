#include "drivers/stepper/drv_stepper.h"

#include "config/project_features.h"

#if MP_ENABLE_HW_STEPPER
#include <Arduino.h>
#include <cstdint>

#include "bsp/bsp_board.h"
#include "bsp/bsp_gpio.h"
#include "bsp/bsp_pins.h"
#include "bsp/bsp_timer.h"

namespace {

constexpr int LEVEL_DRV_AWAKE = (LEVEL_DRV_SLEEP == 0) ? 1 : 0;
constexpr std::uint32_t kDrv8833WakeDelayUs = 1000U;

// DRV8833 四步全步相序表。
//
// 表中每一行表示“一步”时四个 H 桥输入脚的电平：
// - AIN1/AIN2 控制 A 相线圈方向。
// - BIN1/BIN2 控制 B 相线圈方向。
// - 1/0 和 0/1 表示两种相反电流方向。
// - 两个输入全 0 表示该相释放，这里只在 release() 中使用。
//
// 当前先使用低速 4-step 全步，原因是逻辑清晰、便于用万用表/示波器检查。
// 后续需要更平滑走纸时，再考虑 8-step 半步或 PWM 电流控制。
struct StepPhase {
  int ain1;
  int ain2;
  int bin1;
  int bin2;
};

constexpr StepPhase kFullStepTable[] = {
    {HIGH, LOW, HIGH, LOW},  // 0: A+, B+
    {LOW, HIGH, HIGH, LOW},  // 1: A-, B+
    {LOW, HIGH, LOW, HIGH},  // 2: A-, B-
    {HIGH, LOW, LOW, HIGH},  // 3: A+, B-
};

// DRV8833 真实步进电机驱动安全骨架。
//
// 只有 MP_ENABLE_HW_STEPPER=1 时本类才会编译出真实 GPIO 操作。
// 它只描述底层硬件动作，不判断“是否允许打印/走纸”。
class Drv8833StepperDriver final : public mp::StepperDriver {
 public:
  bool init() override {
    // init 的第一件事必须先让 DRV8833 休眠，并关闭所有 H 桥输入。
    // 这样即使后续 pinMode 配置过程中发生问题，也不会主动驱动电机。
    mp::Bsp_WritePinIfValid(mp::PIN_G_NSLEEP, LEVEL_DRV_SLEEP);
    WriteCoilsLow();

    ConfigurePins();
    sleep();
    return true;
  }

  void sleep() override {
    // 休眠前先释放线圈，避免 nSLEEP 变化时 H 桥仍保持有效输入。
    release();
    mp::Bsp_WritePinIfValid(mp::PIN_G_NSLEEP, LEVEL_DRV_SLEEP);
    awake_ = false;
  }

  void wake() override {
    mp::Bsp_WritePinIfValid(mp::PIN_G_NSLEEP, LEVEL_DRV_AWAKE);
    // DRV8833 从 sleep 到可用需要等待稳定时间。
    // 当前按需求先等待至少 1ms，后续应结合芯片手册和实测确认。
    mp::Bsp_DelayUs(kDrv8833WakeDelayUs);
    awake_ = true;

    if (isFault()) {
      release();
      sleep();
    }
  }

  void setSafe() override { sleep(); }

  void stepForward() override {
    if (!CanDrive()) {
      return;
    }

    phaseIndex_ = NextPhase(phaseIndex_);
    ApplyPhase(kFullStepTable[phaseIndex_]);
  }

  void stepBackward() override {
    if (!CanDrive()) {
      return;
    }

    phaseIndex_ = PrevPhase(phaseIndex_);
    ApplyPhase(kFullStepTable[phaseIndex_]);
  }

  void release() override { WriteCoilsLow(); }

  bool isFault() override {
    // DRV8833 nFAULT 通常为低电平有效。
    // 如果当前引脚未分配，BSP 会返回默认 HIGH，也就是“无故障”。
    const bool fault = (mp::Bsp_ReadPinIfValid(mp::PIN_G_NFAULT, HIGH) == LOW);
    if (fault) {
      // 只要检测到 fault，driver 层立即把 H 桥释放并让芯片休眠。
      // 这里不写日志，避免未来在 ISR 或高频路径中误用日志。
      release();
      mp::Bsp_WritePinIfValid(mp::PIN_G_NSLEEP, LEVEL_DRV_SLEEP);
      awake_ = false;
    }
    return fault;
  }

 private:
  void ConfigurePins() {
    // 输出脚使用安全 wrapper：未分配 GPIO 会被跳过，不会报错。
    mp::Bsp_PinModeOutputSafe(mp::PIN_G_NSLEEP, LEVEL_DRV_SLEEP);
    mp::Bsp_PinModeOutputSafe(mp::PIN_AIN1, LOW);
    mp::Bsp_PinModeOutputSafe(mp::PIN_AIN2, LOW);
    mp::Bsp_PinModeOutputSafe(mp::PIN_BIN1, LOW);
    mp::Bsp_PinModeOutputSafe(mp::PIN_BIN2, LOW);

    // nFAULT 是输入脚。这里使用普通 INPUT，不擅自开启内部上下拉；
    // 外部电路的上拉关系需要按原理图和实测确认。
    if (mp::Bsp_IsValidPin(mp::PIN_G_NFAULT)) {
      pinMode(mp::PIN_G_NFAULT, INPUT);
    }
  }

  bool CanDrive() {
    if (!awake_) {
      return false;
    }

    if (isFault()) {
      release();
      sleep();
      return false;
    }

    return true;
  }

  void ApplyPhase(const StepPhase& phase) {
    mp::Bsp_WritePinIfValid(mp::PIN_AIN1, phase.ain1);
    mp::Bsp_WritePinIfValid(mp::PIN_AIN2, phase.ain2);
    mp::Bsp_WritePinIfValid(mp::PIN_BIN1, phase.bin1);
    mp::Bsp_WritePinIfValid(mp::PIN_BIN2, phase.bin2);
  }

  void WriteCoilsLow() {
    mp::Bsp_WritePinIfValid(mp::PIN_AIN1, LOW);
    mp::Bsp_WritePinIfValid(mp::PIN_AIN2, LOW);
    mp::Bsp_WritePinIfValid(mp::PIN_BIN1, LOW);
    mp::Bsp_WritePinIfValid(mp::PIN_BIN2, LOW);
  }

  std::uint8_t NextPhase(std::uint8_t phase) const {
    return static_cast<std::uint8_t>((phase + 1U) % 4U);
  }

  std::uint8_t PrevPhase(std::uint8_t phase) const {
    return static_cast<std::uint8_t>((phase + 3U) % 4U);
  }

  std::uint8_t phaseIndex_ = 0U;
  bool awake_ = false;
};

Drv8833StepperDriver g_drv8833StepperDriver;

}  // namespace

namespace mp {

StepperDriver& GetStepperDriver() {
  return g_drv8833StepperDriver;
}

}  // namespace mp

#endif
