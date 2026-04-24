#pragma once

#include <cstdint>

#include "rtos/task_registry.h"

namespace mp {

// 健康检查快照。
//
// 这份结构体的目标是把“当前健康检查结论”和“任务注册表快照”
// 一起返回给上层，便于 MonitorTask 做决策，也便于后续调试。
struct HealthSnapshot {
  TickType_t snapshotTick;           // 生成快照时的系统 Tick。
  bool allHealthy;                   // 当前关键任务是否全部健康。
  bool hasCriticalTimeout;           // 是否发现关键任务超时。
  bool safeModeRequested;            // 是否建议进入安全模式。
  TaskId failedTaskId;               // 首个失败的任务编号，若无失败则为 COUNT。
  TickType_t failedTaskAgeTicks;     // 失败任务距上次心跳已过去多少 Tick。
  TaskHealthSnapshot taskSnapshot;   // 任务注册表全量快照。
};

// 初始化健康检查服务。
void Health_Init();

// 对外统一的心跳上报入口。
//
// 当前实现会转发到 TaskRegistry，
// 后续如果要接软件 WDT 或统计逻辑，只需要在这里扩展。
void Health_ReportHeartbeat(TaskId id);

// 检查关键任务是否全部健康。
//
// 返回值：
// - true：所有关键任务都在允许的心跳窗口内。
// - false：发现关键任务未注册或心跳超时。
bool Health_CheckAll();

// 读取最近一次健康检查快照。
HealthSnapshot Health_GetSnapshot();

}  // namespace mp
