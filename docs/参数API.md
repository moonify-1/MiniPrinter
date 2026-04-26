# MiniPrinter 参数 API

更新时间：2026-04-26 22:08:29

## 1. 定位

WiFi API 是正式参数修改入口。UART `SET_PARAM` 仍只作为兼容占位，不作为产品参数配置方式。

## 2. 已开放字段

| 查询参数 | 范围 | 作用 |
|---|---:|---|
| `max_heat_dots` | `1..64` | 单组最多同时加热点数。 |
| `temp_stop_c` | `40..80` | 到达该头温后禁止加热。 |
| `temp_resume_c` | `30..70` | 低于该头温后允许恢复；必须小于 `temp_stop_c`。 |
| `heat_start_us` | `1..800` | 低密度起始热脉宽。 |
| `heat_max_us` | `heat_start_us..800` | 参数层允许的最大热脉宽。 |
| `forbid_heat_paper_missing` | `0..1` | 缺纸时是否禁止加热。 |
| `forbid_heat_low_voltage` | `0..1` | 低电压时是否禁止加热。 |
| `forbid_fast_motor_low_voltage` | `0..1` | 低电压时是否禁止真实电机危险动作。 |
| `steps_per_line` | `1..8` | 每打印一行后的走纸步数。 |
| `start_interval_us` | `1000..20000` | 电机起步相位间隔。 |
| `run_interval_us` | `1000..20000` | 电机常速相位间隔。 |
| `fast_interval_us` | `1000..20000` | 电机较快相位间隔。 |
| `max_frame_length` | `64..512` | UART 兼容协议最大帧长。 |

额外约束：

- `temp_resume_c < temp_stop_c`
- `heat_start_us <= heat_max_us`
- `fast_interval_us <= run_interval_us <= start_interval_us`

## 3. 调用示例

```text
PATCH /api/v1/params?heat_start_us=420&heat_max_us=500&steps_per_line=2
```

修改只先写入 RAM。需要持久保存时再调用：

```text
POST /api/v1/params/save
```

## 4. 错误策略

- 出现未知查询参数时返回 `PARAM_NOT_ALLOWED`。
- 出现越界值或组合约束失败时返回 `PARAM_OUT_OF_RANGE`。
- 失败时不会更新当前参数快照。

