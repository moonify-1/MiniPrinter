#include "tasks/task_param.h"

#include <Arduino.h>
#include <cstdint>

#include "rtos/rtos_objects.h"
#include "rtos/task_registry.h"
#include "services/health_service.h"
#include "services/log_service.h"
#include "services/param_service.h"

namespace {

constexpr const char* kParamTaskName = "task_param";
constexpr std::uint32_t kParamTaskStackWords = 2048U;
constexpr UBaseType_t kParamTaskPriority = 1U;
constexpr TickType_t kParamQueueWaitTicks = pdMS_TO_TICKS(200U);
constexpr TickType_t kSaveMergeDelayTicks = pdMS_TO_TICKS(2000U);
constexpr TickType_t kPrintingDeferTicks = pdMS_TO_TICKS(500U);

TaskHandle_t g_paramTaskHandle = nullptr;

bool IsPrintActive() {
  if (mp::g_rtos.systemEvents == nullptr) {
    return false;
  }

  const EventBits_t bits = xEventGroupGetBits(mp::g_rtos.systemEvents);
  return (bits & mp::EVT_PRINT_ACTIVE) != 0U;
}

// 参数任务主循环。
//
// 保存策略：
// - 收到保存请求后不立刻写 Flash。
// - 等待 2 秒，如果期间又收到请求，就重新计时。
// - 如果正在打印，继续延迟，避免打印过程中写 Flash 带来抖动风险。
void TaskParamMain(void* /*context*/) {
  mp::RegisterTask(mp::TaskId::PARAM, kParamTaskName,
                   xTaskGetCurrentTaskHandle());
  mp::Health_ReportHeartbeat(mp::TaskId::PARAM);
  mp::Log_Info("param", "ParamTask alive");

  bool savePending = false;
  TickType_t saveDueTick = 0U;

  for (;;) {
    std::uint32_t request = 0U;
    if (xQueueReceive(mp::g_rtos.qParam, &request, kParamQueueWaitTicks) ==
        pdPASS) {
      if (request == mp::PARAM_REQUEST_SAVE) {
        savePending = true;
        saveDueTick = xTaskGetTickCount() + kSaveMergeDelayTicks;
      }
    }

    mp::Health_ReportHeartbeat(mp::TaskId::PARAM);

    if (!savePending) {
      continue;
    }

    const TickType_t now = xTaskGetTickCount();
    if (static_cast<BaseType_t>(now - saveDueTick) < 0) {
      continue;
    }

    if (IsPrintActive()) {
      saveDueTick = now + kPrintingDeferTicks;
      continue;
    }

    (void)mp::Param_SaveNowForTask();
    savePending = false;
  }
}

}  // namespace

namespace mp {

bool TaskParam_Create() {
  if (g_paramTaskHandle != nullptr) {
    return true;
  }

  if (g_rtos.qParam == nullptr) {
    return false;
  }

  const BaseType_t createResult =
      xTaskCreate(TaskParamMain, kParamTaskName, kParamTaskStackWords, nullptr,
                  kParamTaskPriority, &g_paramTaskHandle);

  if (createResult != pdPASS) {
    g_paramTaskHandle = nullptr;
    return false;
  }

  RegisterTask(TaskId::PARAM, kParamTaskName, g_paramTaskHandle);
  return true;
}

}  // namespace mp
