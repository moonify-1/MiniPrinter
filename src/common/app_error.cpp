#include "app_error.h"

namespace {

// 错误码位域偏移量。
//
// 这些常量只在当前实现文件内部使用，
// 目的是把打包规则集中管理，避免魔法数字散落在代码中。
constexpr mp::AppErrorCode kSeverityShift = 28U;
constexpr mp::AppErrorCode kActionShift = 24U;
constexpr mp::AppErrorCode kModuleShift = 16U;

// 各字段的掩码。
//
// 掩码用于“只取出某一段位”，
// 是解析位域时最常见也最稳定的写法。
constexpr mp::AppErrorCode kNibbleMask = 0x0FU;
constexpr mp::AppErrorCode kByteMask = 0xFFU;
constexpr mp::AppErrorCode kCodeMask = 0xFFFFU;

}  // namespace

namespace mp {

AppErrorCode makeError(ErrorSeverity severity,
                       ErrorRecoverAction action,
                       ErrorModule module,
                       uint16_t code) {
  // 把不同维度的信息打包到同一个 32 位值里，
  // 后续日志、状态机和协议都可以统一传递这个值。
  return (static_cast<AppErrorCode>(severity) & kNibbleMask) << kSeverityShift |
         (static_cast<AppErrorCode>(action) & kNibbleMask) << kActionShift |
         (static_cast<AppErrorCode>(module) & kByteMask) << kModuleShift |
         (static_cast<AppErrorCode>(code) & kCodeMask);
}

ErrorSeverity getErrorSeverity(AppErrorCode error) {
  // 先右移到最低位，再转回枚举类型。
  return static_cast<ErrorSeverity>((error >> kSeverityShift) & kNibbleMask);
}

ErrorModule getErrorModule(AppErrorCode error) {
  // 模块字段位于 bit[23:16]，因此这里右移 16 位后再做掩码。
  return static_cast<ErrorModule>((error >> kModuleShift) & kByteMask);
}

bool isFatalError(AppErrorCode error) {
  // 致命错误通常意味着当前运行模式已经不可信，
  // 上层应考虑进入安全模式、停止关键动作或执行更保守的恢复策略。
  return getErrorSeverity(error) == ErrorSeverity::FATAL;
}

const AppErrorCode APP_OK = 0U;

const AppErrorCode ERR_SYS_INIT_FAILED =
    makeError(ErrorSeverity::FATAL, ErrorRecoverAction::REINITIALIZE,
              ErrorModule::SYSTEM, 0x0001U);

const AppErrorCode ERR_SENSOR_PAPER_MISSING =
    makeError(ErrorSeverity::WARN, ErrorRecoverAction::CHECK_CONDITION,
              ErrorModule::SENSOR, 0x0001U);

const AppErrorCode ERR_SENSOR_TEMP_INVALID =
    makeError(ErrorSeverity::ERROR, ErrorRecoverAction::CHECK_CONDITION,
              ErrorModule::SENSOR, 0x0002U);

const AppErrorCode ERR_POWER_LOW_BATTERY =
    makeError(ErrorSeverity::WARN, ErrorRecoverAction::LIMIT_POWER,
              ErrorModule::POWER, 0x0001U);

const AppErrorCode ERR_HEAD_OVER_TEMP =
    makeError(ErrorSeverity::ERROR, ErrorRecoverAction::WAIT_COOLDOWN,
              ErrorModule::HEAD, 0x0001U);

const AppErrorCode ERR_HEAD_HEAT_TIMEOUT =
    makeError(ErrorSeverity::ERROR, ErrorRecoverAction::ENTER_SAFE_MODE,
              ErrorModule::HEAD, 0x0002U);

const AppErrorCode ERR_MOTOR_FAULT =
    makeError(ErrorSeverity::ERROR, ErrorRecoverAction::REINITIALIZE,
              ErrorModule::MOTOR, 0x0001U);

const AppErrorCode ERR_COMM_CRC =
    makeError(ErrorSeverity::WARN, ErrorRecoverAction::CLEAR_OR_DROP,
              ErrorModule::COMM, 0x0001U);

const AppErrorCode ERR_COMM_FRAME_TOO_LONG =
    makeError(ErrorSeverity::ERROR, ErrorRecoverAction::CLEAR_OR_DROP,
              ErrorModule::COMM, 0x0002U);

const AppErrorCode ERR_QUEUE_FULL =
    makeError(ErrorSeverity::WARN, ErrorRecoverAction::RETRY,
              ErrorModule::QUEUE, 0x0001U);

const AppErrorCode ERR_PARAM_CRC =
    makeError(ErrorSeverity::ERROR, ErrorRecoverAction::REINITIALIZE,
              ErrorModule::PARAM, 0x0001U);

const AppErrorCode ERR_WDT_TASK_TIMEOUT =
    makeError(ErrorSeverity::FATAL, ErrorRecoverAction::RESTART_TASK,
              ErrorModule::WDT, 0x0001U);

const AppErrorCode ERR_HW_DISABLED =
    makeError(ErrorSeverity::WARN, ErrorRecoverAction::CHECK_CONDITION,
              ErrorModule::SYSTEM, 0x0002U);

}  // namespace mp
