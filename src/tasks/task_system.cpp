#include "tasks/task_system.h"

#include <Arduino.h>
#include <cstdio>
#include <cstdint>

#include "app/app_system.h"
#include "bsp/bsp_board.h"
#include "common/app_error.h"
#include "rtos/rtos_objects.h"
#include "rtos/task_registry.h"
#include "services/error_service.h"
#include "services/health_service.h"
#include "services/log_service.h"

namespace {

constexpr const char* kSystemTaskName = "task_system";
constexpr uint32_t kSystemTaskStackWords = 1024U;
constexpr UBaseType_t kSystemTaskPriority = 2U;
constexpr TickType_t kSystemTaskPeriodTicks = pdMS_TO_TICKS(100U);
constexpr TickType_t kErrorQueuePollTicks = 0U;

TaskHandle_t g_systemTaskHandle = nullptr;

struct ErrorDrainResult {
  bool hasWarn;
  bool hasError;
  bool hasFatal;
  mp::ErrorEvent selected;
};

// 把系统状态转成字符串，便于日志输出。
const char* StateToString(mp::SystemState state) {
  switch (state) {
    case mp::SystemState::BOOT:
      return "BOOT";
    case mp::SystemState::INIT:
      return "INIT";
    case mp::SystemState::SELF_TEST:
      return "SELF_TEST";
    case mp::SystemState::IDLE:
      return "IDLE";
    case mp::SystemState::READY:
      return "READY";
    case mp::SystemState::RUNNING:
      return "RUNNING";
    case mp::SystemState::ERROR:
      return "ERROR";
    case mp::SystemState::RECOVERY:
      return "RECOVERY";
    case mp::SystemState::SAFE_MODE:
      return "SAFE_MODE";
    case mp::SystemState::LOW_POWER:
      return "LOW_POWER";
    case mp::SystemState::SHUTDOWN:
      return "SHUTDOWN";
    default:
      return "UNKNOWN";
  }
}

// 读取系统事件组，并转换为状态机输入。
mp::SystemAppInput BuildInputFromEvents(EventBits_t bits, bool hasError,
                                        mp::AppErrorCode errorCode) {
  mp::SystemAppInput input = {};
  input.paperPresent = (bits & mp::EVT_PAPER_PRESENT) != 0U;
  input.tempOk = (bits & mp::EVT_TEMP_OK) != 0U;
  input.batOk = (bits & mp::EVT_BAT_OK) != 0U;
  input.drvReady = (bits & mp::EVT_DRV_READY) != 0U;
  input.vhOff = (bits & mp::EVT_VH_OFF) != 0U;
  input.printActive = (bits & mp::EVT_PRINT_ACTIVE) != 0U;
  input.safeModeRequested = (bits & mp::EVT_SAFE_MODE) != 0U;
  input.lowPowerRequested = (bits & mp::EVT_LOW_POWER) != 0U;
  input.errorPending = (bits & mp::EVT_ERROR_PENDING) != 0U;
  input.hasError = hasError;
  input.errorCode = errorCode;
  return input;
}

bool IsMoreSevere(mp::ErrorSeverity lhs, mp::ErrorSeverity rhs) {
  return static_cast<std::uint8_t>(lhs) > static_cast<std::uint8_t>(rhs);
}

// 尝试从 qError 取出所有待处理错误。
//
// qError 当前传递 ErrorEvent，所以 SystemTask 可以按严重级别分流：
// - WARN：只记录，不强制改变状态。
// - ERROR：进入 ERROR，下一轮尝试 RECOVERY。
// - FATAL：立即安全输出，并进入 SAFE_MODE。
ErrorDrainResult DrainErrorQueue() {
  ErrorDrainResult result = {};
  result.selected.code = mp::APP_OK;
  result.selected.severity = mp::ErrorSeverity::NONE;

  if (mp::g_rtos.qError == nullptr) {
    return result;
  }

  mp::ErrorEvent event = {};
  while (xQueueReceive(mp::g_rtos.qError, &event, kErrorQueuePollTicks) ==
         pdPASS) {
    if (IsMoreSevere(event.severity, result.selected.severity)) {
      result.selected = event;
    }

    switch (event.severity) {
      case mp::ErrorSeverity::WARN:
        result.hasWarn = true;
        mp::Log_Warn("system", "WARN %s code=0x%08lX %s", event.module,
                     static_cast<unsigned long>(event.code), event.detail);
        break;

      case mp::ErrorSeverity::ERROR:
        result.hasError = true;
        mp::Log_Error("system", "ERROR %s code=0x%08lX %s", event.module,
                      static_cast<unsigned long>(event.code), event.detail);
        break;

      case mp::ErrorSeverity::FATAL:
        result.hasFatal = true;
        result.selected = event;
        mp::Bsp_SetAllOutputsSafe();
        mp::Log_Fatal("system", "FATAL %s code=0x%08lX %s", event.module,
                      static_cast<unsigned long>(event.code), event.detail);
        break;

      default:
        break;
    }
  }

  return result;
}

// 处理状态迁移后的配套动作。
void HandleStateTransition(mp::SystemState previous, mp::SystemState current) {
  if (previous == current) {
    return;
  }

  mp::Log_Info("system", "State %s -> %s", StateToString(previous),
               StateToString(current));

  if (mp::g_rtos.systemEvents != nullptr) {
    if (previous == mp::SystemState::INIT &&
        current == mp::SystemState::SELF_TEST) {
      xEventGroupSetBits(mp::g_rtos.systemEvents, mp::EVT_INIT_OK);
    }

    if (previous == mp::SystemState::SELF_TEST &&
        current == mp::SystemState::IDLE) {
      xEventGroupSetBits(mp::g_rtos.systemEvents, mp::EVT_SELFTEST_OK);
    }

    if (current == mp::SystemState::SAFE_MODE) {
      xEventGroupSetBits(mp::g_rtos.systemEvents,
                         mp::EVT_SAFE_MODE | mp::EVT_ERROR_PENDING);
    }
  }
}

// 系统任务主循环。
//
// 当前阶段只做最小状态机驱动，不处理业务打印流程。
void TaskSystemMain(void* /*context*/) {
  mp::RegisterTask(mp::TaskId::SYSTEM, kSystemTaskName,
                   xTaskGetCurrentTaskHandle());
  mp::Health_ReportHeartbeat(mp::TaskId::SYSTEM);

  mp::SystemApp_Init();
  mp::Log_Info("system", "SystemTask alive");

  if (mp::g_rtos.systemEvents != nullptr) {
    // 当前系统启动路径始终从安全输出开始，因此先标记 VH 关闭。
    xEventGroupSetBits(mp::g_rtos.systemEvents, mp::EVT_VH_OFF);
  }

  TickType_t lastWakeTick = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&lastWakeTick, kSystemTaskPeriodTicks);
    mp::Health_ReportHeartbeat(mp::TaskId::SYSTEM);

    const mp::SystemState previousState = mp::SystemApp_GetState();
    const ErrorDrainResult errorDrain = DrainErrorQueue();
    const bool hasStateChangingError =
        errorDrain.hasError || errorDrain.hasFatal;

    if ((errorDrain.hasWarn || errorDrain.hasError) && !errorDrain.hasFatal) {
      mp::Error_ClearRecoverable();
    }

    EventBits_t bits = 0U;
    if (mp::g_rtos.systemEvents != nullptr) {
      bits = xEventGroupGetBits(mp::g_rtos.systemEvents);
    }

    const mp::SystemAppInput input =
        BuildInputFromEvents(bits, hasStateChangingError,
                             errorDrain.selected.code);
    mp::SystemApp_ProcessEvent(input);

    const mp::SystemState currentState = mp::SystemApp_GetState();
    HandleStateTransition(previousState, currentState);

    if (currentState == mp::SystemState::SAFE_MODE) {
      mp::Bsp_SetAllOutputsSafe();
    }
  }
}

}  // namespace

namespace mp {

bool TaskSystem_Create() {
  if (g_systemTaskHandle != nullptr) {
    return true;
  }

  const BaseType_t createResult =
      xTaskCreate(TaskSystemMain, kSystemTaskName, kSystemTaskStackWords,
                  nullptr, kSystemTaskPriority, &g_systemTaskHandle);

  if (createResult != pdPASS) {
    g_systemTaskHandle = nullptr;
    return false;
  }

  RegisterTask(TaskId::SYSTEM, kSystemTaskName, g_systemTaskHandle);
  return true;
}

}  // namespace mp
