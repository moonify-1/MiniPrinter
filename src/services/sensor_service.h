#pragma once

#include <cstdint>

#include "common/app_types.h"

namespace mp {

// 传感器快照。
//
// 这个结构体表示“某一时刻采集到的一组传感器状态”。
// 它是纯数据结构，可以安全地放入 FreeRTOS 队列按值传递。
struct SensorSnapshot {
  bool paperPresent;        // true 表示检测到有纸。
  float headTempC;          // 打印头温度，单位摄氏度。
  std::uint32_t batteryMv;  // 电池电压，单位 mV。
  bool charging;            // true 表示当前处于充电状态。
  bool motorFault;          // true 表示电机驱动报告故障。
  SensorValidity validity;  // 这组读数整体是否可信。
  std::uint32_t tickMs;     // 采样发生时的系统时间，单位 ms。
};

// 初始化传感器服务。
//
// 当前阶段不初始化真实硬件，只清空内部快照。
void SensorService_Init();

// 执行一次传感器采样并更新系统事件。
//
// 函数职责：
// - 通过 SensorDriver 接口读取 mock 数据。
// - 做基础合法性判断。
// - 更新 EVT_PAPER_PRESENT / EVT_TEMP_OK / EVT_BAT_OK / EVT_DRV_READY。
// - 把最新 SensorSnapshot 写入 qSensor。
SensorSnapshot SensorService_Poll();

// 获取最近一次采样快照。
//
// 当前只有 SensorTask 写入快照，其他模块如需跨任务获取最新状态，
// 优先从 qSensor 读取；这个接口主要用于调试或后续测试。
SensorSnapshot SensorService_GetSnapshot();

}  // namespace mp
