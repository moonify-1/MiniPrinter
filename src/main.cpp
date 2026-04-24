#include "common/app_error.h"
#include "common/app_types.h"

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

}  // namespace

void setup() {
  // 显式使用这些常量，避免部分编译配置下出现未使用告警。
  (void)kSystemStateBuildCheck;
  (void)kPrintJobStateBuildCheck;
  (void)kSensorValidityBuildCheck;
  (void)kLogLevelBuildCheck;
  (void)kErrorCodeBuildCheck;
}

void loop() {
  // Step 02 只做公共类型和错误码验证，这里故意保持空实现。
}
