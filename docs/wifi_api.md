# MiniPrinterRTOS WiFi API 说明

更新时间：2026-04-26 21:20:08

## 1. 定位

- WiFi API 是本项目正式产品控制面。
- UART/串口协议只保留为底层临时调试或兼容参考，不作为正式打印文件传输、启动打印或控制入口。
- 当前实验室阶段不实现 API 鉴权、HTTPS、配网 UI 或 mDNS；启用 `MP_ENABLE_WIFI=1` 后，未配置 STA SSID 时设备会启动 `MiniPrinterRTOS` AP fallback。

## 2. 通用返回格式

所有 API 返回 JSON，并至少包含：

```json
{
  "ok": true,
  "code": "OK",
  "message": "human readable message"
}
```

错误返回示例：

```json
{
  "ok": false,
  "code": "PRINT_FILE_COMPLETE_FAILED",
  "message": "upload is incomplete or crc32 mismatch",
  "error": 1616900097
}
```

`error` 是固件内部 `AppErrorCode`，可用于脚本判断具体失败类型。

## 3. 已实现 API

| 方法与路径 | 说明 |
|---|---|
| `GET /api/v1/info` | 返回设备名、API 前缀和功能开关。 |
| `GET /api/v1/status` | 返回系统状态、打印状态、传感器摘要和最近错误摘要。 |
| `GET /api/v1/sensors` | 返回 paper、head_temp_c、battery_mv、charging、motor_fault、validity。 |
| `GET /api/v1/battery` | 返回 battery_mv、charging、low_power 和读数有效性。 |
| `GET /api/v1/error` | 返回最近错误事件。 |
| `POST /api/v1/error/clear` | 清除可恢复错误。 |
| `GET /api/v1/params` | 返回当前参数快照。 |
| `PATCH /api/v1/params?...` | 修改安全参数白名单，先只更新 RAM。 |
| `POST /api/v1/params/save` | 请求 ParamTask 延迟保存参数。 |
| `POST /api/v1/params/factory-reset` | 恢复默认参数并请求保存。 |
| `POST /api/v1/self-test` | 执行当前 mock 自检并设置自检通过事件。 |
| `POST /api/v1/reboot` | 先 safe-off 再重启。 |
| `POST /api/v1/safe-mode` | 主动进入 SAFE_MODE。 |
| `GET /api/v1/health` | 返回关键任务心跳健康快照。 |
| `GET /api/v1/logs/recent` | 返回最近 8 条内存日志快照。 |
| `POST /api/v1/safe-off` | 立即关闭危险输出，是最高优先级安全 API。 |
| `POST /api/v1/print/files?size=<bytes>&crc32=<crc>` | 创建 raw 打印文件上传会话。 |
| `PUT /api/v1/print/files/{file_id}/chunks/{index}` | 上传一个 raw 分片，请求体为二进制数据。 |
| `POST /api/v1/print/files/{file_id}/complete` | 校验总大小、48 字节行对齐和 CRC32。 |
| `GET /api/v1/print/files` | 列出已创建的上传文件。 |
| `DELETE /api/v1/print/files/{file_id}` | 删除上传文件并释放槽位。 |
| `POST /api/v1/print/jobs?file_id=<id>&copies=1` | 根据已完成上传的 `file_id` 创建并启动打印任务。 |
| `GET /api/v1/print/jobs/current` | 查询当前打印任务状态。 |
| `POST /api/v1/print/jobs/current/cancel` | 取消当前打印任务。 |
| `POST /api/v1/feed?steps=<n>` | 走纸请求，当前限制 `1..200` 步。 |
| `POST /api/v1/factory/motor-test?steps=<n>` | 低速小步数电机测试，结束后 release/sleep。 |
| `POST /api/v1/factory/head-shift-test` | 请求体 48 字节 raw，只做 shift/latch，保持 VH 关闭。 |
| `POST /api/v1/factory/head-stb-test?group=<0..5>&pulse_us=<n>` | VH 关闭条件下输出单组 STB 空载测试脉冲。 |

## 4. 打印文件上传限制

- 文件格式：第一阶段只支持 384 点宽 raw 单色位图。
- 行宽：固定 `48 bytes/line`。
- 最大文件：`3072 bytes`，即 `64 lines`。
- 分片大小：建议 `512 bytes`；最后一个分片可以更短。
- 文件槽位：当前最多同时保留 `2` 个上传文件。
- 完成上传前不会启动打印；`complete` 通过后文件状态变为 `COMPLETE`。
- CRC32 使用标准 CRC32/IEEE；`crc32` 参数可以传十进制或 `0x` 前缀十六进制。

## 5. 安全说明

- Task 02 只建立上传和校验路径，不打开 VH，不唤醒电机，不启动真实打印。
- 启动打印必须通过 `POST /api/v1/print/jobs`，不能再把 UART `PRINT_LINE/PRINT_START` 作为正式产品入口。
- `SAFE_MODE` 下禁止启动打印和走纸；`safe-off`、状态查询和取消类安全动作仍可用于收敛系统。
- `POST /api/v1/print/jobs` 第一阶段只支持 `copies=1`，`density` 和 `heat` 可传 `0..100` 做范围校验，但还不会直接改写持久化参数。
- `PATCH /api/v1/params` 第一阶段只开放：`max_heat_dots`、`temp_stop_c`、`temp_resume_c`、`heat_start_us`、`heat_max_us`。
