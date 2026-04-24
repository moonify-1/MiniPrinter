#pragma once

#include <cstdint>

#include "common/app_error.h"
#include "common/fixed_pool.h"
#include "config/default_params.h"
#include "services/sensor_service.h"

namespace mp {

// 单组热敏点安全检查和脉冲计算结果。
//
// error 为 APP_OK 时，pulseUs 才有意义。
// 这里不直接进入 SAFE_MODE，也不操作任何 GPIO。
struct ThermalPulseResult {
  AppErrorCode error;       // 本次计算是否允许加热。
  std::uint32_t pulseUs;    // 建议脉冲宽度，单位 us。
};

// 检查当前传感器和参数状态是否允许打印/加热。
//
// 只返回错误码，由上层决定如何处理。
AppErrorCode ThermalSafety_CheckCanPrint(const SensorSnapshot& sensorSnapshot,
                                         const ParamBlock& params);

// 计算某个 STB 分组的建议加热脉冲。
//
// 注意：
// - 当前算法只是骨架阶段的保守估算。
// - 真机必须结合纸张、打印头、电源能力和灰度效果实测校准。
ThermalPulseResult ThermalSafety_CalcPulseUs(std::uint8_t groupBlackDots,
                                             const SensorSnapshot& sensorSnapshot,
                                             const ParamBlock& params);

// 检查一整行是否满足基础安全约束。
//
// 当前按 6 个 STB 分组、每组 64 点估算每组黑点数。
AppErrorCode ThermalSafety_CheckLine(const LineBuffer* line);

}  // namespace mp
