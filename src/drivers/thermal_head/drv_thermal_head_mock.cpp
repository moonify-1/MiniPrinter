#include "drivers/thermal_head/drv_thermal_head.h"

#include "config/project_features.h"

namespace {

// Mock 版本的热敏打印头驱动。
//
// 它不包含 Arduino.h，不配置 GPIO，也不产生任何真实输出。
// 当前唯一职责是记录接口被调用的次数，方便后续验证上层流程。
class MockThermalHeadDriver final : public mp::ThermalHeadDriver {
 public:
  bool init() override {
    stats_.initCount++;
    return true;
  }

  bool setSafe() override {
    stats_.setSafeCount++;
    return true;
  }

  bool shiftLine48Bytes(const uint8_t* data, size_t len) override {
    stats_.shiftLine48BytesCount++;
    return data != nullptr && len == 48U;
  }

  bool latch() override {
    stats_.latchCount++;
    return true;
  }

  bool setVh(bool /*enable*/) override {
    stats_.setVhCount++;
    return true;
  }

  bool pulseStbGroup(uint8_t group, uint32_t /*pulseUs*/) override {
    stats_.pulseStbGroupCount++;
    return group < 6U;
  }

  bool allStbOff() override {
    stats_.allStbOffCount++;
    return true;
  }

  const mp::ThermalHeadMockStats& stats() const { return stats_; }

  void resetStats() { stats_ = {}; }

 private:
  mp::ThermalHeadMockStats stats_ = {};
};

MockThermalHeadDriver g_mockThermalHeadDriver;

}  // namespace

namespace mp {

ThermalHeadDriver& GetThermalHeadMockDriver() { return g_mockThermalHeadDriver; }

#if !MP_ENABLE_HW_THERMAL_HEAD
ThermalHeadDriver& GetThermalHeadDriver() { return GetThermalHeadMockDriver(); }
#endif

const ThermalHeadMockStats& ThermalHeadMock_GetStats() {
  return g_mockThermalHeadDriver.stats();
}

void ThermalHeadMock_ResetStats() { g_mockThermalHeadDriver.resetStats(); }

}  // namespace mp
