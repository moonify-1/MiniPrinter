#pragma once

#include <cstdint>

#include "common/app_types.h"
#include "config/print_config.h"
#include "config/safety_limits.h"

namespace mp {

// 参数块魔数。
//
// 魔数的作用是帮助系统快速判断一段持久化数据
// “到底是不是我们这个项目期望的参数块格式”。
constexpr std::uint32_t PARAM_BLOCK_MAGIC = 0x4D505042UL;  // "MPPB"

// 参数块版本号。
//
// 后续如果结构体字段发生兼容性变化，就应该提升版本号。
constexpr std::uint16_t PARAM_BLOCK_VERSION = 1U;

// 打印相关参数。
//
// 这组参数描述“每一行打印数据”的组织方式，
// 以及位图解释规则，不直接包含真实硬件驱动逻辑。
struct PrintParams {
  std::uint16_t dotsPerLine;    // 每行总点数，例如 384 点。
  std::uint16_t bytesPerLine;   // 每行位图字节数，例如 48 字节。
  std::uint8_t stbGroupCount;   // STB 分组数量。
  std::uint8_t dotsPerStbGroup; // 每个 STB 组覆盖的点数。
  std::uint8_t monoMsbLeft;     // 单色位图是否按“高位在左”解释，1=是，0=否。
};

// 电机相关参数。
//
// 这里先放“运动节奏参数”的占位结构，
// 不代表已经实现真实电机驱动。
// 当前没有足够硬件依据时，保持保守占位值 0，
// 表示后续必须结合机构、驱动芯片和实测结果再定。
struct MotorParams {
  std::uint16_t startPhaseIntervalUs; // 起步时的相位切换间隔。
  std::uint16_t runPhaseIntervalUs;   // 常速运行时的相位切换间隔。
  std::uint16_t fastPhaseIntervalUs;  // 高速运行时的相位切换间隔。
};

// 传感器相关参数。
//
// 这些参数用于控制“多久采一次、多久判旧、输入去抖多久”，
// 目的是让后续传感器服务层有统一的默认行为。
struct SensorParams {
  std::uint16_t paperDebounceMs;    // 纸检测输入去抖时间。
  std::uint16_t tempSamplePeriodMs; // 温度采样周期。
  std::uint16_t tempStaleTimeoutMs; // 温度数据多久算“过旧”。
};

// 通信相关参数。
//
// 这里定义的是协议层默认行为，
// 不是具体串口、蓝牙或 Wi-Fi 驱动实现。
struct CommParams {
  std::uint16_t maxFrameLength;   // 允许接收的最大帧长度。
  std::uint16_t interByteTimeout; // 帧内字节间超时。
  std::uint8_t enableCrc;         // 是否默认启用 CRC，1=启用，0=关闭。
};

// 日志相关参数。
//
// 这里保留一个带类型的默认日志级别，
// 这样你后面在日志模块里可以直接使用 LogLevel，而不是裸数字。
struct LogParams {
  LogLevel defaultLevel;          // 默认日志级别。
  std::uint8_t enableBootLog;     // 是否输出启动阶段日志。
  std::uint8_t enableProtocolLog; // 是否输出协议细节日志。
};

// 安全相关参数。
//
// 这组参数会直接影响“哪些动作允许执行”，
// 所以后续即使实现真实驱动，也应优先从这里读取策略，而不是写死在代码里。
struct SafetyParams {
  std::uint16_t maxSimultaneousHeatDots; // 允许同时加热的最大点数。
  std::int16_t tempStopThresholdC;       // 到达该温度后停止加热。
  std::int16_t tempResumeThresholdC;     // 温度回落到该值后允许恢复。
  std::uint16_t heatPulseStartUs;        // 初始热脉冲宽度。
  std::uint16_t heatPulseMaxUs;          // 热脉冲允许上限。
  std::uint8_t forbidHeatWhenPaperMissing;   // 缺纸时是否禁止加热。
  std::uint8_t forbidHeatWhenLowVoltage;     // 低电压时是否禁止加热。
  std::uint8_t forbidFastMotorWhenLowVoltage; // 低电压时是否禁止高速电机。
};

// 整体参数块。
//
// 未来如果要把参数写入 NVS、Flash 或文件，
// 一般就会以这个结构体作为顶层对象。
// 现在只定义结构和默认值，不涉及任何存储读写逻辑。
struct ParamBlock {
  std::uint32_t magic; // 用于识别参数块格式是否合法。
  std::uint16_t version; // 用于区分参数结构版本。
  std::uint32_t crc32; // 用于校验整个参数块内容是否损坏。
  PrintParams print; // 打印相关参数。
  MotorParams motor; // 电机相关参数。
  SensorParams sensor; // 传感器相关参数。
  CommParams comm; // 通信相关参数。
  LogParams log; // 日志相关参数。
  SafetyParams safety; // 安全相关参数。
};

// 各子模块默认参数。
//
// 这些默认值当前只承担“初始化骨架”和“文档化基线”的作用。
constexpr PrintParams DEFAULT_PRINT_PARAMS = {
    PRINT_DOTS_PER_LINE,
    PRINT_BYTES_PER_LINE,
    PRINT_STB_GROUP_COUNT,
    PRINT_DOTS_PER_STB_GROUP,
    PRINT_MONO_MSB_LEFT,
};

constexpr MotorParams DEFAULT_MOTOR_PARAMS = {
    0U, // 起步节奏待结合电机与机构实测确认。
    0U, // 常速节奏待结合电机与机构实测确认。
    0U, // 高速节奏待结合电机与机构实测确认。
};

constexpr SensorParams DEFAULT_SENSOR_PARAMS = {
    20U,   // 缺纸开关一类输入常见需要少量去抖。
    100U,  // 当前仅作默认采样节奏占位。
    1000U, // 1 秒未刷新就把温度数据视为陈旧。
};

constexpr CommParams DEFAULT_COMM_PARAMS = {
    256U, // 当前只是协议骨架阶段的保守默认值。
    20U,  // 帧内超时占位，后续可按协议再调。
    1U,   // 缺省启用 CRC，更符合保守策略。
};

constexpr LogParams DEFAULT_LOG_PARAMS = {
    LogLevel::INFO,
    1U, // 默认保留启动日志，便于早期联调。
    0U, // 协议细节日志默认关闭，避免过早刷屏。
};

constexpr SafetyParams DEFAULT_SAFETY_PARAMS = {
    SAFETY_MAX_SIMULTANEOUS_HEAT_DOTS,
    SAFETY_DEFAULT_TEMP_STOP_THRESHOLD_C,
    SAFETY_DEFAULT_TEMP_RESUME_THRESHOLD_C,
    SAFETY_DEFAULT_HEAT_PULSE_START_US,
    SAFETY_DEFAULT_HEAT_PULSE_MAX_US,
    SAFETY_FORBID_HEAT_WHEN_PAPER_MISSING,
    SAFETY_FORBID_HEAT_WHEN_LOW_VOLTAGE,
    SAFETY_FORBID_FAST_MOTOR_WHEN_LOW_VOLTAGE,
};

// 默认参数块。
//
// crc32 当前固定为 0，
// 因为这一步只做结构体骨架，不做真实 CRC 计算和持久化读写。
constexpr ParamBlock DEFAULT_PARAM_BLOCK = {
    PARAM_BLOCK_MAGIC,
    PARAM_BLOCK_VERSION,
    0U,
    DEFAULT_PRINT_PARAMS,
    DEFAULT_MOTOR_PARAMS,
    DEFAULT_SENSOR_PARAMS,
    DEFAULT_COMM_PARAMS,
    DEFAULT_LOG_PARAMS,
    DEFAULT_SAFETY_PARAMS,
};

}  // namespace mp
