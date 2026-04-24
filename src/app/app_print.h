#pragma once

#include <cstdint>

#include "common/app_error.h"
#include "common/app_types.h"

namespace mp {

// 打印控制命令类型。
//
// 这些命令只描述“打印流程控制意图”，不直接等价于硬件动作：
// - PRINT_START：开始一个打印作业。
// - PRINT_END：上位机已经发送完本作业的行数据。
// - PRINT_CANCEL：取消当前作业，并清理已排队行。
// - FEED：走纸若干步，当前 Step 17 只做 mock 调用。
enum class PrintControlType : std::uint8_t {
  PRINT_START = 0,
  PRINT_END,
  PRINT_CANCEL,
  FEED,
};

// 打印控制队列消息。
//
// qPrintCtrl 中传递这个结构体。
// 结构体保持固定大小，方便 FreeRTOS 队列按值复制，避免动态内存。
struct PrintControlMsg {
  PrintControlType type; // 控制命令类型。
  std::uint32_t jobId;   // 作业编号；当前用协议 seq 作为简单来源。
  std::uint32_t value;   // 附加值；FEED 时表示步数，其他命令暂未使用。
};

// 打印作业状态快照。
//
// 这份快照用于日志、STATUS 或调试观察。
// 它不包含行数据本体，也不包含任何硬件句柄。
struct PrintAppSnapshot {
  PrintJobState state;       // 当前打印作业状态。
  std::uint32_t jobId;       // 当前作业编号。
  std::uint32_t printedLines; // 已模拟打印的行数。
  AppErrorCode lastError;    // 最近一次错误码。
  bool endRequested;         // 是否已收到 PRINT_END。
};

// 初始化打印作业状态机。
void PrintApp_Init();

// 标记一个作业开始。
void PrintApp_Start(std::uint32_t jobId);

// 标记上位机已经发送完当前作业。
void PrintApp_RequestEnd();

// 标记当前作业被取消。
void PrintApp_Cancel();

// 标记一行已经被 PrintEngineTask 模拟处理完成。
void PrintApp_MarkLinePrinted();

// 标记当前作业完成。
void PrintApp_Done();

// 标记当前作业失败。
void PrintApp_Failed(AppErrorCode error);

// 获取当前打印状态快照。
PrintAppSnapshot PrintApp_GetSnapshot();

// 判断当前是否正在接收或打印一个作业。
bool PrintApp_IsActive();

// 判断是否已经收到 PRINT_END。
bool PrintApp_IsEndRequested();

}  // namespace mp
