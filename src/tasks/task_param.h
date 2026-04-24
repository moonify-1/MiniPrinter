#pragma once

namespace mp {

// 创建参数任务。
//
// ParamTask 是低优先级任务，只负责延迟合并保存请求。
bool TaskParam_Create();

}  // namespace mp
