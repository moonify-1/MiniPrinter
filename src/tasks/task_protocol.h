#pragma once

namespace mp {

// 创建串口协议任务。
//
// 当前实现使用简单 Serial 轮询，后续如需高吞吐可再替换 DMA / ring buffer。
bool TaskProtocol_Create();

}  // namespace mp
