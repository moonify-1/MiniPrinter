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
  // 第一次烧录时 NVS 里还没有 mp_param 命名空间。
  // 如果这里用只读模式 begin(..., true)，Arduino Preferences 会打印
  // nvs_open failed: NOT_FOUND，并且参数服务会误以为 NVS 不可用。
  // 因此初始化阶段用读写模式打开一次，让命名空间被创建出来。
  if (!g_preferences.begin(kNvsNamespace, false)) {
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
