# Apifox 打印冒烟测试

更新时间：2026-04-27

## 1. 文件说明

- `print_smoke_test.postman_collection.json`：可导入 Apifox 的打印测试集合，按 `00..07` 顺序调用真实 WiFi API。
- `payloads/print_smoke_low_density_4lines.bin`：上传用 raw 打印数据，低密度 4 行，用于降低真实硬件误操作风险。

Apifox 导入时选择 Postman Collection 格式；导入后把集合变量 `baseUrl` 改成串口日志里的设备地址，例如 `http://10.178.73.177`。

## 2. 测试数据格式

| 字段 | 值 | 说明 |
|---|---:|---|
| 文件路径 | `docs/apifox/payloads/print_smoke_low_density_4lines.bin` | Apifox 二进制 Body 选择该文件 |
| 行宽 | `48 bytes/line` | 对应 384 点宽热敏头 |
| 行数 | `4` | 短文件，便于首次联调 |
| 总大小 | `192 bytes` | `4 * 48`，满足 48 字节对齐 |
| CRC32/IEEE | `0x30D148A2` | 创建上传会话时传入 |
| CRC32 十进制 | `819021986` | 固件 complete 后返回该值 |
| 分片大小 | `192 bytes` | 本测试只需要上传第 `0` 片 |

## 3. 请求顺序

| 步骤 | 方法 | 路径 | 关键参数/Body |
|---:|---|---|---|
| 00 | `POST` | `/api/v1/safe-off` | 先关闭危险输出 |
| 01 | `GET` | `/api/v1/status` | 确认设备不在 `SAFE_MODE` |
| 02 | `POST` | `/api/v1/print/files` | `size=192&crc32=0x30D148A2`，响应里保存 `file_id` |
| 03 | `PUT` | `/api/v1/print/files/{{file_id}}/chunks/0` | Body 选择 raw 文件，`Content-Type=application/octet-stream` |
| 04 | `POST` | `/api/v1/print/files/{{file_id}}/complete` | 校验 `state=COMPLETE` 和 `actual_crc32=819021986` |
| 05 | `POST` | `/api/v1/print/jobs` | `file_id={{file_id}}&copies=1&density=50&heat=50` |
| 06 | `GET` | `/api/v1/print/jobs/current` | 查询打印任务状态 |
| 07 | `GET` | `/api/v1/print/files` | 收尾查看上传槽 |

## 4. 注意事项

- 如果 Apifox 导入后第 03 步没有保留文件路径，在该请求的 Body 中手动选择 `payloads/print_smoke_low_density_4lines.bin`。
- 第 03 步必须使用 `application/octet-stream`，不能用 JSON、form-data 或文本 Body。
- 当前项目默认未启用真实热敏头和真实步进电机宏，普通调试固件会走 mock 打印链路。
- 如果后续开启真实硬件宏，必须先完成安全态、shift/latch、STB 空载波形、电机低速和低密度短行验收，再运行本测试。
