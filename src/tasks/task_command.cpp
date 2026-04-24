#include "tasks/task_command.h"

#include <Arduino.h>
#include <cstdint>

#include "protocol/proto_frame.h"
#include "rtos/rtos_objects.h"
#include "rtos/task_registry.h"
#include "services/health_service.h"
#include "services/log_service.h"
#include "services/protocol_service.h"

namespace {

constexpr const char* kCommandTaskName = "task_command";
constexpr std::uint32_t kCommandTaskStackWords = 2048U;
constexpr UBaseType_t kCommandTaskPriority = 2U;
constexpr TickType_t kCommandQueueWaitTicks = pdMS_TO_TICKS(100U);

TaskHandle_t g_commandTaskHandle = nullptr;

// 命令任务主循环。
//
// 这个任务只负责从 qCommand 取出完整协议帧，然后交给 ProtocolService 分发。
// 真实调试动作被封装在 AppFactoryTest 中，CommandTask 不直接操作 GPIO、电机或打印头。
// 所有等待都带 timeout，避免任务永久阻塞导致心跳丢失。
void TaskCommandMain(void* /*context*/) {
  mp::RegisterTask(mp::TaskId::COMMAND, kCommandTaskName,
                   xTaskGetCurrentTaskHandle());
  mp::Health_ReportHeartbeat(mp::TaskId::COMMAND);
  mp::Log_Info("command", "CommandTask alive");

  for (;;) {
    mp::ProtoFrame frame = {};
    if (xQueueReceive(mp::g_rtos.qCommand, &frame, kCommandQueueWaitTicks) ==
        pdPASS) {
      mp::ProtocolService_HandleFrame(frame);
    }

    mp::Health_ReportHeartbeat(mp::TaskId::COMMAND);
  }
}

}  // namespace

namespace mp {

bool TaskCommand_Create() {
  if (g_commandTaskHandle != nullptr) {
    return true;
  }

  if (g_rtos.qCommand == nullptr) {
    return false;
  }

  const BaseType_t createResult =
      xTaskCreate(TaskCommandMain, kCommandTaskName, kCommandTaskStackWords,
                  nullptr, kCommandTaskPriority, &g_commandTaskHandle);

  if (createResult != pdPASS) {
    g_commandTaskHandle = nullptr;
    return false;
  }

  RegisterTask(TaskId::COMMAND, kCommandTaskName, g_commandTaskHandle);
  return true;
}

}  // namespace mp
