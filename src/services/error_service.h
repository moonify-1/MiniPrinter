#pragma once

#include <cstddef>
#include <cstdint>

#include "common/app_error.h"

namespace mp {

// 错误来源模块名长度。
//
// 使用固定长度数组，避免 ErrorEvent 进入 FreeRTOS 队列时依赖动态内存。
constexpr std::size_t ERROR_MODULE_TEXT_LEN = 16U;

// 错误详情文本长度。
//
// 这里保存简短原因，不保存长日志；长日志仍交给 LogService。
constexpr std::size_t ERROR_DETAIL_TEXT_LEN = 64U;

// 统一错误事件。
//
// qError 传递这个结构，而不是只传 AppErrorCode。
// 这样 SystemTask 可以直接看到严重级别、上报模块和现场说明。
struct ErrorEvent {
  AppErrorCode code;                       // 统一错误码。
  ErrorSeverity severity;                  // 错误严重级别。
  char module[ERROR_MODULE_TEXT_LEN];      // 上报模块名，例如 "print"。
  std::uint32_t timestamp;                 // 上报时刻，单位为 FreeRTOS tick。
  char detail[ERROR_DETAIL_TEXT_LEN];      // 简短错误详情。
};

// 上报错误。
//
// 所有模块应通过这个入口上报错误，不直接写 qError。
void Error_Report(AppErrorCode code, const char* module, const char* detail);

// 获取最近一次错误事件。
//
// 如果尚未有错误，返回 code=APP_OK 的空事件。
ErrorEvent Error_GetLast();

// 清除可恢复错误。
//
// WARN / ERROR 可以被清除；FATAL 不通过该接口清除，必须复位或专门恢复。
void Error_ClearRecoverable();

// 当前是否要求进入 SAFE_MODE。
bool Error_IsSafeModeRequired();

}  // namespace mp
