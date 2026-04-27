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
// 注意：这里不启用真实热敏头、电机、传感器宏，避免普通调试固件误加热或误走纸。
#define MP_ENABLE_WIFI 1
#define MP_WIFI_SSID "BCXLY-2.4G"
#define MP_WIFI_PASSWORD "BCXYL000"
#define MP_WIFI_CONNECT_TIMEOUT_MS 15000
