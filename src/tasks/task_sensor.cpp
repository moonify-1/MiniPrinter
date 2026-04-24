#include "tasks/task_sensor.h"

#include <Arduino.h>
#include <cstdint>

#include "rtos/task_registry.h"
#include "services/health_service.h"
#include "services/log_service.h"
#include "services/sensor_service.h"

namespace {

constexpr const char* kSensorTaskName = "task_sensor";
constexpr std::uint32_t kSensorTaskStackWords = 1024U;
constexpr UBaseType_t kSensorTaskPriority = 2U;
constexpr TickType_t kSensorPeriodTicks = pdMS_TO_TICKS(50U);

TaskHandle_t g_sensorTaskHandle = nullptr;

// 传感器任务主循环。
//
// 它只负责周期采样和心跳上报，不直接做打印、走纸或安全策略决策。
// 采样结果会由 SensorService 写入 EventGroup 和 qSensor。
void TaskSensorMain(void* /*context*/) {
  mp::RegisterTask(mp::TaskId::SENSOR, kSensorTaskName,
                   xTaskGetCurrentTaskHandle());
  mp::Health_ReportHeartbeat(mp::TaskId::SENSOR);
  mp::SensorService_Init();
  mp::Log_Info("sensor", "SensorTask alive");

  TickType_t lastWakeTick = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&lastWakeTick, kSensorPeriodTicks);
    mp::Health_ReportHeartbeat(mp::TaskId::SENSOR);
    (void)mp::SensorService_Poll();
  }
}

}  // namespace

namespace mp {

bool TaskSensor_Create() {
  if (g_sensorTaskHandle != nullptr) {
    return true;
  }

  const BaseType_t createResult =
      xTaskCreate(TaskSensorMain, kSensorTaskName, kSensorTaskStackWords,
                  nullptr, kSensorTaskPriority, &g_sensorTaskHandle);

  if (createResult != pdPASS) {
    g_sensorTaskHandle = nullptr;
    return false;
  }

  RegisterTask(TaskId::SENSOR, kSensorTaskName, g_sensorTaskHandle);
  return true;
}

}  // namespace mp
