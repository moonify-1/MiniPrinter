#pragma once

namespace mp {

// 统一的“未分配引脚”标记值。
//
// 后续只要某个网络还没有从原理图确认到具体 GPIO，
// 就一律保持为 PIN_UNASSIGNED。
// 这样 BSP 安全包装层就可以自动跳过这些引脚，
// 避免因为占位阶段的错误假设去误操作硬件。
constexpr int PIN_UNASSIGNED = -1;

// 当前所有网络的 GPIO 仍未确认，
// 因此这里先全部保持为 PIN_UNASSIGNED。
// 等原理图确认后，只需要在本文件集中修改即可。
constexpr int PIN_G_VH = PIN_UNASSIGNED;

constexpr int PIN_STB1 = PIN_UNASSIGNED;
constexpr int PIN_STB2 = PIN_UNASSIGNED;
constexpr int PIN_STB3 = PIN_UNASSIGNED;
constexpr int PIN_STB4 = PIN_UNASSIGNED;
constexpr int PIN_STB5 = PIN_UNASSIGNED;
constexpr int PIN_STB6 = PIN_UNASSIGNED;

constexpr int PIN_DI = PIN_UNASSIGNED;
constexpr int PIN_CLK = PIN_UNASSIGNED;
constexpr int PIN_LAT = PIN_UNASSIGNED;

constexpr int PIN_TM1 = PIN_UNASSIGNED;
constexpr int PIN_PAPER_N = PIN_UNASSIGNED;

constexpr int PIN_G_NSLEEP = PIN_UNASSIGNED;
constexpr int PIN_G_NFAULT = PIN_UNASSIGNED;

constexpr int PIN_AIN1 = PIN_UNASSIGNED;
constexpr int PIN_AIN2 = PIN_UNASSIGNED;
constexpr int PIN_BIN1 = PIN_UNASSIGNED;
constexpr int PIN_BIN2 = PIN_UNASSIGNED;

constexpr int PIN_BAT_STAT = PIN_UNASSIGNED;
constexpr int PIN_G_KEY = PIN_UNASSIGNED;

}  // namespace mp
