#pragma once

#include <cstddef>
#include <cstdint>

#include "common/app_error.h"

namespace mp {

// 热敏打印头硬件动作接口。
//
// 这个接口只描述“驱动层能做什么硬件动作”，不判断“什么时候该做”。
// 例如：
// - 是否允许加热，要由 safety / app / service 层决定。
// - 具体如何移位、锁存、打 STB，由 driver 层实现。
class ThermalHeadDriver {
 public:
  virtual ~ThermalHeadDriver() = default;

  // 初始化打印头相关底层资源。
  //
  // Mock 实现只计数并返回 true。
  // 真实实现后续才会配置 GPIO / SPI / RMT 等硬件资源。
  virtual bool init() = 0;

  // 立即进入安全输出状态。
  //
  // 真实实现应关闭 VH、关闭全部 STB，并把数据线恢复到安全电平。
  // Mock 实现不操作 GPIO，只记录调用次数。
  virtual bool setSafe() = 0;

  // 向打印头移入一行 48 字节点阵数据。
  //
  // data 指向一行数据；len 应为 48。
  // Driver 只负责移位动作，不判断这一行是否应该被打印。
  virtual bool shiftLine48Bytes(const uint8_t* data, size_t len) = 0;

  // 锁存当前已经移入的数据。
  virtual bool latch() = 0;

  // 控制 VH 电源开关。
  //
  // 注意：这里只是接口定义。Mock 不会打开任何硬件电源。
  // 真实实现接入前，仍必须由 safety 层统一决定是否允许 enable=true。
  virtual bool setVh(bool enable) = 0;

  // 对指定 STB 分组输出一个加热脉冲。
  //
  // group 是 STB 分组号，建议后续使用 0~5 表示 STB1~STB6。
  // pulseUs 是脉冲宽度，单位微秒。
  virtual bool pulseStbGroup(uint8_t group, uint32_t pulseUs) = 0;

  // 关闭全部 STB 输出。
  virtual bool allStbOff() = 0;
};

// ThermalHead mock 调用统计。
//
// 这些字段只用于测试和调试，业务代码不要依赖它们。
struct ThermalHeadMockStats {
  uint32_t initCount;
  uint32_t setSafeCount;
  uint32_t shiftLine48BytesCount;
  uint32_t latchCount;
  uint32_t setVhCount;
  uint32_t pulseStbGroupCount;
  uint32_t allStbOffCount;
};

// 获取当前热敏打印头驱动实例。
//
// 当前 Step 09 返回 mock 实例。
// 后续替换真实驱动时，上层仍然只通过 ThermalHeadDriver 接口调用。
ThermalHeadDriver& GetThermalHeadDriver();

// 获取 mock 驱动实例。
//
// 这个入口主要给硬件功能关闭时复用 mock，
// 普通上层代码仍应优先调用 GetThermalHeadDriver()。
ThermalHeadDriver& GetThermalHeadMockDriver();

// 手动波形测试入口。
//
// 只允许未来的 debug 命令显式触发，启动流程绝不能自动调用。
// 默认硬件关闭时返回 ERR_HW_DISABLED。
AppErrorCode ThermalHead_DebugWaveformTest();

// 读取 / 清零 mock 统计。
//
// 这些函数是为了后续单元测试或串口调试观察 mock 是否被调用。
const ThermalHeadMockStats& ThermalHeadMock_GetStats();
void ThermalHeadMock_ResetStats();

}  // namespace mp
