#pragma once

#include "protocol/proto_frame.h"

namespace mp {

// 初始化协议服务。
//
// 当前阶段没有复杂运行时状态，保留这个入口是为了后续接入统计或设备信息。
void ProtocolService_Init();

// 投递一帧完整协议帧给 CommandTask。
//
// ProtocolTask 收到完整帧后调用这个函数。
// 如果命令队列满，会丢弃一条旧帧再尝试保存新帧，避免协议任务被阻塞。
bool ProtocolService_PostFrame(const ProtoFrame& frame);

// 处理一帧命令。
//
// CommandTask 调用本函数完成命令分发。
// 当前只支持 PING / GET_INFO / GET_STATUS，不实现打印命令。
void ProtocolService_HandleFrame(const ProtoFrame& frame);

}  // namespace mp
