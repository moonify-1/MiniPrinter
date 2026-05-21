#include "services/print_file_service.h"

#include <cstring>

#include "services/print_spooler.h"

namespace {

// 固定上传槽。
//
// data 使用静态数组而不是 malloc：
// - 启动时内存占用一眼可见。
// - 不会出现堆碎片。
// - 上传上限由 PRINT_FILE_MAX_BYTES 明确限制。
struct PrintFileSlot {
  mp::PrintFileSnapshot snapshot;
  std::uint8_t data[mp::PRINT_FILE_MAX_BYTES];
  bool written[mp::PRINT_FILE_MAX_BYTES];
};

PrintFileSlot g_slots[mp::PRINT_FILE_SLOT_COUNT] = {};
std::uint32_t g_nextFileId = 1U;

// 标准 CRC32/IEEE 更新函数。
//
// 这里按字节更新，代码短且足够用于当前小文件。
// crc 入参和返回值都保持“运行中取反前”的形式，便于分段计算。
std::uint32_t Crc32Update(std::uint32_t crc, const std::uint8_t* data,
                          std::uint32_t len) {
  for (std::uint32_t index = 0U; index < len; ++index) {
    crc ^= data[index];
    for (std::uint8_t bit = 0U; bit < 8U; ++bit) {
      const bool lsbSet = (crc & 1U) != 0U;
      crc >>= 1U;
      if (lsbSet) {
        crc ^= 0xEDB88320UL;
      }
    }
  }
  return crc;
}

std::uint32_t Crc32Calc(const std::uint8_t* data, std::uint32_t len) {
  return Crc32Update(0xFFFFFFFFUL, data, len) ^ 0xFFFFFFFFUL;
}

void ResetSlot(PrintFileSlot& slot) {
  slot.snapshot = {};
  std::memset(slot.data, 0, sizeof(slot.data));
  std::memset(slot.written, 0, sizeof(slot.written));
}

PrintFileSlot* FindSlot(std::uint32_t fileId) {
  if (fileId == 0U) {
    return nullptr;
  }

  for (std::uint8_t index = 0U; index < mp::PRINT_FILE_SLOT_COUNT; ++index) {
    if (g_slots[index].snapshot.fileId == fileId &&
        g_slots[index].snapshot.state != mp::PrintFileState::EMPTY) {
      return &g_slots[index];
    }
  }
  return nullptr;
}

PrintFileSlot* FindEmptySlot() {
  for (std::uint8_t index = 0U; index < mp::PRINT_FILE_SLOT_COUNT; ++index) {
    if (g_slots[index].snapshot.state == mp::PrintFileState::EMPTY) {
      return &g_slots[index];
    }
  }
  return nullptr;
}

bool IsValidTotalBytes(std::uint32_t totalBytes) {
  return totalBytes > 0U && totalBytes <= mp::PRINT_FILE_MAX_BYTES &&
         (totalBytes % mp::LINE_BUFFER_DATA_SIZE) == 0U;
}

std::uint32_t CountWrittenBytes(const PrintFileSlot& slot) {
  std::uint32_t count = 0U;
  for (std::uint32_t index = 0U; index < slot.snapshot.totalBytes; ++index) {
    if (slot.written[index]) {
      ++count;
    }
  }
  return count;
}

}  // namespace

namespace mp {

void PrintFileService_Init() {
  for (std::uint8_t index = 0U; index < PRINT_FILE_SLOT_COUNT; ++index) {
    ResetSlot(g_slots[index]);
  }
  g_nextFileId = 1U;
}

AppErrorCode PrintFileService_Create(std::uint32_t totalBytes,
                                     std::uint32_t expectedCrc32,
                                     std::uint32_t* fileIdOut) {
  if (fileIdOut == nullptr || !IsValidTotalBytes(totalBytes)) {
    return ERR_COMM_FRAME_TOO_LONG;
  }

  PrintFileSlot* slot = FindEmptySlot();
  if (slot == nullptr) {
    return ERR_QUEUE_FULL;
  }

  ResetSlot(*slot);
  const std::uint32_t fileId = g_nextFileId++;
  slot->snapshot.fileId = fileId;
  slot->snapshot.state = PrintFileState::RECEIVING;
  slot->snapshot.totalBytes = totalBytes;
  slot->snapshot.receivedBytes = 0U;
  slot->snapshot.lineCount = totalBytes / LINE_BUFFER_DATA_SIZE;
  slot->snapshot.expectedCrc32 = expectedCrc32;
  slot->snapshot.actualCrc32 = 0U;
  *fileIdOut = fileId;
  return APP_OK;
}

AppErrorCode PrintFileService_WriteChunk(std::uint32_t fileId,
                                         std::uint32_t index,
                                         const std::uint8_t* data,
                                         std::uint32_t len) {
  PrintFileSlot* slot = FindSlot(fileId);
  if (slot == nullptr || data == nullptr || len == 0U) {
    return ERR_COMM_CRC;
  }

  if (slot->snapshot.state != PrintFileState::RECEIVING) {
    return ERR_COMM_CRC;
  }

  const std::uint32_t offset = index * PRINT_FILE_CHUNK_SIZE;
  if (offset >= slot->snapshot.totalBytes ||
      len > (slot->snapshot.totalBytes - offset)) {
    return ERR_COMM_FRAME_TOO_LONG;
  }

  for (std::uint32_t byteIndex = 0U; byteIndex < len; ++byteIndex) {
    const std::uint32_t pos = offset + byteIndex;
    slot->data[pos] = data[byteIndex];
    slot->written[pos] = true;
  }

  slot->snapshot.receivedBytes = CountWrittenBytes(*slot);
  return APP_OK;
}

AppErrorCode PrintFileService_CreateCompleteFromData(
    const std::uint8_t* data, std::uint32_t len, std::uint32_t* fileIdOut) {
  if (data == nullptr || fileIdOut == nullptr || !IsValidTotalBytes(len)) {
    return ERR_COMM_FRAME_TOO_LONG;
  }

  std::uint32_t fileId = 0U;
  AppErrorCode result = PrintFileService_Create(len, Crc32Calc(data, len),
                                                &fileId);
  if (result != APP_OK) {
    return result;
  }

  // 复用正式分片写入逻辑，让工厂测试和真实上传走同一套边界检查。
  for (std::uint32_t offset = 0U; offset < len;
       offset += PRINT_FILE_CHUNK_SIZE) {
    const std::uint32_t chunkIndex = offset / PRINT_FILE_CHUNK_SIZE;
    const std::uint32_t remain = len - offset;
    const std::uint32_t chunkLen =
        (remain > PRINT_FILE_CHUNK_SIZE) ? PRINT_FILE_CHUNK_SIZE : remain;
    result = PrintFileService_WriteChunk(fileId, chunkIndex, &data[offset],
                                         chunkLen);
    if (result != APP_OK) {
      (void)PrintFileService_Delete(fileId);
      return result;
    }
  }

  result = PrintFileService_Complete(fileId);
  if (result != APP_OK) {
    (void)PrintFileService_Delete(fileId);
    return result;
  }

  *fileIdOut = fileId;
  return APP_OK;
}

AppErrorCode PrintFileService_Complete(std::uint32_t fileId) {
  PrintFileSlot* slot = FindSlot(fileId);
  if (slot == nullptr || slot->snapshot.state != PrintFileState::RECEIVING) {
    return ERR_COMM_CRC;
  }

  slot->snapshot.receivedBytes = CountWrittenBytes(*slot);
  if (slot->snapshot.receivedBytes != slot->snapshot.totalBytes ||
      !IsValidTotalBytes(slot->snapshot.totalBytes)) {
    return ERR_COMM_FRAME_TOO_LONG;
  }

  slot->snapshot.actualCrc32 =
      Crc32Calc(slot->data, slot->snapshot.totalBytes);
  if (slot->snapshot.actualCrc32 != slot->snapshot.expectedCrc32) {
    return ERR_COMM_CRC;
  }

  slot->snapshot.state = PrintFileState::COMPLETE;
  return APP_OK;
}

AppErrorCode PrintFileService_Delete(std::uint32_t fileId) {
  PrintFileSlot* slot = FindSlot(fileId);
  if (slot == nullptr) {
    return ERR_COMM_CRC;
  }

  ResetSlot(*slot);
  return APP_OK;
}

bool PrintFileService_Get(std::uint32_t fileId, PrintFileSnapshot* out) {
  PrintFileSlot* slot = FindSlot(fileId);
  if (slot == nullptr || out == nullptr) {
    return false;
  }

  *out = slot->snapshot;
  return true;
}

std::size_t PrintFileService_List(PrintFileSnapshot* out,
                                  std::size_t maxCount) {
  if (out == nullptr || maxCount == 0U) {
    return 0U;
  }

  std::size_t count = 0U;
  for (std::uint8_t index = 0U; index < PRINT_FILE_SLOT_COUNT; ++index) {
    if (g_slots[index].snapshot.state == PrintFileState::EMPTY) {
      continue;
    }

    if (count >= maxCount) {
      break;
    }
    out[count++] = g_slots[index].snapshot;
  }
  return count;
}

bool PrintFileService_ReadLine(std::uint32_t fileId, std::uint32_t lineNo,
                               std::uint8_t* out, std::size_t outLen) {
  PrintFileSlot* slot = FindSlot(fileId);
  if (slot == nullptr || out == nullptr || outLen < LINE_BUFFER_DATA_SIZE ||
      slot->snapshot.state != PrintFileState::COMPLETE ||
      lineNo >= slot->snapshot.lineCount) {
    return false;
  }

  const std::uint32_t offset =
      lineNo * static_cast<std::uint32_t>(LINE_BUFFER_DATA_SIZE);
  std::memcpy(out, &slot->data[offset], LINE_BUFFER_DATA_SIZE);
  return true;
}

AppErrorCode PrintFileService_QueueForPrint(std::uint32_t fileId,
                                            std::uint32_t jobId) {
  PrintFileSlot* slot = FindSlot(fileId);
  if (slot == nullptr || slot->snapshot.state != PrintFileState::COMPLETE) {
    return ERR_COMM_CRC;
  }

  for (std::uint32_t lineNo = 0U; lineNo < slot->snapshot.lineCount;
       ++lineNo) {
    LineBuffer* line = PrintSpooler_AllocLine(10U);
    if (line == nullptr) {
      PrintSpooler_ClearJob(jobId);
      return ERR_QUEUE_FULL;
    }

    const std::uint32_t offset =
        lineNo * static_cast<std::uint32_t>(LINE_BUFFER_DATA_SIZE);
    line->jobId = jobId;
    line->lineNo = lineNo;
    std::memcpy(line->data, &slot->data[offset], LINE_BUFFER_DATA_SIZE);
    line->blackDotCount =
        PrintSpooler_CountBlackDots(line->data, LINE_BUFFER_DATA_SIZE);
    line->flags = 0U;

    const AppErrorCode submitResult = PrintSpooler_SubmitLine(line);
    if (submitResult != APP_OK) {
      (void)PrintSpooler_FreeLine(line);
      PrintSpooler_ClearJob(jobId);
      return submitResult;
    }
  }

  return APP_OK;
}

}  // namespace mp
