#pragma once

namespace mp {

// 创建监控任务。
//
// 监控任务负责：
// - 周期性检查关键任务心跳。
// - 发现异常后立即收敛到安全输出。
// - 设置系统事件位并记录故障日志。
bool TaskMonitor_Create();

}  // namespace mp
