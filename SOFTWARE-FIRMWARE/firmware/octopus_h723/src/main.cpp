#include <Arduino.h>

#include <cstdint>

#include "identity_protocol.hpp"

#ifndef PAROL6_FIRMWARE_VERSION
#define PAROL6_FIRMWARE_VERSION "unversioned"
#endif

#ifndef PAROL6_BOARD_NAME
#define PAROL6_BOARD_NAME "unknown"
#endif

namespace {

constexpr char kReady[] =
    "PAROL6_SAFE_ID_READY firmware=" PAROL6_FIRMWARE_VERSION
    " outputs=disabled motion=disabled\r\n";
constexpr char kIdentify[] =
    "PAROL6_IDENTIFY product=PAROL6 firmware=safe_identity version="
    PAROL6_FIRMWARE_VERSION " board=" PAROL6_BOARD_NAME
    " mcu=STM32H723ZE transport=usb_cdc outputs=disabled motion=disabled\r\n";
constexpr char kHelp[] =
    "PAROL6_HELP commands=IDENTIFY,STATUS,HELP motion_commands=none\r\n";
constexpr char kRejected[] =
    "PAROL6_ERROR code=command_rejected outputs=disabled motion=disabled\r\n";
constexpr char kTooLong[] =
    "PAROL6_ERROR code=line_too_long outputs=disabled motion=disabled\r\n";

parol6::identity::LineParser parser;
std::uint32_t accepted_commands = 0;
std::uint32_t rejected_commands = 0;
bool previous_dtr = false;

void write_status() {
  Serial.print(
      "PAROL6_STATUS mode=identity_only outputs=disabled motion=disabled "
      "actuator_power=required_off accepted=");
  Serial.print(accepted_commands);
  Serial.print(" rejected=");
  Serial.print(rejected_commands);
  Serial.print("\r\n");
}

void handle_request(const parol6::identity::Request request) {
  using parol6::identity::Request;
  switch (request) {
    case Request::none:
      return;
    case Request::identify:
      ++accepted_commands;
      Serial.print(kIdentify);
      return;
    case Request::status:
      ++accepted_commands;
      write_status();
      return;
    case Request::help:
      ++accepted_commands;
      Serial.print(kHelp);
      return;
    case Request::too_long:
      ++rejected_commands;
      Serial.print(kTooLong);
      return;
    case Request::rejected:
      ++rejected_commands;
      Serial.print(kRejected);
      return;
  }
}

}  // namespace

void setup() {
  // Deliberately initialize USB CDC only. Application code never configures a
  // GPIO output, timer, motor driver, heater, fan, relay, servo, UART, or ADC.
  Serial.begin(3000000);
}

void loop() {
  const bool current_dtr = Serial.dtr();
  if (current_dtr && !previous_dtr) {
    Serial.print(kReady);
  }
  previous_dtr = current_dtr;

  while (Serial.available() > 0) {
    const int value = Serial.read();
    if (value >= 0) {
      handle_request(parser.feed(static_cast<std::uint8_t>(value)));
    }
  }
  delay(1);
}
