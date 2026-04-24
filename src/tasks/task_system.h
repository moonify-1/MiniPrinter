#pragma once

namespace mp {

// 创建系统任务。
//
// 系统任务负责：
// - 周期性读取系统事件组。
// - 处理 qError 中的错误码。
// - 驱动 app_system 状态机。
// - 上报自身心跳。
bool TaskSystem_Create();

}  // namespace mp
