#include "tasks/task_log.h"

#include <Arduino.h>
#include <cstdint>

#include "rtos/rtos_objects.h"
#include "rtos/task_registry.h"
#include "services/health_service.h"
#include "services/log_service.h"

namespace {

constexpr const char* kLogTaskName = "task_log";
constexpr uint32_t kLogTaskStackWords = 1024U;
constexpr UBaseType_t kLogTaskPriority = 1U;
constexpr TickType_t kLogQueueWaitTicks = pdMS_TO_TICKS(200U);
constexpr TickType_t kHeartbeatPeriodTicks = pdMS_TO_TICKS(1000U);

TaskHandle_t g_logTaskHandle = nullptr;

// 把日志级别转成文本。
const char* LevelToString(mp::LogLevel level) {
  switch (level) {
    case mp::LogLevel::FATAL:
      return "FATAL";
    case mp::LogLevel::ERROR:
      return "ERROR";
    case mp::LogLevel::WARN:
      return "WARN";
    case mp::LogLevel::INFO:
      return "INFO";
    case mp::LogLevel::DEBUG:
      return "DEBUG";
    case mp::LogLevel::TRACE:
      return "TRACE";
    default:
      return "UNK";
  }
}

// 通过串口输出一条异步日志。
void PrintLogMessage(const mp::LogMsg& msg) {
  Serial.print("[");
  Serial.print(msg.tickCount);
  Serial.print("][");
  Serial.print(LevelToString(msg.level));
  Serial.print("][");
  Serial.print(msg.module);
  Serial.print("] ");
  Serial.println(msg.text);
}

// 日志任务主循环。
//
// 注意点：
// - 队列等待必须有 timeout，避免永远阻塞。
// - 日志任务优先级低，避免影响更重要的控制路径。
// - 心跳周期固定为 1000ms。
void TaskLogMain(void* /*context*/) {
  mp::RegisterTask(mp::TaskId::LOGGER, kLogTaskName, xTaskGetCurrentTaskHandle());
  TickType_t lastHeartbeatTick = xTaskGetTickCount();
  mp::Health_ReportHeartbeat(mp::TaskId::LOGGER);

  for (;;) {
    mp::LogMsg msg = {};

    if (xQueueReceive(mp::g_rtos.qLog, &msg, kLogQueueWaitTicks) == pdPASS) {
      PrintLogMessage(msg);
    }

    const TickType_t now = xTaskGetTickCount();
    if ((now - lastHeartbeatTick) >= kHeartbeatPeriodTicks) {
      mp::Health_ReportHeartbeat(mp::TaskId::LOGGER);
      lastHeartbeatTick = now;
    }
  }
}

}  // namespace

namespace mp {

bool TaskLog_Create() {
  if (g_logTaskHandle != nullptr) {
    return true;
  }

  if (g_rtos.qLog == nullptr) {
    return false;
  }

  const BaseType_t createResult =
      xTaskCreate(TaskLogMain, kLogTaskName, kLogTaskStackWords, nullptr,
                  kLogTaskPriority, &g_logTaskHandle);

  if (createResult != pdPASS) {
    g_logTaskHandle = nullptr;
    return false;
  }

  RegisterTask(TaskId::LOGGER, kLogTaskName, g_logTaskHandle);
  return true;
}

}  // namespace mp
