#include "services/print_spooler.h"

#include <cstring>

#include "rtos/rtos_objects.h"

namespace {

constexpr TickType_t kNoWait = 0U;

// 统计一个字节中有多少个 bit 为 1。
//
// 当前位图是单色 1 bit/点，因此 bit=1 代表一个黑点。
std::uint8_t CountBits(std::uint8_t value) {
  std::uint8_t count = 0U;
  while (value != 0U) {
    count = static_cast<std::uint8_t>(count + (value & 1U));
    value = static_cast<std::uint8_t>(value >> 1U);
  }
  return count;
}

bool IsQueueReady() {
  return mp::g_rtos.qLineFree != nullptr && mp::g_rtos.qLineReady != nullptr;
}

}  // namespace

namespace mp {

bool PrintSpooler_Init() {
  if (!IsQueueReady()) {
    return false;
  }

  LineBufferPool_Init();
  xQueueReset(g_rtos.qLineFree);
  xQueueReset(g_rtos.qLineReady);

  for (std::size_t index = 0U; index < LINE_BUFFER_POOL_SIZE; ++index) {
    LineBuffer* line = LineBufferPool_Get(index);
    if (xQueueSend(g_rtos.qLineFree, &line, kNoWait) != pdPASS) {
      return false;
    }
  }

  return true;
}

LineBuffer* PrintSpooler_AllocLine(std::uint32_t timeoutMs) {
  if (!IsQueueReady()) {
    return nullptr;
  }

  LineBuffer* line = nullptr;
  const TickType_t waitTicks = pdMS_TO_TICKS(timeoutMs);
  if (xQueueReceive(g_rtos.qLineFree, &line, waitTicks) != pdPASS) {
    return nullptr;
  }

  LineBuffer_Reset(line);
  return line;
}

AppErrorCode PrintSpooler_SubmitLine(LineBuffer* line) {
  if (!LineBufferPool_Contains(line) || g_rtos.qLineReady == nullptr) {
    return ERR_QUEUE_FULL;
  }

  if (xQueueSend(g_rtos.qLineReady, &line, kNoWait) != pdPASS) {
    return ERR_QUEUE_FULL;
  }

  return APP_OK;
}

bool PrintSpooler_FreeLine(LineBuffer* line) {
  if (!LineBufferPool_Contains(line) || g_rtos.qLineFree == nullptr) {
    return false;
  }

  LineBuffer_Reset(line);
  return xQueueSend(g_rtos.qLineFree, &line, kNoWait) == pdPASS;
}

void PrintSpooler_ClearJob(std::uint32_t jobId) {
  if (g_rtos.qLineReady == nullptr) {
    return;
  }

  const UBaseType_t queuedCount = uxQueueMessagesWaiting(g_rtos.qLineReady);
  for (UBaseType_t index = 0U; index < queuedCount; ++index) {
    LineBuffer* line = nullptr;
    if (xQueueReceive(g_rtos.qLineReady, &line, kNoWait) != pdPASS) {
      return;
    }

    if (line != nullptr && line->jobId == jobId) {
      (void)PrintSpooler_FreeLine(line);
      continue;
    }

    (void)xQueueSend(g_rtos.qLineReady, &line, kNoWait);
  }
}

std::uint8_t PrintSpooler_CountBlackDots(const std::uint8_t* data,
                                         std::uint32_t len) {
  if (data == nullptr) {
    return 0U;
  }

  std::uint16_t total = 0U;
  for (std::uint32_t index = 0U; index < len; ++index) {
    total = static_cast<std::uint16_t>(total + CountBits(data[index]));
    if (total > 255U) {
      // LineBuffer::blackDotCount 当前按需求定义为 uint8_t，
      // 但一行最多可能有 384 个黑点，因此这里做饱和保护。
      return 255U;
    }
  }

  return static_cast<std::uint8_t>(total);
}

}  // namespace mp
