#pragma once

#include <cstdint>

namespace mp {

// BSP Watchdog 封装层。
//
// 设计约束：
// 1. 只有 MonitorTask 可以调用这里的 init/feed 接口。
// 2. 其他任务只能上报 heartbeat，不能直接喂硬件看门狗。
// 3. 当 MP_ENABLE_WDT=0 时，cpp 实现会编译成空操作，默认不改变系统行为。

// 在当前任务上初始化并订阅 ESP32 Task Watchdog。
//
// 参数：
// - timeoutMs：看门狗超时时间，开发阶段固定使用 5000ms。
//
// 返回值：
// - true：硬件 WDT 已可用，MonitorTask 后续可以喂狗。
// - false：WDT 未启用或初始化失败。
bool Bsp_WatchdogInitForMonitor(std::uint32_t timeoutMs);

// 由 MonitorTask 喂 ESP32 Task Watchdog。
//
// 注意：这个函数名显式带有 FromMonitor，是为了提醒后续维护者不要从
// SystemTask、SensorTask、PrintEngineTask 等其他任务直接调用。
bool Bsp_WatchdogFeedFromMonitor();

// 读取并转成人类可读的 ESP32 复位原因。
//
// 该接口用于启动日志和后续故障分析；如果 WDT 宏关闭，会返回固定说明。
const char* Bsp_WatchdogResetReasonText();

}  // namespace mp
