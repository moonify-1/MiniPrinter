#include "services/param_service.h"

#include <cstddef>
#include <cstdint>

#include "drivers/storage/drv_nvs.h"
#include "rtos/rtos_objects.h"
#include "services/log_service.h"

namespace {

constexpr TickType_t kParamMutexWaitTicks = pdMS_TO_TICKS(20U);

// NVS 中真实保存的结构。
//
// ParamBlock 本身没有 length 字段，为了满足持久化校验需求，
// 这里在存储层增加一层 record header：
// - magic：判断是不是本项目参数。
// - version：判断结构版本是否兼容。
// - length：判断 ParamBlock 大小是否符合当前固件。
// - crc32：判断 ParamBlock 内容是否损坏。
struct ParamStorageRecord {
  std::uint32_t magic;
  std::uint16_t version;
  std::uint16_t length;
  std::uint32_t crc32;
  mp::ParamBlock params;
};

mp::ParamBlock g_paramSnapshot = mp::DEFAULT_PARAM_BLOCK;
bool g_nvsAvailable = false;
bool g_paramLoadedFromNvs = false;

bool LockParamMutex() {
  if (mp::g_rtos.paramMutex == nullptr) {
    return true;
  }

  return xSemaphoreTake(mp::g_rtos.paramMutex, kParamMutexWaitTicks) == pdTRUE;
}

void UnlockParamMutex() {
  if (mp::g_rtos.paramMutex == nullptr) {
    return;
  }

  xSemaphoreGive(mp::g_rtos.paramMutex);
}

// CRC32/ISO-HDLC 计算。
//
// 算法说明：
// - 初值：0xFFFFFFFF
// - 多项式：0x04C11DB7，按反射实现时使用 0xEDB88320
// - 输入字节反射
// - 输出最终异或：0xFFFFFFFF
//
// 这是最常见的 CRC32 形式，便于后续用 PC 工具复现。
std::uint32_t CalcCrc32(const std::uint8_t* data, std::size_t len) {
  std::uint32_t crc = 0xFFFFFFFFUL;

  if (data == nullptr) {
    return crc ^ 0xFFFFFFFFUL;
  }

  for (std::size_t index = 0U; index < len; ++index) {
    crc ^= data[index];
    for (std::uint8_t bit = 0U; bit < 8U; ++bit) {
      if ((crc & 1U) != 0U) {
        crc = (crc >> 1U) ^ 0xEDB88320UL;
      } else {
        crc >>= 1U;
      }
    }
  }

  return crc ^ 0xFFFFFFFFUL;
}

mp::ParamBlock NormalizeParamBlock(mp::ParamBlock params) {
  params.magic = mp::PARAM_BLOCK_MAGIC;
  params.version = mp::PARAM_BLOCK_VERSION;
  params.crc32 = 0U;
  params.crc32 =
      CalcCrc32(reinterpret_cast<const std::uint8_t*>(&params), sizeof(params));
  return params;
}

mp::ParamBlock MakeDefaultParams() {
  return NormalizeParamBlock(mp::DEFAULT_PARAM_BLOCK);
}

ParamStorageRecord MakeStorageRecord(const mp::ParamBlock& params) {
  ParamStorageRecord record = {};
  record.params = NormalizeParamBlock(params);
  record.magic = mp::PARAM_BLOCK_MAGIC;
  record.version = mp::PARAM_BLOCK_VERSION;
  record.length = static_cast<std::uint16_t>(sizeof(mp::ParamBlock));
  record.crc32 = record.params.crc32;
  return record;
}

bool ValidateStorageRecord(const ParamStorageRecord& record) {
  if (record.magic != mp::PARAM_BLOCK_MAGIC) {
    return false;
  }

  if (record.version != mp::PARAM_BLOCK_VERSION) {
    return false;
  }

  if (record.length != sizeof(mp::ParamBlock)) {
    return false;
  }

  if (record.params.magic != mp::PARAM_BLOCK_MAGIC ||
      record.params.version != mp::PARAM_BLOCK_VERSION) {
    return false;
  }

  mp::ParamBlock crcCheck = record.params;
  crcCheck.crc32 = 0U;
  const std::uint32_t expected =
      CalcCrc32(reinterpret_cast<const std::uint8_t*>(&crcCheck),
                sizeof(crcCheck));

  return expected == record.crc32 && record.params.crc32 == record.crc32;
}

void SetSnapshot(const mp::ParamBlock& params) {
  if (!LockParamMutex()) {
    return;
  }

  g_paramSnapshot = NormalizeParamBlock(params);
  UnlockParamMutex();
}

}  // namespace

namespace mp {

bool Param_Init() {
  SetSnapshot(MakeDefaultParams());
  g_nvsAvailable = DrvNvs_Init();
  g_paramLoadedFromNvs = false;
  return true;
}

bool Param_Load() {
  ParamStorageRecord record = {};

  if (g_nvsAvailable &&
      DrvNvs_ReadParamBlob(&record, sizeof(record)) &&
      ValidateStorageRecord(record)) {
    SetSnapshot(record.params);
    g_paramLoadedFromNvs = true;
    Log_Info("param", "Param load ok crc=0x%08lX",
             static_cast<unsigned long>(record.crc32));
    return true;
  }

  SetSnapshot(MakeDefaultParams());
  g_paramLoadedFromNvs = false;

  if (!g_nvsAvailable) {
    Log_Warn("param", "NVS unavailable, using default params");
  } else {
    Log_Warn("param", "Param invalid or missing, using default params");
  }
  return false;
}

ParamBlock Param_GetSnapshot() {
  if (!LockParamMutex()) {
    return MakeDefaultParams();
  }

  const ParamBlock snapshot = g_paramSnapshot;
  UnlockParamMutex();
  return snapshot;
}

bool Param_Update(const ParamBlock& params) {
  SetSnapshot(params);
  return true;
}

bool Param_RequestSave() {
  if (g_rtos.qParam == nullptr) {
    return false;
  }

  const std::uint32_t request = PARAM_REQUEST_SAVE;
  if (xQueueSend(g_rtos.qParam, &request, 0U) == pdPASS) {
    return true;
  }

  std::uint32_t dropped = 0U;
  (void)xQueueReceive(g_rtos.qParam, &dropped, 0U);
  return xQueueSend(g_rtos.qParam, &request, 0U) == pdPASS;
}

bool Param_ResetDefault() {
  SetSnapshot(MakeDefaultParams());
  return Param_RequestSave();
}

bool Param_SaveNowForTask() {
  if (!g_nvsAvailable) {
    Log_Warn("param", "Param save skipped, NVS unavailable");
    return false;
  }

  const ParamBlock snapshot = Param_GetSnapshot();
  const ParamStorageRecord record = MakeStorageRecord(snapshot);
  const bool ok = DrvNvs_WriteParamBlob(&record, sizeof(record));

  if (ok) {
    Log_Info("param", "Param saved crc=0x%08lX",
             static_cast<unsigned long>(record.crc32));
  } else {
    Log_Error("param", "Param save failed");
  }
  return ok;
}

}  // namespace mp
