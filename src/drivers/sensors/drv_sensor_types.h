#pragma once

#include <cstdint>

namespace mp {

// 充电状态。
//
// 这个枚举是传感器驱动对电源管理芯片状态的抽象。
// 当前 mock 默认返回 UNKNOWN，后续真实驱动可根据 BAT_STAT 等引脚完善。
enum class ChargeStatus : uint8_t {
  UNKNOWN = 0,
  NOT_CHARGING,
  CHARGING,
  FULL,
  FAULT,
};

// 传感器硬件读取接口。
//
// Driver 层只负责“读取当前硬件观察值”，不判断系统是否允许打印。
// 例如缺纸是否要停机、温度是否过高、电压是否限制电机速度，
// 都应由 service / app / safety 层根据这些读数做决策。
class SensorDriver {
 public:
  virtual ~SensorDriver() = default;

  // 读取纸张是否存在。
  //
  // 返回 true 表示检测到有纸，false 表示缺纸。
  virtual bool readPaperPresent() = 0;

  // 读取打印头温度，单位摄氏度。
  virtual float readHeadTempC() = 0;

  // 读取电池电压，单位 mV。
  virtual uint16_t readBatteryMv() = 0;

  // 读取充电状态。
  virtual ChargeStatus readChargeStatus() = 0;

  // 读取电机驱动故障状态。
  virtual bool readMotorFault() = 0;
};

// Sensor mock 默认配置。
//
// paperPresent = true 表示“paper missing = false”。
// 这些默认值只用于骨架阶段，让上层可以先跑通流程。
struct SensorMockConfig {
  bool paperPresent;
  float headTempC;
  uint16_t batteryMv;
  ChargeStatus chargeStatus;
  bool motorFault;
};

// Sensor mock 调用统计。
//
// 这些字段只用于测试和调试，业务代码不要依赖它们。
struct SensorMockStats {
  uint32_t readPaperPresentCount;
  uint32_t readHeadTempCCount;
  uint32_t readBatteryMvCount;
  uint32_t readChargeStatusCount;
  uint32_t readMotorFaultCount;
};

// 获取当前传感器驱动实例。
//
// 当前 Step 09 返回 mock 实例。
// 后续替换真实驱动时，上层仍然只通过 SensorDriver 接口调用。
SensorDriver& GetSensorDriver();

// 读取 / 修改 mock 配置。
//
// 修改配置是为了后续测试不同场景，例如缺纸、过温、低电压。
const SensorMockConfig& SensorMock_GetConfig();
void SensorMock_SetConfig(const SensorMockConfig& config);
void SensorMock_ResetConfig();

// 读取 / 清零 mock 统计。
const SensorMockStats& SensorMock_GetStats();
void SensorMock_ResetStats();

}  // namespace mp
