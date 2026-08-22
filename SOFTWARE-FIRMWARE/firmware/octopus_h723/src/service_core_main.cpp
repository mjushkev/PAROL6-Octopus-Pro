#include <Arduino.h>
#include <stm32h7xx_hal.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "binary_protocol.hpp"
#include "persistent_store.hpp"
#include "safe_core.hpp"

#ifndef PAROL6_FIRMWARE_VERSION
#define PAROL6_FIRMWARE_VERSION "unversioned"
#endif

#ifndef PAROL6_BOARD_NAME
#define PAROL6_BOARD_NAME "unknown"
#endif

namespace {

using parol6::binary::Frame;
using parol6::protocol::ErrorCode;
using parol6::protocol::FrameFlag;
using parol6::protocol::MessageType;

constexpr std::uint32_t kPhysicalFlashBytes = 512U * 1024U;
constexpr std::uint32_t kCapabilities =
    (1UL << 0U) |  // canonical binary protocol over USB CDC
    (1UL << 1U) |  // independent hardware watchdog
    (1UL << 2U) |  // dual-sector persistent config/event storage
    (1UL << 31U);  // all power and motion outputs compiled out
constexpr std::uint16_t kFirmwareMajor = 0;
constexpr std::uint16_t kFirmwareMinor = 3;
constexpr std::uint16_t kFirmwarePatch = 0;

parol6::safe_core::SafeCore core;
parol6::storage::PersistentStore store;
parol6::binary::StreamDecoder decoder;
parol6::binary::ReplayWindow replay_window;
Frame received_frame{};
Frame response_frame{};
std::array<std::uint8_t, parol6::binary::kMaxBodyBytes> response_body{};
std::array<std::uint8_t, parol6::binary::kMaxUartPacketBytes> response_uart{};
IWDG_HandleTypeDef hardware_watchdog{};
bool hardware_watchdog_ready = false;
bool protocol_self_test_ready = false;
bool session_initialized = false;
std::uint32_t current_session_id = 0;
std::uint32_t response_sequence = 1;
std::uint32_t replay_rejections = 0;
std::uint32_t malformed_rejections = 0;

bool initialize_hardware_watchdog() noexcept {
  hardware_watchdog.Instance = IWDG1;
  hardware_watchdog.Init.Prescaler = IWDG_PRESCALER_32;
  hardware_watchdog.Init.Window = IWDG_WINDOW_DISABLE;
  // Nominal 32 kHz LSI / 32 prescaler: 2,000 ticks is about two seconds.
  hardware_watchdog.Init.Reload = 1999U;
  return HAL_IWDG_Init(&hardware_watchdog) == HAL_OK;
}

void feed_hardware_watchdog() noexcept {
  if (hardware_watchdog_ready) {
    HAL_IWDG_Refresh(&hardware_watchdog);
  }
}

std::uint8_t wire_state(const parol6::safe_core::ControllerState state) noexcept {
  using CoreState = parol6::safe_core::ControllerState;
  switch (state) {
    case CoreState::boot_self_test:
      return static_cast<std::uint8_t>(parol6::protocol::ControllerState::BOOT_SELF_TEST);
    case CoreState::not_commissioned:
      return static_cast<std::uint8_t>(parol6::protocol::ControllerState::NOT_COMMISSIONED);
    case CoreState::disarmed:
      return static_cast<std::uint8_t>(parol6::protocol::ControllerState::DISARMED);
    case CoreState::protective_stop:
      return static_cast<std::uint8_t>(parol6::protocol::ControllerState::PROTECTIVE_STOP);
    case CoreState::fault_latched:
      return static_cast<std::uint8_t>(parol6::protocol::ControllerState::FAULT_LATCHED);
  }
  return static_cast<std::uint8_t>(parol6::protocol::ControllerState::FAULT_LATCHED);
}

std::uint32_t next_response_sequence() noexcept {
  const auto value = response_sequence++;
  if (response_sequence == 0U) {
    response_sequence = 1U;
  }
  return value;
}

void begin_response(const Frame& request, const MessageType type,
                    const std::uint16_t payload_length) noexcept {
  response_frame = Frame{};
  response_frame.header.message_type = type;
  response_frame.header.flags =
      static_cast<std::uint8_t>(FrameFlag::RESPONSE);
  response_frame.header.payload_length = payload_length;
  response_frame.header.session_id = request.header.session_id;
  response_frame.header.sequence = next_response_sequence();
  response_frame.header.acknowledgement = request.header.sequence;
  response_frame.header.sender_time_us =
      static_cast<std::uint64_t>(millis()) * 1000ULL;
}

void send_response() noexcept {
  const auto length = parol6::binary::encode_uart(
      response_frame, response_body, response_uart);
  if (length != 0U) {
    Serial.write(response_uart.data(), length);
  }
}

void send_nack(const Frame& request, const ErrorCode error,
               const std::uint16_t detail = 0U) noexcept {
  begin_response(request, MessageType::NACK,
                 parol6::protocol::kNackPayloadBytes);
  auto* payload = response_frame.payload.data();
  payload[0] = static_cast<std::uint8_t>(error);
  payload[1] = static_cast<std::uint8_t>(request.header.message_type);
  parol6::binary::write_u16(payload + 2U, detail);
  parol6::binary::write_u32(payload + 4U, request.header.sequence);
  send_response();
}

void send_heartbeat_reply(const Frame& request) noexcept {
  begin_response(request, MessageType::HEARTBEAT_REPLY,
                 parol6::protocol::kHeartbeatReplyPayloadBytes);
  auto* payload = response_frame.payload.data();
  payload[0] = wire_state(core.state());
  payload[1] = static_cast<std::uint8_t>(core.fault());
  payload[2] = core.config().valid &&
                       store.status() ==
                           parol6::storage::StoreStatus::flash_selected
                   ? 1U
                   : 0U;
  payload[3] = core.outputs().all_safe() ? 0U : 1U;
  parol6::binary::write_u32(payload + 4U, millis());
  parol6::binary::write_u32(payload + 8U, core.accepted_commands());
  parol6::binary::write_u32(payload + 12U, core.rejected_commands());
  parol6::binary::write_u32(payload + 16U, replay_rejections);
  parol6::binary::write_u32(payload + 20U, core.config().sequence);
  parol6::binary::write_u16(payload + 24U, store.event_count());
  payload[26] = static_cast<std::uint8_t>(store.status());
  payload[27] = hardware_watchdog_ready ? 1U : 0U;
  send_response();
}

void send_device_info(const Frame& request) noexcept {
  begin_response(request, MessageType::GET_DEVICE_INFO,
                 parol6::protocol::kDeviceInfoPayloadBytes);
  auto* payload = response_frame.payload.data();
  parol6::binary::write_u16(payload + 0U, kFirmwareMajor);
  parol6::binary::write_u16(payload + 2U, kFirmwareMinor);
  parol6::binary::write_u16(payload + 4U, kFirmwarePatch);
  parol6::binary::write_u16(payload + 6U,
                            parol6::safe_core::kConfigSchemaVersion);
  parol6::binary::write_u32(payload + 8U, kCapabilities);
  parol6::binary::write_u32(payload + 12U, kPhysicalFlashBytes);
  parol6::binary::write_u32(payload + 16U,
                            parol6::storage::kApplicationOrigin);
  parol6::binary::write_u32(payload + 20U,
                            parol6::storage::kApplicationLimit);
  parol6::binary::write_u32(payload + 24U, parol6::storage::kSlotAAddress);
  parol6::binary::write_u32(payload + 28U, parol6::storage::kStorageEnd);
  constexpr char board_id[] = PAROL6_BOARD_NAME;
  const auto board_bytes = std::min(sizeof(board_id) - 1U,
                                    static_cast<std::size_t>(32U));
  std::memcpy(payload + 32U, board_id, board_bytes);
  send_response();
}

void reject_request(const Frame& request, const ErrorCode error,
                    const std::uint16_t detail = 0U) noexcept {
  core.reject_command(millis());
  send_nack(request, error, detail);
}

void handle_request(const Frame& request) noexcept {
  constexpr auto kKnownFlags =
      static_cast<std::uint8_t>(FrameFlag::RESPONSE);
  if ((request.header.flags & static_cast<std::uint8_t>(~kKnownFlags)) != 0U) {
    reject_request(request, ErrorCode::MALFORMED, 5U);
    return;
  }
  if ((request.header.flags & kKnownFlags) != 0U) {
    reject_request(request, ErrorCode::MALFORMED, 1U);
    return;
  }

  if (!session_initialized || request.header.session_id != current_session_id) {
    current_session_id = request.header.session_id;
    session_initialized = true;
    replay_window.reset();
  }
  const auto replay = replay_window.check_and_mark(request.header.sequence);
  if (replay != parol6::binary::ReplayDecision::accept) {
    ++replay_rejections;
    reject_request(request, ErrorCode::REPLAY,
                   replay == parol6::binary::ReplayDecision::duplicate ? 1U
                                                                       : 2U);
    return;
  }

  switch (request.header.message_type) {
    case MessageType::HEARTBEAT:
      if (request.header.payload_length != 0U) {
        reject_request(request, ErrorCode::MALFORMED, 2U);
        return;
      }
      core.accept_safe_command();
      core.heartbeat(millis());
      send_heartbeat_reply(request);
      return;
    case MessageType::GET_DEVICE_INFO:
      if (request.header.payload_length != 0U) {
        reject_request(request, ErrorCode::MALFORMED, 2U);
        return;
      }
      core.accept_safe_command();
      send_device_info(request);
      return;
    case MessageType::HELLO:
    case MessageType::TAKE_CONTROL:
    case MessageType::RELEASE_CONTROL:
    case MessageType::MOTOR_ENABLE:
    case MessageType::MOTOR_OFF:
    case MessageType::CONTROLLED_STOP:
    case MessageType::RESET_FAULT:
    case MessageType::HOME_START:
    case MessageType::HOME_CANCEL:
    case MessageType::TRAJECTORY_BEGIN:
    case MessageType::TRAJECTORY_POINTS:
    case MessageType::TRAJECTORY_COMMIT:
    case MessageType::TRAJECTORY_CANCEL:
    case MessageType::GRIPPER_SET:
    case MessageType::IO_WRITE:
      reject_request(request, ErrorCode::NOT_COMMISSIONED);
      return;
    case MessageType::HEARTBEAT_REPLY:
    case MessageType::STATUS_FAST:
    case MessageType::STATUS_SLOW:
    case MessageType::EVENT:
    case MessageType::NACK:
      reject_request(request, ErrorCode::MALFORMED, 3U);
      return;
  }
  reject_request(request, ErrorCode::MALFORMED, 4U);
}

}  // namespace

void setup() {
  // USB CDC, the independent watchdog, and internal flash storage are the only
  // hardware facilities initialized. No GPIO, timer/PWM, step, enable, relay,
  // gripper, actuator-power, or motor-control peripheral is configured here.
  Serial.begin(3000000);
  hardware_watchdog_ready = initialize_hardware_watchdog();
  protocol_self_test_ready = parol6::binary::self_test();
  store.begin(feed_hardware_watchdog);

  const auto selection = store.selection();
  if (selection.valid &&
      store.status() == parol6::storage::StoreStatus::flash_selected) {
    const auto safe_config =
        parol6::safe_core::make_safe_config(selection.sequence);
    core.boot(safe_config, parol6::safe_core::ConfigRecord{}, millis());
  } else {
    core.boot(parol6::safe_core::ConfigRecord{},
              parol6::safe_core::ConfigRecord{}, millis());
  }

  if (store.status() == parol6::storage::StoreStatus::flash_selected) {
    const bool boot_recorded = store.append_event(
        millis(),
        static_cast<std::uint16_t>(parol6::safe_core::EventCode::boot),
        protocol_self_test_ready ? 0U : 1U,
        hardware_watchdog_ready ? 1U : 0U, selection.sequence);
    if (!boot_recorded) {
      core.boot(parol6::safe_core::ConfigRecord{},
                parol6::safe_core::ConfigRecord{}, millis());
    }
  }
}

void loop() {
  feed_hardware_watchdog();
  core.poll(millis());

  if (!protocol_self_test_ready) {
    // A framing/CRC/replay self-test failure disables all protocol handling.
    // The independent watchdog remains serviced so USB re-enumeration cannot
    // turn a deterministic self-test failure into an uncontrolled reset loop.
    delay(1);
    return;
  }

  while (Serial.available() > 0) {
    const int value = Serial.read();
    if (value < 0) {
      continue;
    }
    const auto result = decoder.feed(static_cast<std::uint8_t>(value),
                                     received_frame);
    if (result == parol6::binary::FeedResult::frame_ready) {
      handle_request(received_frame);
    } else if (result == parol6::binary::FeedResult::rejected) {
      ++malformed_rejections;
      core.reject_command(millis());
    }
  }
  delay(1);
}
