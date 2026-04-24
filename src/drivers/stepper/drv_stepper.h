#pragma once

#include <cstdint>

namespace mp {

// 步进电机硬件动作接口。
//
// 这个接口只描述电机驱动能执行的底层动作：
// - 休眠 / 唤醒。
// - 正向 / 反向走一步。
// - 释放线圈。
// - 读取故障脚状态。
//
// 它不包含走纸距离、速度曲线、打印节奏等业务决策。
class StepperDriver {
 public:
  virtual ~StepperDriver() = default;

  // 初始化电机驱动相关底层资源。
  virtual bool init() = 0;

  // 让电机驱动芯片进入休眠。
  virtual void sleep() = 0;

  // 唤醒电机驱动芯片。
  //
  // 当前 mock 不会真正唤醒 DRV8833。
  virtual void wake() = 0;

  // 立即进入安全状态。
  //
  // 真实实现通常应关闭输出、停止相序，并让驱动休眠。
  virtual void setSafe() = 0;

  // 正向走一步。
  virtual void stepForward() = 0;

  // 反向走一步。
  virtual void stepBackward() = 0;

  // 释放电机线圈。
  virtual void release() = 0;

  // 读取电机驱动故障状态。
  virtual bool isFault() = 0;
};

// Stepper mock 调用统计。
//
// 这些字段只用于测试和调试，业务代码不要依赖它们。
struct StepperMockStats {
  uint32_t initCount;
  uint32_t sleepCount;
  uint32_t wakeCount;
  uint32_t setSafeCount;
  uint32_t stepForwardCount;
  uint32_t stepBackwardCount;
  uint32_t releaseCount;
  uint32_t isFaultCount;
};

// 获取当前步进电机驱动实例。
//
// 当前 Step 09 返回 mock 实例。
// 后续替换真实驱动时，上层仍然只通过 StepperDriver 接口调用。
StepperDriver& GetStepperDriver();

// 获取 mock 步进电机驱动实例。
//
// 默认硬件功能关闭时，GetStepperDriver() 会继续返回 mock。
// 真实硬件分支如果需要做对比测试，也可以显式拿到 mock。
StepperDriver& GetStepperMockDriver();

// 读取 / 清零 mock 统计。
const StepperMockStats& StepperMock_GetStats();
void StepperMock_ResetStats();

}  // namespace mp
