# Codex 进度记录

## Step 00

- 时间：2026-04-24 12:25:08
- 状态：已完成
- 结果：
  - 检查了仓库目录结构。
  - 确认当前仓库已经是 PlatformIO 工程。
  - 未修改 `platformio.ini`。
  - 新建了任务路线、架构说明、引脚占位表、进度记录文档。
  - 未创建真实硬件驱动，未创建打印头加热逻辑。
- 下一步建议：
  - 进入 Step 01，先建立最小分层目录和 RTOS 启动骨架。

## Step 02

- 时间：2026-04-24 12:33:24
- 状态：已完成
- 结果：
  - 新增 `src/common/app_types.h`，定义系统状态、打印任务状态、传感器有效性和日志级别。
  - 新增 `src/common/app_error.h` 与 `src/common/app_error.cpp`，实现统一错误码打包、解包和常用错误常量。
  - `common` 层仅依赖标准头文件 `<cstdint>`，不依赖 Arduino 和 FreeRTOS。
  - `src/main.cpp` 已收敛为最小编译验证入口，不包含业务逻辑。
  - 使用 `python -m platformio run` 编译通过。
  - 使用文本搜索确认 `src/common/` 下未包含 `Arduino.h` 和 `FreeRTOS.h`。
- 下一步建议：
  - 优先执行 Step 01/03 之间的骨架搭建工作，把 `config`、`app`、`tasks`、`rtos` 等目录先建立起来。

## Step 03

- 时间：2026-04-24 12:50:00
- 状态：已完成
- 结果：
  - 新增 `src/config/project_features.h`，集中管理功能开关和默认缺省值。
  - 新增 `src/config/print_config.h`，定义打印行宽和 STB 分组常量。
  - 新增 `src/config/safety_limits.h`，定义温度、热脉冲和低电压安全阈值。
  - 新增 `src/config/default_params.h`，定义 `ParamBlock` 及各参数子结构的默认值。
  - `main.cpp` 已加入最小 include 验证，确保配置头文件参与编译。
  - 未引入 Arduino/FreeRTOS 到 `config` 层。
  - 使用 `python -m platformio run` 编译通过。
  - 使用文本搜索确认 `src/config/` 下未包含 `Arduino.h` 和 `FreeRTOS.h`。
- 下一步建议：
  - 进入 Step 04 时，可以基于 `ParamBlock` 先建立参数服务接口，但仍然不要接入真实 NVS 读写。

## Step 04

- 时间：2026-04-24 12:59:05
- 状态：已完成
- 结果：
  - 新增 `src/bsp/bsp_pins.h`，集中管理板级引脚占位，当前全部保持 `PIN_UNASSIGNED`。
  - 新增 `src/bsp/bsp_gpio.h/.cpp`，提供 GPIO 安全包装接口。
  - 新增 `src/bsp/bsp_board.h/.cpp`，提供板级安全输出入口和初始化函数。
  - `main.cpp` 已调整为最早调用 `Bsp_PreInitSafeOutputs()`，然后再调用 `Serial.begin()`。
  - BSP 当前不会打开 VH，不会输出 STB 有效电平，不会唤醒 DRV8833。
  - 未分配引脚统一跳过，不报错。
  - 使用 `python -m platformio run` 编译通过。
  - BSP 层按设计允许包含 `Arduino.h`，当前仅在 `bsp_gpio.cpp` 和 `bsp_board.cpp` 中使用。
- 下一步建议：
  - 下一步适合补 Step 05：把输入读取和板级状态采样做成只读接口，例如纸检测、故障脚、按键、电池状态，但仍然不要接真实打印或电机驱动。

## Step 05

- 时间：2026-04-24 13:06:00
- 状态：已完成
- 结果：
  - 新增 `src/rtos/rtos_objects.h/.cpp`，定义系统事件组、队列、互斥量和统一创建入口。
  - 新增 `src/rtos/task_registry.h/.cpp`，定义任务编号、登记接口、心跳接口和健康快照结构。
  - `main.cpp` 已在 `Bsp_Init()` 后调用 `Rtos_CreateObjects()`，创建失败时会回退到安全输出并打印错误。
  - 当前只提供 RTOS 对象和任务注册表接口，没有创建实际任务。
  - 当前队列使用 `uint32_t` 占位元素，后续需要按真实消息结构替换。
  - 使用 `python -m platformio run` 编译通过。
- 下一步建议：
  - 下一步适合建立 `services` 或 `app` 层的系统状态容器，并把 `stateMutex` 和 `paramMutex` 真正接入读写路径。

## Step 06

- 时间：2026-04-24 13:14:14
- 状态：已完成
- 结果：
  - 新增 `src/services/log_service.h/.cpp`，实现固定长度异步日志服务。
  - 新增 `src/tasks/task_log.h/.cpp`，实现低优先级日志任务与心跳上报。
  - `src/rtos/rtos_objects.cpp` 中的 `qLog` 已改为真实 `LogMsg` 队列，其余队列仍保持占位结构。
  - `main.cpp` 已在 RTOS 对象创建后初始化日志，并创建 `LogTask`。
  - 启动时会写入一条 `Async log ready` 日志，用于验证异步日志链路。
  - 当前未在 ISR 中提供日志接口。
  - 使用 `python -m platformio run` 编译通过。
- 下一步建议：
  - 下一步适合把 `qError`、`qCommand`、`qSensor` 等队列逐步替换成真实消息结构，并让更多任务通过 `LogService` 统一输出日志。

## Step 07

- 时间：2026-04-24 13:20:00
- 状态：已完成
- 结果：
  - 新增 `src/services/health_service.h/.cpp`，实现任务心跳上报、健康检查和快照读取。
  - 新增 `src/tasks/task_monitor.h/.cpp`，实现 100ms 周期监控任务。
  - `MonitorTask` 优先级高于 `LogTask`，发现关键任务超时后会立即执行 `Bsp_SetAllOutputsSafe()`。
  - 超时时会设置 `EVT_SAFE_MODE | EVT_ERROR_PENDING`，并记录 `FATAL` 日志。
  - `LogTask` 与 `MonitorTask` 都已通过 `Health_ReportHeartbeat()` 上报心跳。
  - `MP_ENABLE_WDT` 已作为预留开关保留，但当前未启用真实硬件 Watchdog。
  - 使用 `python -m platformio run` 编译通过。
- 下一步建议：
  - 下一步适合为 `qError` 和状态服务定义真实错误消息结构，让 MonitorTask 的故障结果不只写日志，还能进入统一错误处理通道。

## Step 08

- 时间：2026-04-24 13:31:00
- 状态：已完成
- 结果：
  - 新增 `src/app/app_system.h/.cpp`，实现硬件无关的系统状态机骨架。
  - 新增 `src/tasks/task_system.h/.cpp`，实现 100ms 周期的 SystemTask。
  - SystemTask 会读取 EventGroup、处理 `qError`、维护系统状态并上报 heartbeat。
  - 初始状态路径为 `BOOT -> INIT -> SELF_TEST -> IDLE`，自检当前为 mock 通过。
  - 当 paper / temp / bat 尚未真实实现时，状态机会停在 `IDLE`，不会强行进入 `READY`。
  - SAFE_MODE 下会调用 `Bsp_SetAllOutputsSafe()`，继续保持危险输出关闭。
  - 使用 `python -m platformio run` 编译通过。
- 下一步建议：
  - 下一步适合补传感器与板级状态输入，把 `EVT_PAPER_PRESENT`、`EVT_TEMP_OK`、`EVT_BAT_OK` 变成真实数据来源，这样系统状态机才能从 `IDLE` 进一步走向 `READY`。

## Step 09

- 时间：2026-04-24 15:57:05
- 状态：已完成
- 结果：
  - 新增 `src/drivers/thermal_head/drv_thermal_head.h`，定义热敏打印头硬件动作接口。
  - 新增 `src/drivers/thermal_head/drv_thermal_head_mock.cpp`，实现不操作 GPIO 的热敏打印头 mock。
  - 新增 `src/drivers/stepper/drv_stepper.h`，定义步进电机硬件动作接口。
  - 新增 `src/drivers/stepper/drv_stepper_mock.cpp`，实现不操作 GPIO、不唤醒 DRV8833 的步进电机 mock。
  - 新增 `src/drivers/sensors/drv_sensor_types.h`，定义传感器读取接口、充电状态和 mock 配置/统计结构。
  - 新增 `src/drivers/sensors/drv_sensors_mock.cpp`，实现可配置固定返回值的传感器 mock。
  - 当前 driver 接口只描述硬件动作，不包含业务决策。
  - 当前 mock 只记录调用次数，不包含 Arduino、FreeRTOS、GPIO 或真实 ESP32 驱动逻辑。
  - 使用 `python -m platformio run` 编译通过。
- 下一步建议：
  - 下一步适合建立 SensorService，把 `SensorDriver` 的 mock 读数转换为 `EVT_PAPER_PRESENT`、`EVT_TEMP_OK`、`EVT_BAT_OK` 等系统事件。

## Step 10

- 时间：2026-04-24 16:03:41
- 状态：已完成
- 结果：
  - 新增 `src/services/sensor_service.h/.cpp`，定义 `SensorSnapshot` 并实现传感器采样服务。
  - 新增 `src/tasks/task_sensor.h/.cpp`，实现 50ms 周期 SensorTask。
  - SensorService 当前通过 `SensorDriver` mock 接口读取纸张、温度、电池、充电状态和电机故障。
  - SensorService 会做基础合法性判断，并更新 `EVT_PAPER_PRESENT`、`EVT_TEMP_OK`、`EVT_BAT_OK`、`EVT_DRV_READY`。
  - `qSensor` 已从占位队列改为长度为 1 的 `SensorSnapshot` 队列，使用覆盖写入保留最新状态。
  - `SensorTask` 已纳入任务注册和健康心跳检查。
  - 默认 mock 条件下，SystemTask 可从 `IDLE` 进入 `READY`。
  - 如果 mock 配置为缺纸，`EVT_PAPER_PRESENT` 会被清除，状态机会停在 `IDLE`，不会进入 `READY`。
  - 使用 `python -m platformio run` 编译通过。
- 下一步建议：
  - 下一步适合加入错误上报服务，把缺纸、温度非法、低电压和电机故障转换为统一 `AppErrorCode` 或告警事件。

## Step 11

- 时间：2026-04-24 16:10:08
- 状态：已完成
- 结果：
  - 新增 `src/protocol/proto_crc.h/.cpp`，实现 CRC-16/CCITT-FALSE。
  - 新增 `src/protocol/proto_frame.h`，定义固定帧格式、最大帧长度和固定 payload 缓冲。
  - 新增 `src/protocol/proto_cmd.h`，定义 `PING`、`GET_INFO`、`GET_STATUS`。
  - 新增 `src/protocol/proto_parser.h/.cpp`，实现可逐字节喂入的协议解析状态机。
  - Parser 会校验 magic、协议版本、header_len、payload_len 和 CRC16，最大帧长度限制为 512 bytes。
  - 新增 `src/services/protocol_service.h/.cpp`，实现完整帧投递和命令响应打包。
  - 新增 `src/tasks/task_protocol.h/.cpp`，实现基于 Serial 轮询的协议接收任务。
  - 新增 `src/tasks/task_command.h/.cpp`，实现命令分发任务。
  - `qCommand` 已从占位队列改为 `ProtoFrame` 队列。
  - 当前不实现打印命令，不直接操作硬件。
  - 使用 `python -m platformio run` 编译通过。
- 下一步建议：
  - 下一步适合补协议响应测试脚本，自动构造 PING/GET_STATUS 帧并校验返回 CRC。

## Step 12

- 时间：2026-04-24 16:16:08
- 状态：已完成
- 结果：
  - 新增 `src/drivers/storage/drv_nvs.h/.cpp`，封装 Arduino Preferences 作为 NVS 存储入口。
  - 新增 `src/services/param_service.h/.cpp`，实现参数初始化、加载、快照读取、更新、请求保存和恢复默认。
  - 新增 `src/tasks/task_param.h/.cpp`，实现低优先级 ParamTask。
  - 参数默认值来自 `config/default_params.h`，运行时会补齐 CRC32。
  - NVS 持久化结构使用 `magic/version/length/crc32 + ParamBlock`，加载时会完整校验。
  - NVS 不可用、参数缺失、版本不匹配、长度不匹配或 CRC 错误时，都会回退默认参数，不阻塞系统启动。
  - 参数保存通过 `qParam` 请求，由 ParamTask 做 2 秒延迟合并，打印活动期间继续延迟写 Flash。
  - CommandTask 增加 `GET_PARAM`、`SET_PARAM`、`SAVE_PARAM`、`FACTORY_RESET` 命令，其中 `SET_PARAM` 当前返回 TODO。
  - 使用 `python -m platformio run` 编译通过。
  - 已按新要求创建 git commit。
- 下一步建议：
  - 下一步适合为 `GET_PARAM/SET_PARAM` 定义正式 payload 格式，避免长期使用文本响应。

## Step 13

- 时间：2026-04-24 16:21:05
- 状态：已完成
- 结果：
  - 新增 `src/common/fixed_pool.h/.cpp`，定义 `LineBuffer` 和 64 行静态 `LineBuffer` 池。
  - 新增 `src/services/print_spooler.h/.cpp`，实现行缓冲申请、提交、释放和按 job 清理。
  - `qLineFree` 和 `qLineReady` 已改为 `LineBuffer*` 队列，容量与固定池一致。
  - 启动时 `PrintSpooler_Init()` 会清空行池，并把全部 `LineBuffer` 放入 `qLineFree`。
  - 新增协议命令 `PRINT_LINE`，payload 必须为 48 bytes。
  - `PRINT_LINE` 当前只计算 `blackDotCount` 并把行放入 `qLineReady`，不会启动真实打印。
  - 队列满或无空闲行时返回 `ERR_QUEUE_FULL`。
  - 没有操作打印头、电机、VH 或 STB。
  - 使用 `python -m platformio run` 编译通过。
  - 已按要求创建 git commit。
- 下一步建议：
  - 下一步适合建立 PrintCtrlTask，只消费 `qLineReady` 做状态推进和安全检查，仍先不接真实打印头加热。

## Step 14

- 时间：2026-04-24 16:25:18
- 状态：已完成
- 结果：
  - 新增 `src/services/thermal_safety_service.h/.cpp`。
  - ThermalSafetyService 不包含 `Arduino.h`，不直接操作 GPIO。
  - 提供 `ThermalSafety_CheckCanPrint()`、`ThermalSafety_CalcPulseUs()`、`ThermalSafety_CheckLine()`。
  - 安全检查覆盖缺纸、温度无效、过温、低电压、单组黑点数和热脉冲硬上限。
  - 初始脉冲算法使用参数中的起步脉冲和最大脉冲，并按温度、电压做保守修正。
  - 当前低电压阈值使用服务内保守占位 `6500mV`，后续应迁移到 `SafetyParams` 并提升参数版本。
  - 函数只返回 `AppErrorCode` 或计算结果，不直接进入 SAFE_MODE。
  - 已在代码注释中写入手工自检用例。
  - 使用 `python -m platformio run` 编译通过。
  - 已按要求创建 git commit。
- 下一步建议：
  - 下一步适合建立 PrintCtrlTask，把 `qLineReady`、SensorSnapshot、ParamBlock 和 ThermalSafetyService 串起来，但继续先不调用真实打印头驱动。

## Step 15

- 时间：2026-04-24 16:30:20
- 状态：已完成
- 结果：
  - 新增 `src/bsp/bsp_spi.h/.cpp`，提供热敏打印头 SPI 初始化和 48 字节行数据发送入口。
  - 新增 `src/bsp/bsp_timer.h/.cpp`，提供微秒级延时封装，便于后续统一替换为更精确的硬件定时。
  - 新增 `src/drivers/thermal_head/drv_thermal_head_esp32.cpp`，实现真实 ThermalHeadDriver 的安全骨架。
  - `MP_ENABLE_HW_THERMAL_HEAD=0` 时，`GetThermalHeadDriver()` 仍返回 mock driver，debug waveform test 返回 `ERR_HW_DISABLED`。
  - `MP_ENABLE_HW_THERMAL_HEAD=1` 时才编译真实 GPIO/SPI 操作路径。
  - 真实路径中 `init()` 首先关闭全部 STB 并关闭 VH，`setVh(true)` 前会确认所有 STB inactive。
  - `pulseStbGroup()` 限制 group 为 0..5，限制脉冲不超过安全硬上限，`pulseUs=0` 不输出 STB，结束后强制关闭对应 STB。
  - SPI 初始频率为 2MHz，LAT pulse 使用 GPIO 翻转和短延时，后续必须用示波器确认。
  - 本步骤没有接入 PrintEngine，没有自动执行 waveform test。
  - 使用 `python -m platformio run` 编译通过。
  - 额外使用 `PLATFORMIO_BUILD_FLAGS=-DMP_ENABLE_HW_THERMAL_HEAD=1` 编译通过，确认真实硬件分支可编译。
  - 已按要求创建 git commit。
- 下一步建议：
  - 下一步适合建立 PrintCtrlTask 的安全调度骨架，先串联 LineBuffer、SensorSnapshot、ParamBlock 和 ThermalSafetyService，默认仍不启用真实打印头硬件。

## Step 16

- 时间：2026-04-24 16:38:44
- 状态：已完成
- 结果：
  - 新增 `src/drivers/stepper/drv_stepper_drv8833.cpp`，实现 DRV8833 步进电机真实驱动安全骨架。
  - 修改 `src/drivers/stepper/drv_stepper.h` 和 `src/drivers/stepper/drv_stepper_mock.cpp` 相关 mock 入口，默认 `MP_ENABLE_HW_STEPPER=0` 时 `GetStepperDriver()` 继续返回 mock。
  - `MP_ENABLE_HW_STEPPER=1` 时才编译真实 DRV8833 GPIO 操作路径。
  - `init()` 第一件事执行 nSLEEP low 和 AIN/BIN 全 low，随后再配置 GPIO 输出模式。
  - `wake()` 拉高 nSLEEP 后等待至少 1ms。
  - `isFault()` 按 nFAULT 低电平有效处理，检测到 fault 后立即 release 并 sleep。
  - `stepForward()` 和 `stepBackward()` 使用清晰 4-step 全步相序表，不做打印许可等业务判断。
  - 本步骤没有创建独立 task，没有接入 PrintEngineTask，也没有自动执行电机测试。
  - 使用 `python -m platformio run` 编译通过。
  - 额外使用 `PLATFORMIO_BUILD_FLAGS=-DMP_ENABLE_HW_STEPPER=1` 编译通过，确认真实硬件分支可编译。
  - 已按要求创建 git commit。
- 下一步建议：
  - 下一步适合建立 PrintEngineTask 或 PrintCtrlTask 的安全调度骨架，先以 mock driver 串联走纸/出行节奏，再决定是否开启真实电机。

## Step 17

- 时间：2026-04-24 16:48:09
- 状态：已完成
- 结果：
  - 新增 `src/app/app_print.h/.cpp`，记录打印作业状态、作业号、已打印行数和结束请求。
  - 新增 `src/services/print_service.h/.cpp`，提供打印控制消息投递、`EVT_PRINT_ACTIVE` 管理、行分组黑点统计和错误上报。
  - 新增 `src/tasks/task_print_engine.h/.cpp`，实现第一版 PrintEngineTask。
  - `qPrintCtrl` 已从占位 `uint32_t` 队列改为 `PrintControlMsg` 队列。
  - 协议层新增 `PRINT_START`、`PRINT_END`、`PRINT_CANCEL`、`FEED` 命令，`PRINT_LINE` 继续只入队行数据。
  - PrintEngineTask 消费 `qPrintCtrl` 和 `qLineReady`，所有队列等待都有 timeout。
  - 每行按 6 个 STB group 统计黑点，调用 `ThermalSafety_CheckLine()` 和 `ThermalSafety_CalcPulseUs()` 生成模拟计划。
  - Step 17 固定调用 `GetThermalHeadMockDriver()` 和 `GetStepperMockDriver()`，不调用真实热敏头或真实 DRV8833 驱动。
  - `PRINT_START` 设置 `EVT_PRINT_ACTIVE`，SystemTask 会进入 `RUNNING`；完成或取消后清除事件，状态回到 `READY` 或 `IDLE`。
  - 异常时执行 `Bsp_SetAllOutputsSafe()`、清理行队列、发送 `qError` 并设置错误事件。
  - PrintEngineTask 已纳入 heartbeat，并在 HealthService 中按关键任务检查。
  - 使用 `python -m platformio run` 编译通过。
  - 额外使用 `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_HW_THERMAL_HEAD=1 -DMP_ENABLE_HW_STEPPER=1"` 编译通过，确认真实驱动可编译但 PrintEngineTask 仍固定 mock。
  - 已按要求创建 git commit。
- 下一步建议：
  - 下一步适合补一个主机侧 Python 测试脚本，自动构造 `PRINT_START -> PRINT_LINE* -> PRINT_END` 帧并校验响应与日志。

## Step 18

- 时间：2026-04-24 16:56:30
- 状态：已完成
- 结果：
  - 修改 `src/tasks/task_print_engine.cpp`，加入真实打印硬件接入保护层。
  - 修改 `src/services/print_service.h/.cpp`，新增真实硬件路径启用判断和真实打印许可检查。
  - 修改 `src/services/thermal_safety_service.cpp`，将 `motorFault` 纳入禁止加热条件。
  - 修改 `src/drivers/thermal_head/drv_thermal_head.h` 及 mock/esp32 实现，让 `latch/setVh/pulseStbGroup/allStbOff/setSafe` 返回 `bool`，便于 PrintEngine 捕获失败。
  - 默认宏关闭时仍走 mock 打印路径，不调用真实 ThermalHeadDriver 或真实 StepperDriver。
  - 真实路径要求 `MP_ENABLE_HW_THERMAL_HEAD=1` 且 `MP_ENABLE_HW_STEPPER=1`，避免真实加热但电机仍为 mock。
  - 真实路径每行都会重新读取最近 `SensorSnapshot`，并检查系统状态为 `RUNNING`、传感器有效、有纸、温度 OK、电池 OK、无电机故障。
  - 每行真实流程按 `shiftLine48Bytes -> latch -> setVh(true) -> 逐组 STB pulse -> allStbOff -> setVh(false) -> stepper 走纸 -> free line buffer` 执行。
  - `groupBlackDots=0` 或 `pulseUs=0` 时跳过对应 STB pulse。
  - 任意步骤失败时执行 `allStbOff`、`setVh(false)`、`stepper.release()`、`Bsp_SetAllOutputsSafe()`、投递 `qError` 并清理排队行。
  - 使用 `python -m platformio run` 编译通过。
  - 额外使用 `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_HW_THERMAL_HEAD=1 -DMP_ENABLE_HW_STEPPER=1"` 编译通过。
  - 已按要求创建 git commit。
- 下一步建议：
  - 下一步适合补主机侧协议测试脚本，并在真实硬件测试前先做无纸/缺纸/过温/低电压保护路径验证。

## Step 19

- 时间：2026-04-24 17:06:03
- 状态：已完成
- 结果：
  - 新增 `src/services/error_service.h/.cpp`，定义 `ErrorEvent` 和统一错误上报服务。
  - `ErrorEvent` 包含 `code`、`severity`、`module`、`timestamp` 和 `detail`。
  - `ErrorService` 提供 `Error_Report()`、`Error_GetLast()`、`Error_ClearRecoverable()`、`Error_IsSafeModeRequired()`。
  - `qError` 已从裸 `AppErrorCode` 队列改为 `ErrorEvent` 队列。
  - PrintService 和 MonitorTask 已改为通过 `Error_Report()` 上报错误，不直接写 `qError`。
  - SystemTask 按严重级别处理错误：`WARN` 只记录并清除可恢复待处理位，`ERROR` 进入 `ERROR` 后允许下一轮 `RECOVERY`，`FATAL` 立即执行 `Bsp_SetAllOutputsSafe()` 并进入 `SAFE_MODE`。
  - SAFE_MODE 下协议白名单只允许 `GET_STATUS`、`GET_ERROR`、`CLEAR_ERROR`、`SELF_TEST`、`REBOOT`、`FACTORY_RESET`。
  - `PRINT_START` 等打印命令在 SAFE_MODE 下会返回 `NACK_SAFE_MODE`。
  - 缺纸、过温、低电压和 DRV fault 已在 PrintService 上报详情中明确区分。
  - 使用 `python -m platformio run` 编译通过。
  - 已按要求创建 git commit。
- 下一步建议：
  - 下一步适合执行 Step 20，将真实 Watchdog 接入 MonitorTask，并确保只有 MonitorTask 能喂狗。

## Step 20

- 时间：2026-04-24 17:13:31
- 状态：已完成
- 结果：
  - 新增 `src/bsp/bsp_watchdog.h/.cpp`，封装 ESP32 Task Watchdog 初始化、MonitorTask 喂狗和复位原因读取。
  - 修改 `src/tasks/task_monitor.cpp`，仅在 `MP_ENABLE_WDT=1` 时初始化真实 WDT，并且只有关键任务心跳正常、无 FATAL/SAFE_MODE 请求时才喂狗。
  - 修改 `src/services/health_service.cpp`，关键任务集合保留 SYSTEM、SENSOR、MONITOR、PRINT_CTRL，LOGGER 不再触发复位。
  - 修改 `src/config/safety_limits.h`，新增 `SAFETY_WDT_TIMEOUT_MS = 5000`。
  - 关键任务超时时会先执行 `Bsp_SetAllOutputsSafe()`，再通过 `Error_Report(ERR_WDT_TASK_TIMEOUT, ...)` 上报 FATAL，并停止喂狗。
  - `Bsp_WatchdogResetReasonText()` 已实现复位原因读取，后续可用于启动日志和故障分析。
- 下一步建议：
  - 下一步适合执行 Step 21，增加 Factory Test / Debug Commands，并确认危险测试默认受硬件宏和安全条件保护。

## Step 21

- 时间：2026-04-24 17:24:04
- 状态：已完成
- 结果：
  - 新增 `src/app/app_factory_test.h/.cpp`，把工厂测试和调试动作集中到 app 层。
  - 新增协议命令：`SAFE_OFF`、`SENSOR_TEST`、`MOTOR_TEST`、`HEAD_SHIFT_TEST`、`HEAD_STB_TEST`、`ENTER_SAFE_MODE`。
  - `SAFE_OFF` 在协议层最高优先级处理，任何状态下都会先执行 `Bsp_SetAllOutputsSafe()`。
  - `SENSOR_TEST` 返回当前 SensorSnapshot，便于无硬件阶段验证 mock 传感器状态。
  - `MOTOR_TEST` 在 `MP_ENABLE_HW_STEPPER=0` 时返回 `ERR_HW_DISABLED`，启用后也限制最大步数并低速执行。
  - `HEAD_SHIFT_TEST` 只允许 48 字节 shift 和 latch，不打开 VH，不输出 STB。
  - `HEAD_STB_TEST` 在 `MP_ENABLE_HW_THERMAL_HEAD=0` 时返回 `ERR_HW_DISABLED`；启用后仍要求 paper/temp/bat 条件通过，并且 pulseUs 受安全硬上限裁剪，测试期间 VH 保持关闭。
  - `ENTER_SAFE_MODE` 会关闭危险输出并通过 ErrorService/SystemApp 锁存 SAFE_MODE。
  - `REBOOT` 进入 FactoryTest 入口，重启前先记录日志并关闭危险输出。
  - 使用 `python -m platformio run` 编译通过。
  - 使用 `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_HW_THERMAL_HEAD=1 -DMP_ENABLE_HW_STEPPER=1"` 额外编译通过。
- 下一步建议：
  - 下一步适合执行 Step 22，补充主机侧测试脚本和验收文档，用脚本实际发送 `STATUS`、`SAFE_OFF`、`SENSOR_TEST` 帧。

## Step 22

- 时间：2026-04-24 17:28:53
- 状态：已完成
- 结果：
  - 新增 `tools/send_frame.py`，可构造 MiniPrinterRTOS UART 协议帧。
  - `send_frame.py` 支持 `ping`、`status`、`safe-off`、`sensor-test` 和 `print-test`。
  - `print-test` 会发送 `PRINT_START -> PRINT_LINE* -> PRINT_END`，行数据可从文件读取或按 pattern 生成。
  - 新增 `tools/make_test_print.py`，可生成 `blank`、`checker`、`vertical_lines`、`low_density_black` 测试图。
  - 新增 `docs/test_plan.md`，覆盖编译、启动、GPIO 安全态、UART 协议、Sensor mock、Queue 满、SAFE_MODE、Watchdog、打印头波形、电机低速测试。
  - 新增 `docs/acceptance.md`，记录可量化验收标准。
  - 使用 `python -m platformio run` 编译通过。
  - `python tools/send_frame.py --help` 和 `python tools/make_test_print.py --help` 正常运行。
  - `send_frame.py ping --dry-run` 和 `print-test --dry-run --lines 1` 可正常构造帧。
  - `make_test_print.py --pattern vertical_lines --lines 2` 生成 96 字节输出，符合每行 48 字节。
- 下一步建议：
  - 后续应在真实串口上执行 `send_frame.py status/safe-off/sensor-test`，再进入硬件宏开启后的分阶段波形测试。

## Step 23

- 时间：2026-04-24 18:05:51
- 状态：已完成
- 结果：
  - 分析当前仓库状态，确认项目已经按 `app/bsp/common/config/drivers/protocol/rtos/services/tasks` 分层初步落地。
  - 新增 `docs/项目结构.md`，记录当前分析、验证结果、主要风险、完整目录结构和下一步建议。
  - 同步更新 `docx/项目结构.md`，保持现有文档目录中的结构快照可直接查看。
  - 修正 `docs/architecture_notes.md` 的“当前状态”，删除“仍只有 `src/main.cpp` 模板文件”的过期描述。
  - 使用 `python -m platformio run` 编译通过。
  - 使用 `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_HW_THERMAL_HEAD=1 -DMP_ENABLE_HW_STEPPER=1"` 额外编译通过。
  - `python tools/send_frame.py --help` 和 `python tools/make_test_print.py --help` 正常运行。
- 下一步建议：
  - 下一步适合接入真实串口执行 `ping/status/safe-off/sensor-test`，再进入 GPIO 波形和传感器真实读取验证。

## Step 24

- 时间：2026-04-26 18:32:41
- 状态：已完成
- 结果：
  - 新增 `docs/创建项目情况.md`，按当前源码列出项目已经实现的打印机功能。
  - 文档明确区分了软件闭环、mock 驱动、受宏保护的真实硬件骨架和仍未完成的真实打印验证。
  - 同步更新 `docs/项目结构.md`，把 `docs/创建项目情况.md` 加入正式项目结构。
  - 使用 `python -m platformio run` 编译通过。
  - 使用 `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_HW_THERMAL_HEAD=1 -DMP_ENABLE_HW_STEPPER=1"` 额外编译通过。
- 下一步建议：
  - 下一步适合接入真实串口执行 `ping/status/safe-off/sensor-test`，确认协议收发和安全关闭命令，再进入 GPIO 波形验证。

## Step 25

- 时间：2026-04-26 18:56:00
- 状态：已完成
- 结果：
  - 读取 `项目背景/` 下的原理图、BOM、ESP32 GPIO 映射图和关键器件规格书。
  - 新增 `docs/硬件匹配分析.md`，对照当前固件列出已匹配项和未匹配项。
  - 确认硬件方向与当前固件架构匹配，但真实 GPIO、真实传感器、电池 ADC、按键、充电状态、电机节奏和热模型仍未完整匹配。
  - 发现当前文档和代码缺少 `EQD` 电池电压 ADC 网络。
  - 同步更新 `docs/项目结构.md`，把 `docs/硬件匹配分析.md` 加入正式项目结构。
- 下一步建议：
  - 下一步适合先补 `docs/pin_map.md` 和 `src/bsp/bsp_pins.h` 的真实 GPIO 映射，并新增 `EQD`，但继续保持真实硬件宏默认关闭。

## Step 26

- 时间：2026-04-26 18:56:49
- 状态：已完成
- 结果：
  - 根据 `项目背景/ESP32GPIO映射图.png` 整理出 `项目背景/ESP32GPIO映射图.md`。
  - 文档包含板卡基础信息、图片标签含义、电源/特殊引脚、左侧排针 GPIO、右侧排针 GPIO 和本项目选引脚建议。
  - 同步更新 `docs/项目结构.md` 与 `docx/项目结构.md`，把新增 Markdown 资料加入项目结构。
- 下一步建议：
  - 后续分配真实热敏头、电机、传感器引脚前，应对照该 Markdown 与 `docs/pin_map.md`，优先避开 BOOT、USB、UART0、JTAG 等敏感引脚。

## Step 27

- 时间：2026-04-26 19:12:13
- 状态：已完成
- 结果：
  - 根据 `docs/硬件匹配分析.md` 新增 `docs/待办任务.md`。
  - `docs/待办任务.md` 把固件和硬件匹配工作拆成 12 个后续任务，并为每个任务写出可直接交给 Codex 执行的提示词、目标和验收标准。
  - 新增 `docs/需要补充的信息.md`，列出需要用户补充的信息、Codex 不确定的问题，以及必须由用户亲自完成的硬件测量和真实测试任务。
  - 同步更新 `docs/项目结构.md` 与 `docx/项目结构.md`。
- 下一步建议：
  - 下一步可从 `docs/待办任务.md` 的 Task 01 开始，先补 `docs/pin_map.md` 和 `src/bsp/bsp_pins.h` 的真实 GPIO 映射，并继续保持真实硬件宏默认关闭。

## Step 28

- 时间：2026-04-26 20:29:20
- 状态：已完成
- 结果：
  - 整理用户已补充的硬件信息，新增 `docs/硬件补充信息整理.md`。
  - 将 `docs/需要补充的信息.md` 缩减为剩余待补充事项，避免重复追问已确认内容。
  - 根据补充信息小幅修正 `docs/待办任务.md`：GPIO 连通性已复核、GPIO35~40 可用、GPIO38 不再阻塞 STB3、`PAPER_N/TM1` 第一阶段保守处理、调试验证改为 WiFi 测试 API 方向。
  - 同步更新 `docs/项目结构.md` 与 `docx/项目结构.md`。
- 下一步建议：
  - 下一步可执行 `docs/待办任务.md` 的 Task 01，将已复核的 GPIO 映射写入 `docs/pin_map.md` 和 `src/bsp/bsp_pins.h`。
