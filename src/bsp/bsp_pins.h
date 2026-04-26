#pragma once

namespace mp {

// 统一的“未分配引脚”标记值。
//
// 后续只要某个网络还没有从原理图确认到具体 GPIO，
// 就一律保持为 PIN_UNASSIGNED。
// 这样 BSP 安全包装层就可以自动跳过这些引脚，
// 避免因为占位阶段的错误假设去误操作硬件。
constexpr int PIN_UNASSIGNED = -1;

// 用户已用万用表复核网络到 GPIO 的连通性。
// 下面常量只描述“线接到哪里”，不代表真实硬件动作已经完成验证。

// G_VH 控制打印头 VH 高压路径。
// 用户已确认方向为高通低断：LOW=关闭 VH，HIGH=打开 VH。
// 任何安全态和 safe-off 都必须让这个引脚保持 LOW。
constexpr int PIN_G_VH = 8;

// STB1~STB6 是热敏头 6 个加热分组选通信号。
// STB 高电平有效；未完成空载波形验证前，不允许接入真实加热打印。
constexpr int PIN_STB1 = 40;
constexpr int PIN_STB2 = 39;
constexpr int PIN_STB3 = 38;
constexpr int PIN_STB4 = 37;
constexpr int PIN_STB5 = 36;
constexpr int PIN_STB6 = 35;

// 打印头串行数据链路。
// DI 是数据输入，CLK 是移位时钟，LAT 是锁存信号。
constexpr int PIN_DI = 9;
constexpr int PIN_CLK = 10;
constexpr int PIN_LAT = 3;

// TM1 是热敏头 NTC 温度 ADC 网络。
// 第一阶段等待用户提供 ADC 原始值，不宣称真实温度保护已校准。
constexpr int PIN_TM1 = 2;

// PAPER_N 是缺纸检测比较器输出。
// 规格书倾向低电平表示有纸，但用户要求第一阶段先忽略缺纸检测。
constexpr int PIN_PAPER_N = 5;

// DRV8833 控制与故障输入。
// G_NSLEEP 低电平让驱动休眠；G_NFAULT 低电平表示故障。
constexpr int PIN_G_NSLEEP = 7;
constexpr int PIN_G_NFAULT = 6;

// DRV8833 两相输入。
// safe-off 时这些引脚必须全部回到 LOW，避免电机保持受力或误动作。
constexpr int PIN_AIN1 = 11;
constexpr int PIN_AIN2 = 12;
constexpr int PIN_BIN1 = 13;
constexpr int PIN_BIN2 = 14;

// IP2326 充电状态输入。
// BAT_STAT 的高低电平语义仍需按数据手册确认，未确认前不要猜测。
constexpr int PIN_BAT_STAT = 16;

// EQD 是 2S 电池电压分压 ADC 网络。
// 分压电阻 220k/100k，固件第一阶段按约 3.2 倍换算 batteryMv。
constexpr int PIN_EQD = 17;

// 用户按键输入。
// 有效电平仍需用户确认；初期只允许作为状态/事件来源，不触发危险动作。
constexpr int PIN_G_KEY = 15;

}  // namespace mp
