#pragma once

#include <cstdint>

#include "app/app_print.h"
#include "common/app_error.h"
#include "common/fixed_pool.h"
#include "config/default_params.h"
#include "config/print_config.h"
#include "services/sensor_service.h"

namespace mp {

// 单行打印计划。
//
// PrintEngineTask 先把 48 字节行数据转换为这份计划：
// - groupBlackDots：每个 STB 组有多少黑点。
// - pulseUs：每个 STB 组建议的模拟加热脉冲。
// 当前 Step 17 只用于日志和 mock driver 调用，不会打开真实 VH/STB。
struct PrintLinePlan {
  std::uint8_t groupBlackDots[PRINT_STB_GROUP_COUNT];
  std::uint32_t pulseUs[PRINT_STB_GROUP_COUNT];
};

// 初始化打印服务。
//
// 当前只初始化打印作业状态，不初始化真实硬件。
void PrintService_Init();

// 向 qPrintCtrl 投递控制消息。
//
// timeoutMs 是等待队列空位的最长时间，单位 ms。
// 返回 false 表示队列未就绪或投递超时。
bool PrintService_PostControl(const PrintControlMsg& msg,
                              std::uint32_t timeoutMs);

// 便捷控制接口：开始打印。
bool PrintService_RequestStart(std::uint32_t jobId);

// 便捷控制接口：结束打印。
bool PrintService_RequestEnd(std::uint32_t jobId);

// 便捷控制接口：取消打印。
bool PrintService_RequestCancel(std::uint32_t jobId);

// 便捷控制接口：走纸若干步。
bool PrintService_RequestFeed(std::uint32_t steps);

// 设置系统打印活动事件位。
//
// SystemTask 通过 EVT_PRINT_ACTIVE 判断是否进入 RUNNING。
void PrintService_SetPrintActive(bool active);

// 计算一行的分组黑点数和模拟脉冲。
//
// 这里会调用 ThermalSafety_CheckLine() 和 ThermalSafety_CalcPulseUs()。
AppErrorCode PrintService_BuildLinePlan(const LineBuffer* line,
                                        const SensorSnapshot& sensor,
                                        const ParamBlock& params,
                                        PrintLinePlan* plan);

// 上报打印错误。
//
// 该函数会尽量向 qError 投递错误码，并设置错误事件位。
void PrintService_ReportError(AppErrorCode error);

}  // namespace mp
