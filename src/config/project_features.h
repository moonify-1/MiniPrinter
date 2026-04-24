#pragma once

// 预处理宏无法放进 namespace 中，
// 所以本文件里的“功能开关宏”会保持全局可见。
//
// 设计目标：
// 1. 如果项目自定义了 project_config.h，就优先读取用户覆盖值。
// 2. 如果项目没有提供 project_config.h，就使用这里的安全缺省值。
#if defined(__has_include)
#if __has_include("project_config.h")
#include "project_config.h"
#endif
#endif

#ifndef MP_ENABLE_HW_THERMAL_HEAD
#define MP_ENABLE_HW_THERMAL_HEAD 0
#endif

#ifndef MP_ENABLE_HW_STEPPER
#define MP_ENABLE_HW_STEPPER 0
#endif

#ifndef MP_ENABLE_WDT
#define MP_ENABLE_WDT 0
#endif

#ifndef MP_ENABLE_WIFI
#define MP_ENABLE_WIFI 0
#endif

#ifndef MP_ENABLE_BLE
#define MP_ENABLE_BLE 0
#endif

namespace mp {

// 把预处理宏再映射成 constexpr，
// 这样普通 C++ 代码就可以用类型安全的方式读取功能开关。
constexpr bool FEATURE_HW_THERMAL_HEAD = (MP_ENABLE_HW_THERMAL_HEAD != 0);
constexpr bool FEATURE_HW_STEPPER = (MP_ENABLE_HW_STEPPER != 0);
constexpr bool FEATURE_WDT = (MP_ENABLE_WDT != 0);
constexpr bool FEATURE_WIFI = (MP_ENABLE_WIFI != 0);
constexpr bool FEATURE_BLE = (MP_ENABLE_BLE != 0);

}  // namespace mp
