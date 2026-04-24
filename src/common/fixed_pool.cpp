#include "common/fixed_pool.h"

#include <cstring>

namespace {

mp::LineBuffer g_lineBufferPool[mp::LINE_BUFFER_POOL_SIZE] = {};

}  // namespace

namespace mp {

void LineBufferPool_Init() {
  for (std::size_t index = 0U; index < LINE_BUFFER_POOL_SIZE; ++index) {
    LineBuffer_Reset(&g_lineBufferPool[index]);
  }
}

LineBuffer* LineBufferPool_Get(std::size_t index) {
  if (index >= LINE_BUFFER_POOL_SIZE) {
    return nullptr;
  }

  return &g_lineBufferPool[index];
}

bool LineBufferPool_Contains(const LineBuffer* line) {
  if (line == nullptr) {
    return false;
  }

  const LineBuffer* begin = &g_lineBufferPool[0];
  const LineBuffer* end = &g_lineBufferPool[LINE_BUFFER_POOL_SIZE];
  return line >= begin && line < end;
}

void LineBuffer_Reset(LineBuffer* line) {
  if (line == nullptr) {
    return;
  }

  line->jobId = 0U;
  line->lineNo = 0U;
  std::memset(line->data, 0, sizeof(line->data));
  line->blackDotCount = 0U;
  line->flags = 0U;
}

}  // namespace mp
