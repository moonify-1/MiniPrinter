#include "app/app_print.h"

#include "rtos/rtos_objects.h"

namespace {

// 打印状态不能用很短超时静默放弃。
//
// 实机一键打印时，HTTP 轮询会频繁读取 PrintAppSnapshot；
// 如果 PrintEngineTask 正好在每行结束时更新 printedLines，5ms 超时可能让纸面已经打印，
// 但 line_done 少记 1 行。这里选择一直等到互斥量可用，让状态记录和真实动作保持一致。
constexpr TickType_t kPrintStateMutexWaitTicks = portMAX_DELAY;

mp::PrintAppSnapshot g_print = {
    mp::PrintJobState::NONE,
    0U,
    0U,
    mp::APP_OK,
    false,
};

// 尝试获取状态互斥量。
//
// 当前复用 stateMutex，避免为 Step 17 再引入新的 RTOS 对象。
// 如果 RTOS 对象尚未创建，则直接放行，便于早期初始化代码调用。
bool LockPrintState() {
  if (mp::g_rtos.stateMutex == nullptr) {
    return true;
  }

  return xSemaphoreTake(mp::g_rtos.stateMutex, kPrintStateMutexWaitTicks) ==
         pdTRUE;
}

// 释放状态互斥量。
void UnlockPrintState() {
  if (mp::g_rtos.stateMutex == nullptr) {
    return;
  }

  xSemaphoreGive(mp::g_rtos.stateMutex);
}

}  // namespace

namespace mp {

void PrintApp_Init() {
  if (!LockPrintState()) {
    return;
  }

  g_print.state = PrintJobState::NONE;
  g_print.jobId = 0U;
  g_print.printedLines = 0U;
  g_print.lastError = APP_OK;
  g_print.endRequested = false;
  UnlockPrintState();
}

void PrintApp_Start(std::uint32_t jobId) {
  if (!LockPrintState()) {
    return;
  }

  g_print.state = PrintJobState::PRINTING;
  g_print.jobId = jobId;
  g_print.printedLines = 0U;
  g_print.lastError = APP_OK;
  g_print.endRequested = false;
  UnlockPrintState();
}

void PrintApp_RequestEnd() {
  if (!LockPrintState()) {
    return;
  }

  g_print.endRequested = true;
  UnlockPrintState();
}

void PrintApp_Cancel() {
  if (!LockPrintState()) {
    return;
  }

  g_print.state = PrintJobState::CANCELLED;
  g_print.endRequested = true;
  UnlockPrintState();
}

void PrintApp_MarkLinePrinted() {
  if (!LockPrintState()) {
    return;
  }

  g_print.printedLines++;
  UnlockPrintState();
}

void PrintApp_Done() {
  if (!LockPrintState()) {
    return;
  }

  g_print.state = PrintJobState::DONE;
  g_print.endRequested = true;
  UnlockPrintState();
}

void PrintApp_Failed(AppErrorCode error) {
  if (!LockPrintState()) {
    return;
  }

  g_print.state = PrintJobState::FAILED;
  g_print.lastError = error;
  g_print.endRequested = true;
  UnlockPrintState();
}

PrintAppSnapshot PrintApp_GetSnapshot() {
  if (!LockPrintState()) {
    PrintAppSnapshot fallback = {};
    fallback.state = PrintJobState::FAILED;
    fallback.lastError = ERR_SYS_INIT_FAILED;
    return fallback;
  }

  const PrintAppSnapshot snapshot = g_print;
  UnlockPrintState();
  return snapshot;
}

bool PrintApp_IsActive() {
  const PrintAppSnapshot snapshot = PrintApp_GetSnapshot();
  return snapshot.state == PrintJobState::RECEIVING ||
         snapshot.state == PrintJobState::QUEUED ||
         snapshot.state == PrintJobState::PRINTING ||
         snapshot.state == PrintJobState::PAUSED;
}

bool PrintApp_IsEndRequested() {
  return PrintApp_GetSnapshot().endRequested;
}

}  // namespace mp
