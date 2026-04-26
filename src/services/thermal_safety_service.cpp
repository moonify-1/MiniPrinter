#include "services/thermal_safety_service.h"

#include "config/print_config.h"
#include "config/safety_limits.h"

namespace {

// JX-2R-01 规格书给出的参考工作点。
//
// VH 是热敏头加热电源，不是 ESP32 的 3.3V 逻辑电源。
// 规格书中 25C、VH=7.2V、64 dots 同时加热时，典型 Ton 约 0.49ms。
// 这里把它作为“未实测前的保守参考点”，后续真实灰度和温升要再校准。
constexpr std::uint32_t kNominalVhMv = 7200U;
constexpr std::uint16_t kTypicalTon25C64DotsUs = 490U;
constexpr std::int16_t kNominalTempC = 25;

// 低电压保护的上方只允许很小范围的电压补偿。
//
// 热敏头每个点近似是电阻负载，功率 P 约等于 V^2 / R。
// 为了保持相近能量，脉宽可按 (额定电压 / 当前电压)^2 缩放。
// 这只是第一阶段模型：没有补偿线阻、MOS 压降和真实 VH_OUT 纹波。
constexpr std::uint32_t kVoltageCompMaxMv = 8400U;

// 行周期约束。
//
// 一行最多 6 个 STB 组。若每组都给 490us，总加热时间约 2940us。
// 因此第一阶段把整行加热预算设为 3000us，折算每个 STB 组最多 500us。
// 这样即使 6 组都满黑，也不会把加热阶段拉得过长。
constexpr std::uint16_t kLineHeatBudgetUs = 3000U;
constexpr std::uint16_t kPerStbHeatBudgetUs =
    kLineHeatBudgetUs / mp::PRINT_STB_GROUP_COUNT;

// 温度补偿斜率。
//
// 头温高于 25C 时减少脉宽，避免温升叠加；低于 25C 时允许很小的补偿，
// 但仍受最大脉宽和行周期双重限制。
constexpr std::uint16_t kWarmReduceUsPer10C = 40U;
constexpr std::uint16_t kColdBoostUsPer10C = 20U;
constexpr std::uint16_t kColdBoostMaxUs = 80U;

std::uint16_t MaxAllowedPulseUs(const mp::ParamBlock& params) {
  std::uint32_t maxUs = params.safety.heatPulseMaxUs;
  if (maxUs == 0U) {
    maxUs = mp::SAFETY_DEFAULT_HEAT_PULSE_MAX_US;
  }

  if (maxUs > mp::SAFETY_DEFAULT_HEAT_PULSE_MAX_US) {
    maxUs = mp::SAFETY_DEFAULT_HEAT_PULSE_MAX_US;
  }

  if (maxUs > kPerStbHeatBudgetUs) {
    maxUs = kPerStbHeatBudgetUs;
  }

  return static_cast<std::uint16_t>(maxUs);
}

std::uint16_t ClampPulseToLimits(std::uint32_t pulseUs,
                                 const mp::ParamBlock& params) {
  const std::uint16_t maxUs = MaxAllowedPulseUs(params);
  if (pulseUs > maxUs) {
    return maxUs;
  }

  return static_cast<std::uint16_t>(pulseUs);
}

std::uint32_t ApplyDotLoadModel(std::uint32_t startPulseUs,
                                std::uint8_t groupBlackDots) {
  if (groupBlackDots == 0U) {
    return 0U;
  }

  // heatPulseStartUs 是“低密度起始脉宽”。
  // 64 dots 同时加热时电源负载最大，因此把脉宽平滑推向规格书典型 490us。
  // 这样 1 个黑点不会被不必要地拉到 490us，64 个黑点也有明确上限参考。
  if (startPulseUs >= kTypicalTon25C64DotsUs) {
    return startPulseUs;
  }

  const std::uint32_t extraAtFullLoad =
      static_cast<std::uint32_t>(kTypicalTon25C64DotsUs) - startPulseUs;
  return startPulseUs +
         ((extraAtFullLoad * groupBlackDots) /
          mp::SAFETY_MAX_SIMULTANEOUS_HEAT_DOTS);
}

std::uint32_t ApplyTempModel(std::uint32_t pulseUs, float headTempC) {
  if (headTempC > static_cast<float>(kNominalTempC)) {
    const float deltaC = headTempC - static_cast<float>(kNominalTempC);
    const std::uint32_t reduceUs = static_cast<std::uint32_t>(
        (deltaC * static_cast<float>(kWarmReduceUsPer10C)) / 10.0F);
    return (pulseUs > reduceUs) ? (pulseUs - reduceUs) : 1U;
  }

  const float deltaC = static_cast<float>(kNominalTempC) - headTempC;
  std::uint32_t boostUs = static_cast<std::uint32_t>(
      (deltaC * static_cast<float>(kColdBoostUsPer10C)) / 10.0F);
  if (boostUs > kColdBoostMaxUs) {
    boostUs = kColdBoostMaxUs;
  }

  return pulseUs + boostUs;
}

std::uint32_t ApplyVoltageModel(std::uint32_t pulseUs,
                                std::uint32_t batteryMv) {
  if (batteryMv == 0U) {
    return pulseUs;
  }

  std::uint32_t vhMv = batteryMv;
  if (vhMv < mp::SAFETY_DEFAULT_LOW_BATTERY_STOP_MV) {
    vhMv = mp::SAFETY_DEFAULT_LOW_BATTERY_STOP_MV;
  }
  if (vhMv > kVoltageCompMaxMv) {
    vhMv = kVoltageCompMaxMv;
  }

  const std::uint64_t numerator =
      static_cast<std::uint64_t>(pulseUs) * kNominalVhMv * kNominalVhMv;
  const std::uint64_t denominator =
      static_cast<std::uint64_t>(vhMv) * vhMv;

  return static_cast<std::uint32_t>((numerator + denominator - 1U) /
                                    denominator);
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

  // 电机驱动故障时禁止继续加热。
  // 原因是打印头加热后必须及时走纸，否则同一位置反复受热会带来风险。
  if (sensorSnapshot.motorFault) {
    return ERR_MOTOR_FAULT;
  }

  if (sensorSnapshot.headTempC >= params.safety.tempStopThresholdC) {
    return ERR_HEAD_OVER_TEMP;
  }

  if (params.safety.forbidHeatWhenLowVoltage != 0U &&
      sensorSnapshot.batteryMv < SAFETY_DEFAULT_LOW_BATTERY_STOP_MV) {
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

  const std::uint32_t startPulseUs =
      (params.safety.heatPulseStartUs == 0U)
          ? mp::SAFETY_DEFAULT_HEAT_PULSE_START_US
          : params.safety.heatPulseStartUs;

  std::uint32_t pulseUs = ApplyDotLoadModel(startPulseUs, groupBlackDots);
  pulseUs = ApplyTempModel(pulseUs, sensorSnapshot.headTempC);
  pulseUs = ApplyVoltageModel(pulseUs, sensorSnapshot.batteryMv);

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
// 7. 25C、7200mV、64 dots、默认参数时，pulseUs 应接近 490us。
// 8. 所有结果都必须同时受 heatPulseMaxUs、800us 硬上限和 500us/组行预算限制。

}  // namespace mp
