# MiniPrinterRTOS Acceptance Criteria

## 可量化验收标准

- `python -m platformio run` 必须通过。
- 默认启动后系统进入 `READY` 或 `IDLE`，不得卡死或复位循环。
- 缺纸时禁止进入真实打印加热流程。
- 过温时禁止进入真实打印加热流程。
- 低电压时禁止加热，并禁止高速电机动作。
- 默认配置不得打开 VH、不得输出 STB 有效电平、不得唤醒或驱动电机。
- mock 打印流程可通过 `PRINT_START -> PRINT_LINE -> PRINT_END` 完成，并输出模拟日志。
- `SAFE_OFF` 在任意状态下都能关闭危险输出。
- `SENSOR_TEST` 能返回当前传感器快照。
- `MOTOR_TEST` 默认返回 `ERR_HW_DISABLED`。
- `HEAD_STB_TEST` 默认返回 `ERR_HW_DISABLED`。
- 所有危险硬件动作必须受 `MP_ENABLE_HW_THERMAL_HEAD` 或 `MP_ENABLE_HW_STEPPER` 宏保护。
- `HEAD_STB_TEST` 的脉冲宽度不得超过 `SAFETY_DEFAULT_HEAT_PULSE_MAX_US`。
- SAFE_MODE 下 `PRINT_START` 必须返回 `NACK_SAFE_MODE`。
- `MP_ENABLE_WDT=0` 时真实 Watchdog 不启用。
- `MP_ENABLE_WDT=1` 时只有 MonitorTask 可以喂狗。
- `python tools/api_client.py --help` 和 `python tools/api_client.py self-test` 必须可运行。
- 正式打印文件上传和启动打印验收使用 WiFi API，不再以 UART `PRINT_LINE` 作为产品入口。

## 当前阶段非验收项

- 不验收高速打印。
- 不验收真实灰度/浓度控制。
- 不验收量产级温度补偿曲线。
- 不验收真实 NVS 参数 UI。
- 不宣称真实设备 WiFi 联机已通过；联机、波形、电机方向和首次真实打印需要用户实测记录。
