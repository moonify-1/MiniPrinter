#pragma once

#include <cstdint>

namespace mp {

// 微秒级短延时。
//
// 当前用于 LAT/STB 这类短脉冲。后续真实硬件联调时，
// 需要用示波器确认实际脉宽和 GPIO 翻转耗时。
void Bsp_DelayUs(std::uint32_t delayUs);

}  // namespace mp
