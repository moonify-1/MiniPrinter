#include "drivers/stepper/drv_stepper.h"

namespace {

// Mock 版本的步进电机驱动。
//
// 它不包含 Arduino.h，不配置 GPIO，也不唤醒真实 DRV8833。
// 当前唯一职责是记录调用次数，并按默认配置返回“无故障”。
class MockStepperDriver final : public mp::StepperDriver {
 public:
  bool init() override {
    stats_.initCount++;
    return true;
  }

  void sleep() override { stats_.sleepCount++; }

  void wake() override { stats_.wakeCount++; }

  void setSafe() override { stats_.setSafeCount++; }

  void stepForward() override { stats_.stepForwardCount++; }

  void stepBackward() override { stats_.stepBackwardCount++; }

  void release() override { stats_.releaseCount++; }

  bool isFault() override {
    stats_.isFaultCount++;
    return motorFault_;
  }

  const mp::StepperMockStats& stats() const { return stats_; }

  void resetStats() { stats_ = {}; }

 private:
  mp::StepperMockStats stats_ = {};
  bool motorFault_ = false;
};

MockStepperDriver g_mockStepperDriver;

}  // namespace

namespace mp {

StepperDriver& GetStepperDriver() { return g_mockStepperDriver; }

const StepperMockStats& StepperMock_GetStats() {
  return g_mockStepperDriver.stats();
}

void StepperMock_ResetStats() { g_mockStepperDriver.resetStats(); }

}  // namespace mp
