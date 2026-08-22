#include <Arduino.h>
#include <TMCStepper.h>
#include <stm32h7xx_hal.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifndef PAROL6_FIRMWARE_VERSION
#define PAROL6_FIRMWARE_VERSION "unversioned"
#endif

namespace {

// Official Octopus Pro V1.1 schematic, DRIVER5 / MOTOR5.
constexpr std::uint32_t kStepPin = PC13;
constexpr std::uint32_t kDirectionPin = PF0;
constexpr std::uint32_t kEnablePin = PF1;  // active low
constexpr std::uint32_t kDriverUartPin = PE4;

constexpr std::uint16_t kRunCurrentMa = 250U;
constexpr std::uint16_t kMaximumPulses = 1600U;
constexpr std::uint16_t kMicrosteps = 16U;
constexpr std::uint32_t kPulseHalfPeriodUs = 1000U;
constexpr float kSenseResistorOhms = 0.11F;
constexpr std::uint8_t kTmcAddress = 0U;
constexpr std::uint8_t kExpectedTmcVersion = 0x21U;
constexpr std::size_t kMaximumLineBytes = 63U;

// PE4 is the Octopus' shared, single-wire PDN_UART signal for MOTOR5.  Use
// TMCStepper's same-pin software-UART path; PE4 is not a dedicated TX/RX pair.
TMC2209Stepper driver(kDriverUartPin, kDriverUartPin, kSenseResistorOhms,
                      kTmcAddress);
IWDG_HandleTypeDef hardware_watchdog{};
std::array<char, kMaximumLineBytes + 1U> line{};
std::size_t line_length = 0;
std::uint32_t arm_token = 0;
bool watchdog_ready = false;
bool jog_used = false;
bool previous_dtr = false;

void disable_driver() noexcept {
  digitalWrite(kEnablePin, HIGH);
  digitalWrite(kStepPin, LOW);
}

bool initialize_watchdog() noexcept {
  hardware_watchdog.Instance = IWDG1;
  hardware_watchdog.Init.Prescaler = IWDG_PRESCALER_32;
  hardware_watchdog.Init.Window = IWDG_WINDOW_DISABLE;
  // About one second at nominal 32 kHz LSI / 32 prescaler.
  hardware_watchdog.Init.Reload = 999U;
  return HAL_IWDG_Init(&hardware_watchdog) == HAL_OK;
}

void feed_watchdog() noexcept {
  if (watchdog_ready) {
    HAL_IWDG_Refresh(&hardware_watchdog);
  }
}

std::uint32_t make_arm_token() noexcept {
  std::uint32_t value = HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2() ^
                        static_cast<std::uint32_t>(micros());
  value ^= value << 13U;
  value ^= value >> 17U;
  value ^= value << 5U;
  return value == 0U ? 0x4A360001U : value;
}

void print_hex8(std::uint32_t value) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  for (int shift = 28; shift >= 0; shift -= 4) {
    Serial.write(kHex[(value >> shift) & 0xFU]);
  }
}

void print_ready() {
  disable_driver();
  Serial.print(
      "PAROL6_J6_DIAG_READY version=" PAROL6_FIRMWARE_VERSION
      " axis=J6 motor_slot=MOTOR5 current_ma=250 microsteps=16 "
      "max_pulses=1600 one_jog_per_boot=1 driver_disabled=1 token=");
  print_hex8(arm_token);
  Serial.print("\r\n");
}

bool configure_and_check_driver(std::uint8_t& connection,
                                std::uint8_t& version,
                                std::uint32_t& status) noexcept {
  disable_driver();
  driver.begin();
  connection = driver.test_connection();
  if (connection != 0U) {
    version = 0U;
    status = driver.DRV_STATUS();
    return false;
  }

  const auto ifcnt_before = driver.IFCNT();
  driver.pdn_disable(true);
  driver.mstep_reg_select(true);
  driver.I_scale_analog(false);
  driver.toff(4);
  driver.blank_time(24);
  driver.rms_current(kRunCurrentMa, 0.35F);
  driver.microsteps(kMicrosteps);
  driver.en_spreadCycle(false);
  driver.pwm_autoscale(true);
  driver.GSTAT(0b111U);
  const auto ifcnt_after = driver.IFCNT();
  version = driver.version();
  status = driver.DRV_STATUS();
  const bool write_acknowledged = ifcnt_after != ifcnt_before;
  const bool hard_fault = driver.ot() || driver.s2ga() || driver.s2gb() ||
                          driver.s2vsa() || driver.s2vsb();
  return write_acknowledged && version == kExpectedTmcVersion && !hard_fault;
}

void print_driver_result(const bool ready, const std::uint8_t connection,
                         const std::uint8_t version,
                         const std::uint32_t status) {
  Serial.print("PAROL6_J6_DRIVER ready=");
  Serial.print(ready ? 1 : 0);
  Serial.print(" connection=");
  Serial.print(connection);
  Serial.print(" version=0x");
  Serial.print(version, HEX);
  Serial.print(" status=0x");
  Serial.print(status, HEX);
  Serial.print(" current_ma=250 driver_disabled=1\r\n");
}

bool parse_jog(const char* command, bool& positive) noexcept {
  if (std::strncmp(command, "J6 ", 3U) != 0) {
    return false;
  }
  char* token_end = nullptr;
  const auto received_token = std::strtoul(command + 3U, &token_end, 16);
  if (token_end != command + 11U || token_end[0] != ' ' ||
      (token_end[1] != '+' && token_end[1] != '-') || token_end[2] != '\0' ||
      received_token != arm_token) {
    return false;
  }
  positive = token_end[1] == '+';
  return true;
}

void run_bounded_jog(const bool positive) {
  jog_used = true;
  digitalWrite(kDirectionPin, positive ? HIGH : LOW);
  digitalWrite(kStepPin, LOW);
  digitalWrite(kEnablePin, LOW);
  delay(20);

  for (std::uint16_t pulse = 0; pulse < kMaximumPulses; ++pulse) {
    feed_watchdog();
    digitalWrite(kStepPin, HIGH);
    delayMicroseconds(kPulseHalfPeriodUs);
    digitalWrite(kStepPin, LOW);
    delayMicroseconds(kPulseHalfPeriodUs);
  }
  disable_driver();
  const auto status = driver.DRV_STATUS();
  Serial.print("PAROL6_J6_JOG_COMPLETE direction=");
  Serial.print(positive ? "+" : "-");
  Serial.print(" pulses=1600 driver_disabled=1 status=0x");
  Serial.print(status, HEX);
  Serial.print("\r\n");
}

void handle_line(const char* command) {
  disable_driver();
  if (std::strcmp(command, "IDENTIFY") == 0) {
    print_ready();
    return;
  }
  if (std::strcmp(command, "CHECK") == 0) {
    std::uint8_t connection = 0;
    std::uint8_t version = 0;
    std::uint32_t status = 0;
    const bool ready =
        configure_and_check_driver(connection, version, status);
    print_driver_result(ready, connection, version, status);
    return;
  }
  bool positive = false;
  if (!parse_jog(command, positive)) {
    Serial.print("PAROL6_J6_ERROR code=bad_command driver_disabled=1\r\n");
    return;
  }
  if (jog_used) {
    Serial.print("PAROL6_J6_ERROR code=jog_already_used driver_disabled=1\r\n");
    return;
  }
  if (!watchdog_ready) {
    Serial.print("PAROL6_J6_ERROR code=watchdog_not_ready driver_disabled=1\r\n");
    return;
  }
  std::uint8_t connection = 0;
  std::uint8_t version = 0;
  std::uint32_t status = 0;
  if (!configure_and_check_driver(connection, version, status)) {
    print_driver_result(false, connection, version, status);
    return;
  }
  run_bounded_jog(positive);
}

}  // namespace

void setup() {
  // Preload safe output levels before changing the pins to output mode.
  digitalWrite(kEnablePin, HIGH);
  digitalWrite(kStepPin, LOW);
  digitalWrite(kDirectionPin, LOW);
  pinMode(kEnablePin, OUTPUT);
  pinMode(kStepPin, OUTPUT);
  pinMode(kDirectionPin, OUTPUT);
  disable_driver();

  Serial.begin(3000000);
  driver.beginSerial(115200);
  watchdog_ready = initialize_watchdog();
  arm_token = make_arm_token();
}

void loop() {
  feed_watchdog();
  disable_driver();

  const bool current_dtr = Serial.dtr();
  if (current_dtr && !previous_dtr) {
    print_ready();
  }
  previous_dtr = current_dtr;

  while (Serial.available() > 0) {
    const int value = Serial.read();
    if (value < 0) {
      continue;
    }
    const char character = static_cast<char>(value);
    if (character == '\r') {
      continue;
    }
    if (character == '\n') {
      line[line_length] = '\0';
      handle_line(line.data());
      line_length = 0;
      continue;
    }
    if (line_length >= kMaximumLineBytes) {
      line_length = 0;
      Serial.print("PAROL6_J6_ERROR code=line_too_long driver_disabled=1\r\n");
      continue;
    }
    line[line_length++] = character;
  }
  delay(1);
}
