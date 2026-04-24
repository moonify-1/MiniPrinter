#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include "bsp/bsp_board.h"
#include "common/app_error.h"
#include "common/app_types.h"
#include "config/default_params.h"
#include "config/print_config.h"
#include "config/project_features.h"
#include "config/safety_limits.h"
#include "rtos/rtos_objects.h"
#include "rtos/task_registry.h"

namespace {

// 这些常量只用于最小编译验证：
// 1. 证明 common / config / bsp / rtos 层头文件可以被主入口正常包含。
// 2. 证明参数块、RTOS 对象和任务注册表类型可以参与编译。
// 3. 不创建任何实际业务任务，也不引入复杂业务逻辑。
const mp::SystemState kSystemStateBuildCheck = mp::SystemState::BOOT;
const mp::PrintJobState kPrintJobStateBuildCheck = mp::PrintJobState::NONE;
const mp::SensorValidity kSensorValidityBuildCheck = mp::SensorValidity::VALID;
const mp::LogLevel kLogLevelBuildCheck = mp::LogLevel::INFO;
const mp::AppErrorCode kErrorCodeBuildCheck = mp::APP_OK;
const mp::ParamBlock kParamBlockBuildCheck = mp::DEFAULT_PARAM_BLOCK;
const bool kFeatureBuildCheck = mp::FEATURE_HW_THERMAL_HEAD ||
                                mp::FEATURE_HW_STEPPER || mp::FEATURE_WDT ||
                                mp::FEATURE_WIFI || mp::FEATURE_BLE;
const std::uint16_t kPrintDotsBuildCheck = mp::PRINT_DOTS_PER_LINE;
const std::uint16_t kHeatDotsBuildCheck =
    mp::SAFETY_MAX_SIMULTANEOUS_HEAT_DOTS;
const std::size_t kTaskRegistryCapacityBuildCheck = mp::kTaskRegistryCapacity;
const mp::RtosObjects* kRtosObjectsBuildCheck = &mp::g_rtos;

}  // namespace

void setup() {
  // 安全输出必须是 setup() 里最早执行的动作。
  mp::Bsp_PreInitSafeOutputs();

  // 只有在安全输出已经处理完之后，才开始做串口初始化。
  Serial.begin(115200);

  // 显式使用这些常量，避免部分编译配置下出现未使用告警。
  (void)kSystemStateBuildCheck;
  (void)kPrintJobStateBuildCheck;
  (void)kSensorValidityBuildCheck;
  (void)kLogLevelBuildCheck;
  (void)kErrorCodeBuildCheck;
  (void)kParamBlockBuildCheck;
  (void)kFeatureBuildCheck;
  (void)kPrintDotsBuildCheck;
  (void)kHeatDotsBuildCheck;
  (void)kTaskRegistryCapacityBuildCheck;
  (void)kRtosObjectsBuildCheck;

  // 板级初始化保持“安全优先”，不做任何功率使能动作。
  mp::Bsp_Init();

  // RTOS 基础对象创建失败时，立即回到安全输出状态，
  // 并通过串口打印错误，避免系统继续进入不完整状态。
  if (!mp::Rtos_CreateObjects()) {
    mp::Bsp_SetAllOutputsSafe();
    Serial.println("ERROR: Rtos_CreateObjects failed");
    return;
  }
}

void loop() {
  // Step 05 只建立 RTOS 对象和任务注册表接口，不创建实际任务。
}
