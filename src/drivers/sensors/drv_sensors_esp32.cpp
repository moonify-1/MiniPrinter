#include "drivers/sensors/drv_sensor_types.h"

#include "config/project_features.h"

#if MP_ENABLE_HW_SENSORS
#include <Arduino.h>

#include "bsp/bsp_pins.h"

namespace {

// 真实传感器驱动第一阶段。
//
// 当前只接入“风险较低、方向明确”的读数：
// - EQD：电池 ADC 分压。
// - G_NFAULT：DRV8833 低有效故障脚。
//
// PAPER_N、TM1、BAT_STAT 暂时保守处理：
// - PAPER_N：用户要求第一阶段先忽略缺纸检测，因此固定返回有纸。
// - TM1：等待 ADC 原始值校准，先返回保守室温占位。
// - BAT_STAT：IP2326 状态语义待确认，先返回 UNKNOWN。
class Esp32SensorDriver final : public mp::SensorDriver {
 public:
  bool readPaperPresent() override {
    EnsurePinsConfigured();
    // TODO(R01): 等用户提供 PAPER_N 有纸/无纸电平后，再启用真实反相判断。
    return true;
  }

  float readHeadTempC() override {
    EnsurePinsConfigured();
    // TODO(R02): 等用户提供 TM1 室温和升温 ADC 原始值后，再实现 NTC 换算。
    return 25.0F;
  }

  std::uint16_t readBatteryMv() override {
    EnsurePinsConfigured();

    // EQD 来自 2S+ 的 220k/100k 分压：
    // ADC 端电压 = 电池电压 * 100k / (220k + 100k)
    // 所以电池电压约等于 ADC 电压 * 3.2。
    const std::uint32_t adcMv =
        static_cast<std::uint32_t>(analogReadMilliVolts(mp::PIN_EQD));
    const std::uint32_t batteryMv = (adcMv * 32U) / 10U;
    return (batteryMv > 65535U) ? 65535U
                                : static_cast<std::uint16_t>(batteryMv);
  }

  mp::ChargeStatus readChargeStatus() override {
    EnsurePinsConfigured();
    // TODO(R03): 查明 IP2326 BAT_STAT 的高低电平语义后，再映射充电状态。
    return mp::ChargeStatus::UNKNOWN;
  }

  bool readMotorFault() override {
    EnsurePinsConfigured();
    // DRV8833 nFAULT 是低有效输入：LOW 表示故障。
    return digitalRead(mp::PIN_G_NFAULT) == LOW;
  }

 private:
  void EnsurePinsConfigured() {
    if (configured_) {
      return;
    }

    pinMode(mp::PIN_PAPER_N, INPUT);
    pinMode(mp::PIN_G_NFAULT, INPUT_PULLUP);
    pinMode(mp::PIN_BAT_STAT, INPUT);
    pinMode(mp::PIN_EQD, INPUT);
    pinMode(mp::PIN_TM1, INPUT);
    configured_ = true;
  }

  bool configured_ = false;
};

Esp32SensorDriver g_sensorDriver;

}  // namespace

namespace mp {

SensorDriver& GetSensorDriver() { return g_sensorDriver; }

}  // namespace mp

#endif
