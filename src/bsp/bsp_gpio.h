#pragma once

namespace mp {

// 判断一个引脚编号是否已被分配。
//
// 当前项目用 PIN_UNASSIGNED = -1 表示“还不知道这个网络接到哪个 GPIO”，
// 因此这里只要 pin >= 0，就认为它是一个可以尝试访问的有效编号。
// 这不是“硬件上绝对合法”的完整校验，
// 只是项目骨架阶段最基础、最安全的第一层过滤。
bool Bsp_IsValidPin(int pin);

// 把一个引脚配置成输出，并立即写入安全默认电平。
//
// 这个接口的目标是：
// 1. 未分配引脚直接跳过，不报错。
// 2. 已分配引脚在被当作输出使用前，尽快进入一个明确的安全状态。
void Bsp_PinModeOutputSafe(int pin, int defaultLevel);

// 只有在引脚有效时才写出电平。
//
// 这样上层代码就不需要在每次写 GPIO 之前都重复判断 pin 是否已分配。
void Bsp_WritePinIfValid(int pin, int level);

// 只有在引脚有效时才读取输入。
//
// 如果当前引脚还未分配，就直接返回调用者提供的默认值。
// 这种写法很适合项目早期做“骨架先行”，
// 因为上层逻辑可以先跑起来，等引脚确认后再接真实硬件。
int Bsp_ReadPinIfValid(int pin, int defaultValue);

}  // namespace mp
