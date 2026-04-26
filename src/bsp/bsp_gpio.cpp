#include "bsp/bsp_gpio.h"

#include <Arduino.h>

namespace {

// 把任意整数电平归一化成 Arduino 常用的 LOW / HIGH。
//
// 这样可以避免上层不小心传入 3、5、255 之类的非标准值，
// 最终仍然只会落到 LOW 或 HIGH 两种安全可控状态。
int NormalizeLevel(int level) {
  return (level != 0) ? HIGH : LOW;
}

}  // namespace

namespace mp {

bool Bsp_IsValidPin(int pin) {
  return pin >= 0;
}

void Bsp_PinModeOutputSafe(int pin, int defaultLevel) {
  if (!Bsp_IsValidPin(pin)) {
    return;
  }

  pinMode(pin, OUTPUT);
  digitalWrite(pin, NormalizeLevel(defaultLevel));
}

void Bsp_PinModeInputPullupIfValid(int pin) {
  if (!Bsp_IsValidPin(pin)) {
    return;
  }

  pinMode(pin, INPUT_PULLUP);
}

void Bsp_WritePinIfValid(int pin, int level) {
  if (!Bsp_IsValidPin(pin)) {
    return;
  }

  digitalWrite(pin, NormalizeLevel(level));
}

int Bsp_ReadPinIfValid(int pin, int defaultValue) {
  if (!Bsp_IsValidPin(pin)) {
    return defaultValue;
  }

  return digitalRead(pin);
}

}  // namespace mp
