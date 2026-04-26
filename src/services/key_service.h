#pragma once

#include <cstdint>

namespace mp {

// 按键事件。
//
// 当前只区分短按和长按，而且只做状态记录，不直接触发危险动作。
enum class KeyEvent : std::uint8_t {
  NONE = 0,
  SHORT_PRESS,
  LONG_PRESS,
};

// 按键状态快照。
//
// 这个结构体可以被 WiFi API 直接展示，便于真实硬件联调时观察：
// - rawLevelHigh：GPIO 原始电平是否为高。
// - pressed：按当前假设解释后，按键是否处于按下状态。
// - lastEvent：最近一次稳定释放后识别到的短按/长按事件。
struct KeySnapshot {
  bool rawLevelHigh;
  bool pressed;
  bool activeLowAssumed;
  KeyEvent lastEvent;
  std::uint32_t lastEventTickMs;
  std::uint32_t pressDurationMs;
};

// 初始化按键输入。
//
// 当前假设 G_KEY 是低有效输入，使用 ESP32 内部上拉。
// 如果用户实测发现相反，只需要修改 KeyService 的解释规则。
void KeyService_Init();

// 周期轮询按键并更新去抖状态。
//
// SensorTask 每 50ms 调用一次即可满足短按/长按识别。
void KeyService_Poll();

// 获取最近一次按键快照。
KeySnapshot KeyService_GetSnapshot();

}  // namespace mp

