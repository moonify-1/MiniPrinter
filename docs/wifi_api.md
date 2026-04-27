# MiniPrinterRTOS WiFi API 详细说明

更新时间：2026-04-27

本文以 `src/services/wifi_api_service.cpp` 为准，记录当前固件真实实现的 HTTP API。UART/串口只保留为烧录、日志和底层临时调试入口，正式控制面是 WiFi API。

## 1. 基础信息

- Base URL 示例：`http://10.178.73.177/api/v1`。实际 IP 以串口日志 `WiFi STA connected ip=...` 为准。
- 当前没有鉴权、HTTPS、mDNS 或配网 UI；仅适合开发和局域网调试。
- 所有 JSON API 返回 `application/json`；二进制分片上传使用 `application/octet-stream`。
- `SAFE_MODE` 下会阻止启动打印、走纸、参数修改和工厂测试；`safe-off`、状态查询、取消等收敛动作仍可用。

## 2. 通用返回与错误

成功响应至少包含：

```json
{
  "ok": true,
  "code": "OK",
  "message": "human readable message"
}
```

错误响应通常包含：

```json
{
  "ok": false,
  "code": "PRINT_FILE_COMPLETE_FAILED",
  "message": "upload is incomplete or crc32 mismatch",
  "error": 1616900097
}
```

`error` 是固件内部 `AppErrorCode` 数值；`SAFE_MODE_BLOCKED` 还会返回 `system_state`；未知路径会返回 `path`。

常见 HTTP 状态码：

| 状态码 | 含义 |
|---:|---|
| 200 | 请求已完成 |
| 201 | 创建上传会话成功 |
| 202 | 请求已入队或即将执行 |
| 400 | 参数缺失、格式错误、范围错误或业务校验失败 |
| 404 | 文件不存在、文件未完成或路径不存在 |
| 409 | SAFE_MODE 拦截、打印忙或无法取消 |
| 500 | 队列、保存、硬件动作或 safe-off 失败 |

## 3. 公共数据结构

### SensorSnapshot

| 字段 | 类型 | 说明 |
|---|---|---|
| `summary` | `bool` | 当前 API 中固定为 true |
| `paper` | `bool` | 是否有纸 |
| `head_temp_c` | `float` | 打印头温度，摄氏度 |
| `battery_mv` | `uint32` | 电池电压，mV |
| `charge_status` | `string` | UNKNOWN/NOT_CHARGING/CHARGING/FULL/FAULT |
| `charge_status_raw` | `uint8` | 原始充电枚举值 |
| `charging` | `bool` | 是否充电中 |
| `motor_fault` | `bool` | 电机故障 |
| `validity` | `uint8` | 0=VALID, 1=INVALID, 2=STALE |
| `tick_ms` | `uint32` | 采样时间 |

### KeySnapshot

| 字段 | 类型 | 说明 |
|---|---|---|
| `raw_level_high` | `bool` | GPIO 原始电平是否为高 |
| `pressed` | `bool` | 按低有效假设解释后的按下状态 |
| `active_low_assumed` | `bool` | 当前是否按低有效解释 |
| `last_event` | `string` | NONE/SHORT_PRESS/LONG_PRESS |
| `last_event_tick_ms` | `uint32` | 最近事件 tick |
| `press_duration_ms` | `uint32` | 按下持续时间 |

### PrintFileSnapshot

| 字段 | 类型 | 说明 |
|---|---|---|
| `file_id` | `uint32` | 文件编号 |
| `state` | `string` | EMPTY/RECEIVING/COMPLETE |
| `total_bytes` | `uint32` | 期望总字节数 |
| `received_bytes` | `uint32` | 已接收字节数 |
| `line_count` | `uint32` | 行数 |
| `expected_crc32` | `uint32` | 声明 CRC32 |
| `actual_crc32` | `uint32` | 实际 CRC32 |

### ParamBlock JSON

| 字段 | 类型 | 说明 |
|---|---|---|
| `magic` | `uint32` | 参数块魔数 |
| `version` | `uint16` | 参数块版本 |
| `crc32` | `uint32` | 参数块 CRC |
| `print` | `object` | dots_per_line/bytes_per_line/stb_group_count |
| `motor` | `object` | steps_per_line/start_interval_us/run_interval_us/fast_interval_us |
| `safety` | `object` | 热安全和低电压限制字段 |
| `comm` | `object` | max_frame_length/inter_byte_timeout_ms/enable_crc |

### ErrorEvent

| 字段 | 类型 | 说明 |
|---|---|---|
| `code` | `uint32` | AppErrorCode |
| `severity` | `string` | NONE/INFO/WARN/ERROR/FATAL |
| `module` | `string` | 模块名 |
| `detail` | `string` | 细节 |
| `timestamp` | `uint32` | 时间戳 |
| `safe_mode` | `bool` | 是否要求安全模式 |

## 4. API 总览

| 分组 | 方法 | 路径 | 说明 |
|---|---|---|---|
| System | `GET` | `/api/v1/info` | 返回设备名、API 前缀和编译功能开关。 |
| System | `GET` | `/api/v1/status` | 返回系统状态、打印状态、传感器摘要、按键和错误摘要。 |
| Sensors | `GET` | `/api/v1/key` | 返回 G_KEY 原始电平、按下状态和最近短按/长按事件。 |
| Sensors | `GET` | `/api/v1/sensors` | 返回纸张、温度、电池、电机故障和读数有效性。 |
| Sensors | `GET` | `/api/v1/battery` | 返回电池电压、低电压事件位、充电状态和读数有效性。 |
| Errors & Logs | `GET` | `/api/v1/error` | 返回最近一次错误事件。 |
| Errors & Logs | `POST` | `/api/v1/error/clear` | 清除可恢复错误并返回最新错误事件。 |
| Params | `GET` | `/api/v1/params` | 返回当前参数快照。 |
| Params | `PATCH` | `/api/v1/params` | 通过 query 参数修改白名单字段，只更新 RAM，不自动保存到 NVS。 |
| Params | `POST` | `/api/v1/params/save` | 请求 ParamTask 延迟保存当前参数。 |
| Params | `POST` | `/api/v1/params/factory-reset` | 恢复默认参数并请求保存。 |
| System | `POST` | `/api/v1/self-test` | 设置自检通过事件位，当前为 mock 自检。 |
| System | `POST` | `/api/v1/reboot` | 先返回响应，再执行 safe-off 和重启。 |
| System | `POST` | `/api/v1/safe-mode` | 主动进入 SAFE_MODE。 |
| Errors & Logs | `GET` | `/api/v1/health` | 返回关键任务注册、心跳和超时状态。 |
| Errors & Logs | `GET` | `/api/v1/logs/recent` | 返回最近 8 条内存日志。 |
| System | `POST` | `/api/v1/safe-off` | 立即关闭 VH/STB/电机等危险输出，是最高优先级安全 API。 |
| Print Files | `POST` | `/api/v1/print/files` | 创建 raw 打印文件上传会话。 |
| Print Files | `GET` | `/api/v1/print/files` | 列出当前非空上传文件槽。 |
| Print Files | `PUT` | `/api/v1/print/files/{file_id}/chunks/{index}` | 上传 raw 二进制分片。Body 为 application/octet-stream；偏移为 index * 512，最后一片可短于 512。 |
| Print Files | `POST` | `/api/v1/print/files/{file_id}/complete` | 校验总大小、48 字节行对齐和 CRC32，成功后文件状态变为 COMPLETE。 |
| Print Files | `DELETE` | `/api/v1/print/files/{file_id}` | 删除上传文件并释放槽位。 |
| Print Jobs | `POST` | `/api/v1/print/jobs` | 根据 COMPLETE 文件创建并启动打印任务；默认 mock 路径不会真实加热或走纸。 |
| Print Jobs | `GET` | `/api/v1/print/jobs/current` | 查询当前打印任务状态。 |
| Print Jobs | `POST` | `/api/v1/print/jobs/current/cancel` | 取消当前打印任务。 |
| Print Jobs | `POST` | `/api/v1/feed` | 请求安全走纸。SAFE_MODE 下会被拒绝。 |
| Factory Tests | `POST` | `/api/v1/factory/motor-test` | 低速小步数电机测试，结束后 release/sleep；需要真实电机宏且 nFAULT 无故障。 |
| Factory Tests | `POST` | `/api/v1/factory/head-shift-test` | Body 必须正好 48 字节 raw，只执行 shift/latch，并保持 VH 关闭。 |
| Factory Tests | `POST` | `/api/v1/factory/head-stb-test` | VH 关闭条件下输出单组 STB 空载测试脉冲。 |

## 5. API 详情

### System

#### GET /api/v1/info

返回设备名、API 前缀和编译功能开关。

- 成功状态码：`200`
- 成功返回：`InfoResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

#### GET /api/v1/status

返回系统状态、打印状态、传感器摘要、按键和错误摘要。

- 成功状态码：`200`
- 成功返回：`StatusResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

### Sensors

#### GET /api/v1/key

返回 G_KEY 原始电平、按下状态和最近短按/长按事件。

- 成功状态码：`200`
- 成功返回：`KeyResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

#### GET /api/v1/sensors

返回纸张、温度、电池、电机故障和读数有效性。

- 成功状态码：`200`
- 成功返回：`SensorsResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

#### GET /api/v1/battery

返回电池电压、低电压事件位、充电状态和读数有效性。

- 成功状态码：`200`
- 成功返回：`BatteryResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

### Errors & Logs

#### GET /api/v1/error

返回最近一次错误事件。

- 成功状态码：`200`
- 成功返回：`ErrorGetResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

#### POST /api/v1/error/clear

清除可恢复错误并返回最新错误事件。

- 成功状态码：`200`
- 成功返回：`ErrorGetResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

### Params

#### GET /api/v1/params

返回当前参数快照。

- 成功状态码：`200`
- 成功返回：`ParamsResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 409 | `SAFE_MODE_BLOCKED` | 处于 SAFE_MODE，动作被拒绝 |

#### PATCH /api/v1/params

通过 query 参数修改白名单字段，只更新 RAM，不自动保存到 NVS。

- 成功状态码：`200`
- 成功返回：`ParamsResponse`

Query 参数：

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `max_heat_dots` | `integer` | 否 | 1..64，最大同时加热点数 |
| `temp_stop_c` | `integer` | 否 | 40..80，停止加热温度 |
| `temp_resume_c` | `integer` | 否 | 30..70，必须小于 temp_stop_c |
| `heat_start_us` | `integer` | 否 | 1..800，初始热脉冲 |
| `heat_max_us` | `integer` | 否 | heat_start_us..800，最大热脉冲 |
| `forbid_heat_paper_missing` | `integer` | 否 | 0/1，缺纸禁止加热 |
| `forbid_heat_low_voltage` | `integer` | 否 | 0/1，低电压禁止加热 |
| `forbid_fast_motor_low_voltage` | `integer` | 否 | 0/1，低电压禁止高速电机 |
| `steps_per_line` | `integer` | 否 | 1..8，每行走纸整步数 |
| `start_interval_us` | `integer` | 否 | 1000..20000，起步相位间隔 |
| `run_interval_us` | `integer` | 否 | 1000..20000，常速相位间隔；需 <= start_interval_us |
| `fast_interval_us` | `integer` | 否 | 1000..20000，快速相位间隔；需 <= run_interval_us |
| `max_frame_length` | `integer` | 否 | 64..PROTO_MAX_FRAME_SIZE，UART 协议最大帧长 |

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 409 | `SAFE_MODE_BLOCKED` | 处于 SAFE_MODE，动作被拒绝 |

#### POST /api/v1/params/save

请求 ParamTask 延迟保存当前参数。

- 成功状态码：`202`
- 成功返回：`SimpleResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

#### POST /api/v1/params/factory-reset

恢复默认参数并请求保存。

- 成功状态码：`202`
- 成功返回：`ParamsResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

### System

#### POST /api/v1/self-test

设置自检通过事件位，当前为 mock 自检。

- 成功状态码：`200`
- 成功返回：`SimpleResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

#### POST /api/v1/reboot

先返回响应，再执行 safe-off 和重启。

- 成功状态码：`202`
- 成功返回：`SimpleResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

#### POST /api/v1/safe-mode

主动进入 SAFE_MODE。

- 成功状态码：`200`
- 成功返回：`SafeModeResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

### Errors & Logs

#### GET /api/v1/health

返回关键任务注册、心跳和超时状态。

- 成功状态码：`200`
- 成功返回：`HealthResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

#### GET /api/v1/logs/recent

返回最近 8 条内存日志。

- 成功状态码：`200`
- 成功返回：`LogsResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

### System

#### POST /api/v1/safe-off

立即关闭 VH/STB/电机等危险输出，是最高优先级安全 API。

- 成功状态码：`200`
- 成功返回：`SimpleResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

### Print Files

#### POST /api/v1/print/files

创建 raw 打印文件上传会话。

- 成功状态码：`201`
- 成功返回：`PrintFileCreateResponse`

Query 参数：

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `size` | `integer` | 是 | 文件字节数，必须 >0、<=3072 且为 48 的整数倍 |
| `crc32` | `integer|string` | 是 | 标准 CRC32/IEEE，支持十进制或 0x 十六进制 |

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 400 | `BAD_REQUEST / PRINT_FILE_CREATE_FAILED` | 缺少 size/crc32、大小不合法、无空槽 |

#### GET /api/v1/print/files

列出当前非空上传文件槽。

- 成功状态码：`200`
- 成功返回：`PrintFileListResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

#### PUT /api/v1/print/files/{file_id}/chunks/{index}

上传 raw 二进制分片。Body 为 application/octet-stream；偏移为 index * 512，最后一片可短于 512。

- 成功状态码：`200`
- 成功返回：`PrintFileCreateResponse`

Path 参数：

| 参数 | 类型 | 说明 |
|---|---|---|
| `file_id` | `integer` | 上传文件 ID |
| `index` | `integer` | 从 0 开始的分片序号 |

Body：`application/octet-stream`，raw 二进制分片；建议每片 512 bytes，最后一片可更短。

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 400 | `BAD_REQUEST / PRINT_FILE_CHUNK_FAILED` | file_id/index 无效、分片越界或文件状态不允许写入 |

#### POST /api/v1/print/files/{file_id}/complete

校验总大小、48 字节行对齐和 CRC32，成功后文件状态变为 COMPLETE。

- 成功状态码：`200`
- 成功返回：`PrintFileCreateResponse`

Path 参数：

| 参数 | 类型 | 说明 |
|---|---|---|
| `file_id` | `integer` | 上传文件 ID |

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 400 | `PRINT_FILE_COMPLETE_FAILED` | 未收全、行不对齐或 CRC32 不匹配 |

#### DELETE /api/v1/print/files/{file_id}

删除上传文件并释放槽位。

- 成功状态码：`200`
- 成功返回：`PrintFileDeleteResponse`

Path 参数：

| 参数 | 类型 | 说明 |
|---|---|---|
| `file_id` | `integer` | 上传文件 ID |

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

### Print Jobs

#### POST /api/v1/print/jobs

根据 COMPLETE 文件创建并启动打印任务；默认 mock 路径不会真实加热或走纸。

- 成功状态码：`202`
- 成功返回：`PrintJobCreateResponse`

Query 参数：

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `file_id` | `integer` | 是 | 必须是 COMPLETE 状态文件 ID |
| `copies` | `integer` | 否 | 当前只支持 1，默认 1 |
| `density` | `integer` | 否 | 0..100，仅范围校验，默认 50 |
| `heat` | `integer` | 否 | 0..100，仅范围校验，默认 50 |

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 400 | `BAD_REQUEST / PARAM_OUT_OF_RANGE` | file_id 缺失、copies/density/heat 非法 |
| 404 | `PRINT_FILE_NOT_READY` | 文件不存在或未 COMPLETE |
| 409 | `PRINT_BUSY / SAFE_MODE_BLOCKED` | 当前有打印任务或处于 SAFE_MODE |

#### GET /api/v1/print/jobs/current

查询当前打印任务状态。

- 成功状态码：`200`
- 成功返回：`PrintJobCurrentResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

#### POST /api/v1/print/jobs/current/cancel

取消当前打印任务。

- 成功状态码：`202`
- 成功返回：`PrintJobCancelResponse`

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 404 | `NOT_FOUND` | 路径错误或动态资源不存在 |

#### POST /api/v1/feed

请求安全走纸。SAFE_MODE 下会被拒绝。

- 成功状态码：`202`
- 成功返回：`FeedResponse`

Query 参数：

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `steps` | `integer` | 否 | 1..200，默认 1 |

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 409 | `SAFE_MODE_BLOCKED` | 处于 SAFE_MODE，动作被拒绝 |

### Factory Tests

#### POST /api/v1/factory/motor-test

低速小步数电机测试，结束后 release/sleep；需要真实电机宏且 nFAULT 无故障。

- 成功状态码：`200`
- 成功返回：`MotorTestResponse`

Query 参数：

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `steps` | `integer` | 否 | 1..200，默认 1 |

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 409 | `SAFE_MODE_BLOCKED` | 处于 SAFE_MODE，动作被拒绝 |

#### POST /api/v1/factory/head-shift-test

Body 必须正好 48 字节 raw，只执行 shift/latch，并保持 VH 关闭。

- 成功状态码：`200`
- 成功返回：`SimpleResponse`

Body：`application/octet-stream`，必须正好 48 bytes raw 行数据。

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 409 | `SAFE_MODE_BLOCKED` | 处于 SAFE_MODE，动作被拒绝 |

#### POST /api/v1/factory/head-stb-test

VH 关闭条件下输出单组 STB 空载测试脉冲。

- 成功状态码：`200`
- 成功返回：`HeadStbResponse`

Query 参数：

| 参数 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `group` | `integer` | 是 | 0..5，STB 组号 |
| `pulse_us` | `integer` | 否 | 脉冲宽度，默认 5 |

常见错误：

| 状态码 | code | 场景 |
|---:|---|---|
| 409 | `SAFE_MODE_BLOCKED` | 处于 SAFE_MODE，动作被拒绝 |

## 6. 打印文件约束

- raw 单色位图，384 点宽，48 bytes/line，高位在左。
- 文件大小必须大于 0、是 48 的整数倍、且不超过 3072 bytes（64 行）。
- 当前最多同时保留 2 个上传文件槽。
- 创建上传时传 `size` 和 CRC32；写入所有分片后必须调用 `complete`，只有 `COMPLETE` 文件才能启动打印。
- `crc32` 参数支持十进制或 `0x` 前缀十六进制。

## 7. Apifox 导入文件

- `docs/apifox/miniprinterrtos.apifox.json`：Apifox 原生候选格式，按常见导出结构组织分组、接口、参数和响应模型。
- `docs/apifox/miniprinterrtos.openapi.json`：OpenAPI 3.0.3 格式，Apifox 官方稳定支持导入；如果原生候选文件不能导入，请使用此文件，数据源格式选 `OpenAPI/Swagger`。
- `docs/apifox/print_smoke_test.postman_collection.json`：打印冒烟测试集合，导入 Apifox 后按 `00..07` 顺序执行，会创建上传会话、上传 raw 分片、complete 校验并启动打印任务。
- `docs/apifox/payloads/print_smoke_low_density_4lines.bin`：冒烟测试集合第 03 步使用的二进制 raw 打印文件，大小 192 bytes，CRC32/IEEE 为 `0x30D148A2`。
- `docs/apifox/print_smoke_test.md`：打印冒烟测试说明，记录 Apifox 运行顺序、变量和预期返回。
- `docs/apifox/payloads/print_real_moderate_48lines.bin`：真实打印测试完整 raw 文件，大小 2304 bytes，CRC32/IEEE 为 `0x282EB33A`。
- `docs/apifox/payloads/print_real_moderate_48lines_chunk_0.bin` 到 `print_real_moderate_48lines_chunk_4.bin`：真实打印测试按 512 bytes 分片后的 Apifox Binary 上传文件。
- `docs/apifox/真实打印测试.md`：真实打印测试说明，记录 5 个分片的 index、文件名、预期 received_bytes 和启动打印参数。

导入后建议创建环境变量：`baseUrl=http://10.178.73.177`，并按串口实际 IP 修改；各接口路径已经包含 `/api/v1` 前缀。
