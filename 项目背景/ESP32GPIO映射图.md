# ESP32-S3-DevKitC-1 GPIO 映射整理

来源图片：[`ESP32GPIO映射图.png`](./ESP32GPIO映射图.png)

## 1. 板卡基础信息

- 板卡：ESP32-S3-DevKitC-1。
- 主控模组：ESP32-S3-WROOM。
- CPU：32-bit Xtensa 双核，最高 240 MHz。
- 无线：Wi-Fi 802.11 b/g/n 2.4 GHz，BLE 5 Mesh。
- 存储/内存：512 KB SRAM，其中 16 KB 位于 RTC 区域；384 KB ROM。
- 外设资源：45 个 GPIO、4 路 SPI、3 路 UART、2 路 I2C、14 路 Touch、2 路 I2S、RMT、LED PWM、USB-OTG、TWAI、2 路 12-bit ADC、LCD interface、DVP。

## 2. 图片标签说明

| 标签 | 含义 | 初学注意 |
|---|---|---|
| `GPIOx` | 普通数字输入/输出引脚 | 可用于按键、LED、控制信号等，但仍要避开启动/下载/USB/JTAG 占用。 |
| 波浪线 | 图中标记为 PWM capable pin | 可用于 PWM 输出；实际使用时一般走 ESP32 的 LEDC 外设。 |
| `ADCx_CH` | 模拟输入通道 | 适合读取电压、NTC 分压等模拟量，输入电压不能超过芯片允许范围。 |
| `TOUCHx` | 电容触摸通道 | 可用于触摸按键，不适合直接接高噪声大电流负载。 |
| `RTC` | RTC 电源域相关引脚 | 深睡眠唤醒、低功耗场景可能会用到。 |
| `FSPI` / `SPI` / `SUBSPI` | SPI 相关复用功能 | 如果不了解当前板卡硬件连接，先不要随意占用。 |
| `U0TXD` / `U0RXD` | UART0 下载/调试串口 | 常用于烧录和日志，作为普通 GPIO 前要确认不会影响下载与调试。 |
| `U1TXD` / `U1RXD` / `U1CTS` / `U1RTS` | UART1 相关复用功能 | 可用于外部串口通信。 |
| `MTMS` / `MTDI` / `MTDO` / `MTCK` | JTAG 调试相关信号 | 如果需要 JTAG 调试，应避免被业务外设占用。 |
| `BOOT` / `LOG` / `VSPI` | 启动或调试相关功能标记 | 初学阶段不建议接热敏头、电机等关键输出。 |
| `USB D+` / `USB D-` | 原生 USB 信号 | 不建议当普通 GPIO 使用，避免影响 USB 下载、调试或通信。 |
| `GND` | 地 | 所有外设必须和开发板共地。 |
| `3V3` / `5V0` | 电源引脚 | `3V3` 给 3.3 V 外设；`5V0` 通常来自 USB/外部 5 V，不能直接接到只支持 3.3 V 的 GPIO。 |

## 3. 电源与特殊固定引脚

| 标记 | 类型 | 说明 |
|---|---|---|
| `3V3` | 电源 | 3.3 V 电源输出/输入，适合低功耗 3.3 V 外设。 |
| `5V0` | 电源 | 5 V 电源轨，通常来自 USB 或板上 5 V。不能直接接 ESP32 GPIO。 |
| `GND` | 地 | 外设地线必须连接到这里，才能形成共同参考电位。 |
| `RST` | 复位 | 拉低会复位开发板。 |
| `BOOT` / `GPIO0` | 启动模式相关 | 上电/复位时电平会影响下载或启动流程，谨慎使用。 |
| `RGB_LED` / `GPIO38` | 板载灯 | 图中标记板载 RGB LED 连接到 GPIO38。 |
| `USB D+` / `GPIO20` | USB | 原生 USB D+。 |
| `USB D-` / `GPIO19` | USB | 原生 USB D-。 |

## 4. 左侧排针 GPIO

| 从上到下 | GPIO | 图片标注的复用功能 | 使用建议 |
|---:|---|---|---|
| 1 | `GPIO4` | `ADC1_3`、`TOUCH4`、`RTC` | 普通 GPIO / ADC / Touch 均可考虑。 |
| 2 | `GPIO5` | `ADC1_4`、`TOUCH5`、`RTC` | 普通 GPIO / ADC / Touch 均可考虑。 |
| 3 | `GPIO6` | `ADC1_5`、`TOUCH6`、`RTC` | 普通 GPIO / ADC / Touch 均可考虑。 |
| 4 | `GPIO7` | `ADC1_6`、`TOUCH7`、`RTC` | 普通 GPIO / ADC / Touch 均可考虑。 |
| 5 | `GPIO15` | `ADC2_4`、`U0RTS`、`RTC`、`XTAL_32K_P` | 若使用外部 32 kHz 晶振或 UART0 RTS，应避开。 |
| 6 | `GPIO16` | `ADC2_5`、`U0CTS`、`RTC`、`XTAL_32K_N` | 若使用外部 32 kHz 晶振或 UART0 CTS，应避开。 |
| 7 | `GPIO17` | `ADC2_6`、`U1TXD`、`RTC` | 可作为 UART1 TX 或普通 GPIO。 |
| 8 | `GPIO18` | `ADC2_7`、`U1RXD`、`RTC`、`CLK_OUT3` | 可作为 UART1 RX 或普通 GPIO。 |
| 9 | `GPIO8` | `ADC1_7`、`TOUCH8`、`RTC` | 普通 GPIO / ADC / Touch 均可考虑。 |
| 10 | `GPIO3` | `ADC1_2`、`TOUCH3`、`RTC`、`JTAG` | 若使用 JTAG 调试，应避免占用。 |
| 11 | `GPIO46` | `LOG` | 启动/日志相关标记，初学阶段不建议接关键输出。 |
| 12 | `GPIO9` | `FSPIHD`、`SUBSPIHD`、`ADC1_8`、`TOUCH9`、`RTC` | 带 SPI 复用；确认未被其他外设占用后再使用。 |
| 13 | `GPIO10` | `FSPICS0`、`SUBSPICS0`、`FSPIIO4`、`ADC1_9`、`TOUCH10`、`RTC` | 带 SPI 片选/数据复用；适合谨慎分配。 |
| 14 | `GPIO11` | `FSPID`、`SUBSPID`、`FSPIIO5`、`ADC2_0`、`TOUCH11`、`RTC` | 带 SPI 数据复用。 |
| 15 | `GPIO12` | `FSPICLK`、`SUBSPICLK`、`FSPIIO6`、`ADC2_1`、`TOUCH12`、`RTC` | 带 SPI 时钟复用；作为普通 GPIO 前先确认 SPI 规划。 |
| 16 | `GPIO13` | `FSPIQ`、`SUBSPIQ`、`FSPIIO7`、`ADC2_2`、`TOUCH13`、`RTC` | 带 SPI 数据复用。 |
| 17 | `GPIO14` | `FSPIWP`、`SUBSPIWP`、`FSPIDQS`、`ADC2_3`、`TOUCH14`、`RTC` | 带 SPI 写保护/数据选通信号复用。 |

## 5. 右侧排针 GPIO

| 从上到下 | GPIO | 图片标注的复用功能 | 使用建议 |
|---:|---|---|---|
| 1 | `GPIO43` | `U0TXD`、`CLK_OUT1` | UART0 TX，常用于下载/日志，谨慎复用。 |
| 2 | `GPIO44` | `U0RXD`、`CLK_OUT2` | UART0 RX，常用于下载/日志，谨慎复用。 |
| 3 | `GPIO1` | `RTC`、`TOUCH1`、`ADC1_0` | 普通 GPIO / ADC / Touch 均可考虑。 |
| 4 | `GPIO2` | `RTC`、`TOUCH2`、`ADC1_1` | 普通 GPIO / ADC / Touch 均可考虑。 |
| 5 | `GPIO42` | `MTMS` | JTAG 相关；需要调试时不要占用。 |
| 6 | `GPIO41` | `MTDI`、`CLK_OUT1` | JTAG 相关；需要调试时不要占用。 |
| 7 | `GPIO40` | `MTDO`、`CLK_OUT2` | JTAG 相关；需要调试时不要占用。 |
| 8 | `GPIO39` | `MTCK`、`CLK_OUT3`、`SUBSPICS1` | JTAG 时钟；需要调试时不要占用。 |
| 9 | `GPIO38` | `FSPIWP`、`SUBSPIWP`、`RGB_LED` | 板载 RGB LED；业务占用会影响板载灯。 |
| 10 | `GPIO37` | `SPIDQS`、`FSPIQ`、`SUBSPIQ` | 带 SPI 复用。 |
| 11 | `GPIO36` | `SPIIO7`、`FSPICLK`、`SUBSPICLK` | 带 SPI 时钟/数据复用。 |
| 12 | `GPIO35` | `SPIIO6`、`FSPID`、`SUBSPID` | 带 SPI 数据复用。 |
| 13 | `GPIO0` | `BOOT` | 启动模式相关；不要随意接上拉/下拉或关键输出。 |
| 14 | `GPIO45` | `VSPI` | 启动/复用功能相关，谨慎使用。 |
| 15 | `GPIO48` | `SPICLK_N` | SPI 差分/时钟相关标记，谨慎使用。 |
| 16 | `GPIO47` | `SPICLK_P` | SPI 差分/时钟相关标记，谨慎使用。 |
| 17 | `GPIO21` | `RTC` | 可作为普通 GPIO，低功耗场景也可能用到 RTC 功能。 |
| 18 | `GPIO20` | `USB D+`、`RTC`、`U1CTS`、`ADC2_9`、`CLK_OUT1` | 原生 USB D+，不建议普通业务占用。 |
| 19 | `GPIO19` | `USB D-`、`RTC`、`U1RTS`、`ADC2_8`、`CLK_OUT2` | 原生 USB D-，不建议普通业务占用。 |

## 6. 本项目选引脚时的建议

- 热敏打印头、电机驱动、加热电源使能等关键输出，优先选择普通 GPIO，避开 `BOOT`、`LOG`、`USB D+`、`USB D-`、`U0TXD`、`U0RXD`、JTAG 相关引脚。
- 如果某个引脚带 `ADC` 标记，它更适合接纸张检测、温度检测、电池电压检测这类输入信号；不要直接接超过 3.3 V 的电压。
- 如果后续需要保留 USB 下载/调试能力，不要占用 `GPIO19` 和 `GPIO20`。
- 如果后续需要保留 JTAG 调试能力，不要占用 `GPIO39`、`GPIO40`、`GPIO41`、`GPIO42`，也要注意图中标注 `JTAG` 的 `GPIO3`。
- `GPIO0` 会影响启动模式，除非非常明确，否则不要用于热敏头加热、电机方向、片选等关键控制。
