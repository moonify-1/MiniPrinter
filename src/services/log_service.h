#pragma once

#include <cstddef>
#include <cstdint>

#include "common/app_types.h"

namespace mp {

// 日志模块名的固定长度。
//
// 使用固定长度数组的好处是：
// 1. 队列元素大小固定，便于 FreeRTOS 按值复制。
// 2. 不需要动态分配内存。
constexpr std::size_t LOG_MODULE_NAME_LEN = 16U;

// 日志正文的固定长度。
//
// 这个长度是“信息量”和“队列占用 RAM”之间的折中。
constexpr std::size_t LOG_TEXT_LEN = 96U;

// 固定长度日志消息。
//
// 当前设计成纯值类型，
// 这样它可以直接进入 qLog，而不依赖堆内存或字符串对象。
struct LogMsg {
  std::uint32_t tickCount;          // 产生日志时的系统 Tick。
  LogLevel level;                   // 日志级别。
  char module[LOG_MODULE_NAME_LEN]; // 模块名，例如 "main"。
  char text[LOG_TEXT_LEN];          // 格式化后的正文。
};

// 初始化日志系统。
void Log_Init();

// 设置日志级别。
void Log_SetLevel(LogLevel level);

// 写入一条普通日志。
//
// 注意：
// - 这是非 ISR 环境接口。
// - 本项目当前明确不允许在 ISR 中写日志。
void Log_Write(LogLevel level, const char* module, const char* fmt, ...);

// 便捷日志接口。
void Log_Fatal(const char* module, const char* fmt, ...);
void Log_Error(const char* module, const char* fmt, ...);
void Log_Warn(const char* module, const char* fmt, ...);
void Log_Info(const char* module, const char* fmt, ...);
void Log_Debug(const char* module, const char* fmt, ...);

// 读取最近日志快照。
//
// out 指向调用方提供的数组；返回实际复制的条数。
// 这是调试 API 使用的只读入口，不会从日志队列中取走消息。
std::size_t Log_GetRecent(LogMsg* out, std::size_t maxCount);

}  // namespace mp
