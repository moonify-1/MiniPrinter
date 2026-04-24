#include "drivers/thermal_head/drv_thermal_head.h"

#include "config/project_features.h"

#if MP_ENABLE_HW_THERMAL_HEAD
#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include "bsp/bsp_board.h"
#include "bsp/bsp_gpio.h"
#include "bsp/bsp_pins.h"
#include "bsp/bsp_spi.h"
#include "bsp/bsp_timer.h"
#include "config/safety_limits.h"

namespace {

constexpr std::uint32_t kLatchPulseUs = 2U;
constexpr std::uint32_t kDebugStbPulseUs = 5U;
constexpr int LEVEL_VH_ON = (LEVEL_VH_OFF == 0) ? 1 : 0;
constexpr int LEVEL_STB_ACTIVE = (LEVEL_STB_INACTIVE == 0) ? 1 : 0;

constexpr int kStbPins[] = {
    mp::PIN_STB1,
    mp::PIN_STB2,
    mp::PIN_STB3,
    mp::PIN_STB4,
    mp::PIN_STB5,
    mp::PIN_STB6,
};

// ESP32 真实热敏头驱动安全骨架。
//
// 只有 MP_ENABLE_HW_THERMAL_HEAD=1 时本类才会编译出真实 GPIO/SPI 操作。
// 这里仍然不包含业务决策；是否允许加热由 ThermalSafetyService 等上层判断。
class Esp32ThermalHeadDriver final : public mp::ThermalHeadDriver {
 public:
  bool init() override {
    // init 的第一件事必须回到安全输出：STB 全关，VH 关闭。
    allStbOff();
    setVh(false);

    ConfigureOutputs();
    allStbOff();
    setVh(false);
    return mp::Bsp_SpiThermalHeadInit();
  }

  void setSafe() override {
    setVh(false);
    allStbOff();
    mp::Bsp_WritePinIfValid(mp::PIN_DI, LOW);
    mp::Bsp_WritePinIfValid(mp::PIN_CLK, LOW);
    mp::Bsp_WritePinIfValid(mp::PIN_LAT, LOW);
  }

  bool shiftLine48Bytes(const std::uint8_t* data, std::size_t len) override {
    if (data == nullptr || len != 48U) {
      return false;
    }

    return mp::Bsp_SpiThermalHeadWrite(data, len);
  }

  void latch() override {
    // LAT 脉冲现在用 GPIO 翻转和短延时实现。
    // 后续必须用示波器确认真实脉宽是否满足打印头时序。
    mp::Bsp_WritePinIfValid(mp::PIN_LAT, HIGH);
    mp::Bsp_DelayUs(kLatchPulseUs);
    mp::Bsp_WritePinIfValid(mp::PIN_LAT, LOW);
  }

  void setVh(bool enable) override {
    // 打开 VH 前必须确认全部 STB 都处于 inactive。
    // 原因：如果某个 STB 仍有效，打开 VH 可能立即形成加热通路。
    if (enable && !AreAllStbInactive()) {
      return;
    }

    mp::Bsp_WritePinIfValid(mp::PIN_G_VH, enable ? LEVEL_VH_ON : LEVEL_VH_OFF);
    vhEnabled_ = enable;
  }

  void pulseStbGroup(std::uint8_t group, std::uint32_t pulseUs) override {
    // group 使用 0..5 映射 STB1..STB6。
    // pulseUs 为 0 表示本组不需要加热，此时不能输出任何 STB 脉冲。
    if (group >= 6U || pulseUs == 0U) {
      return;
    }

    // driver 层仍做一次硬上限裁剪，作为 safety 层之外的最后保险。
    if (pulseUs > mp::SAFETY_DEFAULT_HEAT_PULSE_MAX_US) {
      pulseUs = mp::SAFETY_DEFAULT_HEAT_PULSE_MAX_US;
    }

    const int pin = kStbPins[group];
    stbInactive_[group] = false;
    mp::Bsp_WritePinIfValid(pin, LEVEL_STB_ACTIVE);
    mp::Bsp_DelayUs(pulseUs);
    mp::Bsp_WritePinIfValid(pin, LEVEL_STB_INACTIVE);
    stbInactive_[group] = true;
  }

  void allStbOff() override {
    for (std::uint8_t group = 0U; group < 6U; ++group) {
      mp::Bsp_WritePinIfValid(kStbPins[group], LEVEL_STB_INACTIVE);
      stbInactive_[group] = true;
    }
  }

 private:
  void ConfigureOutputs() {
    // pinMode 通过 Bsp_PinModeOutputSafe() 设置输出模式，同时写入安全默认电平。
    // 当前所有引脚默认都是 PIN_UNASSIGNED，wrapper 会静默跳过。
    mp::Bsp_PinModeOutputSafe(mp::PIN_G_VH, LEVEL_VH_OFF);
    for (std::uint8_t group = 0U; group < 6U; ++group) {
      mp::Bsp_PinModeOutputSafe(kStbPins[group], LEVEL_STB_INACTIVE);
    }
    mp::Bsp_PinModeOutputSafe(mp::PIN_DI, LOW);
    mp::Bsp_PinModeOutputSafe(mp::PIN_CLK, LOW);
    mp::Bsp_PinModeOutputSafe(mp::PIN_LAT, LOW);
  }

  bool AreAllStbInactive() const {
    // 这里检查的是 driver 维护的软件状态。
    // 后续如果硬件支持回读，可再增加真实 GPIO 电平校验。
    for (bool inactive : stbInactive_) {
      if (!inactive) {
        return false;
      }
    }
    return true;
  }

  bool stbInactive_[6] = {true, true, true, true, true, true};
  bool vhEnabled_ = false;
};

Esp32ThermalHeadDriver g_esp32ThermalHeadDriver;

}  // namespace

namespace mp {

ThermalHeadDriver& GetThermalHeadDriver() {
  return g_esp32ThermalHeadDriver;
}

AppErrorCode ThermalHead_DebugWaveformTest() {
  // 这个函数只供未来 debug 命令显式调用。
  // 它保持 VH 关闭，只观察 LAT/STB 低风险波形，不做真实加热。
  g_esp32ThermalHeadDriver.setSafe();
  g_esp32ThermalHeadDriver.latch();
  for (std::uint8_t group = 0U; group < 6U; ++group) {
    g_esp32ThermalHeadDriver.pulseStbGroup(group, kDebugStbPulseUs);
  }
  g_esp32ThermalHeadDriver.setSafe();
  return APP_OK;
}

}  // namespace mp

#else

namespace mp {

AppErrorCode ThermalHead_DebugWaveformTest() {
  return ERR_HW_DISABLED;
}

}  // namespace mp

#endif
