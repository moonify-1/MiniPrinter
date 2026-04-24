#pragma once

#include <cstdint>

namespace mp {

// 基础命令编号。
//
// 当前不实现任何打印命令，只保留基础查询和参数管理命令。
enum class ProtoCmd : std::uint16_t {
  PING = 0x0001U,
  GET_INFO = 0x0002U,
  GET_STATUS = 0x0003U,
  GET_PARAM = 0x0101U,
  SET_PARAM = 0x0102U,
  SAVE_PARAM = 0x0103U,
  FACTORY_RESET = 0x0104U,
};

// 把原始 uint16_t 转成命令枚举。
inline ProtoCmd ToProtoCmd(std::uint16_t raw) {
  return static_cast<ProtoCmd>(raw);
}

}  // namespace mp
