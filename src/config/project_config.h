#pragma once

// 本文件保存当前开发机常用的项目级覆盖配置。
//
// `project_features.h` 会优先包含这个文件；如果这里定义了某个宏，
// 后面的安全缺省值就不会再覆盖它。
//
// 当前为了调试 WiFi API，直接把热点信息编进固件：
// - 优点：VSCode / PlatformIO 可以直接编译烧录，不需要每次手写长命令。
// - 代价：SSID 和密码会进入固件二进制，也会进入 Git 历史。
//
// 注意：当前 mock/API 流程已经验证完成，本配置默认启用真实步进电机和真实热敏头。
// - MP_ENABLE_HW_STEPPER=1：允许 `/api/v1/factory/motor-test` 真实驱动 DRV8833。
// - MP_ENABLE_HW_THERMAL_HEAD=1：允许打印任务进入真实热敏头输出路径。
// - 传感器宏仍保持默认关闭；PAPER_N、TM1、BAT_STAT 的真实语义确认前，不把它们作为强依赖。
#define MP_ENABLE_WIFI 1
#define MP_ENABLE_HW_STEPPER 1
#define MP_ENABLE_HW_THERMAL_HEAD 1
#define MP_WIFI_SSID "BCXLY-2.4G"
#define MP_WIFI_PASSWORD "BCXLY000"
#define MP_WIFI_CONNECT_TIMEOUT_MS 15000
