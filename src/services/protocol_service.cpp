#include "services/protocol_service.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

#include "app/app_system.h"
#include "protocol/proto_cmd.h"
#include "protocol/proto_crc.h"
#include "rtos/rtos_objects.h"
#include "services/param_service.h"
#include "services/sensor_service.h"

namespace {

constexpr std::uint16_t kDeviceIdDefault = 1U;
constexpr std::size_t kStatusPayloadLen = 160U;
constexpr std::size_t kInfoPayloadLen = 96U;
constexpr std::size_t kParamPayloadLen = 160U;

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
      "MiniPrinterRTOS;proto=1;cmd=PING,GET_INFO,GET_STATUS,GET_PARAM,SAVE_PARAM,FACTORY_RESET");
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
  switch (ToProtoCmd(frame.cmd)) {
    case ProtoCmd::PING:
      HandlePing(frame);
      break;
    case ProtoCmd::GET_INFO:
      HandleGetInfo(frame);
      break;
    case ProtoCmd::GET_STATUS:
      HandleGetStatus(frame);
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
    default:
      HandleUnsupported(frame);
      break;
  }
}

}  // namespace mp
