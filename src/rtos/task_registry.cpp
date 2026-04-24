#include "rtos/task_registry.h"

namespace {

constexpr const char* kUnnamedTaskName = "(unnamed)";

// 全局任务注册表。
//
// 当前阶段先采用固定数组，不引入额外锁和动态分配，
// 目的是先把接口骨架稳定下来。
mp::TaskHealthEntry g_registry[mp::kTaskRegistryCapacity] = {};

// 把 TaskId 转成数组索引。
std::size_t ToIndex(mp::TaskId id) {
  return static_cast<std::size_t>(id);
}

// 判断任务编号是否落在合法槽位范围内。
bool IsValidTaskId(mp::TaskId id) {
  return ToIndex(id) < mp::kTaskRegistryCapacity;
}

// 返回指定槽位的可写引用。
mp::TaskHealthEntry& GetEntry(mp::TaskId id) {
  return g_registry[ToIndex(id)];
}

}  // namespace

namespace mp {

void RegisterTask(TaskId id, const char* name, TaskHandle_t handle) {
  if (!IsValidTaskId(id)) {
    return;
  }

  TaskHealthEntry& entry = GetEntry(id);
  entry.id = id;
  entry.name = (name != nullptr) ? name : kUnnamedTaskName;
  entry.handle = handle;
  entry.lastHeartbeat = xTaskGetTickCount();
  entry.hasHeartbeat = false;
  entry.isRegistered = true;
}

void TaskHeartbeat(TaskId id) {
  if (!IsValidTaskId(id)) {
    return;
  }

  TaskHealthEntry& entry = GetEntry(id);
  entry.id = id;
  if (entry.name == nullptr) {
    entry.name = kUnnamedTaskName;
  }
  entry.lastHeartbeat = xTaskGetTickCount();
  entry.hasHeartbeat = true;
}

TaskHealthSnapshot GetTaskHealthSnapshot() {
  TaskHealthSnapshot snapshot = {};
  snapshot.snapshotTick = xTaskGetTickCount();

  for (std::size_t index = 0; index < kTaskRegistryCapacity; ++index) {
    snapshot.entries[index] = g_registry[index];
    snapshot.entries[index].id = static_cast<TaskId>(index);
    if (snapshot.entries[index].name == nullptr) {
      snapshot.entries[index].name = kUnnamedTaskName;
    }
  }

  return snapshot;
}

}  // namespace mp
