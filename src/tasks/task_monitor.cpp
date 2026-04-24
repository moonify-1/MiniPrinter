#include "tasks/task_monitor.h"

#include <Arduino.h>
#include <cstdio>
#include <cstdint>

#include "bsp/bsp_board.h"
#include "config/project_features.h"
#include "rtos/rtos_objects.h"
#include "rtos/task_registry.h"
#include "services/error_service.h"
#include "services/health_service.h"
#include "services/log_service.h"

namespace {

constexpr const char* kMonitorTaskName = "task_monitor";
constexpr uint32_t kMonitorTaskStackWords = 1024U;
constexpr UBaseType_t kMonitorTaskPriority = 2U;
constexpr TickType_t kMonitorPeriodTicks = pdMS_TO_TICKS(100U);

TaskHandle_t g_monitorTaskHandle = nullptr;
bool g_faultLatched = false;

// 输出一条简单的串口故障摘要。
//
// 即使日志任务已经超时，这条串口输出仍然能帮助定位问题。
void PrintFaultSummary(const mp::HealthSnapshot& snapshot) {
  char buffer[96] = {};
  std::snprintf(buffer, sizeof(buffer),
                "FATAL: heartbeat timeout task=%u age=%lu",
                static_cast<unsigned>(snapshot.failedTaskId),
                static_cast<unsigned long>(snapshot.failedTaskAgeTicks));
  Serial.println(buffer);
}

// 监控任务主循环。
//
// 设计目标：
// - 100ms 固定周期执行一次。
// - 优先级高于日志任务。
// - 发现关键任务超时后立即收敛到安全态。
void TaskMonitorMain(void* /*context*/) {
  RegisterTask(mp::TaskId::MONITOR, kMonitorTaskName, xTaskGetCurrentTaskHandle());
  mp::Health_ReportHeartbeat(mp::TaskId::MONITOR);
  mp::Log_Info("monitor", "MonitorTask alive");

  TickType_t lastWakeTick = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&lastWakeTick, kMonitorPeriodTicks);
    mp::Health_ReportHeartbeat(mp::TaskId::MONITOR);

    const bool healthy = mp::Health_CheckAll();

#if MP_ENABLE_WDT
    // 这里预留后续 WDT 接入点。
    // Step 07 明确不启用真实硬件 Watchdog，因此当前不做任何动作。
#endif

    if (healthy) {
      continue;
    }

    // 一旦检测到关键任务异常，先立即把输出拉回安全态。
    mp::Bsp_SetAllOutputsSafe();

    const mp::HealthSnapshot snapshot = mp::Health_GetSnapshot();
    if (!g_faultLatched) {
      g_faultLatched = true;
      mp::Error_Report(mp::ERR_WDT_TASK_TIMEOUT, "monitor",
                       "critical task heartbeat timeout");
      mp::Log_Fatal("monitor", "Task heartbeat timeout id=%u age=%lu",
                    static_cast<unsigned>(snapshot.failedTaskId),
                    static_cast<unsigned long>(snapshot.failedTaskAgeTicks));
      PrintFaultSummary(snapshot);
    }
  }
}

}  // namespace

namespace mp {

bool TaskMonitor_Create() {
  if (g_monitorTaskHandle != nullptr) {
    return true;
  }

  const BaseType_t createResult =
      xTaskCreate(TaskMonitorMain, kMonitorTaskName, kMonitorTaskStackWords,
                  nullptr, kMonitorTaskPriority, &g_monitorTaskHandle);

  if (createResult != pdPASS) {
    g_monitorTaskHandle = nullptr;
    return false;
  }

  RegisterTask(TaskId::MONITOR, kMonitorTaskName, g_monitorTaskHandle);
  return true;
}

}  // namespace mp
