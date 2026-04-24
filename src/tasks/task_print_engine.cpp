#include "tasks/task_print_engine.h"

#include <Arduino.h>
#include <cstdint>

#include "app/app_system.h"
#include "app/app_print.h"
#include "bsp/bsp_board.h"
#include "common/fixed_pool.h"
#include "config/default_params.h"
#include "drivers/stepper/drv_stepper.h"
#include "drivers/thermal_head/drv_thermal_head.h"
#include "rtos/rtos_objects.h"
#include "rtos/task_registry.h"
#include "services/health_service.h"
#include "services/log_service.h"
#include "services/param_service.h"
#include "services/print_service.h"
#include "services/print_spooler.h"
#include "services/sensor_service.h"

namespace {

constexpr const char* kPrintTaskName = "task_print_engine";
constexpr std::uint32_t kPrintTaskStackWords = 3072U;
constexpr UBaseType_t kPrintTaskPriority = 2U;
constexpr TickType_t kControlWaitTicks = pdMS_TO_TICKS(20U);
constexpr TickType_t kLineWaitTicks = pdMS_TO_TICKS(20U);
constexpr TickType_t kMockFeedDelayTicks = pdMS_TO_TICKS(5U);
constexpr TickType_t kRunningStatePollTicks = pdMS_TO_TICKS(10U);
constexpr std::uint32_t kMaxMockFeedSteps = 200U;
constexpr std::uint8_t kRunningStatePollMax = 20U;

TaskHandle_t g_printTaskHandle = nullptr;
bool g_mockDriversReady = false;
bool g_realDriversReady = false;

// 清空已经排队但尚未模拟打印的行。
//
// 取消或异常时必须释放这些 LineBuffer，
// 否则固定池里的缓冲会被耗尽，后续 PRINT_LINE 无法继续入队。
void DrainQueuedLines() {
  if (mp::g_rtos.qLineReady == nullptr) {
    return;
  }

  for (;;) {
    mp::LineBuffer* line = nullptr;
    if (xQueueReceive(mp::g_rtos.qLineReady, &line, 0U) != pdPASS) {
      return;
    }
    (void)mp::PrintSpooler_FreeLine(line);
  }
}

void FinishJobIfIdle() {
  if (!mp::PrintApp_IsActive() || !mp::PrintApp_IsEndRequested()) {
    return;
  }

  if (mp::g_rtos.qLineReady != nullptr &&
      uxQueueMessagesWaiting(mp::g_rtos.qLineReady) > 0U) {
    return;
  }

  const mp::PrintAppSnapshot snapshot = mp::PrintApp_GetSnapshot();
  mp::PrintApp_Done();
  mp::PrintService_SetPrintActive(false);
  (void)mp::GetThermalHeadMockDriver().setSafe();
  mp::GetStepperMockDriver().release();

  if (mp::PrintService_IsRealPrintHardwareEnabled() && g_realDriversReady) {
    (void)mp::GetThermalHeadDriver().setSafe();
    mp::GetStepperDriver().release();
    mp::GetStepperDriver().sleep();
    g_realDriversReady = false;
  }

  mp::Log_Info("print", "Print done job=%lu lines=%lu",
               static_cast<unsigned long>(snapshot.jobId),
               static_cast<unsigned long>(snapshot.printedLines));
}

void StopLineOutputs(mp::ThermalHeadDriver& head, mp::StepperDriver& stepper,
                     bool useRealHardware) {
  // 失败时必须先通过 driver 语义关闭 STB 和 VH，再释放电机。
  // Bsp_SetAllOutputsSafe() 作为最后兜底，防止 driver 状态与实际 GPIO 不一致。
  (void)head.allStbOff();
  (void)head.setVh(false);
  stepper.release();
  if (useRealHardware) {
    stepper.sleep();
  }
  mp::Bsp_SetAllOutputsSafe();
}

void HandlePrintError(mp::AppErrorCode error, const char* reason,
                      mp::ThermalHeadDriver& head,
                      mp::StepperDriver& stepper,
                      bool useRealHardware) {
  StopLineOutputs(head, stepper, useRealHardware);
  DrainQueuedLines();

  mp::PrintApp_Failed(error);
  mp::PrintService_SetPrintActive(false);
  mp::PrintService_ReportError(error);
  g_mockDriversReady = false;
  g_realDriversReady = false;
  mp::Log_Error("print", "Print error reason=%s code=0x%08lX",
                reason != nullptr ? reason : "unknown",
                static_cast<unsigned long>(error));
}

bool WaitUntilSystemRunning() {
  for (std::uint8_t retry = 0U; retry < kRunningStatePollMax; ++retry) {
    if (mp::SystemApp_GetState() == mp::SystemState::RUNNING) {
      return true;
    }

    mp::Health_ReportHeartbeat(mp::TaskId::PRINT_CTRL);
    vTaskDelay(kRunningStatePollTicks);
  }

  return mp::SystemApp_GetState() == mp::SystemState::RUNNING;
}

bool EnsureDriversReady(bool useRealHardware) {
  if (useRealHardware) {
    if (g_realDriversReady) {
      return true;
    }

    mp::ThermalHeadDriver& head = mp::GetThermalHeadDriver();
    mp::StepperDriver& stepper = mp::GetStepperDriver();
    if (!head.init() || !stepper.init()) {
      StopLineOutputs(head, stepper, true);
      return false;
    }

    g_realDriversReady = true;
    return true;
  }

  if (g_mockDriversReady) {
    return true;
  }

  mp::ThermalHeadDriver& head = mp::GetThermalHeadMockDriver();
  mp::StepperDriver& stepper = mp::GetStepperMockDriver();
  if (!head.init() || !stepper.init()) {
    return false;
  }

  g_mockDriversReady = true;
  return true;
}

void HandleControl(const mp::PrintControlMsg& msg) {
  switch (msg.type) {
    case mp::PrintControlType::PRINT_START:
      mp::PrintApp_Start(msg.jobId);
      mp::PrintService_SetPrintActive(true);
      g_mockDriversReady = false;
      g_realDriversReady = false;
      if (!mp::PrintService_IsRealPrintHardwareEnabled()) {
        (void)EnsureDriversReady(false);
        (void)mp::GetThermalHeadMockDriver().setSafe();
        mp::GetStepperMockDriver().setSafe();
      }
      mp::Log_Info("print", "PRINT_START job=%lu mode=%s",
                   static_cast<unsigned long>(msg.jobId),
                   mp::PrintService_IsRealPrintHardwareEnabled() ? "real-guarded"
                                                                 : "mock");
      break;

    case mp::PrintControlType::PRINT_END:
      mp::PrintApp_RequestEnd();
      mp::Log_Info("print", "PRINT_END job=%lu",
                   static_cast<unsigned long>(msg.jobId));
      FinishJobIfIdle();
      break;

    case mp::PrintControlType::PRINT_CANCEL:
      DrainQueuedLines();
      (void)mp::GetThermalHeadMockDriver().setSafe();
      mp::GetStepperMockDriver().setSafe();
      if (mp::PrintService_IsRealPrintHardwareEnabled()) {
        (void)mp::GetThermalHeadDriver().setSafe();
        mp::GetStepperDriver().setSafe();
      }
      mp::PrintApp_Cancel();
      mp::PrintService_SetPrintActive(false);
      g_mockDriversReady = false;
      g_realDriversReady = false;
      mp::Log_Warn("print", "PRINT_CANCEL job=%lu",
                   static_cast<unsigned long>(msg.jobId));
      break;

    case mp::PrintControlType::FEED: {
      const std::uint32_t steps =
          (msg.value == 0U) ? 1U : ((msg.value > kMaxMockFeedSteps)
                                        ? kMaxMockFeedSteps
                                        : msg.value);
      mp::GetStepperMockDriver().wake();
      for (std::uint32_t index = 0U; index < steps; ++index) {
        mp::GetStepperMockDriver().stepForward();
        vTaskDelay(kMockFeedDelayTicks);
      }
      mp::GetStepperMockDriver().release();
      mp::GetStepperMockDriver().sleep();
      mp::Log_Info("print", "FEED mock steps=%lu",
                   static_cast<unsigned long>(steps));
      break;
    }

    default:
      break;
  }
}

void LogLinePlan(const mp::LineBuffer* line, const mp::PrintLinePlan& plan) {
  if (line == nullptr) {
    return;
  }

  mp::Log_Info(
      "print",
      "Line mode=%s job=%lu line=%lu black=%u groups=%u/%u/%u/%u/%u/%u",
      mp::PrintService_IsRealPrintHardwareEnabled() ? "real" : "mock",
      static_cast<unsigned long>(line->jobId),
      static_cast<unsigned long>(line->lineNo),
      static_cast<unsigned>(line->blackDotCount),
      static_cast<unsigned>(plan.groupBlackDots[0]),
      static_cast<unsigned>(plan.groupBlackDots[1]),
      static_cast<unsigned>(plan.groupBlackDots[2]),
      static_cast<unsigned>(plan.groupBlackDots[3]),
      static_cast<unsigned>(plan.groupBlackDots[4]),
      static_cast<unsigned>(plan.groupBlackDots[5]));

  mp::Log_Debug(
      "print",
      "Pulse us=%lu/%lu/%lu/%lu/%lu/%lu",
      static_cast<unsigned long>(plan.pulseUs[0]),
      static_cast<unsigned long>(plan.pulseUs[1]),
      static_cast<unsigned long>(plan.pulseUs[2]),
      static_cast<unsigned long>(plan.pulseUs[3]),
      static_cast<unsigned long>(plan.pulseUs[4]),
      static_cast<unsigned long>(plan.pulseUs[5]));
}

void ProcessLine(mp::LineBuffer* line) {
  if (line == nullptr) {
    return;
  }

  if (!mp::PrintApp_IsActive()) {
    mp::Log_Warn("print", "Drop line while print inactive");
    (void)mp::PrintSpooler_FreeLine(line);
    return;
  }

  const bool useRealHardware = mp::PrintService_IsRealPrintHardwareEnabled();
  if (useRealHardware && !WaitUntilSystemRunning()) {
    mp::ThermalHeadDriver& head = mp::GetThermalHeadDriver();
    mp::StepperDriver& stepper = mp::GetStepperDriver();
    (void)mp::PrintSpooler_FreeLine(line);
    HandlePrintError(mp::ERR_SYS_INIT_FAILED, "system not RUNNING",
                     head, stepper, true);
    return;
  }

  // 打印中每一行都重新读取最近一次传感器快照。
  const mp::SensorSnapshot sensor = mp::SensorService_GetSnapshot();
  const mp::ParamBlock params = mp::Param_GetSnapshot();
  mp::PrintLinePlan plan = {};

  const mp::AppErrorCode planResult =
      mp::PrintService_BuildLinePlan(line, sensor, params, &plan);
  if (planResult != mp::APP_OK) {
    (void)mp::PrintSpooler_FreeLine(line);
    mp::ThermalHeadDriver& head =
        useRealHardware ? mp::GetThermalHeadDriver()
                        : mp::GetThermalHeadMockDriver();
    mp::StepperDriver& stepper =
        useRealHardware ? mp::GetStepperDriver()
                        : mp::GetStepperMockDriver();
    HandlePrintError(planResult, "line safety check", head, stepper,
                     useRealHardware);
    return;
  }

  if (useRealHardware) {
    const mp::AppErrorCode allowReal =
        mp::PrintService_CheckRealPrintAllowed(sensor, params);
    if (allowReal != mp::APP_OK) {
      (void)mp::PrintSpooler_FreeLine(line);
      mp::ThermalHeadDriver& head = mp::GetThermalHeadDriver();
      mp::StepperDriver& stepper = mp::GetStepperDriver();
      HandlePrintError(allowReal, "real print guard", head, stepper, true);
      return;
    }
  }

  if (!EnsureDriversReady(useRealHardware)) {
    (void)mp::PrintSpooler_FreeLine(line);
    mp::ThermalHeadDriver& head =
        useRealHardware ? mp::GetThermalHeadDriver()
                        : mp::GetThermalHeadMockDriver();
    mp::StepperDriver& stepper =
        useRealHardware ? mp::GetStepperDriver()
                        : mp::GetStepperMockDriver();
    HandlePrintError(mp::ERR_SYS_INIT_FAILED, "driver init failed", head,
                     stepper, useRealHardware);
    return;
  }

  mp::ThermalHeadDriver& head =
      useRealHardware ? mp::GetThermalHeadDriver()
                      : mp::GetThermalHeadMockDriver();
  mp::StepperDriver& stepper =
      useRealHardware ? mp::GetStepperDriver() : mp::GetStepperMockDriver();

  if (!head.shiftLine48Bytes(line->data, mp::LINE_BUFFER_DATA_SIZE)) {
    (void)mp::PrintSpooler_FreeLine(line);
    HandlePrintError(mp::ERR_HEAD_HEAT_TIMEOUT, "shift failed", head, stepper,
                     useRealHardware);
    return;
  }

  if (!head.latch()) {
    (void)mp::PrintSpooler_FreeLine(line);
    HandlePrintError(mp::ERR_HEAD_HEAT_TIMEOUT, "latch failed", head, stepper,
                     useRealHardware);
    return;
  }

  if (!head.setVh(true)) {
    (void)mp::PrintSpooler_FreeLine(line);
    HandlePrintError(mp::ERR_HEAD_HEAT_TIMEOUT, "setVh on failed", head,
                     stepper, useRealHardware);
    return;
  }

  for (std::uint8_t group = 0U; group < mp::PRINT_STB_GROUP_COUNT; ++group) {
    if (plan.groupBlackDots[group] == 0U || plan.pulseUs[group] == 0U) {
      continue;
    }

    // STB 分组必须逐个 pulse，禁止同时打开多个 STB。
    if (!head.pulseStbGroup(group, plan.pulseUs[group])) {
      (void)mp::PrintSpooler_FreeLine(line);
      HandlePrintError(mp::ERR_HEAD_HEAT_TIMEOUT, "stb pulse failed", head,
                       stepper, useRealHardware);
      return;
    }
  }

  if (!head.allStbOff()) {
    (void)mp::PrintSpooler_FreeLine(line);
    HandlePrintError(mp::ERR_HEAD_HEAT_TIMEOUT, "allStbOff failed", head,
                     stepper, useRealHardware);
    return;
  }

  if (!head.setVh(false)) {
    (void)mp::PrintSpooler_FreeLine(line);
    HandlePrintError(mp::ERR_HEAD_HEAT_TIMEOUT, "setVh off failed", head,
                     stepper, useRealHardware);
    return;
  }

  if (stepper.isFault()) {
    (void)mp::PrintSpooler_FreeLine(line);
    HandlePrintError(mp::ERR_MOTOR_FAULT, "stepper fault", head, stepper,
                     useRealHardware);
    return;
  }

  if (useRealHardware) {
    stepper.wake();
    if (stepper.isFault()) {
      (void)mp::PrintSpooler_FreeLine(line);
      HandlePrintError(mp::ERR_MOTOR_FAULT, "stepper fault after wake", head,
                       stepper, true);
      return;
    }
  }

  stepper.stepForward();
  if (stepper.isFault()) {
    (void)mp::PrintSpooler_FreeLine(line);
    HandlePrintError(mp::ERR_MOTOR_FAULT, "stepper fault after step", head,
                     stepper, useRealHardware);
    return;
  }
  stepper.release();

  LogLinePlan(line, plan);
  mp::PrintApp_MarkLinePrinted();
  (void)mp::PrintSpooler_FreeLine(line);
}

void TaskPrintEngineMain(void* /*context*/) {
  mp::RegisterTask(mp::TaskId::PRINT_CTRL, kPrintTaskName,
                   xTaskGetCurrentTaskHandle());
  mp::Health_ReportHeartbeat(mp::TaskId::PRINT_CTRL);
  mp::Log_Info("print", "PrintEngineTask alive, guarded flow");

  for (;;) {
    mp::PrintControlMsg control = {};
    if (xQueueReceive(mp::g_rtos.qPrintCtrl, &control, kControlWaitTicks) ==
        pdPASS) {
      HandleControl(control);
    }

    mp::LineBuffer* line = nullptr;
    if (xQueueReceive(mp::g_rtos.qLineReady, &line, kLineWaitTicks) ==
        pdPASS) {
      ProcessLine(line);
    } else {
      FinishJobIfIdle();
    }

    mp::Health_ReportHeartbeat(mp::TaskId::PRINT_CTRL);
  }
}

}  // namespace

namespace mp {

bool TaskPrintEngine_Create() {
  if (g_printTaskHandle != nullptr) {
    return true;
  }

  if (g_rtos.qPrintCtrl == nullptr || g_rtos.qLineReady == nullptr) {
    return false;
  }

  const BaseType_t createResult =
      xTaskCreate(TaskPrintEngineMain, kPrintTaskName, kPrintTaskStackWords,
                  nullptr, kPrintTaskPriority, &g_printTaskHandle);

  if (createResult != pdPASS) {
    g_printTaskHandle = nullptr;
    return false;
  }

  RegisterTask(TaskId::PRINT_CTRL, kPrintTaskName, g_printTaskHandle);
  return true;
}

}  // namespace mp
