#include "services/key_service.h"

#include <Arduino.h>

#include "bsp/bsp_gpio.h"
#include "bsp/bsp_pins.h"

namespace {

// 当前 G_KEY 有效电平尚未实测确认。
// 为了让上层先能观察状态，第一阶段按常见按键电路处理：内部上拉，按下为 LOW。
constexpr bool kKeyActiveLowAssumed = true;

// 去抖时间。
//
// 机械按键按下/松开时会在几毫秒内抖动多次。
// 只有原始状态连续保持 40ms，才承认为稳定状态。
constexpr std::uint32_t kDebounceMs = 40U;

// 长按阈值。
//
// 当前只用于状态展示，不会直接触发打印、走纸或工厂测试。
constexpr std::uint32_t kLongPressMs = 1200U;

bool g_initialized = false;
bool g_rawPressed = false;
bool g_stablePressed = false;
bool g_rawLevelHigh = true;
std::uint32_t g_rawChangedAtMs = 0U;
std::uint32_t g_pressedAtMs = 0U;
mp::KeyEvent g_lastEvent = mp::KeyEvent::NONE;
std::uint32_t g_lastEventTickMs = 0U;

bool RawLevelToPressed(bool rawLevelHigh) {
  return kKeyActiveLowAssumed ? !rawLevelHigh : rawLevelHigh;
}

void PublishReleaseEvent(std::uint32_t nowMs) {
  const std::uint32_t durationMs = nowMs - g_pressedAtMs;
  if (durationMs >= kLongPressMs) {
    g_lastEvent = mp::KeyEvent::LONG_PRESS;
    g_lastEventTickMs = nowMs;
  } else if (durationMs >= kDebounceMs) {
    g_lastEvent = mp::KeyEvent::SHORT_PRESS;
    g_lastEventTickMs = nowMs;
  }
}

}  // namespace

namespace mp {

void KeyService_Init() {
  Bsp_PinModeInputPullupIfValid(PIN_G_KEY);

  g_rawLevelHigh = Bsp_ReadPinIfValid(PIN_G_KEY, HIGH) != LOW;
  g_rawPressed = RawLevelToPressed(g_rawLevelHigh);
  g_stablePressed = g_rawPressed;
  g_rawChangedAtMs = millis();
  g_pressedAtMs = g_stablePressed ? g_rawChangedAtMs : 0U;
  g_lastEvent = KeyEvent::NONE;
  g_lastEventTickMs = 0U;
  g_initialized = true;
}

void KeyService_Poll() {
  if (!g_initialized) {
    KeyService_Init();
  }

  const std::uint32_t nowMs = millis();
  g_rawLevelHigh = Bsp_ReadPinIfValid(PIN_G_KEY, HIGH) != LOW;
  const bool currentRawPressed = RawLevelToPressed(g_rawLevelHigh);

  if (currentRawPressed != g_rawPressed) {
    g_rawPressed = currentRawPressed;
    g_rawChangedAtMs = nowMs;
  }

  if ((nowMs - g_rawChangedAtMs) < kDebounceMs ||
      currentRawPressed == g_stablePressed) {
    return;
  }

  g_stablePressed = currentRawPressed;
  if (g_stablePressed) {
    g_pressedAtMs = nowMs;
  } else {
    PublishReleaseEvent(nowMs);
    g_pressedAtMs = 0U;
  }
}

KeySnapshot KeyService_GetSnapshot() {
  const std::uint32_t nowMs = millis();
  KeySnapshot snapshot = {};
  snapshot.rawLevelHigh = g_rawLevelHigh;
  snapshot.pressed = g_stablePressed;
  snapshot.activeLowAssumed = kKeyActiveLowAssumed;
  snapshot.lastEvent = g_lastEvent;
  snapshot.lastEventTickMs = g_lastEventTickMs;
  snapshot.pressDurationMs =
      g_stablePressed ? (nowMs - g_pressedAtMs) : 0U;
  return snapshot;
}

}  // namespace mp

