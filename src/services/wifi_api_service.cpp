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
#include "services/sensor_service.h"

namespace {

WebServer g_server(80);
bool g_started = false;

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

void SendJson(int status, const String& json) {
  MarkApiConnected();
  g_server.send(status, "application/json", json);
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
