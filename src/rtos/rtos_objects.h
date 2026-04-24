#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace mp {

// RTOS 对象集合。
//
// 这个结构体的作用是把“系统级共享 RTOS 资源”集中放在一个地方，
// 这样后续各任务就不需要各自保存一份句柄，也能减少全局变量散落。
//
// 当前阶段只建立对象骨架，不创建任何实际任务。
struct RtosObjects {
  EventGroupHandle_t systemEvents; // 系统级事件标志组。

  QueueHandle_t qLog;       // 日志消息队列。
  QueueHandle_t qError;     // 错误上报队列。
  QueueHandle_t qCommand;   // 命令输入队列。
  QueueHandle_t qPrintCtrl; // 打印控制队列。
  QueueHandle_t qLineReady; // 已准备好打印的数据行队列。
  QueueHandle_t qLineFree;  // 空闲行缓冲回收队列。
  QueueHandle_t qSensor;    // 传感器数据或事件队列。
  QueueHandle_t qParam;     // 参数更新或参数请求队列。

  SemaphoreHandle_t stateMutex;   // 保护系统状态共享数据。
  SemaphoreHandle_t paramMutex;   // 保护参数块共享数据。
  SemaphoreHandle_t metricsMutex; // 保护统计信息共享数据。
};

// 系统事件位定义。
//
// 每一位只表达一个“是否成立”的系统条件，
// 这样多个任务可以用事件组快速组合判断系统是否已满足某些前提。
constexpr EventBits_t EVT_INIT_OK = (1U << 0);
constexpr EventBits_t EVT_PARAM_READY = (1U << 1);
constexpr EventBits_t EVT_SELFTEST_OK = (1U << 2);
constexpr EventBits_t EVT_PAPER_PRESENT = (1U << 3);
constexpr EventBits_t EVT_TEMP_OK = (1U << 4);
constexpr EventBits_t EVT_BAT_OK = (1U << 5);
constexpr EventBits_t EVT_DRV_READY = (1U << 6);
constexpr EventBits_t EVT_VH_OFF = (1U << 7);
constexpr EventBits_t EVT_COMM_CONNECTED = (1U << 8);
constexpr EventBits_t EVT_PRINT_ACTIVE = (1U << 9);
constexpr EventBits_t EVT_SAFE_MODE = (1U << 10);
constexpr EventBits_t EVT_LOW_POWER = (1U << 11);
constexpr EventBits_t EVT_ERROR_PENDING = (1U << 12);

// 全局 RTOS 对象实例。
//
// 后续任务层会通过它访问统一创建好的事件组、队列和互斥量。
extern RtosObjects g_rtos;

// 创建 RTOS 基础对象。
//
// 返回值说明：
// - true：所有要求的对象都已创建成功。
// - false：中途有任一对象创建失败，本函数会尽量回收已创建的对象。
bool Rtos_CreateObjects();

}  // namespace mp
