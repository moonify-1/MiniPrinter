#include "tasks/task_monitor.h"

#include <Arduino.h>
#include <cstdio>
#include <cstdint>

#include "bsp/bsp_board.h"
#include "bsp/bsp_watchdog.h"
#include "config/project_features.h"
#include "config/safety_limits.h"
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
bool g_watchdogFeedStopped = false;

#if MP_ENABLE_WDT
bool g_watchdogEnabled = false;
bool g_watchdogFeedFailureLogged = false;
#endif

// 输出一条最短串口故障摘要。
//
// 这里直接使用 Serial，是因为日志任务本身不属于关键任务；如果日志队列或 LogTask
// 已经异常，串口 fallback 仍然可以帮助定位是哪一个关键任务超时。
void PrintFaultSummary(const mp::HealthSnapshot& snapshot) {
  char buffer[96] = {};
  std::snprintf(buffer, sizeof(buffer),
                "FATAL: heartbeat timeout task=%u age=%lu",
                static_cast<unsigned>(snapshot.failedTaskId),
                static_cast<unsigned long>(snapshot.failedTaskAgeTicks));
  Serial.println(buffer);
}

// 初始化真实 ESP32 Task Watchdog。
//
// 该函数只在 MonitorTask 上下文调用，因此 esp_task_wdt_add(nullptr) 订阅的是
// MonitorTask 自己，满足“只有 MonitorTask 可以喂狗”的架构约束。
void InitWatchdogIfEnabled() {
#if MP_ENABLE_WDT
  g_watchdogEnabled =
      mp::Bsp_WatchdogInitForMonitor(mp::SAFETY_WDT_TIMEOUT_MS);

  if (g_watchdogEnabled) {
    mp::Log_Info("monitor", "WDT enabled timeout_ms=%lu reset=%s",
                 static_cast<unsigned long>(mp::SAFETY_WDT_TIMEOUT_MS),
                 mp::Bsp_WatchdogResetReasonText());
    return;
  }

  mp::Log_Error("monitor", "WDT init failed reset=%s",
                mp::Bsp_WatchdogResetReasonText());
#endif
}

// 记录“停止喂狗”的原因。
//
// 一旦进入这个状态，MonitorTask 不再调用 Bsp_WatchdogFeedFromMonitor()。
// 如果 MP_ENABLE_WDT=1，硬件 WDT 会在 5s 后复位系统；如果宏关闭，则只是保持软件安全态。
void StopWatchdogFeed(const char* reason) {
  if (g_watchdogFeedStopped) {
    return;
  }

  g_watchdogFeedStopped = true;
  mp::Log_Fatal("monitor", "WDT feed stopped: %s",
                (reason != nullptr) ? reason : "unknown");
}

// 判断当前周期是否允许喂狗。
//
// 喂狗条件集中在这里，便于后续审查：
// - SystemTask / SensorTask / PrintEngineTask / MonitorTask 心跳正常。
// - ErrorService 没有锁存 FATAL 或 ENTER_SAFE_MODE 类不可恢复错误。
// - 当前没有已经锁存的关键任务超时故障。
bool CanFeedWatchdog(bool criticalTasksHealthy) {
  return criticalTasksHealthy && !g_faultLatched &&
         !mp::Error_IsSafeModeRequired();
}

// 在满足全部条件时喂真实 WDT。
//
// MP_ENABLE_WDT=0 时不会编译任何真实喂狗动作，默认行为保持为纯软件监控。
void FeedWatchdogIfAllowed(bool criticalTasksHealthy) {
#if MP_ENABLE_WDT
  if (!g_watchdogEnabled || g_watchdogFeedStopped) {
    return;
  }

  if (!CanFeedWatchdog(criticalTasksHealthy)) {
    StopWatchdogFeed("feed condition failed");
    return;
  }

  if (!mp::Bsp_WatchdogFeedFromMonitor() && !g_watchdogFeedFailureLogged) {
    g_watchdogFeedFailureLogged = true;
    mp::Log_Error("monitor", "WDT feed failed");
  }
#else
  (void)criticalTasksHealthy;
#endif
}

// 处理关键任务心跳超时。
//
// 顺序必须保守：
// 1. 先收敛所有危险输出。
// 2. 再上报 FATAL 错误。
// 3. 最后停止喂狗，等待硬件 WDT 或软件 SAFE_MODE 接管。
void HandleCriticalTimeout() {
  mp::Bsp_SetAllOutputsSafe();
  StopWatchdogFeed("critical task timeout");

  const mp::HealthSnapshot snapshot = mp::Health_GetSnapshot();
  if (g_faultLatched) {
    return;
  }

  g_faultLatched = true;
  mp::Error_Report(mp::ERR_WDT_TASK_TIMEOUT, "monitor",
                   "critical task heartbeat timeout");
  mp::Log_Fatal("monitor", "Task heartbeat timeout id=%u age=%lu",
                static_cast<unsigned>(snapshot.failedTaskId),
                static_cast<unsigned long>(snapshot.failedTaskAgeTicks));
  PrintFaultSummary(snapshot);
}

// 处理已经由其他模块上报的 FATAL / SAFE_MODE 请求。
//
// ErrorService 在 Error_Report() 中会立即锁存 safeModeRequired，因此即使 qError
// 还没被 SystemTask 消费，MonitorTask 也能看到“存在未处理 FATAL”的事实并停止喂狗。
void HandleFatalErrorPending() {
  mp::Bsp_SetAllOutputsSafe();
  StopWatchdogFeed("fatal error pending");
}

// 监控任务主循环。
//
// 设计目标：
// - 100ms 固定周期执行一次。
// - 优先级高于日志任务。
// - 只由本任务初始化和喂真实 Watchdog。
// - 发现关键任务超时或 FATAL 错误后立刻进入安全输出并停止喂狗。
void TaskMonitorMain(void* /*context*/) {
  RegisterTask(mp::TaskId::MONITOR, kMonitorTaskName, xTaskGetCurrentTaskHandle());
  mp::Health_ReportHeartbeat(mp::TaskId::MONITOR);
  mp::Log_Info("monitor", "MonitorTask alive");
  InitWatchdogIfEnabled();

  TickType_t lastWakeTick = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&lastWakeTick, kMonitorPeriodTicks);
    mp::Health_ReportHeartbeat(mp::TaskId::MONITOR);

    const bool healthy = mp::Health_CheckAll();
    if (!healthy) {
      HandleCriticalTimeout();
      continue;
    }

    if (mp::Error_IsSafeModeRequired()) {
      HandleFatalErrorPending();
      continue;
    }

    FeedWatchdogIfAllowed(healthy);
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
