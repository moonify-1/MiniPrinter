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

#ifndef MP_WIFI_SSID
#define MP_WIFI_SSID ""
#endif

#ifndef MP_WIFI_PASSWORD
#define MP_WIFI_PASSWORD ""
#endif

#ifndef MP_WIFI_AP_SSID
#define MP_WIFI_AP_SSID "MiniPrinterRTOS"
#endif

#ifndef MP_WIFI_AP_PASSWORD
#define MP_WIFI_AP_PASSWORD ""
#endif

#ifndef MP_WIFI_CONNECT_TIMEOUT_MS
#define MP_WIFI_CONNECT_TIMEOUT_MS 5000
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

// WiFi 是本项目正式控制面，但开发阶段默认不把 SSID/密码写进仓库。
// 启用方法示例：
// PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_WIFI=1 -DMP_WIFI_SSID=\"your_ssid\" -DMP_WIFI_PASSWORD=\"your_pass\""
constexpr const char* WIFI_SSID = MP_WIFI_SSID;
constexpr const char* WIFI_PASSWORD = MP_WIFI_PASSWORD;
constexpr const char* WIFI_AP_SSID = MP_WIFI_AP_SSID;
constexpr const char* WIFI_AP_PASSWORD = MP_WIFI_AP_PASSWORD;
constexpr unsigned WIFI_CONNECT_TIMEOUT_MS = MP_WIFI_CONNECT_TIMEOUT_MS;

}  // namespace mp
