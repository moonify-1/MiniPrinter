#pragma once

namespace mp {

// 创建日志任务。
//
// 任务职责：
// - 从 qLog 队列阻塞接收日志。
// - 通过 Serial 输出。
// - 每隔固定时间上报一次心跳。
bool TaskLog_Create();

}  // namespace mp
