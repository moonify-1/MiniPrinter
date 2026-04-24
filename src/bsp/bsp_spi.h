#pragma once

#include <cstddef>
#include <cstdint>

namespace mp {

// 初始化热敏头移位数据使用的 SPI。
//
// 当前仅在 MP_ENABLE_HW_THERMAL_HEAD=1 时真正初始化硬件。
// 默认关闭时直接返回 false，避免启动时误操作 SPI/GPIO。
bool Bsp_SpiThermalHeadInit();

// 通过热敏头 SPI 写入一段数据。
//
// data/len 由 driver 层保证为一行 48 字节。
bool Bsp_SpiThermalHeadWrite(const std::uint8_t* data, std::size_t len);

}  // namespace mp
