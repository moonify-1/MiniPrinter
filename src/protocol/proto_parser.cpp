#include "protocol/proto_parser.h"

#include "protocol/proto_crc.h"

namespace {

std::uint16_t ReadLe16(const std::uint8_t* data) {
  return static_cast<std::uint16_t>(data[0]) |
         (static_cast<std::uint16_t>(data[1]) << 8U);
}

}  // namespace

namespace mp {

ProtoParser::ProtoParser() {
  reset();
}

void ProtoParser::reset() {
  state_ = State::WAIT_MAGIC_0;
  frame_ = {};
  for (std::uint8_t& byte : header_) {
    byte = 0U;
  }
  headerIndex_ = 0U;
  payloadIndex_ = 0U;
  calculatedCrc_ = ProtoCrc16Init();
  receivedCrc_ = 0U;
}

void ProtoParser::startFrame() {
  frame_ = {};
  for (std::uint8_t& byte : header_) {
    byte = 0U;
  }
  header_[0] = PROTO_MAGIC_0;
  headerIndex_ = 1U;
  payloadIndex_ = 0U;
  calculatedCrc_ = ProtoCrc16Update(ProtoCrc16Init(), PROTO_MAGIC_0);
  receivedCrc_ = 0U;
  state_ = State::WAIT_MAGIC_1;
}

bool ProtoParser::parseHeader() {
  frame_.protoVer = header_[2];
  frame_.headerLen = header_[3];
  frame_.deviceId = ReadLe16(&header_[4]);
  frame_.seq = ReadLe16(&header_[6]);
  frame_.cmd = ReadLe16(&header_[8]);
  frame_.flags = header_[10];
  frame_.source = header_[11];
  frame_.payloadLen = ReadLe16(&header_[12]);

  if (frame_.protoVer != PROTO_VERSION) {
    return false;
  }

  if (frame_.headerLen != PROTO_HEADER_LEN) {
    return false;
  }

  if (frame_.payloadLen > PROTO_MAX_PAYLOAD_SIZE) {
    return false;
  }

  const std::uint16_t totalLen =
      static_cast<std::uint16_t>(PROTO_HEADER_LEN + frame_.payloadLen +
                                 PROTO_CRC_SIZE);
  return totalLen <= PROTO_MAX_FRAME_SIZE;
}

void ProtoParser::resetForError() {
  reset();
}

ProtoParseResult ProtoParser::feed(std::uint8_t byte, ProtoFrame* outFrame) {
  switch (state_) {
    case State::WAIT_MAGIC_0:
      if (byte == PROTO_MAGIC_0) {
        startFrame();
      }
      return ProtoParseResult::NEED_MORE;

    case State::WAIT_MAGIC_1:
      if (byte == PROTO_MAGIC_1) {
        header_[headerIndex_++] = byte;
        calculatedCrc_ = ProtoCrc16Update(calculatedCrc_, byte);
        state_ = State::READ_HEADER;
        return ProtoParseResult::NEED_MORE;
      }

      reset();
      if (byte == PROTO_MAGIC_0) {
        startFrame();
        return ProtoParseResult::NEED_MORE;
      }
      return ProtoParseResult::FRAME_ERROR;

    case State::READ_HEADER:
      header_[headerIndex_++] = byte;
      calculatedCrc_ = ProtoCrc16Update(calculatedCrc_, byte);

      if (headerIndex_ < PROTO_HEADER_LEN) {
        return ProtoParseResult::NEED_MORE;
      }

      if (!parseHeader()) {
        resetForError();
        return ProtoParseResult::FRAME_ERROR;
      }

      state_ = (frame_.payloadLen == 0U) ? State::READ_CRC_LO
                                         : State::READ_PAYLOAD;
      return ProtoParseResult::NEED_MORE;

    case State::READ_PAYLOAD:
      frame_.payload[payloadIndex_++] = byte;
      calculatedCrc_ = ProtoCrc16Update(calculatedCrc_, byte);

      if (payloadIndex_ >= frame_.payloadLen) {
        state_ = State::READ_CRC_LO;
      }
      return ProtoParseResult::NEED_MORE;

    case State::READ_CRC_LO:
      receivedCrc_ = byte;
      state_ = State::READ_CRC_HI;
      return ProtoParseResult::NEED_MORE;

    case State::READ_CRC_HI:
      receivedCrc_ |= static_cast<std::uint16_t>(byte) << 8U;
      frame_.crc16 = receivedCrc_;

      if (receivedCrc_ != calculatedCrc_) {
        resetForError();
        return ProtoParseResult::FRAME_ERROR;
      }

      if (outFrame != nullptr) {
        *outFrame = frame_;
      }
      reset();
      return ProtoParseResult::FRAME_READY;

    default:
      resetForError();
      return ProtoParseResult::FRAME_ERROR;
  }
}

}  // namespace mp
