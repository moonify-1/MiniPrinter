#pragma once

#include <cstdint>

namespace mp {

// 错误严重级别。
//
// 严重级别回答的是“这个错误到底有多危险”。
// 它不关心错误来自哪个模块，也不直接决定恢复动作，
// 只是为上层提供一个快速判断依据。
enum class ErrorSeverity : uint8_t {
  NONE = 0, // 无错误，通常仅用于 APP_OK。
  INFO,     // 信息类状态，一般只用于提示，不阻断流程。
  WARN,     // 可继续运行，但需要关注。
  ERROR,    // 已影响功能，需要进入错误处理流程。
  FATAL,    // 致命错误，通常不能继续当前运行模式。
};

// 错误来源模块。
//
// 模块字段回答的是“错误是从哪一层来的”，
// 这样在日志、调试输出和协议上报时可以快速聚类。
enum class ErrorModule : uint8_t {
  NONE = 0, // 无模块，仅用于 APP_OK。
  SYSTEM,   // 系统初始化、系统状态机等通用系统问题。
  SENSOR,   // 纸检测、温度采样等传感器相关问题。
  POWER,    // 电池、电源输入、电压状态相关问题。
  HEAD,     // 打印头相关问题。
  MOTOR,    // 电机与电机驱动相关问题。
  COMM,     // 通信协议、帧解析、校验相关问题。
  QUEUE,    // 队列、缓冲区、消息积压相关问题。
  PARAM,    // 参数、配置、校验或持久化数据相关问题。
  WDT,      // 看门狗或任务超时相关问题。
};

// 建议恢复动作。
//
// 这个字段不是“必须如何处理”，
// 而是错误码里携带的一种默认恢复建议。
// 上层策略可以参考它，也可以根据现场条件决定采用更保守的做法。
enum class ErrorRecoverAction : uint8_t {
  NONE = 0,        // 不需要额外动作，通常用于 APP_OK。
  RETRY,           // 稍后重试，适合偶发型问题。
  CHECK_CONDITION, // 先检查现场条件，例如纸张、温度、输入数据是否满足要求。
  REINITIALIZE,    // 重新初始化模块或系统资源。
  LIMIT_POWER,     // 降功耗或限制高负载动作。
  WAIT_COOLDOWN,   // 等待温度回落后再继续。
  CLEAR_OR_DROP,   // 清理当前输入、缓存或异常任务。
  RESTART_TASK,    // 重启任务或调度单元。
  ENTER_SAFE_MODE, // 进入安全模式，保留最小可用能力。
  SHUTDOWN,        // 关停系统或进入停止态。
};

// 全局错误码类型。
//
// 采用 uint32_t 的原因：
// 1. 对单片机友好，大小固定，便于日志和协议传输。
// 2. 可以把严重级别、恢复动作、模块号、模块内错误号打包到一个值里。
//
// 本项目约定位分布如下：
// - bit[31:28]：严重级别
// - bit[27:24]：建议恢复动作
// - bit[23:16]：错误模块
// - bit[15:00]：模块内错误号
using AppErrorCode = uint32_t;

// 组合生成一个 32 位错误码。
//
// 参数说明：
// - severity：错误严重级别。
// - action：建议恢复动作。
// - module：错误来源模块。
// - code：模块内错误号，建议在同一模块内保持唯一。
AppErrorCode makeError(ErrorSeverity severity,
                       ErrorRecoverAction action,
                       ErrorModule module,
                       uint16_t code);

// 从统一错误码中提取严重级别。
ErrorSeverity getErrorSeverity(AppErrorCode error);

// 从统一错误码中提取错误来源模块。
ErrorModule getErrorModule(AppErrorCode error);

// 判断错误是否为致命错误。
//
// 这是一个便捷接口，目的是让上层代码少写重复比较逻辑。
bool isFatalError(AppErrorCode error);

// 常用错误码定义。
extern const AppErrorCode APP_OK;
extern const AppErrorCode ERR_SYS_INIT_FAILED;
extern const AppErrorCode ERR_SENSOR_PAPER_MISSING;
extern const AppErrorCode ERR_SENSOR_TEMP_INVALID;
extern const AppErrorCode ERR_POWER_LOW_BATTERY;
extern const AppErrorCode ERR_HEAD_OVER_TEMP;
extern const AppErrorCode ERR_HEAD_HEAT_TIMEOUT;
extern const AppErrorCode ERR_MOTOR_FAULT;
extern const AppErrorCode ERR_COMM_CRC;
extern const AppErrorCode ERR_COMM_FRAME_TOO_LONG;
extern const AppErrorCode ERR_QUEUE_FULL;
extern const AppErrorCode ERR_PARAM_CRC;
extern const AppErrorCode ERR_WDT_TASK_TIMEOUT;
extern const AppErrorCode ERR_HW_DISABLED;

}  // namespace mp
