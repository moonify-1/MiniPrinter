#include "app/app_factory_test.h"

#include <Arduino.h>

#include "app/app_system.h"
#include "bsp/bsp_board.h"
#include "bsp/bsp_timer.h"
#include "config/print_config.h"
#include "config/project_features.h"
#include "config/safety_limits.h"
#include "drivers/stepper/drv_stepper.h"
#include "drivers/thermal_head/drv_thermal_head.h"
#include "services/error_service.h"
#include "services/log_service.h"
#include "services/param_service.h"
#include "services/sensor_service.h"
#include "services/thermal_safety_service.h"

namespace {

constexpr std::uint32_t kFactoryMotorStepIntervalUs = 5000U;
constexpr std::uint32_t kFactoryMotorMaxSteps = 200U;
constexpr std::uint32_t kFactoryDefaultStbPulseUs = 5U;
constexpr std::uint32_t kFactoryDefaultVhHoldMs = 3000U;
constexpr std::uint32_t kFactoryMaxVhHoldMs = 5000U;

// 调试命令触发 SAFE_MODE 时使用的专用错误码。
//
// 这里不新增全局 extern 常量，是为了保持 Step 21 改动范围小。
// code=0x00F1 预留给“人工调试命令进入安全模式”。
mp::AppErrorCode MakeFactorySafeModeReason() {
  return mp::makeError(mp::ErrorSeverity::FATAL,
                       mp::ErrorRecoverAction::ENTER_SAFE_MODE,
                       mp::ErrorModule::SYSTEM, 0x00F1U);
}

// 将请求步数限制在一个小范围内。
//
// Factory Test 面向人工低速验证，不能让一帧协议数据触发长时间电机运动。
std::uint32_t ClampFactorySteps(std::uint32_t steps) {
  if (steps == 0U) {
    return 0U;
  }
  return (steps > kFactoryMotorMaxSteps) ? kFactoryMotorMaxSteps : steps;
}

// 将 STB 测试脉冲限制在安全硬上限内。
//
// 即使上层协议传入了很大的 pulseUs，最终也不会超过 safety_limits.h 的硬限制。
std::uint32_t ClampStbPulseUs(std::uint32_t pulseUs) {
  const std::uint32_t requested =
      (pulseUs == 0U) ? kFactoryDefaultStbPulseUs : pulseUs;
  return (requested > mp::SAFETY_DEFAULT_HEAT_PULSE_MAX_US)
             ? mp::SAFETY_DEFAULT_HEAT_PULSE_MAX_US
             : requested;
}

// 将 VH 测量窗口限制在几秒内。
//
// 这个测试会打开打印头高压路径，虽然 STB 全关不会主动加热，
// 但仍然不允许长时间保持 VH 打开。
std::uint32_t ClampVhHoldMs(std::uint32_t holdMs) {
  const std::uint32_t requested =
      (holdMs == 0U) ? kFactoryDefaultVhHoldMs : holdMs;
  return (requested > kFactoryMaxVhHoldMs) ? kFactoryMaxVhHoldMs : requested;
}

}  // namespace

namespace mp {

AppErrorCode FactoryTest_SafeOff() {
  Log_Warn("factory", "SAFE_OFF");
  Bsp_SetAllOutputsSafe();
  return APP_OK;
}

SensorSnapshot FactoryTest_ReadSensors() {
  const SensorSnapshot snapshot = SensorService_GetSnapshot();
  Log_Info("factory", "SENSOR_TEST paper=%u temp=%.1f bat=%lu fault=%u",
           snapshot.paperPresent ? 1U : 0U, snapshot.headTempC,
           static_cast<unsigned long>(snapshot.batteryMv),
           snapshot.motorFault ? 1U : 0U);
  return snapshot;
}

AppErrorCode FactoryTest_MotorTest(std::uint32_t steps) {
  const std::uint32_t limitedSteps = ClampFactorySteps(steps);
  Log_Warn("factory", "MOTOR_TEST steps=%lu limited=%lu",
           static_cast<unsigned long>(steps),
           static_cast<unsigned long>(limitedSteps));

#if MP_ENABLE_HW_STEPPER
  const SensorSnapshot sensor = SensorService_GetSnapshot();
  const ParamBlock params = Param_GetSnapshot();
  if (params.safety.forbidFastMotorWhenLowVoltage != 0U &&
      sensor.batteryMv < SAFETY_DEFAULT_LOW_BATTERY_STOP_MV) {
    return ERR_POWER_LOW_BATTERY;
  }
  if (sensor.motorFault) {
    return ERR_MOTOR_FAULT;
  }

  StepperDriver& stepper = GetStepperDriver();
  if (!stepper.init()) {
    stepper.setSafe();
    return ERR_HW_DISABLED;
  }

  stepper.wake();
  for (std::uint32_t index = 0U; index < limitedSteps; ++index) {
    if (stepper.isFault()) {
      stepper.release();
      stepper.sleep();
      return ERR_MOTOR_FAULT;
    }
    stepper.stepForward();
    Bsp_DelayUs(kFactoryMotorStepIntervalUs);
  }

  stepper.release();
  stepper.sleep();
  return APP_OK;
#else
  (void)limitedSteps;
  return ERR_HW_DISABLED;
#endif
}

AppErrorCode FactoryTest_HeadShiftTest(const std::uint8_t* lineData,
                                       std::size_t len) {
  Log_Warn("factory", "HEAD_SHIFT_TEST len=%u",
           static_cast<unsigned>(len));

  if (lineData == nullptr || len != PRINT_BYTES_PER_LINE) {
    return ERR_COMM_FRAME_TOO_LONG;
  }

#if MP_ENABLE_HW_THERMAL_HEAD
  ThermalHeadDriver& head = GetThermalHeadDriver();
  if (!head.init()) {
    (void)head.setSafe();
    return ERR_HW_DISABLED;
  }

  // 该测试只验证 DI/CLK/LAT 数据链路，显式关闭 VH/STB，避免形成加热条件。
  bool ok = head.allStbOff();
  ok = head.setVh(false) && ok;
  ok = head.shiftLine48Bytes(lineData, len) && ok;
  ok = head.latch() && ok;
  (void)head.allStbOff();
  (void)head.setVh(false);
  return ok ? APP_OK : ERR_HW_DISABLED;
#else
  return ERR_HW_DISABLED;
#endif
}

AppErrorCode FactoryTest_HeadStbTest(std::uint8_t group,
                                     std::uint32_t pulseUs) {
  const std::uint32_t safePulseUs = ClampStbPulseUs(pulseUs);
  Log_Warn("factory", "HEAD_STB_TEST group=%u pulse_us=%lu safe_us=%lu",
           static_cast<unsigned>(group),
           static_cast<unsigned long>(pulseUs),
           static_cast<unsigned long>(safePulseUs));

  if (group >= PRINT_STB_GROUP_COUNT) {
    return ERR_COMM_FRAME_TOO_LONG;
  }

#if MP_ENABLE_HW_THERMAL_HEAD
  const SensorSnapshot sensor = SensorService_GetSnapshot();
  const ParamBlock params = Param_GetSnapshot();
  const AppErrorCode canPrint = ThermalSafety_CheckCanPrint(sensor, params);
  if (canPrint != APP_OK) {
    Bsp_SetAllOutputsSafe();
    return canPrint;
  }

  ThermalHeadDriver& head = GetThermalHeadDriver();
  if (!head.init()) {
    (void)head.setSafe();
    return ERR_HW_DISABLED;
  }

  // STB 测试只观察 STB 波形，不打开 VH；这样即使 STB 有脉冲，也不会主动加热。
  bool ok = head.allStbOff();
  ok = head.setVh(false) && ok;
  ok = head.pulseStbGroup(group, safePulseUs) && ok;
  ok = head.allStbOff() && ok;
  ok = head.setVh(false) && ok;
  return ok ? APP_OK : ERR_HW_DISABLED;
#else
  return ERR_HW_DISABLED;
#endif
}

AppErrorCode FactoryTest_VhMeasure(std::uint32_t holdMs) {
  const std::uint32_t safeHoldMs = ClampVhHoldMs(holdMs);
  Log_Warn("factory", "VH_MEASURE hold_ms=%lu safe_ms=%lu",
           static_cast<unsigned long>(holdMs),
           static_cast<unsigned long>(safeHoldMs));

#if MP_ENABLE_HW_THERMAL_HEAD
  ThermalHeadDriver& head = GetThermalHeadDriver();
  if (!head.init()) {
    (void)head.setSafe();
    return ERR_HW_DISABLED;
  }

  // 这个测试只为万用表测 VH_OUT 准备：
  // - STB 全程关闭，避免选通任何加热点。
  // - 不 shift、不 latch，避免改变打印头数据锁存状态。
  // - 延时结束后无论结果如何都回到 setSafe()。
  bool ok = head.allStbOff();
  ok = head.setVh(false) && ok;
  ok = head.setVh(true) && ok;
  delay(safeHoldMs);
  ok = head.setVh(false) && ok;
  ok = head.allStbOff() && ok;
  (void)head.setSafe();
  return ok ? APP_OK : ERR_HW_DISABLED;
#else
  (void)safeHoldMs;
  return ERR_HW_DISABLED;
#endif
}

AppErrorCode FactoryTest_EnterSafeMode() {
  const AppErrorCode reason = MakeFactorySafeModeReason();
  Log_Fatal("factory", "ENTER_SAFE_MODE");
  Bsp_SetAllOutputsSafe();
  Error_Report(reason, "factory", "manual enter safe mode");
  SystemApp_EnterSafeMode(reason);
  return reason;
}

void FactoryTest_Reboot() {
  Log_Warn("factory", "REBOOT");
  Bsp_SetAllOutputsSafe();
  Serial.flush();
  delay(20);
  ESP.restart();
}

}  // namespace mp
