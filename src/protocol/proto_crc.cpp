#include "protocol/proto_crc.h"

namespace mp {

std::uint16_t ProtoCrc16Init() {
  return PROTO_CRC16_INIT;
}

std::uint16_t ProtoCrc16Update(std::uint16_t crc, std::uint8_t byte) {
  crc ^= static_cast<std::uint16_t>(byte) << 8U;

  for (std::uint8_t bit = 0U; bit < 8U; ++bit) {
    if ((crc & 0x8000U) != 0U) {
      crc = static_cast<std::uint16_t>((crc << 1U) ^ 0x1021U);
    } else {
      crc = static_cast<std::uint16_t>(crc << 1U);
    }
  }

  return crc;
}

std::uint16_t ProtoCrc16Calc(const std::uint8_t* data, std::size_t len) {
  std::uint16_t crc = ProtoCrc16Init();

  if (data == nullptr) {
    return crc;
  }

  for (std::size_t index = 0U; index < len; ++index) {
    crc = ProtoCrc16Update(crc, data[index]);
  }

  return crc;
}

}  // namespace mp
