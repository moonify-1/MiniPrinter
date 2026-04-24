#pragma once

#include <cstdint>

#include "common/app_error.h"
#include "common/fixed_pool.h"

namespace mp {

// 初始化打印数据缓存池。
//
// 会把固定池中的全部 LineBuffer 指针放入 qLineFree。
// 不启动真实打印，也不操作打印头、电机、VH 或 STB。
bool PrintSpooler_Init();

// 从空闲队列申请一个行缓冲。
//
// timeoutMs 是等待空闲缓冲的最长时间，单位 ms。
// 返回 nullptr 表示超时或队列未就绪。
LineBuffer* PrintSpooler_AllocLine(std::uint32_t timeoutMs);

// 提交一行数据到 qLineReady。
//
// 成功返回 APP_OK；队列满返回 ERR_QUEUE_FULL。
AppErrorCode PrintSpooler_SubmitLine(LineBuffer* line);

// 释放一行数据回 qLineFree。
bool PrintSpooler_FreeLine(LineBuffer* line);

// 清理某个 jobId 已经排队但尚未打印的行。
//
// 这只是队列清理，不会停止真实硬件，因为当前还没有启动真实打印。
void PrintSpooler_ClearJob(std::uint32_t jobId);

// 计算一行 48 字节位图中的黑点数量。
std::uint8_t PrintSpooler_CountBlackDots(const std::uint8_t* data,
                                         std::uint32_t len);

}  // namespace mp
