#include "services/wifi_api_service.h"

#include "config/project_features.h"

#if MP_ENABLE_WIFI
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <uri/UriBraces.h>

#include <cstring>

#include "app/app_factory_test.h"
#include "app/app_print.h"
#include "app/app_system.h"
#include "common/app_error.h"
#include "config/safety_limits.h"
#include "protocol/proto_frame.h"
#include "rtos/rtos_objects.h"
#include "services/error_service.h"
#include "services/health_service.h"
#include "services/key_service.h"
#include "services/log_service.h"
#include "services/param_service.h"
#include "services/print_file_service.h"
#include "services/print_service.h"
#include "services/print_spooler.h"
#include "services/sensor_service.h"

namespace {

WebServer g_server(80);
bool g_started = false;
std::uint32_t g_nextWifiJobId = 1U;
std::uint32_t g_currentWifiJobId = 0U;
std::uint32_t g_currentWifiFileId = 0U;

// Arduino WebServer 只有在路由注册了 raw 回调时，
// 才会把 application/octet-stream 按真实字节流交给业务代码。
// 这里用一个固定缓冲保存当前请求体，避免把二进制数据放进 String。
struct RawBodyBuffer {
  std::uint8_t data[mp::PRINT_FILE_MAX_BYTES];
  std::uint32_t len;
  bool valid;
  bool overflow;
  bool aborted;
};

RawBodyBuffer g_rawBody = {};

// 工厂真实打印测试使用的固定样张缓冲。
//
// 不放在函数栈上，是因为 HTTP/WebServer 任务已经比较吃栈；
// 3KB 固定数组放全局区更容易看清内存占用，也避免打印测试时栈溢出。
std::uint8_t g_factoryPrintData[mp::PRINT_FILE_MAX_BYTES] = {};

void SendJson(int status, const String& json);

const char* SystemStateText(mp::SystemState state) {
  switch (state) {
    case mp::SystemState::BOOT:
      return "BOOT";
    case mp::SystemState::INIT:
      return "INIT";
    case mp::SystemState::SELF_TEST:
      return "SELF_TEST";
    case mp::SystemState::IDLE:
      return "IDLE";
    case mp::SystemState::READY:
      return "READY";
    case mp::SystemState::RUNNING:
      return "RUNNING";
    case mp::SystemState::ERROR:
      return "ERROR";
    case mp::SystemState::RECOVERY:
      return "RECOVERY";
    case mp::SystemState::SAFE_MODE:
      return "SAFE_MODE";
    case mp::SystemState::LOW_POWER:
      return "LOW_POWER";
    case mp::SystemState::SHUTDOWN:
      return "SHUTDOWN";
    default:
      return "UNKNOWN";
  }
}

const char* PrintStateText(mp::PrintJobState state) {
  switch (state) {
    case mp::PrintJobState::NONE:
      return "NONE";
    case mp::PrintJobState::RECEIVING:
      return "RECEIVING";
    case mp::PrintJobState::QUEUED:
      return "QUEUED";
    case mp::PrintJobState::PRINTING:
      return "PRINTING";
    case mp::PrintJobState::PAUSED:
      return "PAUSED";
    case mp::PrintJobState::DONE:
      return "DONE";
    case mp::PrintJobState::CANCELLED:
      return "CANCELLED";
    case mp::PrintJobState::FAILED:
      return "FAILED";
    default:
      return "UNKNOWN";
  }
}

const char* ErrorSeverityText(mp::ErrorSeverity severity) {
  switch (severity) {
    case mp::ErrorSeverity::NONE:
      return "NONE";
    case mp::ErrorSeverity::INFO:
      return "INFO";
    case mp::ErrorSeverity::WARN:
      return "WARN";
    case mp::ErrorSeverity::ERROR:
      return "ERROR";
    case mp::ErrorSeverity::FATAL:
      return "FATAL";
    default:
      return "UNKNOWN";
  }
}

const char* TaskIdText(mp::TaskId id) {
  switch (id) {
    case mp::TaskId::SYSTEM:
      return "SYSTEM";
    case mp::TaskId::LOGGER:
      return "LOGGER";
    case mp::TaskId::MONITOR:
      return "MONITOR";
    case mp::TaskId::ERROR:
      return "ERROR";
    case mp::TaskId::COMMAND:
      return "COMMAND";
    case mp::TaskId::PRINT_CTRL:
      return "PRINT_CTRL";
    case mp::TaskId::SENSOR:
      return "SENSOR";
    case mp::TaskId::PARAM:
      return "PARAM";
    case mp::TaskId::COMM:
      return "COMM";
    case mp::TaskId::WIFI_API:
      return "WIFI_API";
    case mp::TaskId::METRICS:
      return "METRICS";
    default:
      return "UNKNOWN";
  }
}

const char* KeyEventText(mp::KeyEvent event) {
  switch (event) {
    case mp::KeyEvent::NONE:
      return "NONE";
    case mp::KeyEvent::SHORT_PRESS:
      return "SHORT_PRESS";
    case mp::KeyEvent::LONG_PRESS:
      return "LONG_PRESS";
    default:
      return "UNKNOWN";
  }
}

const char* ChargeStatusText(std::uint8_t status) {
  switch (status) {
    case 0:
      return "UNKNOWN";
    case 1:
      return "NOT_CHARGING";
    case 2:
      return "CHARGING";
    case 3:
      return "FULL";
    case 4:
      return "FAULT";
    default:
      return "UNKNOWN";
  }
}

void MarkApiConnected() {
  if (mp::g_rtos.systemEvents != nullptr) {
    xEventGroupSetBits(mp::g_rtos.systemEvents, mp::EVT_COMM_CONNECTED);
  }
}

String JsonHeader(bool ok, const char* code, const char* message) {
  String json = "{";
  json += "\"ok\":";
  json += ok ? "true" : "false";
  json += ",\"code\":\"";
  json += code;
  json += "\",\"message\":\"";
  json += message;
  json += "\"";
  return json;
}

String JsonQuoted(const char* text) {
  String json = "\"";
  const char* cursor = (text != nullptr) ? text : "";
  while (*cursor != '\0') {
    const char ch = *cursor++;
    if (ch == '"' || ch == '\\') {
      json += "\\";
      json += ch;
    } else if (static_cast<unsigned char>(ch) >= 32U) {
      json += ch;
    }
  }
  json += "\"";
  return json;
}

void SendErrorJson(int status, const char* code, const char* message,
                   mp::AppErrorCode error) {
  String json = JsonHeader(false, code, message);
  json += ",\"error\":";
  json += error;
  json += "}";
  SendJson(status, json);
}

bool IsSafeModeActive() {
  return mp::SystemApp_GetState() == mp::SystemState::SAFE_MODE ||
         mp::Error_IsSafeModeRequired();
}

bool RequireNotSafeMode(const char* action) {
  if (!IsSafeModeActive()) {
    return true;
  }

  String json = JsonHeader(false, "SAFE_MODE_BLOCKED", action);
  json += ",\"system_state\":\"";
  json += SystemStateText(mp::SystemApp_GetState());
  json += "\"}";
  SendJson(409, json);
  return false;
}

void SendJson(int status, const String& json) {
  MarkApiConnected();
  g_server.send(status, "application/json", json);
}

void ResetRawBody() {
  g_rawBody.len = 0U;
  g_rawBody.valid = false;
  g_rawBody.overflow = false;
  g_rawBody.aborted = false;
}

void HandleRawBody() {
  HTTPRaw& raw = g_server.raw();

  if (raw.status == RAW_START) {
    ResetRawBody();
    return;
  }

  if (raw.status == RAW_ABORTED) {
    g_rawBody.aborted = true;
    g_rawBody.valid = false;
    return;
  }

  if (raw.status == RAW_WRITE) {
    if (raw.currentSize >
        (mp::PRINT_FILE_MAX_BYTES - g_rawBody.len)) {
      g_rawBody.overflow = true;
      return;
    }

    std::memcpy(&g_rawBody.data[g_rawBody.len], raw.buf, raw.currentSize);
    g_rawBody.len += static_cast<std::uint32_t>(raw.currentSize);
    return;
  }

  if (raw.status == RAW_END) {
    g_rawBody.valid = !g_rawBody.overflow && !g_rawBody.aborted;
  }
}

bool RequireRawBody() {
  if (g_rawBody.aborted) {
    SendErrorJson(400, "BAD_REQUEST", "raw body upload was aborted",
                  mp::ERR_COMM_CRC);
    ResetRawBody();
    return false;
  }

  if (g_rawBody.overflow) {
    SendErrorJson(400, "BAD_REQUEST", "raw body is too large",
                  mp::ERR_COMM_FRAME_TOO_LONG);
    ResetRawBody();
    return false;
  }

  if (!g_rawBody.valid || g_rawBody.len == 0U) {
    SendErrorJson(400, "BAD_REQUEST",
                  "application/octet-stream binary body is required",
                  mp::ERR_COMM_CRC);
    ResetRawBody();
    return false;
  }

  return true;
}

bool ParseU32Text(const String& text, std::uint32_t* out) {
  if (out == nullptr || text.length() == 0U) {
    return false;
  }

  const bool isHex = text.length() > 2U && text[0] == '0' &&
                     (text[1] == 'x' || text[1] == 'X');
  const std::uint8_t base = isHex ? 16U : 10U;
  std::uint32_t value = 0U;

  for (unsigned index = isHex ? 2U : 0U; index < text.length(); ++index) {
    const char ch = text[index];
    std::uint8_t digit = 0U;
    if (ch >= '0' && ch <= '9') {
      digit = static_cast<std::uint8_t>(ch - '0');
    } else if (base == 16U && ch >= 'a' && ch <= 'f') {
      digit = static_cast<std::uint8_t>(10U + ch - 'a');
    } else if (base == 16U && ch >= 'A' && ch <= 'F') {
      digit = static_cast<std::uint8_t>(10U + ch - 'A');
    } else {
      return false;
    }

    if (digit >= base) {
      return false;
    }
    value = static_cast<std::uint32_t>(value * base + digit);
  }

  *out = value;
  return true;
}

bool ParseOptionalU32Arg(const char* name, std::uint32_t defaultValue,
                         std::uint32_t* out) {
  if (out == nullptr) {
    return false;
  }

  if (!g_server.hasArg(name)) {
    *out = defaultValue;
    return true;
  }

  return ParseU32Text(g_server.arg(name), out);
}

String SensorJsonFields(const mp::SensorSnapshot& sensor) {
  String json;
  json += ",\"paper\":";
  json += sensor.paperPresent ? "true" : "false";
  json += ",\"head_temp_c\":";
  json += String(sensor.headTempC, 1);
  json += ",\"battery_mv\":";
  json += sensor.batteryMv;
  json += ",\"charge_status\":\"";
  json += ChargeStatusText(sensor.chargeStatus);
  json += "\",\"charge_status_raw\":";
  json += sensor.chargeStatus;
  json += ",\"charging\":";
  json += sensor.charging ? "true" : "false";
  json += ",\"motor_fault\":";
  json += sensor.motorFault ? "true" : "false";
  json += ",\"validity\":";
  json += static_cast<unsigned>(sensor.validity);
  json += ",\"tick_ms\":";
  json += sensor.tickMs;
  return json;
}

String SensorSnapshotJson(const mp::SensorSnapshot& sensor) {
  String json = "{\"summary\":true";
  json += SensorJsonFields(sensor);
  json += "}";
  return json;
}

String KeyJson(const mp::KeySnapshot& key) {
  String json = "{";
  json += "\"raw_level_high\":";
  json += key.rawLevelHigh ? "true" : "false";
  json += ",\"pressed\":";
  json += key.pressed ? "true" : "false";
  json += ",\"active_low_assumed\":";
  json += key.activeLowAssumed ? "true" : "false";
  json += ",\"last_event\":\"";
  json += KeyEventText(key.lastEvent);
  json += "\",\"last_event_tick_ms\":";
  json += key.lastEventTickMs;
  json += ",\"press_duration_ms\":";
  json += key.pressDurationMs;
  json += "}";
  return json;
}

String ErrorEventJson(const mp::ErrorEvent& error) {
  String json = "{";
  json += "\"code\":";
  json += error.code;
  json += ",\"severity\":\"";
  json += ErrorSeverityText(error.severity);
  json += "\",\"module\":";
  json += JsonQuoted(error.module);
  json += ",\"detail\":";
  json += JsonQuoted(error.detail);
  json += ",\"timestamp\":";
  json += error.timestamp;
  json += ",\"safe_mode\":";
  json += mp::Error_IsSafeModeRequired() ? "true" : "false";
  json += "}";
  return json;
}

String ParamJson(const mp::ParamBlock& params) {
  String json = "{";
  json += "\"magic\":";
  json += params.magic;
  json += ",\"version\":";
  json += params.version;
  json += ",\"crc32\":";
  json += params.crc32;
  json += ",\"print\":{\"dots_per_line\":";
  json += params.print.dotsPerLine;
  json += ",\"bytes_per_line\":";
  json += params.print.bytesPerLine;
  json += ",\"stb_group_count\":";
  json += params.print.stbGroupCount;
  json += "},\"motor\":{\"steps_per_line\":";
  json += params.motor.stepsPerPrintLine;
  json += ",\"start_interval_us\":";
  json += params.motor.startPhaseIntervalUs;
  json += ",\"run_interval_us\":";
  json += params.motor.runPhaseIntervalUs;
  json += ",\"fast_interval_us\":";
  json += params.motor.fastPhaseIntervalUs;
  json += "},\"safety\":{\"max_heat_dots\":";
  json += params.safety.maxSimultaneousHeatDots;
  json += ",\"temp_stop_c\":";
  json += params.safety.tempStopThresholdC;
  json += ",\"temp_resume_c\":";
  json += params.safety.tempResumeThresholdC;
  json += ",\"heat_start_us\":";
  json += params.safety.heatPulseStartUs;
  json += ",\"heat_max_us\":";
  json += params.safety.heatPulseMaxUs;
  json += ",\"forbid_heat_paper_missing\":";
  json += params.safety.forbidHeatWhenPaperMissing;
  json += ",\"forbid_heat_low_voltage\":";
  json += params.safety.forbidHeatWhenLowVoltage;
  json += ",\"forbid_fast_motor_low_voltage\":";
  json += params.safety.forbidFastMotorWhenLowVoltage;
  json += "},\"comm\":{\"max_frame_length\":";
  json += params.comm.maxFrameLength;
  json += ",\"inter_byte_timeout_ms\":";
  json += params.comm.interByteTimeout;
  json += ",\"enable_crc\":";
  json += params.comm.enableCrc;
  json += "}}";
  return json;
}

const char* PrintFileStateText(mp::PrintFileState state) {
  switch (state) {
    case mp::PrintFileState::EMPTY:
      return "EMPTY";
    case mp::PrintFileState::RECEIVING:
      return "RECEIVING";
    case mp::PrintFileState::COMPLETE:
      return "COMPLETE";
    default:
      return "UNKNOWN";
  }
}

String PrintFileJson(const mp::PrintFileSnapshot& file) {
  String json = "{";
  json += "\"file_id\":";
  json += file.fileId;
  json += ",\"state\":\"";
  json += PrintFileStateText(file.state);
  json += "\",\"total_bytes\":";
  json += file.totalBytes;
  json += ",\"received_bytes\":";
  json += file.receivedBytes;
  json += ",\"line_count\":";
  json += file.lineCount;
  json += ",\"expected_crc32\":";
  json += file.expectedCrc32;
  json += ",\"actual_crc32\":";
  json += file.actualCrc32;
  json += "}";
  return json;
}

void HandleInfo() {
  String json = JsonHeader(true, "OK", "MiniPrinterRTOS WiFi API");
  json += ",\"device\":\"MiniPrinterRTOS\"";
  json += ",\"api_prefix\":\"/api/v1\"";
  json += ",\"wifi_control_surface\":\"official\"";
  json += ",\"features\":{";
  json += "\"wifi\":";
  json += mp::FEATURE_WIFI ? "true" : "false";
  json += ",\"hw_thermal_head\":";
  json += mp::FEATURE_HW_THERMAL_HEAD ? "true" : "false";
  json += ",\"hw_stepper\":";
  json += mp::FEATURE_HW_STEPPER ? "true" : "false";
  json += ",\"hw_sensors\":";
  json += mp::FEATURE_HW_SENSORS ? "true" : "false";
  json += ",\"wdt\":";
  json += mp::FEATURE_WDT ? "true" : "false";
  json += "}}";
  SendJson(200, json);
}

void HandleStatus() {
  const mp::SensorSnapshot sensor = mp::SensorService_GetSnapshot();
  const mp::KeySnapshot key = mp::KeyService_GetSnapshot();
  const mp::PrintAppSnapshot print = mp::PrintApp_GetSnapshot();
  const mp::ErrorEvent error = mp::Error_GetLast();

  String json = JsonHeader(true, "OK", "status");
  json += ",\"system_state\":\"";
  json += SystemStateText(mp::SystemApp_GetState());
  json += "\",\"print\":{\"state\":\"";
  json += PrintStateText(print.state);
  json += "\",\"job_id\":";
  json += print.jobId;
  json += ",\"line_done\":";
  json += print.printedLines;
  json += ",\"end_requested\":";
  json += print.endRequested ? "true" : "false";
  json += ",\"last_error\":";
  json += print.lastError;
  json += "}";
  json += ",\"sensors\":{";
  json += "\"summary\":true";
  json += SensorJsonFields(sensor);
  json += "}";
  json += ",\"key\":";
  json += KeyJson(key);
  json += ",\"error\":{\"code\":";
  json += error.code;
  json += ",\"severity\":\"";
  json += ErrorSeverityText(error.severity);
  json += "\",\"safe_mode\":";
  json += mp::Error_IsSafeModeRequired() ? "true" : "false";
  json += "}}";
  SendJson(200, json);
}

void HandleKey() {
  String json = JsonHeader(true, "OK", "key");
  json += ",\"key\":";
  json += KeyJson(mp::KeyService_GetSnapshot());
  json += "}";
  SendJson(200, json);
}

void HandleSensors() {
  const mp::SensorSnapshot sensor = mp::SensorService_GetSnapshot();
  String json = JsonHeader(true, "OK", "sensors");
  json += ",\"sensors\":";
  json += SensorSnapshotJson(sensor);
  json += "}";
  SendJson(200, json);
}

void HandleBattery() {
  const mp::SensorSnapshot sensor = mp::SensorService_GetSnapshot();
  EventBits_t bits = 0U;
  if (mp::g_rtos.systemEvents != nullptr) {
    bits = xEventGroupGetBits(mp::g_rtos.systemEvents);
  }

  String json = JsonHeader(true, "OK", "battery");
  json += ",\"battery_mv\":";
  json += sensor.batteryMv;
  json += ",\"battery_ok\":";
  json += ((bits & mp::EVT_BAT_OK) != 0U) ? "true" : "false";
  json += ",\"low_voltage_stop_mv\":";
  json += mp::SAFETY_DEFAULT_LOW_BATTERY_STOP_MV;
  json += ",\"charge_status\":\"";
  json += ChargeStatusText(sensor.chargeStatus);
  json += "\",\"charge_status_raw\":";
  json += sensor.chargeStatus;
  json += ",\"charging\":";
  json += sensor.charging ? "true" : "false";
  json += ",\"low_power\":";
  json += ((bits & mp::EVT_LOW_POWER) != 0U) ? "true" : "false";
  json += ",\"validity\":";
  json += static_cast<unsigned>(sensor.validity);
  json += "}";
  SendJson(200, json);
}

void HandleErrorGet() {
  String json = JsonHeader(true, "OK", "error");
  json += ",\"error\":";
  json += ErrorEventJson(mp::Error_GetLast());
  json += "}";
  SendJson(200, json);
}

void HandleErrorClear() {
  mp::Error_ClearRecoverable();
  String json = JsonHeader(true, "OK", "recoverable error cleared");
  json += ",\"error\":";
  json += ErrorEventJson(mp::Error_GetLast());
  json += "}";
  SendJson(200, json);
}

void HandlePrintFilesCreate() {
  std::uint32_t totalBytes = 0U;
  std::uint32_t expectedCrc32 = 0U;
  if (!g_server.hasArg("size") || !g_server.hasArg("crc32") ||
      !ParseU32Text(g_server.arg("size"), &totalBytes) ||
      !ParseU32Text(g_server.arg("crc32"), &expectedCrc32)) {
    SendErrorJson(400, "BAD_REQUEST",
                  "size and crc32 query args are required",
                  mp::ERR_COMM_FRAME_TOO_LONG);
    return;
  }

  std::uint32_t fileId = 0U;
  const mp::AppErrorCode result =
      mp::PrintFileService_Create(totalBytes, expectedCrc32, &fileId);
  if (result != mp::APP_OK) {
    SendErrorJson(400, "PRINT_FILE_CREATE_FAILED",
                  "invalid size or no upload slot", result);
    return;
  }

  mp::PrintFileSnapshot snapshot = {};
  (void)mp::PrintFileService_Get(fileId, &snapshot);
  String json = JsonHeader(true, "OK", "print file upload created");
  json += ",\"max_bytes\":";
  json += mp::PRINT_FILE_MAX_BYTES;
  json += ",\"chunk_size\":";
  json += mp::PRINT_FILE_CHUNK_SIZE;
  json += ",\"file\":";
  json += PrintFileJson(snapshot);
  json += "}";
  SendJson(201, json);
}

void HandlePrintFilesList() {
  mp::PrintFileSnapshot files[mp::PRINT_FILE_SLOT_COUNT] = {};
  const std::size_t count =
      mp::PrintFileService_List(files, mp::PRINT_FILE_SLOT_COUNT);

  String json = JsonHeader(true, "OK", "print files");
  json += ",\"max_bytes\":";
  json += mp::PRINT_FILE_MAX_BYTES;
  json += ",\"chunk_size\":";
  json += mp::PRINT_FILE_CHUNK_SIZE;
  json += ",\"files\":[";
  for (std::size_t index = 0U; index < count; ++index) {
    if (index != 0U) {
      json += ",";
    }
    json += PrintFileJson(files[index]);
  }
  json += "]}";
  SendJson(200, json);
}

void HandlePrintFileChunk(std::uint32_t fileId, std::uint32_t index) {
  if (!RequireRawBody()) {
    return;
  }

  const mp::AppErrorCode result = mp::PrintFileService_WriteChunk(
      fileId, index, g_rawBody.data, g_rawBody.len);
  ResetRawBody();
  if (result != mp::APP_OK) {
    SendErrorJson(400, "PRINT_FILE_CHUNK_FAILED",
                  "chunk rejected by upload service", result);
    return;
  }

  mp::PrintFileSnapshot snapshot = {};
  (void)mp::PrintFileService_Get(fileId, &snapshot);
  String json = JsonHeader(true, "OK", "chunk stored");
  json += ",\"file\":";
  json += PrintFileJson(snapshot);
  json += "}";
  SendJson(200, json);
}

void HandlePrintFileChunkRoute() {
  std::uint32_t fileId = 0U;
  std::uint32_t chunkIndex = 0U;
  if (!ParseU32Text(g_server.pathArg(0), &fileId) ||
      !ParseU32Text(g_server.pathArg(1), &chunkIndex)) {
    SendErrorJson(400, "BAD_REQUEST", "invalid file_id or chunk index",
                  mp::ERR_COMM_CRC);
    ResetRawBody();
    return;
  }

  HandlePrintFileChunk(fileId, chunkIndex);
}

void HandlePrintFileComplete(std::uint32_t fileId) {
  const mp::AppErrorCode result = mp::PrintFileService_Complete(fileId);
  if (result != mp::APP_OK) {
    SendErrorJson(400, "PRINT_FILE_COMPLETE_FAILED",
                  "upload is incomplete or crc32 mismatch", result);
    return;
  }

  mp::PrintFileSnapshot snapshot = {};
  (void)mp::PrintFileService_Get(fileId, &snapshot);
  String json = JsonHeader(true, "OK", "print file complete");
  json += ",\"file\":";
  json += PrintFileJson(snapshot);
  json += "}";
  SendJson(200, json);
}

void HandlePrintFileDelete(std::uint32_t fileId) {
  const mp::AppErrorCode result = mp::PrintFileService_Delete(fileId);
  if (result != mp::APP_OK) {
    SendErrorJson(404, "PRINT_FILE_NOT_FOUND", "file_id not found", result);
    return;
  }

  String json = JsonHeader(true, "OK", "print file deleted");
  json += ",\"file_id\":";
  json += fileId;
  json += "}";
  SendJson(200, json);
}

void HandlePrintJobCreate() {
  if (!RequireNotSafeMode("start print is blocked in SAFE_MODE")) {
    return;
  }

  if (mp::PrintApp_IsActive()) {
    SendErrorJson(409, "PRINT_BUSY", "current print job is active",
                  mp::ERR_QUEUE_FULL);
    return;
  }

  std::uint32_t fileId = 0U;
  std::uint32_t copies = 1U;
  std::uint32_t density = 50U;
  std::uint32_t heat = 50U;
  if (!g_server.hasArg("file_id") ||
      !ParseU32Text(g_server.arg("file_id"), &fileId) ||
      !ParseOptionalU32Arg("copies", 1U, &copies) ||
      !ParseOptionalU32Arg("density", 50U, &density) ||
      !ParseOptionalU32Arg("heat", 50U, &heat)) {
    SendErrorJson(400, "BAD_REQUEST",
                  "file_id is required; copies/density/heat must be numbers",
                  mp::ERR_COMM_CRC);
    return;
  }

  if (copies != 1U || density > 100U || heat > 100U) {
    SendErrorJson(400, "PARAM_OUT_OF_RANGE",
                  "v1 supports copies=1 and density/heat in 0..100",
                  mp::ERR_COMM_FRAME_TOO_LONG);
    return;
  }

  mp::PrintFileSnapshot file = {};
  if (!mp::PrintFileService_Get(fileId, &file) ||
      file.state != mp::PrintFileState::COMPLETE) {
    SendErrorJson(404, "PRINT_FILE_NOT_READY",
                  "file_id is missing or not COMPLETE", mp::ERR_COMM_CRC);
    return;
  }

  const std::uint32_t jobId = g_nextWifiJobId++;
  if (!mp::PrintService_RequestStart(jobId)) {
    SendErrorJson(500, "PRINT_START_FAILED",
                  "failed to queue print start", mp::ERR_QUEUE_FULL);
    return;
  }

  const mp::AppErrorCode queueResult =
      mp::PrintFileService_QueueForPrint(fileId, jobId);
  if (queueResult != mp::APP_OK) {
    (void)mp::PrintService_RequestCancel(jobId);
    SendErrorJson(500, "PRINT_QUEUE_FAILED",
                  "failed to queue file lines for print", queueResult);
    return;
  }

  if (!mp::PrintService_RequestEnd(jobId)) {
    (void)mp::PrintService_RequestCancel(jobId);
    SendErrorJson(500, "PRINT_END_FAILED", "failed to queue print end",
                  mp::ERR_QUEUE_FULL);
    return;
  }

  g_currentWifiJobId = jobId;
  g_currentWifiFileId = fileId;

  String json = JsonHeader(true, "OK", "print job queued");
  json += ",\"job_id\":";
  json += jobId;
  json += ",\"file_id\":";
  json += fileId;
  json += ",\"line_total\":";
  json += file.lineCount;
  json += ",\"copies\":1}";
  SendJson(202, json);
}

void FillFactoryVisiblePrintData(std::uint32_t lines) {
  // 每行 48 字节 = 384 dots。
  // 这里生成 C0 00 重复的低密度竖向点迹：
  // - 0xC0 的二进制是 11000000，表示每 16 个点里只打前 2 个点。
  // - 点数低，第一次真实打印更温和。
  // - 多行重复后，纸面上比单行更容易看到实际效果。
  const std::uint32_t totalBytes =
      lines * static_cast<std::uint32_t>(mp::LINE_BUFFER_DATA_SIZE);
  for (std::uint32_t index = 0U; index < totalBytes; ++index) {
    g_factoryPrintData[index] = ((index % 2U) == 0U) ? 0xC0U : 0x00U;
  }
}

void HandleFactoryRealPrintTest() {
  if (!RequireNotSafeMode("factory real print is blocked in SAFE_MODE")) {
    return;
  }

  if (!mp::PrintService_IsRealPrintHardwareEnabled()) {
    SendErrorJson(409, "HW_DISABLED",
                  "real print needs hw_stepper and hw_thermal_head",
                  mp::ERR_HW_DISABLED);
    return;
  }

  if (mp::PrintApp_IsActive()) {
    SendErrorJson(409, "PRINT_BUSY", "current print job is active",
                  mp::ERR_QUEUE_FULL);
    return;
  }

  std::uint32_t lines = 16U;
  std::uint32_t density = 25U;
  std::uint32_t heat = 25U;
  if (!ParseOptionalU32Arg("lines", 16U, &lines) ||
      !ParseOptionalU32Arg("density", 25U, &density) ||
      !ParseOptionalU32Arg("heat", 25U, &heat)) {
    SendErrorJson(400, "BAD_REQUEST",
                  "lines/density/heat must be numbers", mp::ERR_COMM_CRC);
    return;
  }

  if (lines == 0U || lines > mp::PRINT_FILE_MAX_LINES || density > 100U ||
      heat > 100U) {
    SendErrorJson(400, "PARAM_OUT_OF_RANGE",
                  "lines must be 1..max; density/heat must be 0..100",
                  mp::ERR_COMM_FRAME_TOO_LONG);
    return;
  }

  FillFactoryVisiblePrintData(lines);

  std::uint32_t fileId = 0U;
  const std::uint32_t totalBytes =
      lines * static_cast<std::uint32_t>(mp::LINE_BUFFER_DATA_SIZE);
  mp::AppErrorCode result = mp::PrintFileService_CreateCompleteFromData(
      g_factoryPrintData, totalBytes, &fileId);
  if (result != mp::APP_OK) {
    SendErrorJson(500, "PRINT_FILE_CREATE_FAILED",
                  "failed to create internal print file", result);
    return;
  }

  const std::uint32_t jobId = g_nextWifiJobId++;
  if (!mp::PrintService_RequestStart(jobId)) {
    (void)mp::PrintFileService_Delete(fileId);
    SendErrorJson(500, "PRINT_START_FAILED",
                  "failed to queue print start", mp::ERR_QUEUE_FULL);
    return;
  }

  result = mp::PrintFileService_QueueForPrint(fileId, jobId);
  if (result != mp::APP_OK) {
    (void)mp::PrintService_RequestCancel(jobId);
    (void)mp::PrintFileService_Delete(fileId);
    SendErrorJson(500, "PRINT_QUEUE_FAILED",
                  "failed to queue factory print lines", result);
    return;
  }

  if (!mp::PrintService_RequestEnd(jobId)) {
    (void)mp::PrintService_RequestCancel(jobId);
    (void)mp::PrintFileService_Delete(fileId);
    SendErrorJson(500, "PRINT_END_FAILED", "failed to queue print end",
                  mp::ERR_QUEUE_FULL);
    return;
  }

  g_currentWifiJobId = jobId;
  g_currentWifiFileId = fileId;

  String json = JsonHeader(true, "OK", "factory real print queued");
  json += ",\"job_id\":";
  json += jobId;
  json += ",\"file_id\":";
  json += fileId;
  json += ",\"line_total\":";
  json += lines;
  json += ",\"total_bytes\":";
  json += totalBytes;
  json += ",\"density\":";
  json += density;
  json += ",\"heat\":";
  json += heat;
  json += "}";
  SendJson(202, json);
}

void HandlePrintJobCurrent() {
  const mp::PrintAppSnapshot print = mp::PrintApp_GetSnapshot();
  String json = JsonHeader(true, "OK", "current print job");
  json += ",\"job_id\":";
  json += print.jobId;
  json += ",\"file_id\":";
  json += g_currentWifiFileId;
  json += ",\"state\":\"";
  json += PrintStateText(print.state);
  json += "\",\"line_done\":";
  json += print.printedLines;
  json += ",\"end_requested\":";
  json += print.endRequested ? "true" : "false";
  json += ",\"error\":";
  json += print.lastError;
  json += "}";
  SendJson(200, json);
}

void HandlePrintJobCancel() {
  const mp::PrintAppSnapshot print = mp::PrintApp_GetSnapshot();
  const std::uint32_t jobId =
      (print.jobId != 0U) ? print.jobId : g_currentWifiJobId;
  if (jobId == 0U || !mp::PrintService_RequestCancel(jobId)) {
    SendErrorJson(409, "PRINT_CANCEL_FAILED",
                  "no current job or cancel queue is full", mp::ERR_QUEUE_FULL);
    return;
  }

  String json = JsonHeader(true, "OK", "print cancel queued");
  json += ",\"job_id\":";
  json += jobId;
  json += ",\"file_id\":";
  json += g_currentWifiFileId;
  json += "}";
  SendJson(202, json);
}

void HandleFeed() {
  if (!RequireNotSafeMode("feed is blocked in SAFE_MODE")) {
    return;
  }

  std::uint32_t steps = 1U;
  if (!ParseOptionalU32Arg("steps", 1U, &steps) || steps == 0U ||
      steps > 200U) {
    SendErrorJson(400, "PARAM_OUT_OF_RANGE",
                  "steps must be in 1..200", mp::ERR_COMM_FRAME_TOO_LONG);
    return;
  }

  if (!mp::PrintService_RequestFeed(steps)) {
    SendErrorJson(500, "FEED_FAILED", "failed to queue feed request",
                  mp::ERR_QUEUE_FULL);
    return;
  }

  String json = JsonHeader(true, "OK", "feed queued");
  json += ",\"steps\":";
  json += steps;
  json += "}";
  SendJson(202, json);
}

bool ApplyU16Arg(const char* name, std::uint16_t minValue,
                 std::uint16_t maxValue, std::uint16_t* field) {
  if (!g_server.hasArg(name)) {
    return true;
  }

  std::uint32_t value = 0U;
  if (!ParseU32Text(g_server.arg(name), &value) || value < minValue ||
      value > maxValue || field == nullptr) {
    return false;
  }

  *field = static_cast<std::uint16_t>(value);
  return true;
}

bool ApplyU8Arg(const char* name, std::uint8_t minValue,
                std::uint8_t maxValue, std::uint8_t* field) {
  if (!g_server.hasArg(name)) {
    return true;
  }

  std::uint32_t value = 0U;
  if (!ParseU32Text(g_server.arg(name), &value) || value < minValue ||
      value > maxValue || field == nullptr) {
    return false;
  }

  *field = static_cast<std::uint8_t>(value);
  return true;
}

bool ApplyI16Arg(const char* name, std::int16_t minValue,
                 std::int16_t maxValue, std::int16_t* field) {
  if (!g_server.hasArg(name)) {
    return true;
  }

  std::uint32_t value = 0U;
  if (!ParseU32Text(g_server.arg(name), &value) ||
      value > static_cast<std::uint32_t>(maxValue) ||
      field == nullptr) {
    return false;
  }

  const std::int16_t signedValue = static_cast<std::int16_t>(value);
  if (signedValue < minValue) {
    return false;
  }

  *field = signedValue;
  return true;
}

bool IsKnownParamArg(const String& name) {
  return name == "max_heat_dots" || name == "temp_stop_c" ||
         name == "temp_resume_c" || name == "heat_start_us" ||
         name == "heat_max_us" || name == "forbid_heat_paper_missing" ||
         name == "forbid_heat_low_voltage" ||
         name == "forbid_fast_motor_low_voltage" ||
         name == "steps_per_line" || name == "start_interval_us" ||
         name == "run_interval_us" || name == "fast_interval_us" ||
         name == "max_frame_length";
}

const char* ParamPatchAllowListText() {
  return "allowed params: max_heat_dots, temp_stop_c, temp_resume_c, "
         "heat_start_us, heat_max_us, forbid_heat_paper_missing, "
         "forbid_heat_low_voltage, forbid_fast_motor_low_voltage, "
         "steps_per_line, start_interval_us, run_interval_us, "
         "fast_interval_us, max_frame_length";
}

void HandleParamsGet() {
  String json = JsonHeader(true, "OK", "params");
  json += ",\"params\":";
  json += ParamJson(mp::Param_GetSnapshot());
  json += "}";
  SendJson(200, json);
}

void HandleParamsPatch() {
  if (!RequireNotSafeMode("parameter changes are blocked in SAFE_MODE")) {
    return;
  }

  for (int index = 0; index < g_server.args(); ++index) {
    if (!IsKnownParamArg(g_server.argName(index))) {
      SendErrorJson(400, "PARAM_NOT_ALLOWED",
                    ParamPatchAllowListText(), mp::ERR_PARAM_CRC);
      return;
    }
  }

  mp::ParamBlock params = mp::Param_GetSnapshot();
  bool ok = true;
  ok = ApplyU16Arg("max_heat_dots", 1U, 64U,
                   &params.safety.maxSimultaneousHeatDots) && ok;
  ok = ApplyI16Arg("temp_stop_c", 40, 80,
                   &params.safety.tempStopThresholdC) && ok;
  ok = ApplyI16Arg("temp_resume_c", 30, 70,
                   &params.safety.tempResumeThresholdC) && ok;
  ok = ApplyU16Arg("heat_start_us", 1U, mp::SAFETY_DEFAULT_HEAT_PULSE_MAX_US,
                   &params.safety.heatPulseStartUs) && ok;
  ok = ApplyU16Arg("heat_max_us", params.safety.heatPulseStartUs,
                   mp::SAFETY_DEFAULT_HEAT_PULSE_MAX_US,
                   &params.safety.heatPulseMaxUs) && ok;
  ok = ApplyU8Arg("forbid_heat_paper_missing", 0U, 1U,
                  &params.safety.forbidHeatWhenPaperMissing) && ok;
  ok = ApplyU8Arg("forbid_heat_low_voltage", 0U, 1U,
                  &params.safety.forbidHeatWhenLowVoltage) && ok;
  ok = ApplyU8Arg("forbid_fast_motor_low_voltage", 0U, 1U,
                  &params.safety.forbidFastMotorWhenLowVoltage) && ok;
  ok = ApplyU8Arg("steps_per_line", 1U, 8U,
                  &params.motor.stepsPerPrintLine) && ok;
  ok = ApplyU16Arg("start_interval_us", 1000U, 20000U,
                   &params.motor.startPhaseIntervalUs) && ok;
  ok = ApplyU16Arg("run_interval_us", 1000U, 20000U,
                   &params.motor.runPhaseIntervalUs) && ok;
  ok = ApplyU16Arg("fast_interval_us", 1000U, 20000U,
                   &params.motor.fastPhaseIntervalUs) && ok;
  ok = ApplyU16Arg("max_frame_length", 64U, mp::PROTO_MAX_FRAME_SIZE,
                   &params.comm.maxFrameLength) && ok;
  ok = ok &&
       params.safety.tempResumeThresholdC < params.safety.tempStopThresholdC;
  ok = ok && params.safety.heatPulseStartUs <= params.safety.heatPulseMaxUs;
  ok = ok && params.motor.fastPhaseIntervalUs <= params.motor.runPhaseIntervalUs;
  ok = ok && params.motor.runPhaseIntervalUs <= params.motor.startPhaseIntervalUs;

  if (!ok) {
    SendErrorJson(400, "PARAM_OUT_OF_RANGE", ParamPatchAllowListText(),
                  mp::ERR_PARAM_CRC);
    return;
  }

  (void)mp::Param_Update(params);
  String json = JsonHeader(true, "OK", "params updated in RAM");
  json += ",\"params\":";
  json += ParamJson(mp::Param_GetSnapshot());
  json += "}";
  SendJson(200, json);
}

void HandleParamsSave() {
  if (!mp::Param_RequestSave()) {
    SendErrorJson(500, "PARAM_SAVE_FAILED", "failed to queue save request",
                  mp::ERR_QUEUE_FULL);
    return;
  }

  String json = JsonHeader(true, "OK", "param save queued");
  json += "}";
  SendJson(202, json);
}

void HandleParamsFactoryReset() {
  if (!mp::Param_ResetDefault()) {
    SendErrorJson(500, "PARAM_RESET_FAILED", "failed to reset params",
                  mp::ERR_QUEUE_FULL);
    return;
  }

  String json = JsonHeader(true, "OK", "default params restored");
  json += ",\"params\":";
  json += ParamJson(mp::Param_GetSnapshot());
  json += "}";
  SendJson(202, json);
}

void HandleSelfTest() {
  if (mp::g_rtos.systemEvents != nullptr) {
    xEventGroupSetBits(mp::g_rtos.systemEvents, mp::EVT_SELFTEST_OK);
  }

  String json = JsonHeader(true, "OK", "self test mock passed");
  json += "}";
  SendJson(200, json);
}

void HandleSafeMode() {
  const mp::AppErrorCode reason = mp::FactoryTest_EnterSafeMode();
  String json = JsonHeader(true, "OK", "entered safe mode");
  json += ",\"reason\":";
  json += reason;
  json += "}";
  SendJson(200, json);
}

void HandleReboot() {
  String json = JsonHeader(true, "OK", "rebooting after safe-off");
  json += "}";
  SendJson(202, json);
  delay(20);
  mp::FactoryTest_Reboot();
}

void HandleHealth() {
  const mp::HealthSnapshot health = mp::Health_GetSnapshot();
  String json = JsonHeader(true, "OK", "health");
  json += ",\"all_healthy\":";
  json += health.allHealthy ? "true" : "false";
  json += ",\"has_critical_timeout\":";
  json += health.hasCriticalTimeout ? "true" : "false";
  json += ",\"safe_mode_requested\":";
  json += health.safeModeRequested ? "true" : "false";
  json += ",\"failed_task\":\"";
  json += TaskIdText(health.failedTaskId);
  json += "\",\"failed_task_age_ticks\":";
  json += health.failedTaskAgeTicks;
  json += ",\"tasks\":[";
  for (std::size_t index = 0U; index < mp::kTaskRegistryCapacity; ++index) {
    const mp::TaskHealthEntry& task = health.taskSnapshot.entries[index];
    if (index != 0U) {
      json += ",";
    }
    json += "{\"id\":\"";
    json += TaskIdText(task.id);
    json += "\",\"name\":";
    json += JsonQuoted(task.name);
    json += ",\"registered\":";
    json += task.isRegistered ? "true" : "false";
    json += ",\"heartbeat\":";
    json += task.hasHeartbeat ? "true" : "false";
    json += ",\"last_tick\":";
    json += task.lastHeartbeat;
    json += "}";
  }
  json += "]}";
  SendJson(200, json);
}

void HandleLogsRecent() {
  mp::LogMsg logs[8] = {};
  const std::size_t count = mp::Log_GetRecent(logs, 8U);
  String json = JsonHeader(true, "OK", "recent logs");
  json += ",\"logs\":[";
  for (std::size_t index = 0U; index < count; ++index) {
    if (index != 0U) {
      json += ",";
    }
    json += "{\"tick\":";
    json += logs[index].tickCount;
    json += ",\"level\":";
    json += static_cast<unsigned>(logs[index].level);
    json += ",\"module\":";
    json += JsonQuoted(logs[index].module);
    json += ",\"text\":";
    json += JsonQuoted(logs[index].text);
    json += "}";
  }
  json += "]}";
  SendJson(200, json);
}

void HandleFactoryHeadShiftTest() {
  if (!RequireNotSafeMode("head shift test is blocked in SAFE_MODE")) {
    ResetRawBody();
    return;
  }

  if (!RequireRawBody()) {
    return;
  }

  const mp::AppErrorCode result = mp::FactoryTest_HeadShiftTest(
      g_rawBody.data, static_cast<std::size_t>(g_rawBody.len));
  ResetRawBody();
  if (result != mp::APP_OK) {
    SendErrorJson(400, "HEAD_SHIFT_TEST_FAILED",
                  "body must be exactly 48 raw bytes; hardware macro required",
                  result);
    return;
  }

  String json = JsonHeader(true, "OK", "head shift test done with VH off");
  json += "}";
  SendJson(200, json);
}

void HandleFactoryHeadStbTest() {
  if (!RequireNotSafeMode("head stb test is blocked in SAFE_MODE")) {
    return;
  }

  std::uint32_t group = 0U;
  std::uint32_t pulseUs = 5U;
  if (!g_server.hasArg("group") ||
      !ParseU32Text(g_server.arg("group"), &group) ||
      !ParseOptionalU32Arg("pulse_us", 5U, &pulseUs) ||
      group >= mp::PRINT_STB_GROUP_COUNT) {
    SendErrorJson(400, "BAD_REQUEST",
                  "group must be 0..5 and pulse_us must be numeric",
                  mp::ERR_COMM_FRAME_TOO_LONG);
    return;
  }

  const mp::AppErrorCode result =
      mp::FactoryTest_HeadStbTest(static_cast<std::uint8_t>(group), pulseUs);
  if (result != mp::APP_OK) {
    SendErrorJson(400, "HEAD_STB_TEST_FAILED",
                  "hardware macro and safety conditions are required", result);
    return;
  }

  String json = JsonHeader(true, "OK", "head stb test done with VH off");
  json += ",\"group\":";
  json += group;
  json += ",\"pulse_us\":";
  json += pulseUs;
  json += "}";
  SendJson(200, json);
}

void HandleFactoryMotorTest() {
  if (!RequireNotSafeMode("motor test is blocked in SAFE_MODE")) {
    return;
  }

  std::uint32_t steps = 1U;
  if (!ParseOptionalU32Arg("steps", 1U, &steps) || steps == 0U ||
      steps > 200U) {
    SendErrorJson(400, "PARAM_OUT_OF_RANGE",
                  "steps must be in 1..200", mp::ERR_COMM_FRAME_TOO_LONG);
    return;
  }

  const mp::AppErrorCode result = mp::FactoryTest_MotorTest(steps);
  if (result != mp::APP_OK) {
    SendErrorJson(400, "MOTOR_TEST_FAILED",
                  "hardware macro is required and nFAULT must stay inactive",
                  result);
    return;
  }

  String json = JsonHeader(true, "OK", "motor test done and released");
  json += ",\"steps\":";
  json += steps;
  json += "}";
  SendJson(200, json);
}

void HandleSafeOff() {
  const mp::AppErrorCode result = mp::FactoryTest_SafeOff();
  if (result != mp::APP_OK) {
    String json = JsonHeader(false, "SAFE_OFF_FAILED", "safe-off failed");
    json += ",\"error\":";
    json += result;
    json += "}";
    SendJson(500, json);
    return;
  }

  String json = JsonHeader(true, "OK", "dangerous outputs are off");
  json += "}";
  SendJson(200, json);
}

void HandleNotFound() {
  constexpr const char* kPrintFilesPrefix = "/api/v1/print/files/";
  const String uri = g_server.uri();
  if (uri.startsWith(kPrintFilesPrefix)) {
    const String rest = uri.substring(std::strlen(kPrintFilesPrefix));
    const int slash = rest.indexOf('/');
    const String fileIdText = (slash < 0) ? rest : rest.substring(0, slash);
    const String suffix = (slash < 0) ? "" : rest.substring(slash + 1);

    std::uint32_t fileId = 0U;
    if (!ParseU32Text(fileIdText, &fileId)) {
      SendErrorJson(400, "BAD_REQUEST", "invalid file_id",
                    mp::ERR_COMM_CRC);
      return;
    }

    if (g_server.method() == HTTP_DELETE && suffix.length() == 0U) {
      HandlePrintFileDelete(fileId);
      return;
    }

    if (g_server.method() == HTTP_POST && suffix == "complete") {
      HandlePrintFileComplete(fileId);
      return;
    }

    constexpr const char* kChunksPrefix = "chunks/";
    if (g_server.method() == HTTP_PUT && suffix.startsWith(kChunksPrefix)) {
      std::uint32_t chunkIndex = 0U;
      if (!ParseU32Text(suffix.substring(std::strlen(kChunksPrefix)),
                        &chunkIndex)) {
        SendErrorJson(400, "BAD_REQUEST", "invalid chunk index",
                      mp::ERR_COMM_CRC);
        return;
      }
      HandlePrintFileChunk(fileId, chunkIndex);
      return;
    }
  }

  String json = JsonHeader(false, "NOT_FOUND", "unknown api route");
  json += ",\"path\":\"";
  json += g_server.uri();
  json += "\"}";
  SendJson(404, json);
}

bool HasStaConfig() {
  return std::strlen(mp::WIFI_SSID) > 0U;
}

void StartAccessPointFallback() {
  WiFi.mode(WIFI_AP);
  const bool hasPassword = std::strlen(mp::WIFI_AP_PASSWORD) >= 8U;
  if (hasPassword) {
    WiFi.softAP(mp::WIFI_AP_SSID, mp::WIFI_AP_PASSWORD);
  } else {
    WiFi.softAP(mp::WIFI_AP_SSID);
  }
  mp::Log_Warn("wifi", "WiFi AP fallback started ssid=%s ip=%s",
               mp::WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
}

void ConnectWifi() {
  if (!HasStaConfig()) {
    StartAccessPointFallback();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(mp::WIFI_SSID, mp::WIFI_PASSWORD);

  const std::uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - startMs) < mp::WIFI_CONNECT_TIMEOUT_MS) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    mp::Log_Info("wifi", "WiFi STA connected ip=%s",
                 WiFi.localIP().toString().c_str());
    return;
  }

  mp::Log_Warn("wifi", "WiFi STA failed, starting AP fallback");
  WiFi.disconnect(true);
  StartAccessPointFallback();
}

void RegisterRoutes() {
  g_server.on("/api/v1/info", HTTP_GET, HandleInfo);
  g_server.on("/api/v1/status", HTTP_GET, HandleStatus);
  g_server.on("/api/v1/key", HTTP_GET, HandleKey);
  g_server.on("/api/v1/sensors", HTTP_GET, HandleSensors);
  g_server.on("/api/v1/battery", HTTP_GET, HandleBattery);
  g_server.on("/api/v1/error", HTTP_GET, HandleErrorGet);
  g_server.on("/api/v1/error/clear", HTTP_POST, HandleErrorClear);
  g_server.on("/api/v1/params", HTTP_GET, HandleParamsGet);
  g_server.on("/api/v1/params", HTTP_PATCH, HandleParamsPatch);
  g_server.on("/api/v1/params/save", HTTP_POST, HandleParamsSave);
  g_server.on("/api/v1/params/factory-reset", HTTP_POST,
              HandleParamsFactoryReset);
  g_server.on("/api/v1/self-test", HTTP_POST, HandleSelfTest);
  g_server.on("/api/v1/reboot", HTTP_POST, HandleReboot);
  g_server.on("/api/v1/safe-mode", HTTP_POST, HandleSafeMode);
  g_server.on("/api/v1/health", HTTP_GET, HandleHealth);
  g_server.on("/api/v1/logs/recent", HTTP_GET, HandleLogsRecent);
  g_server.on("/api/v1/safe-off", HTTP_POST, HandleSafeOff);
  g_server.on("/api/v1/print/files", HTTP_POST, HandlePrintFilesCreate);
  g_server.on("/api/v1/print/files", HTTP_GET, HandlePrintFilesList);
  g_server.on(UriBraces("/api/v1/print/files/{}/chunks/{}"), HTTP_PUT,
              HandlePrintFileChunkRoute, HandleRawBody);
  g_server.on("/api/v1/print/jobs", HTTP_POST, HandlePrintJobCreate);
  g_server.on("/api/v1/print/jobs/current", HTTP_GET, HandlePrintJobCurrent);
  g_server.on("/api/v1/print/jobs/current/cancel", HTTP_POST,
              HandlePrintJobCancel);
  g_server.on("/api/v1/feed", HTTP_POST, HandleFeed);
  g_server.on("/api/v1/factory/motor-test", HTTP_POST,
              HandleFactoryMotorTest);
  g_server.on("/api/v1/factory/real-print-test", HTTP_POST,
              HandleFactoryRealPrintTest);
  g_server.on("/api/v1/factory/head-shift-test", HTTP_POST,
              HandleFactoryHeadShiftTest, HandleRawBody);
  g_server.on("/api/v1/factory/head-stb-test", HTTP_POST,
              HandleFactoryHeadStbTest);
  g_server.onNotFound(HandleNotFound);
}

}  // namespace

namespace mp {

bool WifiApiService_Begin() {
  if (g_started) {
    return true;
  }

  ConnectWifi();
  RegisterRoutes();
  g_server.begin();
  g_started = true;
  Log_Info("wifi", "HTTP API started on /api/v1");
  return true;
}

void WifiApiService_Poll() {
  if (!g_started) {
    return;
  }

  g_server.handleClient();
}

}  // namespace mp

#else

namespace mp {

bool WifiApiService_Begin() { return true; }

void WifiApiService_Poll() {}

}  // namespace mp

#endif
