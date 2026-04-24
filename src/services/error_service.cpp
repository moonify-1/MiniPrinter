#include "services/error_service.h"

#include <cstring>

#include "rtos/rtos_objects.h"
#include "services/log_service.h"

namespace {

constexpr TickType_t kNoWait = 0U;

mp::ErrorEvent g_lastError = {};
bool g_hasLastError = false;
bool g_safeModeRequired = false;

// 安全复制短文本。
//
// 嵌入式代码中不要假设调用者传来的字符串一定非空或长度合适。
void CopyText(char* dst, std::size_t dstLen, const char* src) {
  if (dst == nullptr || dstLen == 0U) {
    return;
  }

  const char* text = (src != nullptr) ? src : "";
  std::strncpy(dst, text, dstLen - 1U);
  dst[dstLen - 1U] = '\0';
}

bool ShouldEnterSafeMode(mp::AppErrorCode code) {
  const mp::ErrorSeverity severity = mp::getErrorSeverity(code);
  if (severity == mp::ErrorSeverity::FATAL) {
    return true;
  }

  // 某些非 FATAL 错误也可能显式要求安全模式。
  const std::uint8_t action =
      static_cast<std::uint8_t>((code >> 24U) & 0x0FU);
  return action ==
         static_cast<std::uint8_t>(mp::ErrorRecoverAction::ENTER_SAFE_MODE);
}

}  // namespace

namespace mp {

void Error_Report(AppErrorCode code, const char* module, const char* detail) {
  ErrorEvent event = {};
  event.code = code;
  event.severity = getErrorSeverity(code);
  event.timestamp = xTaskGetTickCount();
  CopyText(event.module, sizeof(event.module), module);
  CopyText(event.detail, sizeof(event.detail), detail);

  g_lastError = event;
  g_hasLastError = true;

  if (ShouldEnterSafeMode(code)) {
    g_safeModeRequired = true;
  }

  if (g_rtos.qError != nullptr) {
    if (xQueueSend(g_rtos.qError, &event, kNoWait) != pdPASS) {
      ErrorEvent dropped = {};
      (void)xQueueReceive(g_rtos.qError, &dropped, kNoWait);
      (void)xQueueSend(g_rtos.qError, &event, kNoWait);
    }
  }

  if (g_rtos.systemEvents != nullptr) {
    EventBits_t bits = EVT_ERROR_PENDING;
    if (g_safeModeRequired) {
      bits |= EVT_SAFE_MODE;
    }
    xEventGroupSetBits(g_rtos.systemEvents, bits);
  }

  switch (event.severity) {
    case ErrorSeverity::WARN:
      Log_Warn("error", "%s code=0x%08lX detail=%s", event.module,
               static_cast<unsigned long>(event.code), event.detail);
      break;
    case ErrorSeverity::FATAL:
      Log_Fatal("error", "%s code=0x%08lX detail=%s", event.module,
                static_cast<unsigned long>(event.code), event.detail);
      break;
    case ErrorSeverity::ERROR:
      Log_Error("error", "%s code=0x%08lX detail=%s", event.module,
                static_cast<unsigned long>(event.code), event.detail);
      break;
    default:
      Log_Info("error", "%s code=0x%08lX detail=%s", event.module,
               static_cast<unsigned long>(event.code), event.detail);
      break;
  }
}

ErrorEvent Error_GetLast() {
  if (g_hasLastError) {
    return g_lastError;
  }

  ErrorEvent empty = {};
  empty.code = APP_OK;
  empty.severity = ErrorSeverity::NONE;
  return empty;
}

void Error_ClearRecoverable() {
  if (g_safeModeRequired) {
    return;
  }

  if (g_rtos.systemEvents != nullptr) {
    xEventGroupClearBits(g_rtos.systemEvents, EVT_ERROR_PENDING);
  }
}

bool Error_IsSafeModeRequired() {
  return g_safeModeRequired;
}

}  // namespace mp
