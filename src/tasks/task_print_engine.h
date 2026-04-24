#pragma once

namespace mp {

// 创建打印引擎任务。
//
// Step 17 只跑 mock 打印流程：
// - 消费 qPrintCtrl 和 qLineReady。
// - 调用安全服务计算模拟脉冲。
// - 调用 mock ThermalHeadDriver / mock StepperDriver。
bool TaskPrintEngine_Create();

}  // namespace mp
