#pragma once

#include <cstddef>
#include <cstdint>

namespace mp {

// 单行打印数据的固定字节数。
//
// 当前热敏头每行 384 点，单色位图 1 bit/点：
// 384 / 8 = 48 bytes。
constexpr std::size_t LINE_BUFFER_DATA_SIZE = 48U;

// 行缓冲池默认容量。
//
// 固定 64 行的原因：
// - 不使用 malloc/free，内存占用启动时就确定。
// - 队列里只传指针，避免重复复制 48 字节数据。
constexpr std::size_t LINE_BUFFER_POOL_SIZE = 64U;

// 行缓冲。
//
// 这个结构体只保存“已经解包好的一行打印位图数据”，
// 不包含打印头、电机或加热控制逻辑。
struct LineBuffer {
  std::uint32_t jobId;        // 打印任务编号，用于清理某个任务的残留行。
  std::uint32_t lineNo;       // 当前任务内的行号。
  std::uint8_t data[48];      // 一行 48 字节位图数据。
  std::uint8_t blackDotCount; // 当前行黑点数量，用于后续能量估算。
  std::uint32_t flags;        // 预留标志位，例如压缩、反色、最后一行等。
};

// 初始化固定行缓冲池。
//
// 只清空静态数组，不做任何动态内存分配。
void LineBufferPool_Init();

// 获取池中第 index 个缓冲。
//
// index 超出范围时返回 nullptr。
LineBuffer* LineBufferPool_Get(std::size_t index);

// 判断一个指针是否来自固定池。
bool LineBufferPool_Contains(const LineBuffer* line);

// 清空一个 LineBuffer 的内容。
void LineBuffer_Reset(LineBuffer* line);

}  // namespace mp
