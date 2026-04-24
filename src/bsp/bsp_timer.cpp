#include "bsp/bsp_timer.h"

#include <Arduino.h>

namespace mp {

void Bsp_DelayUs(std::uint32_t delayUs) {
  // delayMicroseconds() 是 Arduino 提供的忙等短延时。
  // 它适合当前这种几微秒到几百微秒的波形骨架验证；
  // 后续如果需要更稳定的脉宽，应替换为硬件定时器或 RMT。
  delayMicroseconds(delayUs);
}

}  // namespace mp
