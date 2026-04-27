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

## Step 29

- 时间：2026-04-26 20:52:50
- 状态：已完成
- 结果：
  - 按“打印文件传输和启动打印必须使用 WiFi”的原则重构 `docs/待办任务.md`。
  - 在 `docs/待办任务.md` 中新增 WiFi API 总原则和 `/api/v1` 初始接口设计，覆盖状态展示、传感器、电池、错误、参数、安全关闭、工厂测试、打印文件上传、启动打印、取消打印和走纸。
  - 将原先以串口/`send_frame.py` 为主的任务调整为 WiFi API 正式控制面；UART 仅保留为临时底层调试参考，不再作为打印文件传输或启动打印方案。
  - 同步修正 `docs/需要补充的信息.md`，将 WiFi 待补充项改为连接方式、配置方式和 API 鉴权策略。
- 下一步建议：
  - 下一步应优先执行 `docs/待办任务.md` 的 Task 01，建立 WiFi API 基础框架，再做打印文件上传和启动打印 API。

## Step 30

- 时间：2026-04-26 21:20:08
- 状态：已完成
- 对应任务：`docs/待办任务.md` Task 01：建立 WiFi API 基础框架
- 结果：
  - 新增 `src/services/wifi_api_service.h/.cpp`，作为 HTTP/JSON WiFi API 服务入口。
  - 新增 `src/tasks/task_wifi_api.h/.cpp`，在 `MP_ENABLE_WIFI=1` 时创建 FreeRTOS 轮询任务；默认关闭时不占用任务栈。
  - `src/config/project_features.h` 新增 WiFi STA/AP fallback 配置宏，仍然不把 SSID/密码写入仓库。
  - `src/main.cpp` 接入 `TaskWifiApi_Create()`，保持业务逻辑不进入 `loop()`。
  - 已实现最小 API：`GET /api/v1/info`、`GET /api/v1/status`、`POST /api/v1/safe-off`。
  - API 返回统一 JSON 字段 `ok`、`code`、`message`，并明确 WiFi API 是正式产品控制面。
  - 本阶段不实现打印文件上传，不开启真实热敏头或电机宏。
- 验证：
  - `python -m platformio run` 通过。
  - `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_WIFI=1" python -m platformio run` 通过。
- 下一步建议：
  - 执行 Task 02，新增打印文件 WiFi 上传服务，先只支持 384 点宽 raw 数据。

## Step 31

- 时间：2026-04-26 21:20:08
- 状态：已完成
- 对应任务：`docs/待办任务.md` Task 02：设计并实现打印文件 WiFi 上传
- 结果：
  - 新增 `src/services/print_file_service.h/.cpp`，使用固定 RAM 槽管理 WiFi 上传的 raw 打印文件。
  - 当前最多保留 2 个上传文件，每个文件最多 64 行 / 3072 字节。
  - 支持 `POST /api/v1/print/files` 创建上传会话，要求传入 `size` 和 `crc32`。
  - 支持 `PUT /api/v1/print/files/{file_id}/chunks/{index}` 上传分片，分片偏移按 512 字节计算。
  - 支持 `POST /api/v1/print/files/{file_id}/complete` 校验总大小、48 字节行对齐和 CRC32。
  - 支持 `GET /api/v1/print/files` 列表和 `DELETE /api/v1/print/files/{file_id}` 删除。
  - 新增 `docs/wifi_api.md`，记录最大文件大小、分片大小和错误返回格式。
- 验证：
  - `python -m platformio run` 通过。
  - `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_WIFI=1" python -m platformio run` 通过。
- 下一步建议：
  - 执行 Task 03，把 `COMPLETE` 状态的 `file_id` 接入 WiFi 启动打印、当前打印查询和取消 API。

## Step 32

- 时间：2026-04-26 21:29:25
- 状态：已完成
- 对应任务：`docs/待办任务.md` Task 03：实现 WiFi 启动打印和任务控制
- 结果：
  - `PrintFileService` 新增 `PrintFileService_QueueForPrint()`，把已完成上传的 raw 文件逐行复制进现有 `PrintSpooler`。
  - 新增 `POST /api/v1/print/jobs`，根据 `file_id` 创建打印任务，第一阶段只支持 `copies=1`。
  - 新增 `GET /api/v1/print/jobs/current`，返回当前 `job_id`、`file_id`、状态、已打印行数和错误码。
  - 新增 `POST /api/v1/print/jobs/current/cancel`，复用 `PrintService_RequestCancel()` 取消当前任务。
  - 新增 `POST /api/v1/feed`，复用 `PrintService_RequestFeed()` 并限制 `steps=1..200`。
  - `SAFE_MODE` 下禁止启动打印和走纸，避免危险动作通过 WiFi API 触发。
  - HTTP 层仍只负责收发 JSON 和投递服务请求，不直接操作打印头、电机、VH 或 STB。
- 验证：
  - `python -m platformio run` 通过。
  - `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_WIFI=1" python -m platformio run` 通过。
- 下一步建议：
  - 执行 Task 04，补齐传感器、电池、错误、参数、健康和工厂测试等 WiFi 状态/控制 API。

## Step 33

- 时间：2026-04-26 21:34:32
- 状态：已完成
- 对应任务：`docs/待办任务.md` Task 04：补全 WiFi 状态展示和控制 API
- 结果：
  - 新增 `GET /api/v1/sensors` 和 `GET /api/v1/battery`。
  - 新增 `GET /api/v1/error` 和 `POST /api/v1/error/clear`。
  - 新增 `GET /api/v1/params`、`PATCH /api/v1/params`、`POST /api/v1/params/save`、`POST /api/v1/params/factory-reset`。
  - 新增 `POST /api/v1/self-test`、`POST /api/v1/reboot`、`POST /api/v1/safe-mode`。
  - 新增 `GET /api/v1/health` 和 `GET /api/v1/logs/recent`。
  - `LogService` 新增最近日志快照接口，便于 WiFi API 返回最近 8 条日志。
  - 参数 PATCH 第一阶段只开放安全白名单字段，并做范围校验；保存仍通过 `ParamService` 请求 ParamTask。
  - `SAFE_MODE` 下禁止参数修改、启动打印和走纸；状态/错误/健康等只读接口仍可访问。
- 验证：
  - `python -m platformio run` 通过。
  - `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_WIFI=1" python -m platformio run` 通过。
- 下一步建议：
  - 执行 Task 05，补全真实 GPIO 映射文档和 BSP 引脚常量。

## Step 34

- 时间：2026-04-26 21:34:32
- 状态：已完成
- 对应任务：`docs/待办任务.md` Task 05：补全真实 GPIO 映射文档和 BSP 引脚常量
- 结果：
  - 更新 `docs/pin_map.md`，把用户已复核连通性的 MCU 网络到 GPIO 映射整理成正式表格。
  - 更新 `src/bsp/bsp_pins.h`，写入 PAPER_N、G_NFAULT、G_NSLEEP、G_KEY、BAT_STAT、EQD、G_VH、LAT、DI、CLK、AIN1、AIN2、BIN1、BIN2、TM1、STB1~STB6 的 constexpr 引脚常量。
  - 新增 `PIN_EQD`，用于后续真实电池电压 ADC 读取。
  - 每个危险或易混淆网络旁补充了用途、有效电平或安全风险注释。
  - 未开启真实硬件功能宏，未修改打印头、电机业务逻辑。
- 验证：
  - `python -m platformio run` 通过。
- 下一步建议：
  - 执行 Task 06，加入真实传感器驱动骨架，优先接入 EQD 和 G_NFAULT。

## Step 35

- 时间：2026-04-26 21:34:32
- 状态：已完成
- 对应任务：`docs/待办任务.md` Task 06：实现真实传感器驱动骨架
- 结果：
  - `src/config/project_features.h` 新增 `MP_ENABLE_HW_SENSORS`，默认 `0`。
  - 新增 `src/drivers/sensors/drv_sensors_esp32.cpp`，仅在 `MP_ENABLE_HW_SENSORS=1` 时接管 `GetSensorDriver()`。
  - `EQD` 按 220k/100k 分压约 3.2 倍换算 `batteryMv`，保留 ADC 校准 TODO。
  - `G_NFAULT` 按低有效读取 `motorFault`。
  - `PAPER_N` 第一阶段按用户要求固定返回 `paperPresent=true`，代码中保留 TODO。
  - `TM1` 第一阶段返回保守室温占位，不宣称真实温度保护已完成，代码中保留 TODO。
  - `BAT_STAT` 在 IP2326 语义确认前返回 `UNKNOWN`，不猜测充电状态。
  - `GET /api/v1/info` 增加 `hw_sensors` 功能开关展示。
- 验证：
  - `python -m platformio run` 通过。
  - `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_HW_SENSORS=1" python -m platformio run` 通过。
- 下一步建议：
  - 执行 Task 07，准备 WiFi 工厂测试 API 和空载波形验证文档。

## Step 36

- 时间：2026-04-26 21:34:32
- 状态：已完成
- 对应任务：`docs/待办任务.md` Task 07：验证安全输出和 GPIO 空载波形
- 结果：
  - 新增 `POST /api/v1/factory/head-shift-test`，请求体必须是 48 字节 raw，只执行 shift/latch，保持 VH 关闭。
  - 新增 `POST /api/v1/factory/head-stb-test`，按 `group` 和 `pulse_us` 触发单组 STB 空载脉冲，底层仍由 `FactoryTest_HeadStbTest()` 保证 VH 关闭。
  - 新增 `docs/硬件空载波形验证.md`，列出 safe-off、DI/CLK/LAT、STB1~STB6、G_VH 的测点、期望和用户实测记录表。
  - 文档明确：未验证前不得接入真实加热打印。
- 验证：
  - `python -m platformio run` 通过。
  - `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_HW_THERMAL_HEAD=1" python -m platformio run` 通过。
- 下一步建议：
  - 执行 Task 08，接入真实步进电机低速测试 WiFi API。

## Step 37

- 时间：2026-04-26 21:34:32
- 状态：已完成
- 对应任务：`docs/待办任务.md` Task 08：接入真实步进电机低速测试
- 结果：
  - 新增 `POST /api/v1/factory/motor-test?steps=<n>`，限制 `steps=1..200`。
  - API 复用 `FactoryTest_MotorTest()`，真实电机动作仍受 `MP_ENABLE_HW_STEPPER=1` 控制。
  - 默认硬件宏关闭时不动作，返回结构化 JSON 错误。
  - `SAFE_MODE` 下禁止 motor-test。
  - 更新 `docs/需要补充的信息.md`，保留真实走纸方向和相序待用户低速实测确认。
- 验证：
  - `python -m platformio run` 通过。
  - `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_HW_STEPPER=1" python -m platformio run` 通过。
- 下一步建议：
  - 执行 Task 09，把走纸节奏从占位推进到 2 步/行的可解释模型。

## Step 38

- 时间：2026-04-26 21:34:32
- 状态：已完成
- 对应任务：`docs/待办任务.md` Task 09：按机芯规格闭环走纸节奏
- 结果：
  - `ParamBlock` 版本升到 2，`MotorParams` 新增 `stepsPerPrintLine`。
  - 默认走纸模型按 `8 dots/mm` 与 `0.0625mm/step` 推导为每打印一行走 2 步。
  - 默认步进间隔更新为起步 5000us、常速 3000us、较快 2000us 的保守占位。
  - `PrintEngineTask` 每行打印后按 `params.motor.stepsPerPrintLine` 执行走纸，不再固定只走 1 步。
  - `GET /api/v1/params` 返回 motor 走纸节奏参数。
  - 新增 `docs/走纸节奏模型.md` 说明点距、步距和行进给关系。
- 验证：
  - `python -m platformio run` 通过。
  - `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_HW_STEPPER=1" python -m platformio run` 通过。
- 下一步建议：
  - 执行 Task 10，按规格书完善保守热敏头热模型。

## Step 39

- 时间：2026-04-26 21:53:40
- 状态：已完成
- 对应任务：`docs/待办任务.md` Task 10：按规格书完善热敏头热模型
- 结果：
  - `ThermalSafety_CalcPulseUs()` 改为可解释的保守热模型。
  - 保留单组最大 `64 dots` 同时加热限制。
  - 新增 `VH=7200mV`、25C 满载典型 `Ton=490us`、3000us 行加热预算、500us/组预算等明确常量。
  - 脉宽计算按黑点负载、温度、电压依次补偿，最终仍受参数上限和固件硬上限限制。
  - 新增 `docs/热敏头热模型.md`，说明模型公式、常量和仍需用户实测的项目。
- 验证：
  - `python -m platformio run` 通过。
  - `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_HW_THERMAL_HEAD=1" python -m platformio run` 通过。
- 下一步建议：
  - 执行 Task 11，实现按键输入状态读取与 WiFi API 展示。

## Step 40

- 时间：2026-04-26 21:58:47
- 状态：已完成
- 对应任务：`docs/待办任务.md` Task 11：实现按键输入服务
- 结果：
  - 新增 `KeyService`，周期读取 `G_KEY`，支持 40ms 去抖、短按和长按事件记录。
  - `TaskSensor` 复用现有 50ms 周期轮询按键，不新增 FreeRTOS 任务。
  - 新增 `GET /api/v1/key`，并在 `GET /api/v1/status` 中加入 `key` 字段。
  - 当前按 `INPUT_PULLUP`、低有效假设处理，API 返回 `active_low_assumed=true`。
  - 新增 `docs/按键输入服务.md`，并在 `docs/需要补充的信息.md` 记录 `G_KEY` 实测项。
- 验证：
  - `python -m platformio run` 通过。
- 下一步建议：
  - 执行 Task 12，完善电池状态与低电压策略展示。

## Step 41

- 时间：2026-04-26 22:03:34
- 状态：已完成
- 对应任务：`docs/待办任务.md` Task 12：完善 IP2326 充电状态与电池策略
- 结果：
  - 新增统一低电压保护线 `SAFETY_DEFAULT_LOW_BATTERY_STOP_MV=6500mV`。
  - `SensorService` 根据 `batteryMv` 维护 `EVT_BAT_OK` 和 `EVT_LOW_POWER`。
  - `SensorSnapshot` 增加 `chargeStatus` 原始枚举值，WiFi API 可区分 `UNKNOWN` 和真实充电状态。
  - `GET /api/v1/status`、`GET /api/v1/sensors`、`GET /api/v1/battery` 增加电池可读字段。
  - 真实步进电机工厂测试在硬件宏开启时增加低电压和电机故障检查。
  - 新增 `docs/电池策略.md`，明确 `EQD`、`BAT_STAT` 和低电压保护边界。
- 验证：
  - `python -m platformio run` 通过。
  - `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_HW_SENSORS=1" python -m platformio run` 通过。
  - `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_HW_STEPPER=1" python -m platformio run` 通过。
- 下一步建议：
  - 执行 Task 13，补齐参数 API 白名单和校验。

## Step 42

- 时间：2026-04-26 22:08:29
- 状态：已完成
- 对应任务：`docs/待办任务.md` Task 13：完善参数 API 和参数校验
- 结果：
  - `PATCH /api/v1/params` 改为严格白名单，未知查询参数返回 `PARAM_NOT_ALLOWED`。
  - 参数修改先写入临时副本，全部校验通过后才调用 `Param_Update()`。
  - 白名单扩展到热安全、低电压保护开关、电机走纸节奏和 `max_frame_length`。
  - 增加组合约束：`temp_resume_c < temp_stop_c`、`heat_start_us <= heat_max_us`、`fast <= run <= start`。
  - `GET /api/v1/params` 返回更完整的 safety/comm 字段。
  - 新增 `docs/参数API.md` 记录允许字段、范围、调用方式和错误策略。
- 验证：
  - `python -m platformio run` 通过。
  - `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_WIFI=1" python -m platformio run` 通过。
- 下一步建议：
  - 执行 Task 14，补 WiFi API 主机侧测试脚本。

## Step 43

- 时间：2026-04-26 22:13:14
- 状态：已完成
- 对应任务：`docs/待办任务.md` Task 14：补 WiFi API 自动化测试和主机侧测试脚本
- 结果：
  - 新增 `tools/api_client.py`，使用 Python 标准库访问 `/api/v1` WiFi API。
  - 覆盖 info/status/key/sensors/battery/error/params/health/logs/safe-off。
  - 支持 raw 打印文件上传、分片 PUT、complete、按 `file_id` 启动打印、取消、走纸。
  - 支持 motor-test、head-shift-test、head-stb-test 等工厂测试 API。
  - `self-test` 为本地 dry-run，不访问设备，用于验证 raw 行宽、分片和 CRC32 逻辑。
  - 更新 `docs/test_plan.md` 和 `docs/acceptance.md`，明确正式验收应以 WiFi API 为主。
- 验证：
  - `python -m platformio run` 通过。
  - `python tools/api_client.py --help` 通过。
  - `python tools/api_client.py self-test` 通过。
  - `python tools/make_test_print.py --help` 通过。
- 下一步建议：
  - 执行 Task 15，补真实硬件分阶段验收记录。

## Step 44

- 时间：2026-04-26 22:16:38
- 状态：已完成
- 对应任务：`docs/待办任务.md` Task 15：真实硬件分阶段验收记录
- 结果：
  - 新增 `docs/硬件分阶段验收记录.md`。
  - 验收阶段覆盖 WiFi API 连接、状态查询、安全态、传感器、电池、safe-off、空载波形、电机低速、文件上传、mock 打印和低密度短行真实打印。
  - 每个阶段都包含前置条件、API/命令示例、期望结果、用户实测结果和是否允许进入下一阶段。
  - 明确任何失败都必须停止后续危险测试，并执行 safe-off 或断电。
- 验证：
  - `python -m platformio run` 通过。
- 下一步建议：
  - 等用户按文档完成真实设备逐阶段实测记录。

## Step 45

- 时间：2026-04-27 10:19:24
- 状态：已完成
- 对应任务：结合 `项目背景/` 真实硬件资料，核对当前代码是否匹配，并更新 `docs/项目情况.md`
- 结果：
  - 重新核对 `项目背景/` 下 JX-2R-01 机芯规格书、BOM、原理图、DRV8833、IP2326、TPS563200 和 ESP32 GPIO 映射整理。
  - 更新 `docs/项目情况.md`，明确当前结论：软件架构与真实硬件方向匹配，但真实稳定打印仍缺少 WiFi 联机、GPIO 波形、传感器校准、电机方向和低密度短行打印验证。
  - 在项目情况中列出当前已有功能：安全启动、RTOS 任务、GPIO 映射、WiFi API、UART 兼容协议、打印文件上传、打印执行链路、传感器/电池/按键、安全策略、参数/NVS 和主机侧工具。
  - 标出未完全匹配项：`PAPER_N`、`TM1`、`BAT_STAT`、真实热敏头、真实步进电机、电池 ADC 校准、WiFi 实机验收和固件侧自动化测试。
  - 同步修正 `docs/项目结构.md` 和 `docx/项目结构.md` 中对 `docs/项目情况.md` 与下一步建议的描述。
- 验证：
  - `python -m platformio run` 通过。
  - `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_WIFI=1" python -m platformio run` 通过。
  - `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_HW_SENSORS=1" python -m platformio run` 通过。
  - `PLATFORMIO_BUILD_FLAGS="-DMP_ENABLE_HW_THERMAL_HEAD=1 -DMP_ENABLE_HW_STEPPER=1" python -m platformio run` 通过。
  - `python tools/api_client.py self-test` 通过。
  - `python tools/make_test_print.py --help` 通过。
- 下一步建议：
  - 按 `docs/硬件分阶段验收记录.md` 先填写 WiFi API 联机、安全态、sensors、battery 和 safe-off 实测结果。

## Step 46

- 时间：2026-04-27 10:48:59
- 状态：已完成
- 对应任务：创建 `docs/项目启动说明.md`，说明当前项目使用方式、如何开始打印、如何传输文件
- 结果：
  - 新增 `docs/项目启动说明.md`。
  - 明确当前正式控制面是 WiFi API，UART/串口只用于烧录、日志和临时底层调试。
  - 写清楚 `MP_ENABLE_WIFI=1` 编译烧录方式、AP fallback 和局域网 STA 两种启动方式。
  - 写清楚 raw 打印文件格式：384 点宽、48 字节/行、文件大小必须为 48 的整数倍、当前最大 3072 bytes。
  - 写清楚文件传输流程：生成 raw -> `upload` 创建会话和分片上传 -> `complete` 校验 CRC32 -> 取得 `file_id`。
  - 写清楚启动打印流程：`start <file_id>`，以及 `status`、`cancel`、`safe-off` 等常用命令。
  - 区分 mock 打印和真实打印，强调真实打印必须先完成安全态、shift/latch、STB、电机和低密度短行验证。
  - 同步更新 `docs/项目结构.md` 和 `docx/项目结构.md`。
- 验证：
  - `python tools/api_client.py self-test` 通过。
  - `python tools/api_client.py upload --help` 通过。
  - `python tools/api_client.py start --help` 通过。
  - `python tools/make_test_print.py --pattern low_density_black --lines 4 -o <temp> --summary` 通过，输出文件为 192 bytes。
  - `python -m platformio run` 通过。
- 下一步建议：
  - 按新文档先跑 mock 打印闭环，再进入真实硬件分阶段验收。

## Step 47

- 时间：2026-04-27 11:41:51
- 状态：已完成
- 对应任务：检查并更新 `docs/硬件匹配分析.md`
- 结果：
  - 确认旧版 `docs/硬件匹配分析.md` 已不是最新项目内容：仍写着 GPIO 全部未分配、缺少 `EQD`、按键未实现、真实传感器完全 mock。
  - 根据当前 `src/bsp/bsp_pins.h`、`docs/pin_map.md`、真实传感器骨架、按键服务、WiFi API、热敏头和 DRV8833 驱动骨架重写硬件匹配分析。
  - 新文档明确：GPIO 映射、WiFi API、上传/启动打印、按键、电池 ADC 骨架、真实热敏头/电机骨架已落地。
  - 新文档保留风险边界：`PAPER_N`、`TM1`、`BAT_STAT`、电机方向、热模型、真实打印效果仍需实测和校准。
- 验证：
  - `docs/硬件匹配分析.md` 读回检查通过。
  - `python -m platformio run` 通过。
- 下一步建议：
  - 按 `docs/硬件分阶段验收记录.md` 做实物验收，不要跳过空载波形直接真实加热打印。

## Step 48

- 时间：2026-04-27 14:21:06
- 状态：已完成
- 对应任务：分析串口崩溃日志并修复启动重启循环
- 结果：
  - 确认 `ESP-ROM`、`rst`、`boot`、`load`、`entry` 是正常 ESP32-S3 ROM/bootloader 启动日志。
  - 确认 `Stack canary watchpoint triggered (task_log)` 是异常，代表 `task_log` 栈溢出，是反复重启的直接原因。
  - 将所有任务栈常量从 `StackWords` 语义修正为 `StackBytes`，并按 ESP32 Arduino `xTaskCreate()` 字节单位调大。
  - `task_log`、`task_system`、`task_monitor`、`task_sensor`、`task_command`、`task_protocol`、`task_param` 调整为 4096 bytes。
  - `task_print_engine` 和 `task_wifi_api` 调整为 8192 bytes。
  - 修复 `DrvNvs_Init()` 第一次启动只读打开 NVS 命名空间导致 `NOT_FOUND` 的问题，改为初始化时创建命名空间。
  - 更新 `docs/项目启动说明.md`，解释启动日志、Guru Meditation 和 NVS NOT_FOUND 的含义。
- 验证：
  - `python -m platformio run` 通过。
- 下一步建议：
  - 重新烧录后观察串口是否不再出现 `Guru Meditation Error` 和重启循环。

## Step 49

- 时间：2026-04-27 14:37:42
- 状态：已完成
- 对应任务：将当前调试 WiFi 配置写入项目，减少每次烧录时的命令行硬编码
- 结果：
  - 新增 `src/config/project_config.h`，通过项目配置覆盖入口启用 `MP_ENABLE_WIFI=1`。
  - 写入当前调试热点 `IQOO10`、连接密码和 `15000ms` 连接超时。
  - 保持真实热敏头、电机、传感器等危险硬件宏关闭，普通调试固件仍不会真实加热或走纸。
  - 更新 `docs/项目启动说明.md`，说明现在可直接 `python -m platformio run -t upload` 烧录 WiFi API 固件。
  - 同步更新 `docs/项目结构.md` 和 `docx/项目结构.md`，记录新增配置文件。
- 验证：
  - `python -m platformio run` 通过。
- 下一步建议：
  - 烧录后用串口确认 `WiFi STA connected ip=...`，再用 Apifox 访问 `/api/v1/info`。

## Step 50

- 时间：2026-04-27 15:02:00
- 状态：已完成
- 对应任务：整理完整 WiFi API 文档，并生成 Apifox 可导入数据源
- 结果：
  - 以 `src/services/wifi_api_service.cpp` 为准重写 `docs/wifi_api.md`。
  - 文档覆盖当前 29 个 HTTP API，包括路径、方法、path/query/body 参数、成功返回结构、常见错误和限制。
  - 新增 `docs/apifox/miniprinterrtos.apifox.json`，作为 Apifox 原生候选导入文件。
  - 新增 `docs/apifox/miniprinterrtos.openapi.json`，作为 Apifox 稳定支持的 OpenAPI/Swagger 兜底导入文件。
  - 同步更新 `docs/项目结构.md` 和 `docx/项目结构.md`，记录新增 `docs/apifox/` 目录。
- 验证：
  - `python -m json.tool docs/apifox/miniprinterrtos.apifox.json` 通过。
  - `python -m json.tool docs/apifox/miniprinterrtos.openapi.json` 通过。
  - 源码路由、`docs/wifi_api.md` 和 OpenAPI 路径覆盖检查通过，均为 29 个接口。
  - `python -m platformio run` 通过。
- 下一步建议：
  - 在 Apifox 中优先尝试导入 `miniprinterrtos.apifox.json`；如版本不兼容，改用 `miniprinterrtos.openapi.json`。

## Step 51

- 时间：2026-04-27 16:02:21
- 状态：已完成
- 对应任务：将当前调试 WiFi 修改为 `BCXLY-2.4G`
- 结果：
  - 更新 `src/config/project_config.h`，把 `MP_WIFI_SSID` 改为 `BCXLY-2.4G`。
  - 更新 `MP_WIFI_PASSWORD` 为新的调试密码。
  - 同步更新 `docs/项目启动说明.md` 中的当前调试 SSID。
  - 未修改真实热敏头、电机、传感器等危险硬件宏。
- 验证：
  - `python -m platformio run` 通过。
- 下一步建议：
  - 重新烧录后通过串口确认新的 `WiFi STA connected ip=...`，再更新 Apifox 环境里的 `baseUrl`。

## Step 52

- 时间：2026-04-27 16:07:56
- 状态：已完成
- 对应任务：纠正 `BCXLY-2.4G` 调试 WiFi 密码
- 结果：
  - 更新 `src/config/project_config.h`，将 `MP_WIFI_PASSWORD` 修正为用户确认的密码。
  - 未修改 SSID、连接超时或任何真实硬件功能宏。
- 验证：
  - `python -m platformio run` 通过。
- 下一步建议：
  - 重新烧录后通过串口确认新的局域网 IP，并同步更新 Apifox 环境 `baseUrl`。

## Step 53

- 时间：2026-04-27 16:45:00
- 状态：已完成
- 对应任务：根据 `docs/wifi_api.md` 和当前固件实现生成 Apifox 打印测试文件
- 结果：
  - 新增 `docs/apifox/print_smoke_test.postman_collection.json`，作为可导入 Apifox 的打印冒烟测试集合。
  - 新增 `docs/apifox/payloads/print_smoke_low_density_4lines.bin`，作为测试集合上传用 raw 二进制打印数据。
  - 新增 `docs/apifox/print_smoke_test.md`，说明 Apifox 导入方式、请求顺序、二进制 Body 和期望校验。
  - 测试文件按固件真实 API 组织：`safe-off -> status -> create file -> PUT chunk -> complete -> start job -> current job -> list files`。
  - 测试 payload 为 4 行低密度 raw 图，大小 192 bytes，CRC32/IEEE 为 `0x30D148A2`。
  - 同步更新 `docs/wifi_api.md`、`docs/项目结构.md` 和 `docx/项目结构.md`。
- 验证：
  - `python -m json.tool docs/apifox/print_smoke_test.postman_collection.json` 通过。
  - 重新读取 payload，确认大小为 192 bytes，CRC32/IEEE 为 `0x30D148A2`。
- 下一步建议：
  - 在 Apifox 导入测试集合后，把 `baseUrl` 改成串口日志中的设备 IP，再按 `00..07` 顺序执行。

## Step 54

- 时间：2026-04-27 17:01:20
- 状态：已完成
- 对应任务：补全 Apifox 打印冒烟测试文档的逐步参数说明
- 结果：
  - 重写 `docs/apifox/print_smoke_test.md`，从概览说明扩展为可手工照填的 Apifox 操作文档。
  - 每一步都补齐 Method、URL、Path 参数、Query 参数、Body、Header 和成功响应重点。
  - 重点补充 Step 02 的 `size=192`、`crc32=0x30D148A2`，Step 03 的 `index=0` 和 Binary 文件 Body，Step 04 的 complete 校验要求。
  - 增加 `PRINT_FILE_COMPLETE_FAILED` 的排查方式，要求先看 `received_bytes` 是否等于 `192`。
- 验证：
  - 已读回检查文档内容，确认覆盖 `00..07` 全流程和常见失败原因。
- 下一步建议：
  - 用户按文档在 Apifox 中重新执行 Step 03，确认 `received_bytes=192` 后再执行 Step 04。

## Step 55

- 时间：2026-04-27 17:17:18
- 状态：已完成
- 对应任务：修复 WiFi API 二进制 chunk 上传遇到 `0x00` 被截断的问题
- 结果：
  - 修改 `src/services/wifi_api_service.cpp`，为 `/api/v1/print/files/{file_id}/chunks/{index}` 注册 `UriBraces` 动态路由和 raw body 回调。
  - 新增固定 `RawBodyBuffer`，使用 `HTTPRaw.buf/currentSize` 收集真实字节流，不再用 `g_server.arg("plain")` 和 `String` 读取二进制 Body。
  - `HandlePrintFileChunk()` 现在把完整 raw body 写入 `PrintFileService_WriteChunk()`，可正确处理包含 `0x00` 的 raw 打印数据。
  - 同步修复 `POST /api/v1/factory/head-shift-test` 的 raw body 读取方式，避免 48 字节测试行中含 `0x00` 时被截断。
  - 更新 `docs/apifox/print_smoke_test.md`，说明如果仍看到 `received_bytes=1`，需要重新烧录修复后的固件。
- 验证：
  - `python -m platformio run` 通过。
  - `python tools/api_client.py self-test` 通过。
  - 重新计算 `docs/apifox/payloads/print_smoke_low_density_4lines.bin`，确认大小 192 bytes、CRC32/IEEE `0x30D148A2`、包含 144 个 `0x00` 字节，首个 `0x00` 位于索引 1。
  - 本机可 ping 通 `192.168.1.168`，但当前 HTTP API 请求超时或连接被关闭；`platformio device list` 未发现明确 USB 烧录串口，因此本轮无法在本机完成实机刷写后的 HTTP 验证。
- 下一步建议：
  - 重新烧录固件后，在 Apifox 从 Step 00 开始重跑，Step 03 应返回 `received_bytes=192`。
