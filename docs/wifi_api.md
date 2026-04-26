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
| `POST /api/v1/safe-off` | 立即关闭危险输出，是最高优先级安全 API。 |
| `POST /api/v1/print/files?size=<bytes>&crc32=<crc>` | 创建 raw 打印文件上传会话。 |
| `PUT /api/v1/print/files/{file_id}/chunks/{index}` | 上传一个 raw 分片，请求体为二进制数据。 |
| `POST /api/v1/print/files/{file_id}/complete` | 校验总大小、48 字节行对齐和 CRC32。 |
| `GET /api/v1/print/files` | 列出已创建的上传文件。 |
| `DELETE /api/v1/print/files/{file_id}` | 删除上传文件并释放槽位。 |

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
- 后续启动打印必须通过 `POST /api/v1/print/jobs`，不能再把 UART `PRINT_LINE/PRINT_START` 作为正式产品入口。
