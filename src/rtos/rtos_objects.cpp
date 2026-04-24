#include "rtos/rtos_objects.h"

#include <cstdint>

namespace {

// 当前阶段还没有定义真实消息结构体，
// 所以各个队列先统一用一个轻量的 32 位占位槽位。
//
// 这样可以先把系统骨架搭起来，后续再替换成具体消息类型。
using QueuePlaceholder = std::uint32_t;

constexpr UBaseType_t kDefaultQueueLength = 8U;
constexpr UBaseType_t kLineQueueLength = 4U;

// 判断 RTOS 对象是否已经全部创建完成。
//
// 这个函数的目的是让 Rtos_CreateObjects() 具备“幂等性”：
// 如果对象已经创建过，再次调用会直接返回成功，而不是重复分配内存。
bool AreObjectsReady(const mp::RtosObjects& objects) {
  return objects.systemEvents != nullptr && objects.qLog != nullptr &&
         objects.qError != nullptr && objects.qCommand != nullptr &&
         objects.qPrintCtrl != nullptr && objects.qLineReady != nullptr &&
         objects.qLineFree != nullptr && objects.qSensor != nullptr &&
         objects.qParam != nullptr && objects.stateMutex != nullptr &&
         objects.paramMutex != nullptr && objects.metricsMutex != nullptr;
}

// 统一创建一个占位队列。
QueueHandle_t CreatePlaceholderQueue(UBaseType_t length) {
  return xQueueCreate(length, sizeof(QueuePlaceholder));
}

// 删除单个队列并把句柄清空。
void DeleteQueueHandle(QueueHandle_t& handle) {
  if (handle == nullptr) {
    return;
  }

  vQueueDelete(handle);
  handle = nullptr;
}

// 删除单个事件组并把句柄清空。
void DeleteEventGroupHandle(EventGroupHandle_t& handle) {
  if (handle == nullptr) {
    return;
  }

  vEventGroupDelete(handle);
  handle = nullptr;
}

// 删除单个互斥量并把句柄清空。
//
// FreeRTOS 的互斥量本质上也是一种队列对象，
// 所以释放时同样使用 vSemaphoreDelete。
void DeleteMutexHandle(SemaphoreHandle_t& handle) {
  if (handle == nullptr) {
    return;
  }

  vSemaphoreDelete(handle);
  handle = nullptr;
}

// 回收当前已经创建出来的 RTOS 对象。
//
// 这样一旦中途分配失败，就不会留下半初始化状态。
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
  g_rtos.qLog = CreatePlaceholderQueue(kDefaultQueueLength);
  g_rtos.qError = CreatePlaceholderQueue(kDefaultQueueLength);
  g_rtos.qCommand = CreatePlaceholderQueue(kDefaultQueueLength);
  g_rtos.qPrintCtrl = CreatePlaceholderQueue(kDefaultQueueLength);
  g_rtos.qLineReady = CreatePlaceholderQueue(kLineQueueLength);
  g_rtos.qLineFree = CreatePlaceholderQueue(kLineQueueLength);
  g_rtos.qSensor = CreatePlaceholderQueue(kDefaultQueueLength);
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
