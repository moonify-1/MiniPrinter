#pragma once

#include <cstddef>
#include <cstdint>

#include "common/app_error.h"
#include "common/fixed_pool.h"

namespace mp {

// 打印文件上传状态。
//
// WiFi API 第一阶段只支持 raw 位图文件：
// - 每行固定 48 字节。
// - 文件先上传到固定 RAM 槽位。
// - complete 校验通过后，后续打印任务才能按 file_id 启动。
enum class PrintFileState : std::uint8_t {
  EMPTY = 0,     // 槽位空闲，没有有效文件。
  RECEIVING,     // 正在接收分片，还不能用于打印。
  COMPLETE,      // 已收完并通过 CRC32/行对齐校验，可以启动打印。
};

// 上传文件状态快照。
//
// 这个结构只保存元数据，不暴露文件内容本体。
// WiFi API 用它生成 JSON，打印任务也可以用它确认文件是否可用。
struct PrintFileSnapshot {
  std::uint32_t fileId;       // 上传文件编号，0 表示无效。
  PrintFileState state;       // 当前文件状态。
  std::uint32_t totalBytes;   // 期望总字节数，必须是 48 的整数倍。
  std::uint32_t receivedBytes; // 已成功写入的字节数。
  std::uint32_t lineCount;    // 行数，等于 totalBytes / 48。
  std::uint32_t expectedCrc32; // 创建上传时声明的 CRC32。
  std::uint32_t actualCrc32;   // complete 时计算得到的 CRC32。
};

// 打印文件上传能力边界。
//
// 当前不接入文件系统，所以只保留小型固定 RAM 槽。
// 64 行正好匹配当前 LineBuffer 固定池容量，后续启动 mock 打印时可以一次入队。
constexpr std::uint8_t PRINT_FILE_SLOT_COUNT = 2U;
constexpr std::uint32_t PRINT_FILE_CHUNK_SIZE = 512U;
constexpr std::uint32_t PRINT_FILE_MAX_LINES =
    static_cast<std::uint32_t>(LINE_BUFFER_POOL_SIZE);
constexpr std::uint32_t PRINT_FILE_MAX_BYTES =
    PRINT_FILE_MAX_LINES * static_cast<std::uint32_t>(LINE_BUFFER_DATA_SIZE);

// 初始化打印文件服务。
//
// 只清空固定槽位，不分配堆内存，不操作打印头或电机。
void PrintFileService_Init();

// 创建一个 raw 打印文件上传会话。
//
// totalBytes 必须：
// - 大于 0。
// - 不超过 PRINT_FILE_MAX_BYTES。
// - 是 48 字节行宽的整数倍。
// expectedCrc32 是标准 CRC32/IEEE 校验值，complete 阶段会重新计算。
AppErrorCode PrintFileService_Create(std::uint32_t totalBytes,
                                     std::uint32_t expectedCrc32,
                                     std::uint32_t* fileIdOut);

// 写入一个分片。
//
// index 是从 0 开始的分片序号，实际写入偏移为 index * PRINT_FILE_CHUNK_SIZE。
// 允许最后一个分片短于 PRINT_FILE_CHUNK_SIZE，但不允许越过 totalBytes。
AppErrorCode PrintFileService_WriteChunk(std::uint32_t fileId,
                                         std::uint32_t index,
                                         const std::uint8_t* data,
                                         std::uint32_t len);

// 完成上传并校验文件。
//
// 校验项：
// - 已收到字节数等于 totalBytes。
// - totalBytes 仍保持 48 字节行对齐。
// - 重新计算 CRC32 等于 expectedCrc32。
AppErrorCode PrintFileService_Complete(std::uint32_t fileId);

// 删除一个上传文件，释放槽位。
AppErrorCode PrintFileService_Delete(std::uint32_t fileId);

// 读取指定文件快照。
bool PrintFileService_Get(std::uint32_t fileId, PrintFileSnapshot* out);

// 列出当前非空文件。
std::size_t PrintFileService_List(PrintFileSnapshot* out, std::size_t maxCount);

// 读取文件中某一行 raw 数据。
//
// lineNo 从 0 开始；out 必须至少能容纳 48 字节。
bool PrintFileService_ReadLine(std::uint32_t fileId, std::uint32_t lineNo,
                               std::uint8_t* out, std::size_t outLen);

// 把已完成上传的文件复制进现有打印行队列。
//
// 这个函数只做“文件内容 -> LineBuffer 队列”的搬运：
// - 不直接启动打印头。
// - 不打开 VH。
// - 不控制电机。
// 真正的打印流程仍由 PrintService 和 PrintEngineTask 调度。
AppErrorCode PrintFileService_QueueForPrint(std::uint32_t fileId,
                                            std::uint32_t jobId);

}  // namespace mp
