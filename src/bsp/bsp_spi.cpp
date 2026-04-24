#include "bsp/bsp_spi.h"

#include "config/project_features.h"

#if MP_ENABLE_HW_THERMAL_HEAD
#include <SPI.h>

#include "bsp/bsp_gpio.h"
#include "bsp/bsp_pins.h"

namespace {

constexpr std::uint32_t kThermalHeadSpiHz = 2000000UL;

}  // namespace
#endif

namespace mp {

bool Bsp_SpiThermalHeadInit() {
#if MP_ENABLE_HW_THERMAL_HEAD
  if (!Bsp_IsValidPin(PIN_CLK) || !Bsp_IsValidPin(PIN_DI)) {
    return false;
  }

  // 当前先使用 2MHz，优先保证波形容易观察和调试。
  // 后续提速前应先用示波器确认 CLK/DI 时序、线缆完整性和打印头采样边沿。
  //
  // SPI.begin(sck, miso, mosi, ss) 是 Arduino-ESP32 的 SPI 初始化接口：
  // - sck：时钟输出，这里接打印头 CLK。
  // - miso：本项目打印头暂不回读数据，因此传 -1。
  // - mosi：主机输出数据，这里接打印头 DI。
  // - ss：片选脚，本项目暂不用硬件片选，因此传 -1。
  SPI.begin(PIN_CLK, -1, PIN_DI, -1);
  return true;
#else
  return false;
#endif
}

bool Bsp_SpiThermalHeadWrite(const std::uint8_t* data, std::size_t len) {
#if MP_ENABLE_HW_THERMAL_HEAD
  if (data == nullptr || len == 0U) {
    return false;
  }

  // beginTransaction/endTransaction 用于声明本次 SPI 传输的频率、位序和模式。
  // 这样即使后续系统里还有其他 SPI 设备，也能在每次传输前恢复正确配置。
  SPI.beginTransaction(SPISettings(kThermalHeadSpiHz, SPI_MSBFIRST, SPI_MODE0));
  SPI.writeBytes(data, len);
  SPI.endTransaction();
  return true;
#else
  (void)data;
  (void)len;
  return false;
#endif
}

}  // namespace mp
