#include "rtos/rtos_objects.h"

#include <cstdint>

#include "app/app_print.h"
#include "common/fixed_pool.h"
#include "protocol/proto_frame.h"
#include "services/log_service.h"
#include "services/sensor_service.h"

namespace {

// 当前仍有部分队列的真实消息结构还未定义，
// 这些队列先统一使用一个轻量的 32 位占位槽位。
using QueuePlaceholder = std::uint32_t;

constexpr UBaseType_t kLogQueueLength = 16U;
constexpr UBaseType_t kDefaultQueueLength = 8U;
constexpr UBaseType_t kLineQueueLength =
    static_cast<UBaseType_t>(mp::LINE_BUFFER_POOL_SIZE);
constexpr UBaseType_t kSensorQueueLength = 1U;
constexpr UBaseType_t kCommandQueueLength = 4U;
constexpr UBaseType_t kPrintCtrlQueueLength = 8U;

// 判断 RTOS 对象是否已经完整创建。
bool AreObjectsReady(const mp::RtosObjects& objects) {
  return objects.systemEvents != nullptr && objects.qLog != nullptr &&
         objects.qError != nullptr && objects.qCommand != nullptr &&
         objects.qPrintCtrl != nullptr && objects.qLineReady != nullptr &&
         objects.qLineFree != nullptr && objects.qSensor != nullptr &&
         objects.qParam != nullptr && objects.stateMutex != nullptr &&
         objects.paramMutex != nullptr && objects.metricsMutex != nullptr;
}

// 创建占位队列。
QueueHandle_t CreatePlaceholderQueue(UBaseType_t length) {
  return xQueueCreate(length, sizeof(QueuePlaceholder));
}

// 创建真实日志队列。
//
// qLog 不能继续使用占位大小，
// 因为异步日志系统已经定义了固定长度的 LogMsg。
QueueHandle_t CreateLogQueue(UBaseType_t length) {
  return xQueueCreate(length, sizeof(mp::LogMsg));
}

// 创建传感器快照队列。
//
// qSensor 只保留最新状态，所以队列长度固定为 1。
// SensorService 会用 xQueueOverwrite 覆盖旧快照，避免消费者读到积压旧状态。
QueueHandle_t CreateSensorQueue() {
  return xQueueCreate(kSensorQueueLength, sizeof(mp::SensorSnapshot));
}

// 创建协议命令队列。
//
// ProtocolTask 放入完整 ProtoFrame，CommandTask 取出并分发。
QueueHandle_t CreateCommandQueue() {
  return xQueueCreate(kCommandQueueLength, sizeof(mp::ProtoFrame));
}

// 创建打印控制队列。
//
// qPrintCtrl 传递 PrintControlMsg，而不是裸整数。
// 这样后续 PrintEngineTask 能明确区分 START/END/CANCEL/FEED。
QueueHandle_t CreatePrintCtrlQueue() {
  return xQueueCreate(kPrintCtrlQueueLength, sizeof(mp::PrintControlMsg));
}

// 创建行缓冲指针队列。
//
// LineBuffer 本体存放在固定池中，队列只传指针，
// 这样不会在 FreeRTOS 队列里反复复制 48 字节数据。
QueueHandle_t CreateLineBufferPointerQueue() {
  return xQueueCreate(kLineQueueLength, sizeof(mp::LineBuffer*));
}

// 删除单个队列并清空句柄。
void DeleteQueueHandle(QueueHandle_t& handle) {
  if (handle == nullptr) {
    return;
  }

  vQueueDelete(handle);
  handle = nullptr;
}

// 删除单个事件组并清空句柄。
void DeleteEventGroupHandle(EventGroupHandle_t& handle) {
  if (handle == nullptr) {
    return;
  }

  vEventGroupDelete(handle);
  handle = nullptr;
}

// 删除单个互斥量并清空句柄。
//
// FreeRTOS 的互斥量底层也是队列对象，
// 所以释放时使用 vSemaphoreDelete。
void DeleteMutexHandle(SemaphoreHandle_t& handle) {
  if (handle == nullptr) {
    return;
  }

  vSemaphoreDelete(handle);
  handle = nullptr;
}

// 回收已经成功创建的对象，避免半初始化状态残留。
void CleanupObjects(mp::RtosObjects& objects) {
  DeleteEventGroupHandle(objects.systemEvents);

  DeleteQueueHandle(objects.qLog);
  DeleteQueueHandle(objects.qError);
  DeleteQueueHandle(objects.qCommand);
  DeleteQueueHandle(objects.qPrintCtrl);
  DeleteQueueHandle(objects.qLineReady);
  DeleteQueueHandle(objects.qLineFree);
  DeleteQueueHandle(objects.qSensor);
  DeleteQueueHandle(objects.qParam);

  DeleteMutexHandle(objects.stateMutex);
  DeleteMutexHandle(objects.paramMutex);
  DeleteMutexHandle(objects.metricsMutex);
}

}  // namespace

namespace mp {

RtosObjects g_rtos = {};

bool Rtos_CreateObjects() {
  if (AreObjectsReady(g_rtos)) {
    return true;
  }

  CleanupObjects(g_rtos);

  g_rtos.systemEvents = xEventGroupCreate();
  g_rtos.qLog = CreateLogQueue(kLogQueueLength);
  g_rtos.qError = CreatePlaceholderQueue(kDefaultQueueLength);
  g_rtos.qCommand = CreateCommandQueue();
  g_rtos.qPrintCtrl = CreatePrintCtrlQueue();
  g_rtos.qLineReady = CreateLineBufferPointerQueue();
  g_rtos.qLineFree = CreateLineBufferPointerQueue();
  g_rtos.qSensor = CreateSensorQueue();
  g_rtos.qParam = CreatePlaceholderQueue(kDefaultQueueLength);

  g_rtos.stateMutex = xSemaphoreCreateMutex();
  g_rtos.paramMutex = xSemaphoreCreateMutex();
  g_rtos.metricsMutex = xSemaphoreCreateMutex();

  if (!AreObjectsReady(g_rtos)) {
    CleanupObjects(g_rtos);
    return false;
  }

  return true;
}

}  // namespace mp
