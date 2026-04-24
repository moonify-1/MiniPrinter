#pragma once

#include <cstddef>
#include <cstdint>

namespace mp {

// 协议固定字段。
//
// 多字节字段统一使用 little-endian：
// 低字节在前，高字节在后。
constexpr std::uint8_t PROTO_MAGIC_0 = 0xA5U;
constexpr std::uint8_t PROTO_MAGIC_1 = 0x5AU;
constexpr std::uint8_t PROTO_VERSION = 1U;
constexpr std::uint8_t PROTO_HEADER_LEN = 14U;
constexpr std::uint16_t PROTO_MAX_FRAME_SIZE = 512U;
constexpr std::uint16_t PROTO_CRC_SIZE = 2U;
constexpr std::uint16_t PROTO_MAX_PAYLOAD_SIZE =
    PROTO_MAX_FRAME_SIZE - PROTO_HEADER_LEN - PROTO_CRC_SIZE;

// 协议标志位。
constexpr std::uint8_t PROTO_FLAG_RESPONSE = 0x80U;
constexpr std::uint8_t PROTO_FLAG_ERROR = 0x40U;

// 协议来源。
constexpr std::uint8_t PROTO_SOURCE_HOST = 1U;
constexpr std::uint8_t PROTO_SOURCE_DEVICE = 2U;

// 完整协议帧。
//
// payload 使用固定数组，避免在串口接收路径上动态分配内存。
struct ProtoFrame {
  std::uint8_t protoVer;
  std::uint8_t headerLen;
  std::uint16_t deviceId;
  std::uint16_t seq;
  std::uint16_t cmd;
  std::uint8_t flags;
  std::uint8_t source;
  std::uint16_t payloadLen;
  std::uint8_t payload[PROTO_MAX_PAYLOAD_SIZE];
  std::uint16_t crc16;
};

}  // namespace mp
