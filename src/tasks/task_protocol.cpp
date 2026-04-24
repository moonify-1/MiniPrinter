#include "tasks/task_protocol.h"

#include <Arduino.h>
#include <cstdint>

#include "protocol/proto_parser.h"
#include "rtos/task_registry.h"
#include "services/health_service.h"
#include "services/log_service.h"
#include "services/protocol_service.h"

namespace {

constexpr const char* kProtocolTaskName = "task_protocol";
constexpr std::uint32_t kProtocolTaskStackWords = 2048U;
constexpr UBaseType_t kProtocolTaskPriority = 2U;
constexpr TickType_t kProtocolPollPeriodTicks = pdMS_TO_TICKS(10U);
constexpr std::uint8_t kMaxBytesPerCycle = 64U;

TaskHandle_t g_protocolTaskHandle = nullptr;

// 串口协议任务主循环。
//
// 它只负责：
// - 从 Serial 读取字节。
// - 喂给 ProtoParser。
// - 把完整帧投递给 CommandTask。
void TaskProtocolMain(void* /*context*/) {
  mp::RegisterTask(mp::TaskId::COMM, kProtocolTaskName,
                   xTaskGetCurrentTaskHandle());
  mp::Health_ReportHeartbeat(mp::TaskId::COMM);
  mp::ProtocolService_Init();
  mp::Log_Info("protocol", "ProtocolTask alive");

  mp::ProtoParser parser;
  TickType_t lastWakeTick = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&lastWakeTick, kProtocolPollPeriodTicks);
    mp::Health_ReportHeartbeat(mp::TaskId::COMM);

    std::uint8_t bytesHandled = 0U;
    while (Serial.available() > 0 && bytesHandled < kMaxBytesPerCycle) {
      const int rawByte = Serial.read();
      if (rawByte < 0) {
        break;
      }

      mp::ProtoFrame frame = {};
      const mp::ProtoParseResult result =
          parser.feed(static_cast<std::uint8_t>(rawByte), &frame);

      if (result == mp::ProtoParseResult::FRAME_READY) {
        (void)mp::ProtocolService_PostFrame(frame);
      }

      ++bytesHandled;
    }
  }
}

}  // namespace

namespace mp {

bool TaskProtocol_Create() {
  if (g_protocolTaskHandle != nullptr) {
    return true;
  }

  const BaseType_t createResult =
      xTaskCreate(TaskProtocolMain, kProtocolTaskName, kProtocolTaskStackWords,
                  nullptr, kProtocolTaskPriority, &g_protocolTaskHandle);

  if (createResult != pdPASS) {
    g_protocolTaskHandle = nullptr;
    return false;
  }

  RegisterTask(TaskId::COMM, kProtocolTaskName, g_protocolTaskHandle);
  return true;
}

}  // namespace mp
