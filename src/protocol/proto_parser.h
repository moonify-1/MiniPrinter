#pragma once

#include <cstddef>
#include <cstdint>

#include "protocol/proto_frame.h"

namespace mp {

// Parser 喂入一个字节后的结果。
enum class ProtoParseResult : std::uint8_t {
  NEED_MORE = 0,  // 当前帧还没收完整。
  FRAME_READY,   // outFrame 已写入一帧完整且 CRC 正确的帧。
  FRAME_ERROR,   // 当前字节导致帧错误，Parser 已回到等待 magic 状态。
};

// 逐字节协议解析器。
//
// 串口每次只能可靠拿到“若干字节”，所以 Parser 必须能逐字节喂入。
// 它内部保存当前解析状态，直到完整帧收完或发现错误。
class ProtoParser {
 public:
  ProtoParser();

  // 清空解析状态，回到等待 magic 的初始状态。
  void reset();

  // 喂入一个字节。
  //
  // 参数说明：
  // - byte：串口收到的单个字节。
  // - outFrame：当返回 FRAME_READY 时，这里会被写入完整帧。
  ProtoParseResult feed(std::uint8_t byte, ProtoFrame* outFrame);

 private:
  enum class State : std::uint8_t {
    WAIT_MAGIC_0 = 0,
    WAIT_MAGIC_1,
    READ_HEADER,
    READ_PAYLOAD,
    READ_CRC_LO,
    READ_CRC_HI,
  };

  void startFrame();
  bool parseHeader();
  void resetForError();

  State state_;
  ProtoFrame frame_;
  std::uint8_t header_[PROTO_HEADER_LEN];
  std::uint8_t headerIndex_;
  std::uint16_t payloadIndex_;
  std::uint16_t calculatedCrc_;
  std::uint16_t receivedCrc_;
};

}  // namespace mp
