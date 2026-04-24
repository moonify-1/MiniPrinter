#pragma once

#include <cstddef>
#include <cstdint>

namespace mp {

// CRC16 初值。
//
// 本协议使用 CRC-16/CCITT-FALSE：
// - 初值：0xFFFF
// - 多项式：0x1021
// - 输入不反射
// - 输出不反射
// - 最终异或值：0x0000
constexpr std::uint16_t PROTO_CRC16_INIT = 0xFFFFU;

// 初始化 CRC16。
std::uint16_t ProtoCrc16Init();

// 向 CRC16 累积一个字节。
//
// Parser 会逐字节接收串口数据，因此提供单字节 update 接口更自然。
std::uint16_t ProtoCrc16Update(std::uint16_t crc, std::uint8_t byte);

// 一次性计算一段缓冲区的 CRC16。
std::uint16_t ProtoCrc16Calc(const std::uint8_t* data, std::size_t len);

}  // namespace mp
