#include "common/app_error.h"
#include "common/app_types.h"
#include "config/default_params.h"
#include "config/print_config.h"
#include "config/project_features.h"
#include "config/safety_limits.h"

namespace {

// 这些变量只用于最小编译验证：
// 1. 证明 common 层头文件可以被主入口正常包含。
// 2. 证明 app_error.cpp 中定义的全局错误码可以被正常链接。
// 3. 不引入任何真实业务流程、硬件访问或 RTOS 逻辑。
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

}  // namespace

void setup() {
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
}

void loop() {
  // Step 02 只做公共类型和错误码验证，这里故意保持空实现。
}
