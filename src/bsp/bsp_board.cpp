#include "bsp/bsp_board.h"

#include <Arduino.h>

#include "bsp/bsp_gpio.h"
#include "bsp/bsp_pins.h"

namespace {

// 仅在当前文件内部使用的输入配置辅助函数。
//
// 这里故意只用 INPUT，而不擅自使用 INPUT_PULLUP / INPUT_PULLDOWN，
// 因为外部电路的上下拉关系目前还没有完全确认。
// 在原理图和实测结论明确之前，保持中性配置更稳妥。
void PinModeInputIfValid(int pin) {
  if (!mp::Bsp_IsValidPin(pin)) {
    return;
  }

  pinMode(pin, INPUT);
}

}  // namespace

namespace mp {

void Bsp_SetAllOutputsSafe() {
  // 高压相关输出必须保持关闭，严禁在 BSP 骨架阶段意外打开。
  Bsp_PinModeOutputSafe(PIN_G_VH, LEVEL_VH_OFF);

  // 所有热敏头分组选通信号都必须保持无效态，
  // 这样即使后续别的脚被误操作，也不会形成有效加热窗口。
  Bsp_PinModeOutputSafe(PIN_STB1, LEVEL_STB_INACTIVE);
  Bsp_PinModeOutputSafe(PIN_STB2, LEVEL_STB_INACTIVE);
  Bsp_PinModeOutputSafe(PIN_STB3, LEVEL_STB_INACTIVE);
  Bsp_PinModeOutputSafe(PIN_STB4, LEVEL_STB_INACTIVE);
  Bsp_PinModeOutputSafe(PIN_STB5, LEVEL_STB_INACTIVE);
  Bsp_PinModeOutputSafe(PIN_STB6, LEVEL_STB_INACTIVE);

  // 打印移位链路在安全态下全部保持低电平，
  // 避免出现误时钟、误锁存或伪造串行数据。
  Bsp_PinModeOutputSafe(PIN_DI, LOW);
  Bsp_PinModeOutputSafe(PIN_CLK, LOW);
  Bsp_PinModeOutputSafe(PIN_LAT, LOW);

  // DRV8833 保持休眠，避免电机驱动在启动早期被意外唤醒。
  Bsp_PinModeOutputSafe(PIN_G_NSLEEP, LEVEL_DRV_SLEEP);

  // H 桥控制输入全部拉低，避免形成任何有效驱动相位。
  Bsp_PinModeOutputSafe(PIN_AIN1, LOW);
  Bsp_PinModeOutputSafe(PIN_AIN2, LOW);
  Bsp_PinModeOutputSafe(PIN_BIN1, LOW);
  Bsp_PinModeOutputSafe(PIN_BIN2, LOW);
}

void Bsp_PreInitSafeOutputs() {
  Bsp_SetAllOutputsSafe();
}

void Bsp_Init() {
  // 初始化入口先再次执行一次安全输出，
  // 即使前面已经调用过，也应允许重复调用且结果保持一致。
  Bsp_SetAllOutputsSafe();

  // 下面这些脚当前被视为输入来源。
  // 由于具体外部电路细节尚未最终确认，因此先统一配置为普通输入。
  PinModeInputIfValid(PIN_TM1);
  PinModeInputIfValid(PIN_PAPER_N);
  PinModeInputIfValid(PIN_G_NFAULT);
  PinModeInputIfValid(PIN_BAT_STAT);
  PinModeInputIfValid(PIN_G_KEY);
}

}  // namespace mp
