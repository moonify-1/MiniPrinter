#include "services/health_service.h"

#include "config/project_features.h"

namespace {

constexpr TickType_t kLoggerTimeoutTicks = pdMS_TO_TICKS(3000U);
constexpr TickType_t kSystemTimeoutTicks = pdMS_TO_TICKS(1000U);
constexpr TickType_t kSensorTimeoutTicks = pdMS_TO_TICKS(500U);
constexpr TickType_t kMonitorTimeoutTicks = pdMS_TO_TICKS(500U);
constexpr TickType_t kPrintTimeoutTicks = pdMS_TO_TICKS(1000U);

mp::HealthSnapshot g_healthSnapshot = {};

// 判断一个任务是否属于当前阶段的“关键任务”。
//
// 当前阶段已经落地并真正运行的关键任务包括：
// - SYSTEM：维护系统状态机。
// - SENSOR：刷新纸张、温度、电池和电机故障事件。
// - LOGGER：负责异步日志输出。
// - MONITOR：负责心跳检查和安全收敛。
// - PRINT_CTRL：打印流程执行任务，超时后应停止所有输出。
bool IsCriticalTask(mp::TaskId id) {
  switch (id) {
    case mp::TaskId::SYSTEM:
    case mp::TaskId::SENSOR:
    case mp::TaskId::LOGGER:
    case mp::TaskId::MONITOR:
    case mp::TaskId::PRINT_CTRL:
      return true;
    default:
      return false;
  }
}

// 获取指定任务的允许心跳超时时间。
TickType_t GetTimeoutTicks(mp::TaskId id) {
  switch (id) {
    case mp::TaskId::SYSTEM:
      return kSystemTimeoutTicks;
    case mp::TaskId::SENSOR:
      return kSensorTimeoutTicks;
    case mp::TaskId::LOGGER:
      return kLoggerTimeoutTicks;
    case mp::TaskId::MONITOR:
      return kMonitorTimeoutTicks;
    case mp::TaskId::PRINT_CTRL:
      return kPrintTimeoutTicks;
    default:
      return 0U;
  }
}

// 生成一份“全健康”的默认快照。
mp::HealthSnapshot MakeDefaultSnapshot() {
  mp::HealthSnapshot snapshot = {};
  snapshot.snapshotTick = xTaskGetTickCount();
  snapshot.allHealthy = true;
  snapshot.hasCriticalTimeout = false;
  snapshot.safeModeRequested = false;
  snapshot.failedTaskId = mp::TaskId::COUNT;
  snapshot.failedTaskAgeTicks = 0U;
  snapshot.taskSnapshot = mp::GetTaskHealthSnapshot();
  return snapshot;
}

}  // namespace

namespace mp {

void Health_Init() {
  g_healthSnapshot = MakeDefaultSnapshot();
}

void Health_ReportHeartbeat(TaskId id) {
  TaskHeartbeat(id);
}

bool Health_CheckAll() {
  HealthSnapshot snapshot = {};
  snapshot.taskSnapshot = GetTaskHealthSnapshot();
  snapshot.snapshotTick = snapshot.taskSnapshot.snapshotTick;
  snapshot.allHealthy = true;
  snapshot.hasCriticalTimeout = false;
  snapshot.safeModeRequested = false;
  snapshot.failedTaskId = TaskId::COUNT;
  snapshot.failedTaskAgeTicks = 0U;

  for (std::size_t index = 0; index < kTaskRegistryCapacity; ++index) {
    const TaskHealthEntry& entry = snapshot.taskSnapshot.entries[index];

    if (!IsCriticalTask(entry.id)) {
      continue;
    }

    if (!entry.isRegistered) {
      snapshot.allHealthy = false;
      snapshot.hasCriticalTimeout = true;
      snapshot.safeModeRequested = true;
      snapshot.failedTaskId = entry.id;
      break;
    }

    const TickType_t timeoutTicks = GetTimeoutTicks(entry.id);
    if (timeoutTicks == 0U) {
      continue;
    }

    const TickType_t ageTicks = snapshot.snapshotTick - entry.lastHeartbeat;
    if (ageTicks > timeoutTicks) {
      snapshot.allHealthy = false;
      snapshot.hasCriticalTimeout = true;
      snapshot.safeModeRequested = true;
      snapshot.failedTaskId = entry.id;
      snapshot.failedTaskAgeTicks = ageTicks;
      break;
    }
  }

#if MP_ENABLE_WDT
  // 这里仅保留后续软件/硬件 WDT 集成的预留位置。
  // Step 07 明确不启用真实 Watchdog，因此当前不做任何喂狗或配置动作。
#endif

  g_healthSnapshot = snapshot;
  return snapshot.allHealthy;
}

HealthSnapshot Health_GetSnapshot() {
  return g_healthSnapshot;
}

}  // namespace mp
