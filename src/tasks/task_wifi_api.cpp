#include "tasks/task_wifi_api.h"

#include <Arduino.h>

#include "config/project_features.h"
#include "rtos/task_registry.h"
#include "services/health_service.h"
#include "services/log_service.h"
#include "services/wifi_api_service.h"

namespace {

constexpr const char* kWifiApiTaskName = "task_wifi_api";
// ESP32 Arduino 的 xTaskCreate() 栈大小参数按字节计算。
// WebServer 处理 HTTP/JSON 时栈使用量高于普通轮询任务。
constexpr std::uint32_t kWifiApiTaskStackBytes = 8192U;
constexpr UBaseType_t kWifiApiTaskPriority = 1U;
constexpr TickType_t kWifiApiPollTicks = pdMS_TO_TICKS(10U);

TaskHandle_t g_wifiApiTaskHandle = nullptr;

#if MP_ENABLE_WIFI
void TaskWifiApiMain(void* /*context*/) {
  mp::RegisterTask(mp::TaskId::WIFI_API, kWifiApiTaskName,
                   xTaskGetCurrentTaskHandle());
  mp::Health_ReportHeartbeat(mp::TaskId::WIFI_API);

  if (!mp::WifiApiService_Begin()) {
    mp::Log_Error("wifi", "WifiApiService_Begin failed");
    vTaskDelete(nullptr);
    return;
  }

  for (;;) {
    mp::WifiApiService_Poll();
    mp::Health_ReportHeartbeat(mp::TaskId::WIFI_API);
    vTaskDelay(kWifiApiPollTicks);
  }
}
#endif

}  // namespace

namespace mp {

bool TaskWifiApi_Create() {
#if MP_ENABLE_WIFI
  if (g_wifiApiTaskHandle != nullptr) {
    return true;
  }

  const BaseType_t createResult =
      xTaskCreate(TaskWifiApiMain, kWifiApiTaskName, kWifiApiTaskStackBytes,
                  nullptr, kWifiApiTaskPriority, &g_wifiApiTaskHandle);

  if (createResult != pdPASS) {
    g_wifiApiTaskHandle = nullptr;
    return false;
  }

  RegisterTask(TaskId::WIFI_API, kWifiApiTaskName, g_wifiApiTaskHandle);
  return true;
#else
  return true;
#endif
}

}  // namespace mp
