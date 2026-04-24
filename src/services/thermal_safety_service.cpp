#include "services/thermal_safety_service.h"

#include "config/print_config.h"
#include "config/safety_limits.h"

namespace {

// 当前 ParamBlock 尚未包含低电压阈值字段。
//
// 这里先用 2S 锂电的保守占位值 6500mV。
// 后续做电源实测后，建议把它迁移到 SafetyParams，
// 同时提升 PARAM_BLOCK_VERSION，避免 NVS 中旧参数结构被误读。
constexpr std::uint32_t kDefaultLowBatteryStopMv = 6500U;

// 温度补偿门槛。
//
// 当前只是初始算法：
// - 温度越接近停机阈值，脉冲越短。
// - 这不是最终热模型，必须通过真实打印效果和温升曲线校准。
constexpr float kWarmTempReduceMarginC = 10.0F;
constexpr std::uint16_t kWarmTempReduceUs = 50U;
constexpr std::uint16_t kLowVoltageBoostUs = 50U;

std::uint16_t ClampPulseToLimits(std::uint32_t pulseUs,
                                 const mp::ParamBlock& params) {
  std::uint32_t maxUs = params.safety.heatPulseMaxUs;
  if (maxUs > mp::SAFETY_DEFAULT_HEAT_PULSE_MAX_US) {
    maxUs = mp::SAFETY_DEFAULT_HEAT_PULSE_MAX_US;
  }

  if (pulseUs > maxUs) {
    return static_cast<std::uint16_t>(maxUs);
  }

  return static_cast<std::uint16_t>(pulseUs);
}

std::uint8_t CountBits(std::uint8_t value) {
  std::uint8_t count = 0U;
  while (value != 0U) {
    count = static_cast<std::uint8_t>(count + (value & 1U));
    value = static_cast<std::uint8_t>(value >> 1U);
  }
  return count;
}

// 统计指定 STB 分组覆盖的 64 个点中有多少黑点。
//
// 当前位图按 384 点/行、48 字节/行处理。
// 每组 64 点正好对应 8 字节。
std::uint8_t CountGroupBlackDots(const mp::LineBuffer* line,
                                 std::uint8_t groupIndex) {
  if (line == nullptr || groupIndex >= mp::PRINT_STB_GROUP_COUNT) {
    return 0U;
  }

  constexpr std::uint8_t kBytesPerGroup =
      mp::PRINT_DOTS_PER_STB_GROUP / 8U;
  const std::uint8_t startByte =
      static_cast<std::uint8_t>(groupIndex * kBytesPerGroup);

  std::uint8_t total = 0U;
  for (std::uint8_t offset = 0U; offset < kBytesPerGroup; ++offset) {
    total = static_cast<std::uint8_t>(
        total + CountBits(line->data[startByte + offset]));
  }

  return total;
}

}  // namespace

namespace mp {

AppErrorCode ThermalSafety_CheckCanPrint(const SensorSnapshot& sensorSnapshot,
                                         const ParamBlock& params) {
  if (params.safety.forbidHeatWhenPaperMissing != 0U &&
      !sensorSnapshot.paperPresent) {
    return ERR_SENSOR_PAPER_MISSING;
  }

  if (sensorSnapshot.validity != SensorValidity::VALID) {
    return ERR_SENSOR_TEMP_INVALID;
  }

  if (sensorSnapshot.headTempC >= params.safety.tempStopThresholdC) {
    return ERR_HEAD_OVER_TEMP;
  }

  if (params.safety.forbidHeatWhenLowVoltage != 0U &&
      sensorSnapshot.batteryMv < kDefaultLowBatteryStopMv) {
    return ERR_POWER_LOW_BATTERY;
  }

  return APP_OK;
}

ThermalPulseResult ThermalSafety_CalcPulseUs(
    std::uint8_t groupBlackDots,
    const SensorSnapshot& sensorSnapshot,
    const ParamBlock& params) {
  ThermalPulseResult result = {};
  result.error = ThermalSafety_CheckCanPrint(sensorSnapshot, params);
  result.pulseUs = 0U;

  if (result.error != APP_OK) {
    return result;
  }

  if (groupBlackDots == 0U) {
    return result;
  }

  if (groupBlackDots > params.safety.maxSimultaneousHeatDots ||
      groupBlackDots > SAFETY_MAX_SIMULTANEOUS_HEAT_DOTS) {
    result.error = ERR_HEAD_HEAT_TIMEOUT;
    return result;
  }

  std::uint32_t pulseUs = params.safety.heatPulseStartUs;

  const float warmTempC =
      static_cast<float>(params.safety.tempStopThresholdC) -
      kWarmTempReduceMarginC;
  if (sensorSnapshot.headTempC >= warmTempC &&
      pulseUs > kWarmTempReduceUs) {
    pulseUs -= kWarmTempReduceUs;
  }

  // 电压偏低但还没到停机阈值时，先轻微增加脉冲。
  // 这个补偿只用于骨架验证，后续必须实测校准，否则可能影响打印效果和温升。
  if (sensorSnapshot.batteryMv < (kDefaultLowBatteryStopMv + 500U)) {
    pulseUs += kLowVoltageBoostUs;
  }

  result.pulseUs = ClampPulseToLimits(pulseUs, params);
  return result;
}

AppErrorCode ThermalSafety_CheckLine(const LineBuffer* line) {
  if (line == nullptr) {
    return ERR_QUEUE_FULL;
  }

  for (std::uint8_t group = 0U; group < PRINT_STB_GROUP_COUNT; ++group) {
    const std::uint8_t groupBlackDots = CountGroupBlackDots(line, group);
    if (groupBlackDots > SAFETY_MAX_SIMULTANEOUS_HEAT_DOTS) {
      return ERR_HEAD_HEAT_TIMEOUT;
    }
  }

  return APP_OK;
}

// 手工自检建议：
// 1. paperPresent=false，应返回 ERR_SENSOR_PAPER_MISSING。
// 2. validity=INVALID，应返回 ERR_SENSOR_TEMP_INVALID。
// 3. headTempC >= tempStopThresholdC，应返回 ERR_HEAD_OVER_TEMP。
// 4. batteryMv < 6500，应返回 ERR_POWER_LOW_BATTERY。
// 5. groupBlackDots=0 时 pulseUs 应为 0。
// 6. groupBlackDots=65 时应拒绝加热。

}  // namespace mp
