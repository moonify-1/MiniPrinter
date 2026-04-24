#include "services/print_service.h"

#include "app/app_system.h"
#include "config/project_features.h"
#include "rtos/rtos_objects.h"
#include "services/error_service.h"
#include "services/thermal_safety_service.h"

namespace {

constexpr TickType_t kNoWait = 0U;

// 统计一个字节中有多少个黑点。
//
// 当前单色位图约定 bit=1 表示黑点。
std::uint8_t CountBits(std::uint8_t value) {
  std::uint8_t count = 0U;
  while (value != 0U) {
    count = static_cast<std::uint8_t>(count + (value & 1U));
    value = static_cast<std::uint8_t>(value >> 1U);
  }
  return count;
}

// 统计某个 STB 组覆盖的 64 个点。
//
// 384 点/行被拆成 6 组，每组 64 点，也就是 8 字节。
std::uint8_t CountGroupBlackDots(const mp::LineBuffer* line,
                                 std::uint8_t group) {
  if (line == nullptr || group >= mp::PRINT_STB_GROUP_COUNT) {
    return 0U;
  }

  constexpr std::uint8_t kBytesPerGroup =
      mp::PRINT_DOTS_PER_STB_GROUP / 8U;
  const std::uint8_t startByte =
      static_cast<std::uint8_t>(group * kBytesPerGroup);

  std::uint8_t total = 0U;
  for (std::uint8_t offset = 0U; offset < kBytesPerGroup; ++offset) {
    total = static_cast<std::uint8_t>(
        total + CountBits(line->data[startByte + offset]));
  }
  return total;
}

const char* DetailForPrintError(mp::AppErrorCode error) {
  if (error == mp::ERR_SENSOR_PAPER_MISSING) {
    return "paper missing: stop print until paper present";
  }

  if (error == mp::ERR_HEAD_OVER_TEMP) {
    return "head over temperature: wait cooldown";
  }

  if (error == mp::ERR_POWER_LOW_BATTERY) {
    return "low battery: block heating and fast motor";
  }

  if (error == mp::ERR_MOTOR_FAULT) {
    return "DRV fault: release motor and stop print";
  }

  return "print service error";
}

}  // namespace

namespace mp {

void PrintService_Init() {
  PrintApp_Init();
  PrintService_SetPrintActive(false);
}

bool PrintService_PostControl(const PrintControlMsg& msg,
                              std::uint32_t timeoutMs) {
  if (g_rtos.qPrintCtrl == nullptr) {
    return false;
  }

  const TickType_t waitTicks = pdMS_TO_TICKS(timeoutMs);
  return xQueueSend(g_rtos.qPrintCtrl, &msg, waitTicks) == pdPASS;
}

bool PrintService_RequestStart(std::uint32_t jobId) {
  const PrintControlMsg msg = {PrintControlType::PRINT_START, jobId, 0U};
  return PrintService_PostControl(msg, 10U);
}

bool PrintService_RequestEnd(std::uint32_t jobId) {
  const PrintControlMsg msg = {PrintControlType::PRINT_END, jobId, 0U};
  return PrintService_PostControl(msg, 10U);
}

bool PrintService_RequestCancel(std::uint32_t jobId) {
  const PrintControlMsg msg = {PrintControlType::PRINT_CANCEL, jobId, 0U};
  return PrintService_PostControl(msg, 10U);
}

bool PrintService_RequestFeed(std::uint32_t steps) {
  const PrintControlMsg msg = {PrintControlType::FEED, 0U, steps};
  return PrintService_PostControl(msg, 10U);
}

void PrintService_SetPrintActive(bool active) {
  if (g_rtos.systemEvents == nullptr) {
    return;
  }

  if (active) {
    xEventGroupSetBits(g_rtos.systemEvents, EVT_PRINT_ACTIVE);
  } else {
    xEventGroupClearBits(g_rtos.systemEvents, EVT_PRINT_ACTIVE);
  }
}

AppErrorCode PrintService_BuildLinePlan(const LineBuffer* line,
                                        const SensorSnapshot& sensor,
                                        const ParamBlock& params,
                                        PrintLinePlan* plan) {
  if (line == nullptr || plan == nullptr) {
    return ERR_QUEUE_FULL;
  }

  const AppErrorCode lineCheck = ThermalSafety_CheckLine(line);
  if (lineCheck != APP_OK) {
    return lineCheck;
  }

  for (std::uint8_t group = 0U; group < PRINT_STB_GROUP_COUNT; ++group) {
    const std::uint8_t blackDots = CountGroupBlackDots(line, group);
    const ThermalPulseResult pulse =
        ThermalSafety_CalcPulseUs(blackDots, sensor, params);
    if (pulse.error != APP_OK) {
      return pulse.error;
    }

    plan->groupBlackDots[group] = blackDots;
    plan->pulseUs[group] = pulse.pulseUs;
  }

  return APP_OK;
}

bool PrintService_IsRealPrintHardwareEnabled() {
#if MP_ENABLE_HW_THERMAL_HEAD && MP_ENABLE_HW_STEPPER
  return true;
#else
  return false;
#endif
}

AppErrorCode PrintService_CheckRealPrintAllowed(const SensorSnapshot& sensor,
                                                const ParamBlock& params) {
  if (!PrintService_IsRealPrintHardwareEnabled()) {
    return ERR_HW_DISABLED;
  }

  if (SystemApp_GetState() != SystemState::RUNNING) {
    return ERR_SYS_INIT_FAILED;
  }

  // ThermalSafety_CheckCanPrint() 内部会检查：
  // - 纸张存在。
  // - 传感器读数有效。
  // - 温度未超过停机阈值。
  // - 电池未低于保守低电压阈值。
  // - 电机驱动未报告故障。
  return ThermalSafety_CheckCanPrint(sensor, params);
}

void PrintService_ReportError(AppErrorCode error) {
  (void)kNoWait;
  Error_Report(error, "print", DetailForPrintError(error));
}

}  // namespace mp
