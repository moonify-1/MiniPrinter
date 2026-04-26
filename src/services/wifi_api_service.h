#pragma once

namespace mp {

// WiFi API 服务初始化。
//
// 这个服务是“正式产品控制面”的入口：
// - HTTP 层只负责接收请求、返回 JSON。
// - 真正的业务动作仍调用 app/services 层，例如 FactoryTest_SafeOff()。
// - 当 MP_ENABLE_WIFI=0 时，本接口保持可编译但不创建网络资源。
bool WifiApiService_Begin();

// 轮询 HTTP 客户端。
//
// Arduino WebServer 使用协作式轮询模型，必须由一个 FreeRTOS 任务周期调用。
void WifiApiService_Poll();

}  // namespace mp
