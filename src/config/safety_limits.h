#pragma once

#include <cstdint>

namespace mp {

// 安全阈值初始值。
//
// 这里的值不代表“最终调参结果”，
// 而是当前固件骨架阶段可以明确落盘的保守安全基线。
constexpr std::uint16_t SAFETY_MAX_SIMULTANEOUS_HEAT_DOTS = 64U;
constexpr std::int16_t SAFETY_DEFAULT_TEMP_STOP_THRESHOLD_C = 60;
constexpr std::int16_t SAFETY_DEFAULT_TEMP_RESUME_THRESHOLD_C = 50;
constexpr std::uint16_t SAFETY_DEFAULT_HEAT_PULSE_MAX_US = 800U;
constexpr std::uint16_t SAFETY_DEFAULT_HEAT_PULSE_START_US = 400U;

// 这里故意使用 0/1 的固定宽度整数，
// 是为了让它们后续可以稳定地进入参数结构体和 CRC 计算。
constexpr std::uint8_t SAFETY_FORBID_HEAT_WHEN_PAPER_MISSING = 1U;
constexpr std::uint8_t SAFETY_FORBID_HEAT_WHEN_LOW_VOLTAGE = 1U;
constexpr std::uint8_t SAFETY_FORBID_FAST_MOTOR_WHEN_LOW_VOLTAGE = 1U;

// 开发阶段真实 Watchdog 超时时间。
//
// 选择 5000ms 的原因：
// - 足够长：避免普通日志输出、串口调试或短时间临界区误触发复位。
// - 足够短：关键任务真的卡死时，系统不会长时间保持未知状态。
constexpr std::uint32_t SAFETY_WDT_TIMEOUT_MS = 5000U;

static_assert(SAFETY_DEFAULT_TEMP_RESUME_THRESHOLD_C <
                  SAFETY_DEFAULT_TEMP_STOP_THRESHOLD_C,
              "resume temperature must be lower than stop temperature");

static_assert(SAFETY_DEFAULT_HEAT_PULSE_START_US <=
                  SAFETY_DEFAULT_HEAT_PULSE_MAX_US,
              "start heat pulse must not exceed max heat pulse");

}  // namespace mp
