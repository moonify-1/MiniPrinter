#include "drivers/storage/drv_nvs.h"

#include <Preferences.h>

namespace {

constexpr const char* kNvsNamespace = "mp_param";
constexpr const char* kParamBlobKey = "blob";

// Preferences 是 Arduino ESP32 对 NVS 的轻量封装。
//
// 这里每次读写都 begin/end，是为了让驱动保持无状态，
// 避免长期持有 NVS 句柄导致后续扩展时生命周期不清楚。
Preferences g_preferences;

}  // namespace

namespace mp {

bool DrvNvs_Init() {
  if (!g_preferences.begin(kNvsNamespace, true)) {
    return false;
  }

  g_preferences.end();
  return true;
}

bool DrvNvs_ReadParamBlob(void* data, std::size_t len) {
  if (data == nullptr || len == 0U) {
    return false;
  }

  if (!g_preferences.begin(kNvsNamespace, true)) {
    return false;
  }

  const std::size_t storedLen = g_preferences.getBytesLength(kParamBlobKey);
  if (storedLen != len) {
    g_preferences.end();
    return false;
  }

  const std::size_t readLen = g_preferences.getBytes(kParamBlobKey, data, len);
  g_preferences.end();
  return readLen == len;
}

bool DrvNvs_WriteParamBlob(const void* data, std::size_t len) {
  if (data == nullptr || len == 0U) {
    return false;
  }

  if (!g_preferences.begin(kNvsNamespace, false)) {
    return false;
  }

  const std::size_t writtenLen = g_preferences.putBytes(kParamBlobKey, data, len);
  g_preferences.end();
  return writtenLen == len;
}

bool DrvNvs_EraseParamBlob() {
  if (!g_preferences.begin(kNvsNamespace, false)) {
    return false;
  }

  const bool removed = g_preferences.remove(kParamBlobKey);
  g_preferences.end();
  return removed;
}

}  // namespace mp
