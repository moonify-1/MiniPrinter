#pragma once

namespace mp {

// 创建传感器任务。
//
// 返回值说明：
// - true：任务已存在或创建成功。
// - false：FreeRTOS 创建任务失败。
bool TaskSensor_Create();

}  // namespace mp
