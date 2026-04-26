#pragma once

namespace mp {

// 创建 WiFi API 任务。
//
// 当 MP_ENABLE_WIFI=0 时直接返回 true，不占用任务栈；
// 当 MP_ENABLE_WIFI=1 时创建一个专门轮询 HTTP API 的 FreeRTOS 任务。
bool TaskWifiApi_Create();

}  // namespace mp
