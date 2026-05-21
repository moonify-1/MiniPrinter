#pragma once

#include <cstddef>
#include <cstdint>

#include "common/app_error.h"
#include "services/sensor_service.h"

namespace mp {

// 工厂测试 / 调试命令入口。
//
// 这些函数只给显式调试命令调用，不会在启动流程或普通打印流程里自动执行。
// 目的：
// - 把“危险或半危险”的手动测试集中管理。
// - 默认硬件宏关闭时返回 ERR_HW_DISABLED，避免误操作真实硬件。
// - 每个入口都写日志，便于串口回溯是谁触发了测试。

// 立即关闭所有危险输出。
//
// SAFE_OFF 是最高优先级调试命令，协议层会在 SAFE_MODE 检查之前直接调用它。
AppErrorCode FactoryTest_SafeOff();

// 读取最近一次传感器快照。
//
// 这里不主动做业务决策，只把 SensorService 当前缓存的状态交给协议层输出。
SensorSnapshot FactoryTest_ReadSensors();

// 低速电机测试。
//
// steps 表示走多少个低速整步；MP_ENABLE_HW_STEPPER=0 时固定返回 ERR_HW_DISABLED。
AppErrorCode FactoryTest_MotorTest(std::uint32_t steps);

// 打印头移位测试。
//
// 只允许 shift 48 字节并 latch，不打开 VH，不输出 STB。
// MP_ENABLE_HW_THERMAL_HEAD=0 时固定返回 ERR_HW_DISABLED。
AppErrorCode FactoryTest_HeadShiftTest(const std::uint8_t* lineData,
                                       std::size_t len);

// 打印头 STB 脉冲测试。
//
// 安全约束：
// - MP_ENABLE_HW_THERMAL_HEAD 必须为 1。
// - 纸张、温度、电池条件必须通过 ThermalSafetyService 检查。
// - pulseUs 会被裁剪到安全硬上限以内。
// - 测试过程中始终保持 VH 关闭，只观察 STB 波形。
AppErrorCode FactoryTest_HeadStbTest(std::uint8_t group,
                                     std::uint32_t pulseUs);

// VH 输出测量测试。
//
// 安全约束：
// - 只打开 VH，高压测量窗口内始终保持 STB1~STB6 关闭。
// - 不移位、不 latch、不输出 STB 脉冲，因此不主动形成加热通路。
// - holdMs 由 HTTP 层限制在小范围内；测试结束后再次 setSafe()。
AppErrorCode FactoryTest_VhMeasure(std::uint32_t holdMs);

// 手动进入 SAFE_MODE。
//
// 该入口会先关闭危险输出，再通过 ErrorService/SystemApp 锁存安全模式。
AppErrorCode FactoryTest_EnterSafeMode();

// 手动重启。
//
// 该入口会先关闭危险输出并记录日志，再调用 ESP.restart()。
void FactoryTest_Reboot();

}  // namespace mp
