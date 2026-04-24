#pragma once

#include "common/app_error.h"
#include "common/app_types.h"

namespace mp {

// 系统状态机输入快照。
//
// 这个结构体由 SystemTask 负责填充，
// app_system 只根据这些“抽象条件”做状态转移判断，
// 不直接接触 GPIO、串口或具体硬件。
struct SystemAppInput {
  bool paperPresent;       // 当前是否检测到纸张存在。
  bool tempOk;             // 当前温度条件是否允许继续运行。
  bool batOk;              // 当前电池 / 电源条件是否允许继续运行。
  bool drvReady;           // 电机驱动是否准备就绪。
  bool vhOff;              // 高压路径当前是否确认处于关闭状态。
  bool printActive;        // 当前是否存在打印活动。
  bool safeModeRequested;  // 是否已有模块请求进入安全模式。
  bool lowPowerRequested;  // 是否进入低功耗 / 低电压受限状态。
  bool errorPending;       // 当前是否存在待处理错误。
  bool hasError;           // 本轮是否收到了新的错误。
  AppErrorCode errorCode;  // 最新错误码。
};

// 初始化系统状态机。
void SystemApp_Init();

// 根据最新输入处理一次状态机。
void SystemApp_ProcessEvent(const SystemAppInput& input);

// 读取当前系统状态。
SystemState SystemApp_GetState();

// 直接进入安全模式。
//
// 这个接口用于“无论当前状态机走到哪里，都必须立刻收敛”的场景。
void SystemApp_EnterSafeMode(AppErrorCode reason);

}  // namespace mp
