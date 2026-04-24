#include "services/health_service.h"

#include "config/project_features.h"

namespace {

constexpr TickType_t kSystemTimeoutTicks = pdMS_TO_TICKS(1000U);
constexpr TickType_t kSensorTimeoutTicks = pdMS_TO_TICKS(500U);
constexpr TickType_t kMonitorTimeoutTicks = pdMS_TO_TICKS(500U);
constexpr TickType_t kPrintTimeoutTicks = pdMS_TO_TICKS(1000U);

mp::HealthSnapshot g_healthSnapshot = {};

// 判断一个任务是否属于 Watchdog 喂狗条件里的“关键任务”。
//
// 关键任务只保留会直接影响安全状态和打印动作的任务：
// - SYSTEM：维护系统状态机。
// - SENSOR：刷新纸张、温度、电池和电机故障状态。
// - MONITOR：执行心跳检查和安全收敛。
// - PRINT_CTRL：执行打印流程，卡死时必须停止输出。
//
// LOGGER 不在这里：日志任务卡住会影响可观测性，但不应直接触发硬件复位。
bool IsCriticalTask(mp::TaskId id) {
  switch (id) {
    case mp::TaskId::SYSTEM:
    case mp::TaskId::SENSOR:
    case mp::TaskId::MONITOR:
    case mp::TaskId::PRINT_CTRL:
      return true;
    default:
      return false;
  }
}

// 获取指定关键任务允许的最大心跳间隔。
//
// FreeRTOS 的 TickType_t 是系统 tick 计数类型；pdMS_TO_TICKS() 会把毫秒
// 转成 tick，避免手工依赖 configTICK_RATE_HZ。
TickType_t GetTimeoutTicks(mp::TaskId id) {
  switch (id) {
    case mp::TaskId::SYSTEM:
      return kSystemTimeoutTicks;
    case mp::TaskId::SENSOR:
      return kSensorTimeoutTicks;
    case mp::TaskId::MONITOR:
      return kMonitorTimeoutTicks;
    case mp::TaskId::PRINT_CTRL:
      return kPrintTimeoutTicks;
    default:
      return 0U;
  }
}

// 生成一份“全健康”的默认快照。
//
// Health_Init() 会使用它建立初始状态；后续 Health_CheckAll() 会覆盖为真实检查结果。
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

  g_healthSnapshot = snapshot;
  return snapshot.allHealthy;
}

HealthSnapshot Health_GetSnapshot() {
  return g_healthSnapshot;
}

}  // namespace mp
