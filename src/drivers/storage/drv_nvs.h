#pragma once

#include <cstddef>
#include <cstdint>

namespace mp {

// NVS 存储驱动初始化。
//
// 当前实现封装 ESP32 Arduino Preferences。
// 返回 false 表示 NVS 当前不可用，上层必须回退默认参数。
bool DrvNvs_Init();

// 读取参数二进制块。
//
// 参数说明：
// - data：接收缓冲区。
// - len：期望读取的字节数。
//
// 返回 true 表示 NVS 中存在同等长度的数据，并且已经完整读出。
bool DrvNvs_ReadParamBlob(void* data, std::size_t len);

// 写入参数二进制块。
//
// 注意：Flash 有擦写寿命限制，上层必须做延迟合并，不能频繁调用。
bool DrvNvs_WriteParamBlob(const void* data, std::size_t len);

// 删除参数二进制块。
//
// 工厂恢复时可以使用；失败时上层仍应回退默认参数。
bool DrvNvs_EraseParamBlob();

}  // namespace mp
