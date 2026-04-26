#include "services/sensor_service.h"

#include "config/safety_limits.h"
#include "drivers/sensors/drv_sensor_types.h"
#include "rtos/rtos_objects.h"

namespace {

// 当前阶段的基础合法范围。
//
// 这些值不是最终产品参数，只是用来过滤明显不合理的 mock/读数：
// - 温度低于 -20℃ 或高于 120℃ 通常说明读数异常。
// - 电池电压低于 3V 或高于 9V 对当前 2S 电池假设明显不合理。
constexpr float kHeadTempValidMinC = -20.0F;
constexpr float kHeadTempValidMaxC = 120.0F;
constexpr std::uint32_t kBatteryValidMinMv = 3000U;
constexpr std::uint32_t kBatteryValidMaxMv = 9000U;

mp::SensorSnapshot g_latestSnapshot = {};

// 把 FreeRTOS Tick 转成毫秒。
std::uint32_t NowMs() {
  return static_cast<std::uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

// 判断温度读数是否在基础合法范围内。
//
// NaN 与任意数比较都会返回 false，因此这里无需额外引入 <cmath>。
bool IsTempReadingValid(float tempC) {
  return tempC >= kHeadTempValidMinC && tempC <= kHeadTempValidMaxC;
}

// 判断电池电压读数是否在基础合法范围内。
bool IsBatteryReadingValid(std::uint32_t batteryMv) {
  return batteryMv >= kBatteryValidMinMv && batteryMv <= kBatteryValidMaxMv;
}

// 根据最新快照更新系统事件位。
//
// EventGroup 的每一位代表一个条件是否成立：
// - 条件成立就 set。
// - 条件不成立就 clear。
void UpdateSystemEvents(const mp::SensorSnapshot& snapshot) {
  if (mp::g_rtos.systemEvents == nullptr) {
    return;
  }

  EventBits_t setBits = 0U;
  EventBits_t clearBits = 0U;

  if (snapshot.validity == mp::SensorValidity::VALID && snapshot.paperPresent) {
    setBits |= mp::EVT_PAPER_PRESENT;
  } else {
    clearBits |= mp::EVT_PAPER_PRESENT;
  }

  if (snapshot.validity == mp::SensorValidity::VALID &&
      snapshot.headTempC < mp::SAFETY_DEFAULT_TEMP_STOP_THRESHOLD_C) {
    setBits |= mp::EVT_TEMP_OK;
  } else {
    clearBits |= mp::EVT_TEMP_OK;
  }

  if (snapshot.validity == mp::SensorValidity::VALID &&
      snapshot.batteryMv >= mp::SAFETY_DEFAULT_LOW_BATTERY_STOP_MV) {
    setBits |= mp::EVT_BAT_OK;
    clearBits |= mp::EVT_LOW_POWER;
  } else if (snapshot.validity == mp::SensorValidity::VALID) {
    clearBits |= mp::EVT_BAT_OK;
    setBits |= mp::EVT_LOW_POWER;
  } else {
    clearBits |= mp::EVT_BAT_OK;
    clearBits |= mp::EVT_LOW_POWER;
  }

  if (snapshot.validity == mp::SensorValidity::VALID && !snapshot.motorFault) {
    setBits |= mp::EVT_DRV_READY;
  } else {
    clearBits |= mp::EVT_DRV_READY;
  }

  if (clearBits != 0U) {
    xEventGroupClearBits(mp::g_rtos.systemEvents, clearBits);
  }

  if (setBits != 0U) {
    xEventGroupSetBits(mp::g_rtos.systemEvents, setBits);
  }
}

// 将最新快照发送到 qSensor。
//
// qSensor 在 Step 10 被设计成长度为 1 的队列，
// xQueueOverwrite 会覆盖旧元素，保证队列里始终保留最新状态。
void PublishSnapshot(const mp::SensorSnapshot& snapshot) {
  if (mp::g_rtos.qSensor == nullptr) {
    return;
  }

  xQueueOverwrite(mp::g_rtos.qSensor, &snapshot);
}

}  // namespace

namespace mp {

void SensorService_Init() {
  g_latestSnapshot = {};
  g_latestSnapshot.validity = SensorValidity::STALE;
}

SensorSnapshot SensorService_Poll() {
  SensorDriver& driver = GetSensorDriver();

  SensorSnapshot snapshot = {};
  snapshot.paperPresent = driver.readPaperPresent();
  snapshot.headTempC = driver.readHeadTempC();
  snapshot.batteryMv = driver.readBatteryMv();
  const ChargeStatus chargeStatus = driver.readChargeStatus();
  snapshot.chargeStatus = static_cast<std::uint8_t>(chargeStatus);
  snapshot.charging = chargeStatus == ChargeStatus::CHARGING;
  snapshot.motorFault = driver.readMotorFault();
  snapshot.tickMs = NowMs();

  const bool tempValid = IsTempReadingValid(snapshot.headTempC);
  const bool batteryValid = IsBatteryReadingValid(snapshot.batteryMv);
  snapshot.validity =
      (tempValid && batteryValid) ? SensorValidity::VALID : SensorValidity::INVALID;

  g_latestSnapshot = snapshot;
  UpdateSystemEvents(snapshot);
  PublishSnapshot(snapshot);

  return snapshot;
}

SensorSnapshot SensorService_GetSnapshot() {
  return g_latestSnapshot;
}

}  // namespace mp
