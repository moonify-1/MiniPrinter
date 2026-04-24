#pragma once

#include <cstddef>
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace mp {

// 任务编号。
//
// 这里先定义的是“预留任务槽位”，
// 并不代表这些任务在当前步骤已经真的创建。
enum class TaskId : std::uint8_t {
  SYSTEM = 0,   // 系统主控任务。
  LOGGER,       // 日志任务。
  MONITOR,      // 监控任务。
  ERROR,        // 错误处理任务。
  COMMAND,      // 命令处理任务。
  PRINT_CTRL,   // 打印控制任务。
  SENSOR,       // 传感器采样任务。
  PARAM,        // 参数管理任务。
  COMM,         // 通信任务。
  METRICS,      // 指标统计任务。
  COUNT,        // 任务槽位总数，不是实际任务。
};

// 固定任务槽位数量。
//
// 使用固定大小数组的原因：
// 1. 实现简单，适合嵌入式早期骨架。
// 2. 不需要动态分配，避免注册表本身再引入额外内存风险。
constexpr std::size_t kTaskRegistryCapacity =
    static_cast<std::size_t>(TaskId::COUNT);

// 单个任务的健康信息快照。
//
// 这份结构体只做“当前状态记录”，
// 不负责看门狗决策，也不负责超时处理。
struct TaskHealthEntry {
  TaskId id;                 // 任务编号。
  const char* name;          // 任务名称，便于日志输出。
  TaskHandle_t handle;       // FreeRTOS 任务句柄。
  TickType_t lastHeartbeat;  // 最近一次心跳 Tick。
  bool isRegistered;         // 该槽位是否已登记。
  bool hasHeartbeat;         // 是否至少上报过一次心跳。
};

// 全量任务健康快照。
//
// 调用 GetTaskHealthSnapshot() 时会返回这个结构，
// 这样上层可以一次拿到全部任务的登记和心跳状态。
struct TaskHealthSnapshot {
  TickType_t snapshotTick;                      // 生成快照时的系统 Tick。
  TaskHealthEntry entries[kTaskRegistryCapacity]; // 所有任务槽位的副本。
};

// 注册一个任务到任务表。
//
// 参数说明：
// - id：任务槽位编号。
// - name：任务名称；如果传入空指针，会退化成 "(unnamed)"。
// - handle：任务句柄；当前步骤允许先登记空句柄，用于早期占位。
void RegisterTask(TaskId id, const char* name, TaskHandle_t handle);

// 记录一次任务心跳。
//
// 这个函数只更新时间戳，
// 具体“多久算超时”由后续监控逻辑决定。
void TaskHeartbeat(TaskId id);

// 获取当前任务注册表的完整快照。
TaskHealthSnapshot GetTaskHealthSnapshot();

}  // namespace mp
