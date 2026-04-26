# MiniPrinterRTOS Test Plan

## 编译测试

- 默认配置执行 `python -m platformio run`，必须成功。
- 硬件保护宏检查：分别使用 `-DMP_ENABLE_HW_THERMAL_HEAD=1`、`-DMP_ENABLE_HW_STEPPER=1`、`-DMP_ENABLE_WDT=1` 编译，确认受保护分支可编译。
- 不修改 `platformio.ini`，临时宏通过 `PLATFORMIO_BUILD_FLAGS` 传入。

## 启动测试

- 烧录默认固件后打开 115200 串口。
- 观察启动日志，应看到 LogTask、SystemTask、SensorTask、MonitorTask、PrintEngineTask alive。
- 默认 mock sensor 条件下，系统应进入 `READY`；若 mock 改为缺纸，应停在 `IDLE`。

## GPIO 安全态测试

- 上电后测量 `G_VH`、`STB1~STB6`、`G_NSLEEP`、`AIN1/AIN2/BIN1/BIN2`。
- 默认固件不得打开 VH，不得输出 STB 有效电平，不得唤醒 DRV8833。
- 发送 `SAFE_OFF` 后再次确认所有危险输出回到安全态。

## UART 协议测试

- 使用 `python tools/send_frame.py ping --port COMx` 发送 PING，应收到响应帧。
- 使用 `python tools/send_frame.py status --port COMx` 读取状态。
- 使用 `python tools/send_frame.py safe-off --port COMx` 验证 SAFE_OFF 可执行。
- 使用 `python tools/send_frame.py sensor-test --port COMx` 验证 Sensor mock 快照可读取。
- UART 只作为底层临时调试参考，正式打印文件上传、启动打印和控制验收应走 WiFi API。

## WiFi API 主机侧测试

- `python tools/api_client.py --help` 应正常显示命令列表。
- `python tools/api_client.py self-test` 应在不连接设备的情况下完成本地分片和 CRC dry-run。
- 设备启用 `MP_ENABLE_WIFI=1` 后，可用 `python tools/api_client.py --base http://<device-ip> info/status/sensors/battery` 查询状态。
- 打印文件验收应使用 `tools/make_test_print.py` 生成 raw，再用 `tools/api_client.py upload` 上传，并用 `start/cancel/feed` 验证控制路径。
- 真实工厂测试命令必须按文档阶段执行；失败时停止后续危险测试。

## Sensor mock 测试

- 默认 mock：paper present、温度约 25C、电池约 7400mV、无 motor fault。
- 修改 mock 配置为缺纸后，`GET_STATUS`/`SENSOR_TEST` 应显示 paper=0，系统不得进入 READY。
- 修改 mock 温度异常或低电压后，打印启动应被安全层拒绝。

## Queue 满测试

- 连续发送超过 `LINE_BUFFER_POOL_SIZE` 的 `PRINT_LINE`，不消费或延迟消费时应返回 `ERR_QUEUE_FULL`。
- 日志队列满时 DEBUG/INFO 可以丢弃，ERROR/FATAL 应保留 fallback 输出。

## SAFE_MODE 测试

- 发送 `ENTER_SAFE_MODE` 后系统应进入 SAFE_MODE。
- SAFE_MODE 下 `GET_STATUS`、`GET_ERROR`、`CLEAR_ERROR`、`SELF_TEST`、`REBOOT`、`FACTORY_RESET`、`SAFE_OFF` 应可用。
- SAFE_MODE 下 `PRINT_START` 必须返回 `NACK_SAFE_MODE`。

## Watchdog 测试

- 默认 `MP_ENABLE_WDT=0` 时，行为应与普通软件监控一致。
- 使用 `MP_ENABLE_WDT=1` 编译启动后，日志应显示 `WDT enabled`。
- 人为停止 SystemTask/SensorTask/PrintEngineTask 心跳时，MonitorTask 应先执行安全输出，再停止喂狗。
- LogTask 卡住不应直接触发复位。

## 打印头波形测试

- 默认 `MP_ENABLE_HW_THERMAL_HEAD=0` 时，`HEAD_SHIFT_TEST` 和 `HEAD_STB_TEST` 应返回 `ERR_HW_DISABLED`。
- 启用硬件宏后，先只执行 `HEAD_SHIFT_TEST`，用示波器确认 DI/CLK/LAT。
- `HEAD_STB_TEST` 必须在 paper/temp/bat OK 时才允许执行，pulseUs 不得超过安全硬上限。
- `HEAD_STB_TEST` 期间 VH 必须保持关闭。

## 电机低速测试

- 默认 `MP_ENABLE_HW_STEPPER=0` 时，`MOTOR_TEST` 应返回 `ERR_HW_DISABLED`。
- 启用硬件宏后，先发送小步数，例如 1 到 10 步。
- 测试前后确认 nSLEEP、AIN/BIN 最终回到安全状态。
- 真实硬件逐阶段结果填写到 `docs/硬件分阶段验收记录.md`。
