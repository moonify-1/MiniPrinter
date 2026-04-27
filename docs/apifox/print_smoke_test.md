# Apifox 打印冒烟测试

更新时间：2026-04-27

本文用于在 Apifox 中手工调用 MiniPrinterRTOS WiFi API，完成一次最小打印链路测试。  
按 `00 -> 07` 顺序执行，不要跳步。所有接口前缀都是：

```text
{{baseUrl}}/api/v1
```

如果串口日志显示：

```text
WiFi STA connected ip=192.168.1.168
```

则 Apifox 环境变量填写：

```text
baseUrl = http://192.168.1.168
```

## 1. 测试文件

| 项目 | 值 |
|---|---|
| raw 文件 | `docs/apifox/payloads/print_smoke_low_density_4lines.bin` |
| 图案 | 低密度黑点 |
| 行宽 | `48 bytes/line` |
| 行数 | `4` |
| 总大小 | `192 bytes` |
| CRC32/IEEE 十六进制 | `0x30D148A2` |
| CRC32/IEEE 十进制 | `819021986` |
| 分片序号 | `0` |

这个文件只有 4 行，目的是先验证 API 链路。当前默认固件没有启用真实热敏头和真实步进电机宏，所以普通调试固件走 mock 打印路径，不会真实加热或走纸。

## 2. Apifox 通用设置

每个请求都建议带：

| Header | 值 |
|---|---|
| `Accept` | `application/json` |

上传二进制分片的第 03 步还必须额外带：

| Header | 值 |
|---|---|
| `Content-Type` | `application/octet-stream` |

如果 Apifox 页面里有 `Path` 参数和 `Query` 参数，按本文对应步骤填写。  
如果 Body 写着 `none`，就不要填 JSON、Text、form-data 或 Binary。

## 3. Step 00：安全关闭危险输出

### 作用

测试前先关闭 VH、STB、电机等危险输出。

### Apifox 填写

| 项目 | 填写 |
|---|---|
| Method | `POST` |
| URL | `{{baseUrl}}/api/v1/safe-off` |
| Path 参数 | 无 |
| Query 参数 | 无 |
| Body | `none` |
| Header | `Accept: application/json` |

### 成功响应重点

```json
{
  "ok": true,
  "code": "OK"
}
```

如果这一步都连不上，先检查设备 IP、电脑和设备是否在同一局域网。

## 4. Step 01：查询整机状态

### 作用

确认设备在线，并确认当前没有处于 `SAFE_MODE`。

### Apifox 填写

| 项目 | 填写 |
|---|---|
| Method | `GET` |
| URL | `{{baseUrl}}/api/v1/status` |
| Path 参数 | 无 |
| Query 参数 | 无 |
| Body | `none` |
| Header | `Accept: application/json` |

### 成功响应重点

```json
{
  "ok": true,
  "system_state": "READY"
}
```

`system_state` 不一定必须是 `READY`，但不能是 `SAFE_MODE`。如果是 `SAFE_MODE`，启动打印会被拒绝。

## 5. Step 02：创建打印文件上传会话

### 作用

告诉固件：我要上传一个 raw 打印文件，它的大小是多少，CRC32 是多少。固件会分配一个 `file_id`。

### Apifox 填写

| 项目 | 填写 |
|---|---|
| Method | `POST` |
| URL | `{{baseUrl}}/api/v1/print/files` |
| Path 参数 | 无 |
| Query 参数 `size` | `192` |
| Query 参数 `crc32` | `0x30D148A2` |
| Body | `none` |
| Header | `Accept: application/json` |

完整 URL 也可以直接写成：

```text
{{baseUrl}}/api/v1/print/files?size=192&crc32=0x30D148A2
```

注意参数名是 `crc32`，不是 `srs32`。

### 成功响应重点

```json
{
  "ok": true,
  "code": "OK",
  "file": {
    "file_id": 1,
    "state": "RECEIVING",
    "total_bytes": 192,
    "received_bytes": 0,
    "line_count": 4,
    "expected_crc32": 819021986
  }
}
```

把返回里的 `file.file_id` 记下来。后续 `{file_id}` 都填这个值。

例如返回 `file_id = 1`，后面就用：

```text
file_id = 1
```

## 6. Step 03：上传第 0 片二进制数据

### 作用

把 `print_smoke_low_density_4lines.bin` 的原始二进制内容传给固件。  
本测试文件只有 `192 bytes`，小于固件建议分片 `512 bytes`，所以只上传一个分片，`index=0`。

### Apifox 填写

| 项目 | 填写 |
|---|---|
| Method | `PUT` |
| URL | `{{baseUrl}}/api/v1/print/files/{file_id}/chunks/{index}` |
| Path 参数 `file_id` | Step 02 返回的 `file.file_id`，例如 `1` |
| Path 参数 `index` | `0` |
| Query 参数 | 无 |
| Body | 选择 `Binary` |
| Binary 文件 | `docs/apifox/payloads/print_smoke_low_density_4lines.bin` |
| Header `Accept` | `application/json` |
| Header `Content-Type` | `application/octet-stream` |

如果 Step 02 返回 `file_id = 1`，完整 URL 就是：

```text
{{baseUrl}}/api/v1/print/files/1/chunks/0
```

Body 只能选择 `Binary`，并选择 `.bin` 文件。不要选择 `JSON`、`Text`、`form-data`，也不要在 Body 里手打 `1`、`192` 或其他文字。

### 成功响应重点

```json
{
  "ok": true,
  "code": "OK",
  "file": {
    "file_id": 1,
    "state": "RECEIVING",
    "total_bytes": 192,
    "received_bytes": 192
  }
}
```

这里最重要的是：

```text
received_bytes = 192
total_bytes    = 192
```

如果 `received_bytes` 不是 `192`，不要执行 Step 04，先重新检查 Body 是否真的选择了 Binary 文件。

## 7. Step 04：完成上传并校验 CRC

### 作用

让固件检查上传是否完整，并重新计算 CRC32。校验通过后，文件状态才会变成 `COMPLETE`，后面才能启动打印。

### Apifox 填写

| 项目 | 填写 |
|---|---|
| Method | `POST` |
| URL | `{{baseUrl}}/api/v1/print/files/{file_id}/complete` |
| Path 参数 `file_id` | Step 02 返回的 `file.file_id`，例如 `1` |
| Query 参数 | 无 |
| Body | `none` |
| Header | `Accept: application/json` |

如果 Step 02 返回 `file_id = 1`，完整 URL 就是：

```text
{{baseUrl}}/api/v1/print/files/1/complete
```

### 成功响应重点

```json
{
  "ok": true,
  "code": "OK",
  "file": {
    "file_id": 1,
    "state": "COMPLETE",
    "total_bytes": 192,
    "received_bytes": 192,
    "line_count": 4,
    "expected_crc32": 819021986,
    "actual_crc32": 819021986
  }
}
```

必须看到：

```text
state        = COMPLETE
actual_crc32 = 819021986
```

### 常见失败

如果返回：

```json
{
  "ok": false,
  "code": "PRINT_FILE_COMPLETE_FAILED",
  "message": "upload is incomplete or crc32 mismatch"
}
```

先调用 Step 07 的 `GET /api/v1/print/files` 看 `received_bytes`：

- `received_bytes < 192`：第 03 步没有真正上传完整二进制文件，回到 Step 03 重新选 Binary 文件上传。
- `received_bytes = 192` 但仍失败：第 02 步的 `crc32` 填错，或者第 03 步上传的不是本文这个 `.bin` 文件。

## 8. Step 05：启动打印任务

### 作用

根据已经 `COMPLETE` 的文件创建打印任务。

### Apifox 填写

| 项目 | 填写 |
|---|---|
| Method | `POST` |
| URL | `{{baseUrl}}/api/v1/print/jobs` |
| Path 参数 | 无 |
| Query 参数 `file_id` | Step 02 返回的 `file.file_id`，例如 `1` |
| Query 参数 `copies` | `1` |
| Query 参数 `density` | `50` |
| Query 参数 `heat` | `50` |
| Body | `none` |
| Header | `Accept: application/json` |

如果 `file_id = 1`，完整 URL 可以写成：

```text
{{baseUrl}}/api/v1/print/jobs?file_id=1&copies=1&density=50&heat=50
```

当前 v1 只支持：

```text
copies = 1
density = 0..100
heat = 0..100
```

### 成功响应重点

```json
{
  "ok": true,
  "code": "OK",
  "job_id": 1,
  "file_id": 1,
  "line_total": 4,
  "copies": 1
}
```

把 `job_id` 记下来即可，但后续查询当前任务不需要手动传 `job_id`。

### 常见失败

| code | 原因 |
|---|---|
| `PRINT_FILE_NOT_READY` | Step 04 没有成功，文件不是 `COMPLETE` |
| `PRINT_BUSY` | 当前已有打印任务 |
| `SAFE_MODE_BLOCKED` | 设备处于 `SAFE_MODE` |
| `PARAM_OUT_OF_RANGE` | `copies`、`density` 或 `heat` 填错 |

## 9. Step 06：查询当前打印任务

### 作用

查看当前打印任务状态。由于本测试只有 4 行，mock 打印可能很快完成，所以状态可能已经是 `DONE` 或 `NONE`。

### Apifox 填写

| 项目 | 填写 |
|---|---|
| Method | `GET` |
| URL | `{{baseUrl}}/api/v1/print/jobs/current` |
| Path 参数 | 无 |
| Query 参数 | 无 |
| Body | `none` |
| Header | `Accept: application/json` |

### 成功响应重点

```json
{
  "ok": true,
  "code": "OK",
  "job_id": 1,
  "file_id": 1,
  "state": "DONE",
  "line_done": 4,
  "error": 0
}
```

常见状态：

| state | 含义 |
|---|---|
| `QUEUED` | 已排队 |
| `PRINTING` | 正在打印 |
| `DONE` | 已完成 |
| `FAILED` | 打印失败 |
| `CANCELLED` | 已取消 |
| `NONE` | 当前没有活动任务 |

## 10. Step 07：查看上传文件槽

### 作用

查看固件当前保存了哪些上传文件，以及每个文件的状态。

### Apifox 填写

| 项目 | 填写 |
|---|---|
| Method | `GET` |
| URL | `{{baseUrl}}/api/v1/print/files` |
| Path 参数 | 无 |
| Query 参数 | 无 |
| Body | `none` |
| Header | `Accept: application/json` |

### 成功响应重点

```json
{
  "ok": true,
  "code": "OK",
  "max_bytes": 3072,
  "chunk_size": 512,
  "files": [
    {
      "file_id": 1,
      "state": "COMPLETE",
      "total_bytes": 192,
      "received_bytes": 192,
      "line_count": 4
    }
  ]
}
```

如果要释放文件槽，可以手动调用：

```text
DELETE {{baseUrl}}/api/v1/print/files/{file_id}
```

例如：

```text
DELETE {{baseUrl}}/api/v1/print/files/1
```

## 11. 一次成功测试的最短参数清单

假设设备 IP 是 `192.168.1.168`，Step 02 返回 `file_id=1`，则完整流程如下：

```text
00 POST http://192.168.1.168/api/v1/safe-off
01 GET  http://192.168.1.168/api/v1/status
02 POST http://192.168.1.168/api/v1/print/files?size=192&crc32=0x30D148A2
03 PUT  http://192.168.1.168/api/v1/print/files/1/chunks/0
   Body: Binary -> docs/apifox/payloads/print_smoke_low_density_4lines.bin
   Header: Content-Type=application/octet-stream
04 POST http://192.168.1.168/api/v1/print/files/1/complete
05 POST http://192.168.1.168/api/v1/print/jobs?file_id=1&copies=1&density=50&heat=50
06 GET  http://192.168.1.168/api/v1/print/jobs/current
07 GET  http://192.168.1.168/api/v1/print/files
```

## 12. 真实硬件提醒

当前测试文件是低密度短行文件，但如果后续开启真实热敏头和真实步进电机宏，仍必须先完成：

1. `safe-off` 安全输出确认。
2. `head-shift-test` 空载移位和 latch 波形确认。
3. `head-stb-test` 空载 STB 波形确认。
4. `motor-test` 低速走纸方向确认。
5. 电池、电机故障、纸张、温度保护确认。

没有完成这些实测前，不要直接做真实加热打印。
