#include <Arduino.h>
#include <stm32h7xx_hal.h>

#include <cstdint>

#include "safe_core.hpp"
#include "safe_core_protocol.hpp"

#ifndef PAROL6_FIRMWARE_VERSION
#define PAROL6_FIRMWARE_VERSION "unversioned"
#endif

#ifndef PAROL6_BOARD_NAME
#define PAROL6_BOARD_NAME "unknown"
#endif

namespace {

constexpr std::uint32_t kHardwareWatchdogTimeoutUs = 2000000U;
constexpr auto kFactorySafeConfig = parol6::safe_core::make_safe_config(1U);

parol6::safe_core::SafeCore core;
parol6::safe_core::LineParser parser;
IWDG_HandleTypeDef hardware_watchdog{};
bool hardware_watchdog_ready = false;
bool previous_dtr = false;

bool initialize_hardware_watchdog() {
  hardware_watchdog.Instance = IWDG1;
  hardware_watchdog.Init.Prescaler = IWDG_PRESCALER_32;
  hardware_watchdog.Init.Window = IWDG_WINDOW_DISABLE;
  // Nominal 32 kHz LSI / 32 prescaler: 2,000 ticks is about two seconds.
  hardware_watchdog.Init.Reload = 1999U;
  return HAL_IWDG_Init(&hardware_watchdog) == HAL_OK;
}

void feed_hardware_watchdog() {
  if (hardware_watchdog_ready) {
    HAL_IWDG_Refresh(&hardware_watchdog);
  }
}

void write_identify() {
  Serial.print(
      "PAROL6_IDENTIFY product=PAROL6 firmware=safe_core version="
      PAROL6_FIRMWARE_VERSION " board=" PAROL6_BOARD_NAME
      " mcu=STM32H723ZE transport=usb_cdc outputs=disabled motion=disabled\r\n");
}

void write_status() {
  Serial.print("PAROL6_STATUS mode=safe_core state=");
  Serial.print(parol6::safe_core::state_name(core.state()));
  Serial.print(" fault=");
  Serial.print(parol6::safe_core::fault_name(core.fault()));
  Serial.print(
      " outputs=disabled motion=disabled actuator_power=required_off accepted=");
  Serial.print(core.accepted_commands());
  Serial.print(" rejected=");
  Serial.print(core.rejected_commands());
  Serial.print("\r\n");
}

void write_diagnostics() {
  const auto selection = core.config();
  Serial.print("PAROL6_DIAGNOSTICS config_valid=");
  Serial.print(selection.valid ? 1 : 0);
  Serial.print(" config_slot=");
  Serial.print(selection.slot);
  Serial.print(" config_sequence=");
  Serial.print(selection.sequence);
  Serial.print(" event_count=");
  Serial.print(core.events().size());
  Serial.print(" event_total=");
  Serial.print(core.events().total());
  Serial.print(" heartbeat_timeout_ms=");
  Serial.print(parol6::safe_core::kControlHeartbeatTimeoutMs);
  Serial.print(" hardware_watchdog_ms=");
  Serial.print(kHardwareWatchdogTimeoutUs / 1000U);
  Serial.print(" hardware_watchdog_ready=");
  Serial.print(hardware_watchdog_ready ? 1 : 0);
  Serial.print(" config_storage=algorithm_only event_storage=ram_only ");
  Serial.print("outputs=disabled motion=disabled\r\n");
}

void handle_request(parol6::safe_core::Request request, std::uint32_t now_ms) {
  using parol6::safe_core::Request;
  switch (request) {
    case Request::none:
      return;
    case Request::identify:
      core.accept_safe_command();
      write_identify();
      return;
    case Request::status:
      core.accept_safe_command();
      write_status();
      return;
    case Request::heartbeat:
      core.accept_safe_command();
      core.heartbeat(now_ms);
      Serial.print(
          "PAROL6_HEARTBEAT_REPLY state=not_commissioned outputs=disabled "
          "motion=disabled\r\n");
      return;
    case Request::diagnostics:
      core.accept_safe_command();
      write_diagnostics();
      return;
    case Request::help:
      core.accept_safe_command();
      Serial.print(
          "PAROL6_HELP commands=IDENTIFY,STATUS,HEARTBEAT,DIAGNOSTICS,HELP "
          "motion_commands=none\r\n");
      return;
    case Request::too_long:
      core.reject_command(now_ms);
      Serial.print(
          "PAROL6_ERROR code=line_too_long outputs=disabled motion=disabled\r\n");
      return;
    case Request::rejected:
      core.reject_command(now_ms);
      Serial.print(
          "PAROL6_ERROR code=command_rejected outputs=disabled motion=disabled\r\n");
      return;
  }
}

}  // namespace

void setup() {
  // USB CDC and the MCU watchdog are the only hardware facilities initialized.
  // Application code configures no GPIO, actuator, PWM, motor, relay, or UART.
  Serial.begin(3000000);
  core.boot(kFactorySafeConfig, parol6::safe_core::ConfigRecord{}, millis());
  hardware_watchdog_ready = initialize_hardware_watchdog();
}

void loop() {
  feed_hardware_watchdog();
  const auto now_ms = millis();
  core.poll(now_ms);

  const bool current_dtr = Serial.dtr();
  if (current_dtr && !previous_dtr) {
    Serial.print(
        "PAROL6_SAFE_CORE_READY firmware=" PAROL6_FIRMWARE_VERSION
        " state=not_commissioned outputs=disabled motion=disabled\r\n");
  }
  previous_dtr = current_dtr;

  while (Serial.available() > 0) {
    const int value = Serial.read();
    if (value >= 0) {
      handle_request(parser.feed(static_cast<std::uint8_t>(value)), now_ms);
    }
  }
  delay(1);
}
