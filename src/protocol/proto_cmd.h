#pragma once

#include <cstdint>

namespace mp {

// 基础命令编号。
//
// 当前只实现最小打印控制命令。
// PRINT_LINE 仍只进入队列，不启动真实硬件打印。
enum class ProtoCmd : std::uint16_t {
  PING = 0x0001U,
  GET_INFO = 0x0002U,
  GET_STATUS = 0x0003U,
  GET_ERROR = 0x0004U,
  CLEAR_ERROR = 0x0005U,
  SELF_TEST = 0x0006U,
  REBOOT = 0x0007U,
  GET_PARAM = 0x0101U,
  SET_PARAM = 0x0102U,
  SAVE_PARAM = 0x0103U,
  FACTORY_RESET = 0x0104U,
  PRINT_START = 0x0200U,
  PRINT_LINE = 0x0201U,
  PRINT_END = 0x0202U,
  PRINT_CANCEL = 0x0203U,
  FEED = 0x0204U,
  SAFE_OFF = 0x0300U,
  SENSOR_TEST = 0x0301U,
  MOTOR_TEST = 0x0302U,
  HEAD_SHIFT_TEST = 0x0303U,
  HEAD_STB_TEST = 0x0304U,
  ENTER_SAFE_MODE = 0x0305U,
};

// 把原始 uint16_t 转成命令枚举。
inline ProtoCmd ToProtoCmd(std::uint16_t raw) {
  return static_cast<ProtoCmd>(raw);
}

}  // namespace mp
