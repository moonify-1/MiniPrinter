#include "services/wifi_api_service.h"

#include "config/project_features.h"

#if MP_ENABLE_WIFI
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include <cstring>

#include "app/app_factory_test.h"
#include "app/app_print.h"
#include "app/app_system.h"
#include "common/app_error.h"
#include "rtos/rtos_objects.h"
#include "services/error_service.h"
#include "services/log_service.h"
#include "services/print_file_service.h"
#include "services/sensor_service.h"

namespace {

WebServer g_server(80);
bool g_started = false;

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

void SendErrorJson(int status, const char* code, const char* message,
                   mp::AppErrorCode error) {
  String json = JsonHeader(false, code, message);
  json += ",\"error\":";
  json += error;
  json += "}";
  SendJson(status, json);
}

void SendJson(int status, const String& json) {
  MarkApiConnected();
  g_server.send(status, "application/json", json);
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

String SensorJsonFields(const mp::SensorSnapshot& sensor) {
  String json;
  json += ",\"paper\":";
  json += sensor.paperPresent ? "true" : "false";
  json += ",\"head_temp_c\":";
  json += String(sensor.headTempC, 1);
  json += ",\"battery_mv\":";
  json += sensor.batteryMv;
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
  json += ",\"wdt\":";
  json += mp::FEATURE_WDT ? "true" : "false";
  json += "}}";
  SendJson(200, json);
}

void HandleStatus() {
  const mp::SensorSnapshot sensor = mp::SensorService_GetSnapshot();
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
  json += ",\"error\":{\"code\":";
  json += error.code;
  json += ",\"severity\":\"";
  json += ErrorSeverityText(error.severity);
  json += "\",\"safe_mode\":";
  json += mp::Error_IsSafeModeRequired() ? "true" : "false";
  json += "}}";
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
  const String body = g_server.arg("plain");
  const mp::AppErrorCode result = mp::PrintFileService_WriteChunk(
      fileId, index, reinterpret_cast<const std::uint8_t*>(body.c_str()),
      static_cast<std::uint32_t>(body.length()));
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
  g_server.on("/api/v1/safe-off", HTTP_POST, HandleSafeOff);
  g_server.on("/api/v1/print/files", HTTP_POST, HandlePrintFilesCreate);
  g_server.on("/api/v1/print/files", HTTP_GET, HandlePrintFilesList);
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
