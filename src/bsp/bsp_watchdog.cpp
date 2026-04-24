#include "bsp/bsp_watchdog.h"

#include "config/project_features.h"

#if MP_ENABLE_WDT
#include <algorithm>

#include "esp_err.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

namespace {

#if MP_ENABLE_WDT
bool g_watchdogReady = false;

// ESP32 Arduino 当前使用的 Task Watchdog API 以“秒”为单位配置超时。
// 上层使用毫秒更直观，因此这里统一做向上取整，避免 5001ms 被截断成 5s。
std::uint32_t ToTimeoutSeconds(std::uint32_t timeoutMs) {
  const std::uint32_t rounded = (timeoutMs + 999U) / 1000U;
  return std::max<std::uint32_t>(1U, rounded);
}
#endif

}  // namespace

namespace mp {

bool Bsp_WatchdogInitForMonitor(std::uint32_t timeoutMs) {
#if MP_ENABLE_WDT
  if (g_watchdogReady) {
    return true;
  }

  const std::uint32_t timeoutSeconds = ToTimeoutSeconds(timeoutMs);

  // esp_task_wdt_init() 初始化 ESP32 的 Task Watchdog。
  // 第二个参数为 true 表示超时后触发 panic/reset；开发阶段先用 5s，便于观察日志。
  const esp_err_t initResult = esp_task_wdt_init(timeoutSeconds, true);
  if (initResult != ESP_OK && initResult != ESP_ERR_INVALID_STATE) {
    return false;
  }

  // esp_task_wdt_add(nullptr) 表示订阅“当前正在运行的任务”。
  // 本函数只允许 MonitorTask 调用，因此硬件 WDT 只会监控 MonitorTask 的喂狗节奏。
  const esp_err_t addResult = esp_task_wdt_add(nullptr);
  if (addResult != ESP_OK && addResult != ESP_ERR_INVALID_ARG) {
    return false;
  }

  g_watchdogReady = true;
  return true;
#else
  (void)timeoutMs;
  return false;
#endif
}

bool Bsp_WatchdogFeedFromMonitor() {
#if MP_ENABLE_WDT
  if (!g_watchdogReady) {
    return false;
  }

  // esp_task_wdt_reset() 会重置当前任务的 WDT 计时器。
  // 因为只有 MonitorTask 会调用本函数，所以满足“只有 MonitorTask 喂狗”的约束。
  return esp_task_wdt_reset() == ESP_OK;
#else
  return false;
#endif
}

const char* Bsp_WatchdogResetReasonText() {
#if MP_ENABLE_WDT
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
      return "POWERON";
    case ESP_RST_EXT:
      return "EXT";
    case ESP_RST_SW:
      return "SW";
    case ESP_RST_PANIC:
      return "PANIC";
    case ESP_RST_INT_WDT:
      return "INT_WDT";
    case ESP_RST_TASK_WDT:
      return "TASK_WDT";
    case ESP_RST_WDT:
      return "OTHER_WDT";
    case ESP_RST_DEEPSLEEP:
      return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
      return "BROWNOUT";
    case ESP_RST_SDIO:
      return "SDIO";
    default:
      return "UNKNOWN";
  }
#else
  return "WDT disabled";
#endif
}

}  // namespace mp
