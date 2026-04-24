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
#include "services/log_service.h"
#include "tasks/task_log.h"

namespace {

// 这些常量只用于最小编译验证：
// 1. 证明 common / config / bsp / rtos / services / tasks 层头文件可正常包含。
// 2. 证明参数块、RTOS 对象、日志服务和任务注册表类型可参与编译。
// 3. 不引入业务流程，主循环仍保持空实现。
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

  // 只有在安全输出已处理后，才开始串口初始化。
  Serial.begin(115200);

  // 显式使用这些常量，避免未使用告警。
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

  // 板级初始化保持安全优先。
  mp::Bsp_Init();

  // 先建立 RTOS 对象，再初始化日志系统。
  if (!mp::Rtos_CreateObjects()) {
    mp::Bsp_SetAllOutputsSafe();
    Serial.println("ERROR: Rtos_CreateObjects failed");
    return;
  }

  mp::Log_Init();

  // 创建日志任务失败时，不继续进入后续流程。
  if (!mp::TaskLog_Create()) {
    mp::Bsp_SetAllOutputsSafe();
    Serial.println("ERROR: TaskLog_Create failed");
    return;
  }

  // 用一条启动日志验证异步日志链路已经可用。
  mp::Log_Info("main", "Async log ready");
}

void loop() {
  // Step 06 仍然只保留最小主循环，不写业务逻辑。
}
