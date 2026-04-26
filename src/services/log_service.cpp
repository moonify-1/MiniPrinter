#include "services/log_service.h"

#include <Arduino.h>
#include <cstdarg>
#include <cstdio>
#include <cstdint>

#include "config/default_params.h"
#include "rtos/rtos_objects.h"

namespace {

constexpr TickType_t kWarnQueueWaitTicks = pdMS_TO_TICKS(10U);
constexpr TickType_t kErrorQueueWaitTicks = pdMS_TO_TICKS(20U);
constexpr std::size_t kFallbackTextLen = 48U;

// 日志运行时状态。
//
// 当前保持最小实现：
// - 记录当前日志级别。
// - 记录不同场景下的丢弃次数。
// - 不额外引入复杂锁逻辑。
struct LogRuntimeState {
  mp::LogLevel level;             // 当前生效日志级别。
  std::uint32_t droppedInfoDebug; // DEBUG / INFO / TRACE 被丢弃次数。
  std::uint32_t droppedWarn;      // WARN 投递失败次数。
  std::uint32_t fallbackError;    // ERROR / FATAL 触发 fallback 次数。
  std::uint32_t droppedFromIsr;   // ISR 环境误调用被丢弃次数。
  bool initialized;               // 是否已初始化。
};

LogRuntimeState g_logState = {
    mp::DEFAULT_LOG_PARAMS.defaultLevel,
    0U,
    0U,
    0U,
    0U,
    false,
};

constexpr std::size_t kRecentLogCapacity = 8U;
mp::LogMsg g_recentLogs[kRecentLogCapacity] = {};
std::size_t g_recentLogWriteIndex = 0U;
std::size_t g_recentLogCount = 0U;

// 把日志级别转成人类可读字符串。
const char* LevelToString(mp::LogLevel level) {
  switch (level) {
    case mp::LogLevel::FATAL:
      return "FATAL";
    case mp::LogLevel::ERROR:
      return "ERROR";
    case mp::LogLevel::WARN:
      return "WARN";
    case mp::LogLevel::INFO:
      return "INFO";
    case mp::LogLevel::DEBUG:
      return "DEBUG";
    case mp::LogLevel::TRACE:
      return "TRACE";
    default:
      return "UNK";
  }
}

// 当前日志级别是否允许输出。
bool ShouldLog(mp::LogLevel level) {
  return static_cast<std::uint8_t>(level) <=
         static_cast<std::uint8_t>(g_logState.level);
}

// 把字符串安全复制到固定长度数组中。
void CopyStringToBuffer(char* dst, std::size_t dstLen, const char* src) {
  if (dst == nullptr || dstLen == 0U) {
    return;
  }

  const char* safeSrc = (src != nullptr) ? src : "-";
  std::snprintf(dst, dstLen, "%s", safeSrc);
}

// 把 printf 风格日志格式化到固定长度正文缓冲区。
void FormatLogText(char* dst, std::size_t dstLen, const char* fmt, va_list args) {
  if (dst == nullptr || dstLen == 0U) {
    return;
  }

  const char* safeFmt = (fmt != nullptr) ? fmt : "(null fmt)";
  std::vsnprintf(dst, dstLen, safeFmt, args);
}

// 检查当前是否在 ISR 中。
//
// 这是保护措施，不是鼓励在 ISR 里调用日志。
bool IsInIsrContext() {
#if defined(ARDUINO_ARCH_ESP32)
  return xPortInIsrContext();
#else
  return false;
#endif
}

// 尝试把日志压入 qLog。
bool EnqueueLog(const mp::LogMsg& msg, TickType_t waitTicks) {
  if (mp::g_rtos.qLog == nullptr) {
    return false;
  }

  return xQueueSend(mp::g_rtos.qLog, &msg, waitTicks) == pdPASS;
}

void StoreRecentLog(const mp::LogMsg& msg) {
  g_recentLogs[g_recentLogWriteIndex] = msg;
  g_recentLogWriteIndex = (g_recentLogWriteIndex + 1U) % kRecentLogCapacity;
  if (g_recentLogCount < kRecentLogCapacity) {
    ++g_recentLogCount;
  }
}

// 输出短兜底日志。
//
// 仅在 ERROR / FATAL 且 qLog 无法接收时使用。
void EmitFallbackShortLog(const mp::LogMsg& msg) {
  char shortText[kFallbackTextLen + 1U] = {};
  std::snprintf(shortText, sizeof(shortText), "%s", msg.text);

  Serial.print("[FB][");
  Serial.print(LevelToString(msg.level));
  Serial.print("][");
  Serial.print(msg.module);
  Serial.print("] ");
  Serial.println(shortText);
}

// 根据日志级别选择不同的入队等待策略。
TickType_t GetQueueWaitTicks(mp::LogLevel level) {
  switch (level) {
    case mp::LogLevel::WARN:
      return kWarnQueueWaitTicks;
    case mp::LogLevel::ERROR:
    case mp::LogLevel::FATAL:
      return kErrorQueueWaitTicks;
    case mp::LogLevel::INFO:
    case mp::LogLevel::DEBUG:
    case mp::LogLevel::TRACE:
    default:
      return 0U;
  }
}

// 处理 qLog 满时的分级策略。
void HandleQueueFull(const mp::LogMsg& msg) {
  switch (msg.level) {
    case mp::LogLevel::FATAL:
    case mp::LogLevel::ERROR:
      ++g_logState.fallbackError;
      EmitFallbackShortLog(msg);
      break;

    case mp::LogLevel::WARN:
      ++g_logState.droppedWarn;
      break;

    case mp::LogLevel::INFO:
    case mp::LogLevel::DEBUG:
    case mp::LogLevel::TRACE:
    default:
      ++g_logState.droppedInfoDebug;
      break;
  }
}

// 可变参数日志的统一内部实现。
void Log_WriteV(mp::LogLevel level, const char* module, const char* fmt,
                va_list args) {
  if (IsInIsrContext()) {
    ++g_logState.droppedFromIsr;
    return;
  }

  if (!g_logState.initialized || !ShouldLog(level)) {
    return;
  }

  mp::LogMsg msg = {};
  msg.tickCount = static_cast<std::uint32_t>(xTaskGetTickCount());
  msg.level = level;

  CopyStringToBuffer(msg.module, sizeof(msg.module), module);
  FormatLogText(msg.text, sizeof(msg.text), fmt, args);
  StoreRecentLog(msg);

  if (!EnqueueLog(msg, GetQueueWaitTicks(level))) {
    HandleQueueFull(msg);
  }
}

}  // namespace

namespace mp {

void Log_Init() {
  g_logState.level = DEFAULT_LOG_PARAMS.defaultLevel;
  g_logState.droppedInfoDebug = 0U;
  g_logState.droppedWarn = 0U;
  g_logState.fallbackError = 0U;
  g_logState.droppedFromIsr = 0U;
  g_logState.initialized = true;
  g_recentLogWriteIndex = 0U;
  g_recentLogCount = 0U;
}

void Log_SetLevel(LogLevel level) {
  g_logState.level = level;
}

void Log_Write(LogLevel level, const char* module, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  Log_WriteV(level, module, fmt, args);
  va_end(args);
}

void Log_Fatal(const char* module, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  Log_WriteV(LogLevel::FATAL, module, fmt, args);
  va_end(args);
}

void Log_Error(const char* module, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  Log_WriteV(LogLevel::ERROR, module, fmt, args);
  va_end(args);
}

void Log_Warn(const char* module, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  Log_WriteV(LogLevel::WARN, module, fmt, args);
  va_end(args);
}

void Log_Info(const char* module, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  Log_WriteV(LogLevel::INFO, module, fmt, args);
  va_end(args);
}

void Log_Debug(const char* module, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  Log_WriteV(LogLevel::DEBUG, module, fmt, args);
  va_end(args);
}

std::size_t Log_GetRecent(LogMsg* out, std::size_t maxCount) {
  if (out == nullptr || maxCount == 0U) {
    return 0U;
  }

  const std::size_t count =
      (g_recentLogCount < maxCount) ? g_recentLogCount : maxCount;
  const std::size_t first =
      (g_recentLogWriteIndex + kRecentLogCapacity - g_recentLogCount) %
      kRecentLogCapacity;

  for (std::size_t index = 0U; index < count; ++index) {
    out[index] = g_recentLogs[(first + index) % kRecentLogCapacity];
  }
  return count;
}

}  // namespace mp
