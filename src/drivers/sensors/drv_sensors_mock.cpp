#include "drivers/sensors/drv_sensor_types.h"

namespace {

constexpr mp::SensorMockConfig kDefaultSensorMockConfig = {
    true,                       // paperPresent：默认不缺纸。
    25.0F,                      // headTempC：室温占位值。
    7400U,                      // batteryMv：2S 锂电常见满电附近占位值。
    mp::ChargeStatus::UNKNOWN,  // chargeStatus：当前未接真实充电状态。
    false,                      // motorFault：默认无电机故障。
};

// Mock 版本的传感器驱动。
//
// 它不包含 Arduino.h，不读取 ADC/GPIO，也不做阈值判断。
// 当前只返回可配置的固定值，并记录每个读取接口的调用次数。
class MockSensorDriver final : public mp::SensorDriver {
 public:
  bool readPaperPresent() override {
    stats_.readPaperPresentCount++;
    return config_.paperPresent;
  }

  float readHeadTempC() override {
    stats_.readHeadTempCCount++;
    return config_.headTempC;
  }

  uint16_t readBatteryMv() override {
    stats_.readBatteryMvCount++;
    return config_.batteryMv;
  }

  mp::ChargeStatus readChargeStatus() override {
    stats_.readChargeStatusCount++;
    return config_.chargeStatus;
  }

  bool readMotorFault() override {
    stats_.readMotorFaultCount++;
    return config_.motorFault;
  }

  const mp::SensorMockConfig& config() const { return config_; }

  void setConfig(const mp::SensorMockConfig& config) { config_ = config; }

  void resetConfig() { config_ = kDefaultSensorMockConfig; }

  const mp::SensorMockStats& stats() const { return stats_; }

  void resetStats() { stats_ = {}; }

 private:
  mp::SensorMockConfig config_ = kDefaultSensorMockConfig;
  mp::SensorMockStats stats_ = {};
};

MockSensorDriver g_mockSensorDriver;

}  // namespace

namespace mp {

SensorDriver& GetSensorDriver() { return g_mockSensorDriver; }

const SensorMockConfig& SensorMock_GetConfig() {
  return g_mockSensorDriver.config();
}

void SensorMock_SetConfig(const SensorMockConfig& config) {
  g_mockSensorDriver.setConfig(config);
}

void SensorMock_ResetConfig() { g_mockSensorDriver.resetConfig(); }

const SensorMockStats& SensorMock_GetStats() {
  return g_mockSensorDriver.stats();
}

void SensorMock_ResetStats() { g_mockSensorDriver.resetStats(); }

}  // namespace mp
