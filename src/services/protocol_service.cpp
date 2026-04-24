#include "services/protocol_service.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

#include "app/app_factory_test.h"
#include "app/app_system.h"
#include "protocol/proto_cmd.h"
#include "protocol/proto_crc.h"
#include "rtos/rtos_objects.h"
#include "services/error_service.h"
#include "services/param_service.h"
#include "services/print_service.h"
#include "services/print_spooler.h"
#include "services/sensor_service.h"

namespace {

constexpr std::uint16_t kDeviceIdDefault = 1U;
constexpr std::size_t kStatusPayloadLen = 160U;
constexpr std::size_t kInfoPayloadLen = 256U;
constexpr std::size_t kParamPayloadLen = 160U;
constexpr std::size_t kPrintPayloadLen = 80U;
constexpr std::size_t kFactoryPayloadLen = 160U;

std::uint32_t g_protocolPrintJobId = 0U;
std::uint32_t g_protocolNextLineNo = 0U;

void WriteLe16(std::uint8_t* data, std::uint16_t value) {
  data[0] = static_cast<std::uint8_t>(value & 0x00FFU);
  data[1] = static_cast<std::uint8_t>((value >> 8U) & 0x00FFU);
}

// 把系统状态转成短文本，便于 STATUS 响应阅读。
const char* SystemStateToText(mp::SystemState state) {
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

const char* ErrorSeverityToText(mp::ErrorSeverity severity) {
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

bool IsSafeModeAllowed(mp::ProtoCmd cmd) {
  switch (cmd) {
    case mp::ProtoCmd::GET_STATUS:
    case mp::ProtoCmd::SENSOR_TEST:
    case mp::ProtoCmd::GET_ERROR:
    case mp::ProtoCmd::CLEAR_ERROR:
    case mp::ProtoCmd::SELF_TEST:
    case mp::ProtoCmd::REBOOT:
    case mp::ProtoCmd::FACTORY_RESET:
    case mp::ProtoCmd::SAFE_OFF:
    case mp::ProtoCmd::ENTER_SAFE_MODE:
      return true;
    default:
      return false;
  }
}

// 发送一帧响应。
//
// 响应帧复用请求的 device_id / seq / cmd，
// flags 会加上 RESPONSE，source 固定为 DEVICE。
bool SendResponse(const mp::ProtoFrame& request, std::uint8_t extraFlags,
                  const std::uint8_t* payload, std::uint16_t payloadLen) {
  if (payloadLen > mp::PROTO_MAX_PAYLOAD_SIZE) {
    return false;
  }

  std::uint8_t frameBytes[mp::PROTO_MAX_FRAME_SIZE] = {};
  std::size_t offset = 0U;

  frameBytes[offset++] = mp::PROTO_MAGIC_0;
  frameBytes[offset++] = mp::PROTO_MAGIC_1;
  frameBytes[offset++] = mp::PROTO_VERSION;
  frameBytes[offset++] = mp::PROTO_HEADER_LEN;
  WriteLe16(&frameBytes[offset], request.deviceId != 0U ? request.deviceId
                                                        : kDeviceIdDefault);
  offset += 2U;
  WriteLe16(&frameBytes[offset], request.seq);
  offset += 2U;
  WriteLe16(&frameBytes[offset], request.cmd);
  offset += 2U;
  frameBytes[offset++] = static_cast<std::uint8_t>(mp::PROTO_FLAG_RESPONSE |
                                                   extraFlags);
  frameBytes[offset++] = mp::PROTO_SOURCE_DEVICE;
  WriteLe16(&frameBytes[offset], payloadLen);
  offset += 2U;

  if (payload != nullptr && payloadLen > 0U) {
    std::memcpy(&frameBytes[offset], payload, payloadLen);
    offset += payloadLen;
  }

  const std::uint16_t crc = mp::ProtoCrc16Calc(frameBytes, offset);
  WriteLe16(&frameBytes[offset], crc);
  offset += 2U;

  Serial.write(frameBytes, offset);
  return true;
}

std::uint16_t CopyTextPayload(std::uint8_t* dst, std::size_t dstLen,
                              const char* text) {
  if (dst == nullptr || dstLen == 0U) {
    return 0U;
  }

  const int written = std::snprintf(reinterpret_cast<char*>(dst), dstLen, "%s",
                                    text != nullptr ? text : "");
  if (written <= 0) {
    return 0U;
  }

  const std::size_t len = static_cast<std::size_t>(written);
  return static_cast<std::uint16_t>((len < dstLen) ? len : (dstLen - 1U));
}

void HandlePing(const mp::ProtoFrame& frame) {
  constexpr std::uint8_t payload[] = {'P', 'O', 'N', 'G'};
  (void)SendResponse(frame, 0U, payload, sizeof(payload));
}

void HandleGetInfo(const mp::ProtoFrame& frame) {
  std::uint8_t payload[kInfoPayloadLen] = {};
  const std::uint16_t payloadLen = CopyTextPayload(
      payload, sizeof(payload),
      "MiniPrinterRTOS;proto=1;cmd=PING,GET_INFO,GET_STATUS,GET_ERROR,CLEAR_ERROR,SELF_TEST,REBOOT,GET_PARAM,SAVE_PARAM,FACTORY_RESET,PRINT_START,PRINT_LINE,PRINT_END,PRINT_CANCEL,FEED,SAFE_OFF,SENSOR_TEST,MOTOR_TEST,HEAD_SHIFT_TEST,HEAD_STB_TEST,ENTER_SAFE_MODE");
  (void)SendResponse(frame, 0U, payload, payloadLen);
}

void HandleGetStatus(const mp::ProtoFrame& frame) {
  const mp::SystemState state = mp::SystemApp_GetState();
  const mp::SensorSnapshot sensor = mp::SensorService_GetSnapshot();
  const std::int32_t tempX10 =
      static_cast<std::int32_t>(sensor.headTempC * 10.0F);

  std::uint8_t payload[kStatusPayloadLen] = {};
  const int written = std::snprintf(
      reinterpret_cast<char*>(payload), sizeof(payload),
      "state=%s,paper=%u,temp_x10=%ld,bat_mv=%lu,charging=%u,motor_fault=%u,valid=%u,tick_ms=%lu",
      SystemStateToText(state), sensor.paperPresent ? 1U : 0U,
      static_cast<long>(tempX10), static_cast<unsigned long>(sensor.batteryMv),
      sensor.charging ? 1U : 0U, sensor.motorFault ? 1U : 0U,
      static_cast<unsigned>(sensor.validity),
      static_cast<unsigned long>(sensor.tickMs));

  std::uint16_t payloadLen = 0U;
  if (written > 0) {
    const std::size_t len = static_cast<std::size_t>(written);
    payloadLen =
        static_cast<std::uint16_t>((len < sizeof(payload)) ? len
                                                           : (sizeof(payload) - 1U));
  }

  (void)SendResponse(frame, 0U, payload, payloadLen);
}

void HandleUnsupported(const mp::ProtoFrame& frame) {
  constexpr std::uint8_t payload[] = {
      'E', 'R', 'R', '_', 'U', 'N', 'S', 'U', 'P', 'P', 'O', 'R', 'T', 'E', 'D'};
  (void)SendResponse(frame, mp::PROTO_FLAG_ERROR, payload, sizeof(payload));
}

void SendErrorCode(const mp::ProtoFrame& frame, mp::AppErrorCode error) {
  std::uint8_t payload[kPrintPayloadLen] = {};
  const int written = std::snprintf(
      reinterpret_cast<char*>(payload), sizeof(payload), "ERR=0x%08lX",
      static_cast<unsigned long>(error));

  std::uint16_t payloadLen = 0U;
  if (written > 0) {
    const std::size_t len = static_cast<std::size_t>(written);
    payloadLen =
        static_cast<std::uint16_t>((len < sizeof(payload)) ? len
                                                           : (sizeof(payload) - 1U));
  }

  (void)SendResponse(frame, mp::PROTO_FLAG_ERROR, payload, payloadLen);
}

void SendErrorText(const mp::ProtoFrame& frame, const char* text) {
  std::uint8_t payload[kPrintPayloadLen] = {};
  const std::uint16_t payloadLen =
      CopyTextPayload(payload, sizeof(payload), text);
  (void)SendResponse(frame, mp::PROTO_FLAG_ERROR, payload, payloadLen);
}

void HandleSafeModeNack(const mp::ProtoFrame& frame) {
  SendErrorText(frame, "NACK_SAFE_MODE");
}

void SendQueuedText(const mp::ProtoFrame& frame, const char* text);

void HandleGetError(const mp::ProtoFrame& frame) {
  const mp::ErrorEvent error = mp::Error_GetLast();
  std::uint8_t payload[kStatusPayloadLen] = {};
  const int written = std::snprintf(
      reinterpret_cast<char*>(payload), sizeof(payload),
      "code=0x%08lX,severity=%s,module=%s,tick=%lu,detail=%s,safe=%u",
      static_cast<unsigned long>(error.code),
      ErrorSeverityToText(error.severity), error.module,
      static_cast<unsigned long>(error.timestamp), error.detail,
      mp::Error_IsSafeModeRequired() ? 1U : 0U);

  std::uint16_t payloadLen = 0U;
  if (written > 0) {
    const std::size_t len = static_cast<std::size_t>(written);
    payloadLen =
        static_cast<std::uint16_t>((len < sizeof(payload)) ? len
                                                           : (sizeof(payload) - 1U));
  }

  (void)SendResponse(frame, 0U, payload, payloadLen);
}

void HandleClearError(const mp::ProtoFrame& frame) {
  mp::Error_ClearRecoverable();
  SendQueuedText(frame, mp::Error_IsSafeModeRequired()
                            ? "CLEAR_ERROR_REJECTED_SAFE_MODE"
                            : "CLEAR_ERROR_OK");
}

void HandleSelfTest(const mp::ProtoFrame& frame) {
  if (mp::g_rtos.systemEvents != nullptr) {
    xEventGroupSetBits(mp::g_rtos.systemEvents, mp::EVT_SELFTEST_OK);
  }
  SendQueuedText(frame, "SELF_TEST_OK_MOCK");
}

void HandleReboot(const mp::ProtoFrame& frame) {
  SendQueuedText(frame, "REBOOTING");
  mp::FactoryTest_Reboot();
}

void HandleGetParam(const mp::ProtoFrame& frame) {
  const mp::ParamBlock params = mp::Param_GetSnapshot();
  std::uint8_t payload[kParamPayloadLen] = {};
  const int written = std::snprintf(
      reinterpret_cast<char*>(payload), sizeof(payload),
      "magic=0x%08lX,version=%u,len=%u,crc=0x%08lX,temp_stop=%d,temp_resume=%d,max_frame=%u",
      static_cast<unsigned long>(params.magic),
      static_cast<unsigned>(params.version),
      static_cast<unsigned>(sizeof(mp::ParamBlock)),
      static_cast<unsigned long>(params.crc32),
      static_cast<int>(params.safety.tempStopThresholdC),
      static_cast<int>(params.safety.tempResumeThresholdC),
      static_cast<unsigned>(params.comm.maxFrameLength));

  std::uint16_t payloadLen = 0U;
  if (written > 0) {
    const std::size_t len = static_cast<std::size_t>(written);
    payloadLen =
        static_cast<std::uint16_t>((len < sizeof(payload)) ? len
                                                           : (sizeof(payload) - 1U));
  }

  (void)SendResponse(frame, 0U, payload, payloadLen);
}

void HandleSetParamTodo(const mp::ProtoFrame& frame) {
  constexpr std::uint8_t payload[] = {'T', 'O', 'D', 'O', '_', 'S', 'E', 'T', '_',
                                      'P', 'A', 'R', 'A', 'M'};
  (void)SendResponse(frame, mp::PROTO_FLAG_ERROR, payload, sizeof(payload));
}

void HandleSaveParam(const mp::ProtoFrame& frame) {
  const bool queued = mp::Param_RequestSave();
  constexpr std::uint8_t okPayload[] = {'S', 'A', 'V', 'E', '_', 'Q', 'U', 'E', 'U', 'E', 'D'};
  constexpr std::uint8_t failPayload[] = {'S', 'A', 'V', 'E', '_', 'Q', 'U', 'E', 'U', 'E',
                                          '_', 'F', 'A', 'I', 'L'};
  if (queued) {
    (void)SendResponse(frame, 0U, okPayload, sizeof(okPayload));
  } else {
    (void)SendResponse(frame, mp::PROTO_FLAG_ERROR, failPayload, sizeof(failPayload));
  }
}

void HandleFactoryReset(const mp::ProtoFrame& frame) {
  const bool queued = mp::Param_ResetDefault();
  constexpr std::uint8_t okPayload[] = {'D', 'E', 'F', 'A', 'U', 'L', 'T',
                                        '_', 'R', 'E', 'S', 'T', 'O', 'R', 'E', 'D'};
  constexpr std::uint8_t failPayload[] = {'D', 'E', 'F', 'A', 'U', 'L', 'T',
                                          '_', 'S', 'A', 'V', 'E', '_', 'F', 'A', 'I', 'L'};
  if (queued) {
    (void)SendResponse(frame, 0U, okPayload, sizeof(okPayload));
  } else {
    (void)SendResponse(frame, mp::PROTO_FLAG_ERROR, failPayload, sizeof(failPayload));
  }
}

void HandlePrintLine(const mp::ProtoFrame& frame) {
  if (frame.payloadLen != mp::LINE_BUFFER_DATA_SIZE) {
    SendErrorCode(frame, mp::ERR_COMM_FRAME_TOO_LONG);
    return;
  }

  mp::LineBuffer* line = mp::PrintSpooler_AllocLine(0U);
  if (line == nullptr) {
    SendErrorCode(frame, mp::ERR_QUEUE_FULL);
    return;
  }

  line->jobId = (g_protocolPrintJobId != 0U) ? g_protocolPrintJobId : frame.seq;
  line->lineNo = g_protocolNextLineNo++;
  std::memcpy(line->data, frame.payload, sizeof(line->data));
  line->blackDotCount =
      mp::PrintSpooler_CountBlackDots(line->data, sizeof(line->data));
  line->flags = 0U;

  const mp::AppErrorCode result = mp::PrintSpooler_SubmitLine(line);
  if (result != mp::APP_OK) {
    (void)mp::PrintSpooler_FreeLine(line);
    SendErrorCode(frame, result);
    return;
  }

  std::uint8_t payload[kPrintPayloadLen] = {};
  const int written = std::snprintf(
      reinterpret_cast<char*>(payload), sizeof(payload),
      "PRINT_LINE_QUEUED,black=%u", static_cast<unsigned>(line->blackDotCount));

  std::uint16_t payloadLen = 0U;
  if (written > 0) {
    const std::size_t len = static_cast<std::size_t>(written);
    payloadLen =
        static_cast<std::uint16_t>((len < sizeof(payload)) ? len
                                                           : (sizeof(payload) - 1U));
  }

  (void)SendResponse(frame, 0U, payload, payloadLen);
}

std::uint32_t ReadLe32OrDefault(const mp::ProtoFrame& frame,
                                std::uint32_t defaultValue) {
  if (frame.payloadLen < 4U) {
    return defaultValue;
  }

  return static_cast<std::uint32_t>(frame.payload[0]) |
         (static_cast<std::uint32_t>(frame.payload[1]) << 8U) |
         (static_cast<std::uint32_t>(frame.payload[2]) << 16U) |
         (static_cast<std::uint32_t>(frame.payload[3]) << 24U);
}

std::uint32_t ReadLe32AtOrDefault(const mp::ProtoFrame& frame,
                                  std::uint16_t offset,
                                  std::uint32_t defaultValue) {
  if (frame.payloadLen < static_cast<std::uint16_t>(offset + 4U)) {
    return defaultValue;
  }

  return static_cast<std::uint32_t>(frame.payload[offset]) |
         (static_cast<std::uint32_t>(frame.payload[offset + 1U]) << 8U) |
         (static_cast<std::uint32_t>(frame.payload[offset + 2U]) << 16U) |
         (static_cast<std::uint32_t>(frame.payload[offset + 3U]) << 24U);
}

void SendQueuedText(const mp::ProtoFrame& frame, const char* text) {
  std::uint8_t payload[kPrintPayloadLen] = {};
  const std::uint16_t payloadLen =
      CopyTextPayload(payload, sizeof(payload), text);
  (void)SendResponse(frame, 0U, payload, payloadLen);
}

void HandlePrintStart(const mp::ProtoFrame& frame) {
  const bool queued = mp::PrintService_RequestStart(frame.seq);
  if (!queued) {
    SendErrorCode(frame, mp::ERR_QUEUE_FULL);
    return;
  }

  g_protocolPrintJobId = frame.seq;
  g_protocolNextLineNo = 0U;
  SendQueuedText(frame, "PRINT_START_QUEUED");
}

void HandlePrintEnd(const mp::ProtoFrame& frame) {
  const bool queued = mp::PrintService_RequestEnd(frame.seq);
  if (!queued) {
    SendErrorCode(frame, mp::ERR_QUEUE_FULL);
    return;
  }

  SendQueuedText(frame, "PRINT_END_QUEUED");
}

void HandlePrintCancel(const mp::ProtoFrame& frame) {
  const bool queued = mp::PrintService_RequestCancel(frame.seq);
  if (!queued) {
    SendErrorCode(frame, mp::ERR_QUEUE_FULL);
    return;
  }

  SendQueuedText(frame, "PRINT_CANCEL_QUEUED");
}

void HandleFeed(const mp::ProtoFrame& frame) {
  const std::uint32_t steps = ReadLe32OrDefault(frame, 1U);
  const bool queued = mp::PrintService_RequestFeed(steps);
  if (!queued) {
    SendErrorCode(frame, mp::ERR_QUEUE_FULL);
    return;
  }

  SendQueuedText(frame, "FEED_QUEUED");
}

void HandleSafeOff(const mp::ProtoFrame& frame) {
  const mp::AppErrorCode result = mp::FactoryTest_SafeOff();
  if (result != mp::APP_OK) {
    SendErrorCode(frame, result);
    return;
  }

  SendQueuedText(frame, "SAFE_OFF_OK");
}

void HandleSensorTest(const mp::ProtoFrame& frame) {
  const mp::SensorSnapshot sensor = mp::FactoryTest_ReadSensors();
  const std::int32_t tempX10 =
      static_cast<std::int32_t>(sensor.headTempC * 10.0F);

  std::uint8_t payload[kFactoryPayloadLen] = {};
  const int written = std::snprintf(
      reinterpret_cast<char*>(payload), sizeof(payload),
      "SENSOR_TEST,paper=%u,temp_x10=%ld,bat_mv=%lu,charging=%u,motor_fault=%u,valid=%u,tick_ms=%lu",
      sensor.paperPresent ? 1U : 0U, static_cast<long>(tempX10),
      static_cast<unsigned long>(sensor.batteryMv),
      sensor.charging ? 1U : 0U, sensor.motorFault ? 1U : 0U,
      static_cast<unsigned>(sensor.validity),
      static_cast<unsigned long>(sensor.tickMs));

  std::uint16_t payloadLen = 0U;
  if (written > 0) {
    const std::size_t len = static_cast<std::size_t>(written);
    payloadLen =
        static_cast<std::uint16_t>((len < sizeof(payload)) ? len
                                                           : (sizeof(payload) - 1U));
  }

  (void)SendResponse(frame, 0U, payload, payloadLen);
}

void HandleMotorTest(const mp::ProtoFrame& frame) {
  const std::uint32_t steps = ReadLe32OrDefault(frame, 1U);
  const mp::AppErrorCode result = mp::FactoryTest_MotorTest(steps);
  if (result != mp::APP_OK) {
    SendErrorCode(frame, result);
    return;
  }

  SendQueuedText(frame, "MOTOR_TEST_OK");
}

void HandleHeadShiftTest(const mp::ProtoFrame& frame) {
  const mp::AppErrorCode result =
      mp::FactoryTest_HeadShiftTest(frame.payload, frame.payloadLen);
  if (result != mp::APP_OK) {
    SendErrorCode(frame, result);
    return;
  }

  SendQueuedText(frame, "HEAD_SHIFT_TEST_OK");
}

void HandleHeadStbTest(const mp::ProtoFrame& frame) {
  const std::uint8_t group = (frame.payloadLen > 0U) ? frame.payload[0] : 0U;
  const std::uint32_t pulseUs = ReadLe32AtOrDefault(frame, 1U, 5U);
  const mp::AppErrorCode result =
      mp::FactoryTest_HeadStbTest(group, pulseUs);
  if (result != mp::APP_OK) {
    SendErrorCode(frame, result);
    return;
  }

  SendQueuedText(frame, "HEAD_STB_TEST_OK");
}

void HandleEnterSafeMode(const mp::ProtoFrame& frame) {
  const mp::AppErrorCode reason = mp::FactoryTest_EnterSafeMode();
  std::uint8_t payload[kPrintPayloadLen] = {};
  const int written = std::snprintf(
      reinterpret_cast<char*>(payload), sizeof(payload),
      "ENTER_SAFE_MODE,reason=0x%08lX",
      static_cast<unsigned long>(reason));

  std::uint16_t payloadLen = 0U;
  if (written > 0) {
    const std::size_t len = static_cast<std::size_t>(written);
    payloadLen =
        static_cast<std::uint16_t>((len < sizeof(payload)) ? len
                                                           : (sizeof(payload) - 1U));
  }

  (void)SendResponse(frame, 0U, payload, payloadLen);
}

}  // namespace

namespace mp {

void ProtocolService_Init() {}

bool ProtocolService_PostFrame(const ProtoFrame& frame) {
  if (g_rtos.qCommand == nullptr) {
    return false;
  }

  if (xQueueSend(g_rtos.qCommand, &frame, 0U) == pdPASS) {
    if (g_rtos.systemEvents != nullptr) {
      xEventGroupSetBits(g_rtos.systemEvents, EVT_COMM_CONNECTED);
    }
    return true;
  }

  ProtoFrame dropped = {};
  (void)xQueueReceive(g_rtos.qCommand, &dropped, 0U);

  const bool queued = xQueueSend(g_rtos.qCommand, &frame, 0U) == pdPASS;
  if (queued && g_rtos.systemEvents != nullptr) {
    xEventGroupSetBits(g_rtos.systemEvents, EVT_COMM_CONNECTED);
  }
  return queued;
}

void ProtocolService_HandleFrame(const ProtoFrame& frame) {
  const ProtoCmd cmd = ToProtoCmd(frame.cmd);

  // SAFE_OFF 是最高优先级命令：无论系统是否已经 SAFE_MODE，都必须立即执行。
  if (cmd == ProtoCmd::SAFE_OFF) {
    HandleSafeOff(frame);
    return;
  }

  if (SystemApp_GetState() == SystemState::SAFE_MODE &&
      !IsSafeModeAllowed(cmd)) {
    HandleSafeModeNack(frame);
    return;
  }

  switch (cmd) {
    case ProtoCmd::PING:
      HandlePing(frame);
      break;
    case ProtoCmd::GET_INFO:
      HandleGetInfo(frame);
      break;
    case ProtoCmd::GET_STATUS:
      HandleGetStatus(frame);
      break;
    case ProtoCmd::GET_ERROR:
      HandleGetError(frame);
      break;
    case ProtoCmd::CLEAR_ERROR:
      HandleClearError(frame);
      break;
    case ProtoCmd::SELF_TEST:
      HandleSelfTest(frame);
      break;
    case ProtoCmd::REBOOT:
      HandleReboot(frame);
      break;
    case ProtoCmd::GET_PARAM:
      HandleGetParam(frame);
      break;
    case ProtoCmd::SET_PARAM:
      HandleSetParamTodo(frame);
      break;
    case ProtoCmd::SAVE_PARAM:
      HandleSaveParam(frame);
      break;
    case ProtoCmd::FACTORY_RESET:
      HandleFactoryReset(frame);
      break;
    case ProtoCmd::PRINT_START:
      HandlePrintStart(frame);
      break;
    case ProtoCmd::PRINT_LINE:
      HandlePrintLine(frame);
      break;
    case ProtoCmd::PRINT_END:
      HandlePrintEnd(frame);
      break;
    case ProtoCmd::PRINT_CANCEL:
      HandlePrintCancel(frame);
      break;
    case ProtoCmd::FEED:
      HandleFeed(frame);
      break;
    case ProtoCmd::SENSOR_TEST:
      HandleSensorTest(frame);
      break;
    case ProtoCmd::MOTOR_TEST:
      HandleMotorTest(frame);
      break;
    case ProtoCmd::HEAD_SHIFT_TEST:
      HandleHeadShiftTest(frame);
      break;
    case ProtoCmd::HEAD_STB_TEST:
      HandleHeadStbTest(frame);
      break;
    case ProtoCmd::ENTER_SAFE_MODE:
      HandleEnterSafeMode(frame);
      break;
    default:
      HandleUnsupported(frame);
      break;
  }
}

}  // namespace mp
