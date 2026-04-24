#pragma once

#include <cstdint>

#include "config/default_params.h"

namespace mp {

// qParam 使用的轻量请求编号。
//
// 当前只需要“延迟保存”一种请求，所以不把整个 ParamBlock 放进队列。
constexpr std::uint32_t PARAM_REQUEST_SAVE = 1U;

// 参数服务初始化。
//
// 会准备默认参数和 NVS 驱动状态；不会因为 NVS 不可用而失败启动。
bool Param_Init();

// 从 NVS 加载参数。
//
// 成功时使用 NVS 参数；失败、CRC 错误或版本不匹配时回退默认参数。
bool Param_Load();

// 获取当前参数快照。
ParamBlock Param_GetSnapshot();

// 更新内存中的参数快照。
//
// 这个函数只更新 RAM，不立即写 Flash。
// 需要保存时调用 Param_RequestSave()，由 ParamTask 延迟合并写入。
bool Param_Update(const ParamBlock& params);

// 请求保存参数。
//
// 该函数只向 qParam 投递保存请求，不直接写 Flash。
bool Param_RequestSave();

// 恢复默认参数，并请求后续保存。
bool Param_ResetDefault();

// ParamTask 内部使用的立即保存入口。
//
// 普通业务代码不应直接调用它，避免绕过延迟合并策略。
bool Param_SaveNowForTask();

}  // namespace mp
