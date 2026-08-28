#include <AccelStepper.h>
#include <Arduino.h>
#include <TMCStepper.h>
#include <stm32h7xx_hal.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "calibration_store.hpp"
#include "p6b1_protocol.hpp"

#ifndef PAROL6_FIRMWARE_VERSION
#define PAROL6_FIRMWARE_VERSION "unversioned"
#endif

namespace {

constexpr std::uint32_t kBaud = 3000000U;
constexpr std::uint32_t kTmcBaud = 115200U;
// Owner-plan commissioning currents. These remain deliberately below the
// provisional ceilings and must be validated with loaded thermal testing.
constexpr std::array<std::uint16_t, 6> kRunCurrentMa = {
    0U, 0U, 700U, 700U, 700U, 450U};
constexpr std::uint16_t kMicrosteps = 16U;
// J2 is gravity-loaded. Its Servo42C current is configured locally, so the
// Octopus improves lift margin by demanding less speed and acceleration.
constexpr std::array<float, 2> kServoMaximumPulseRate = {500.0F, 350.0F};
constexpr float kJ2MaximumPulseAcceleration = 900.0F;
constexpr std::uint16_t kServoPulseWidthUs = 1000U;
constexpr std::uint16_t kTmcPulseWidthUs = 8U;
constexpr float kSenseResistorOhms = 0.11F;
constexpr std::uint8_t kTmcAddress = 0U;
constexpr std::uint8_t kExpectedTmcVersion = 0x21U;
constexpr std::size_t kLineCapacity = 127U;
constexpr std::uint8_t kDebounceSamples = 8U;
constexpr std::uint32_t kHostMotionTimeoutMs = 2000U;
constexpr std::uint32_t kP6b1WatchdogMs = 250U;
constexpr std::uint32_t kP6b1ProfileCrc32c = 0x9F6BC640U;
constexpr std::size_t kP6b1QueueCapacity = 512U;
constexpr std::uint32_t kP6b1StatusPeriodMs = 20U;
constexpr std::uint32_t kMotorHoldTimeoutMs = 2000U;
constexpr std::uint32_t kHoldKeepaliveTimeoutMs = 400U;
constexpr std::int32_t kMaximumJogMilliDegrees = 10000;
constexpr std::int32_t kLimitTestInsetMilliDegrees = 10000;
constexpr std::int32_t kDirectionDiscoveryJogMilliDegrees = 2000;
constexpr std::int32_t kMaximumHoldTravelMilliDegrees = 45000;
constexpr std::int32_t kMinimumHoldSpeedMilliDegreesPerSecond = 3000;
constexpr std::int32_t kMaximumHoldSpeedMilliDegreesPerSecond = 45000;
constexpr std::int32_t kHomeSeekMilliDegrees = 90000;
constexpr std::int32_t kHomeInitialReleaseMilliDegrees = 30000;
constexpr std::int32_t kHomeBackoffMilliDegrees = 5000;
constexpr std::int32_t kHomeMarginMilliDegrees = 500;
constexpr std::int32_t kHomeBoundaryGuardMilliDegrees = 2000;
constexpr std::int32_t kHomeLatchMilliDegrees = 3000;
constexpr std::int32_t kJ1HardMinimumMilliDegrees = -230000;
constexpr std::int32_t kJ1HardMaximumMilliDegrees = 35000;
constexpr std::int32_t kJ6HardMinimumMilliDegrees = -180000;
constexpr std::int32_t kJ6HardMaximumMilliDegrees = 180000;
constexpr bool kJ6SensorHomeEnabled = false;
constexpr std::size_t kJ5Axis = 4U;
constexpr std::int32_t kJ5PostHomeStandbyMilliDegrees = -130000;
constexpr std::uint32_t kMinimumCoordinatedDurationMs = 500U;
constexpr std::uint32_t kMaximumCoordinatedDurationMs = 60000U;
// Owner-selected 80% commissioning stage. J1/J2 remain at their separately
// validated Servo42C pulse ceilings; J3-J6 use 80% of the reviewed ceilings.
constexpr std::array<float, 6> kCoordinatedMaximumDegreesPerSecond = {
    4.0F, 1.0F, 36.0F, 36.0F, 36.0F, 36.0F};
constexpr std::array<float, 6> kCoordinatedMaximumAccelerationDegreesPerSecond2 = {
    8.0F, 2.5F, 96.0F, 96.0F, 96.0F, 96.0F};

constexpr std::array<std::uint32_t, 6> kStepPins = {
    PF13, PG0, PF11, PG4, PF9, PC13};
constexpr std::array<std::uint32_t, 6> kDirectionPins = {
    PF12, PG1, PG3, PC1, PF10, PF0};
constexpr std::array<std::uint32_t, 6> kEnablePins = {
    PF14, PF15, PG5, PA2, PG2, PF1};
constexpr std::array<std::uint32_t, 6> kHomePins = {
    // Owner-verified as-built mechanical switch cycle:
    // physical J2 -> STOP2, physical J3 -> STOP4, physical J5 -> STOP1.
    PG6, PG10, PG12, PG11, PG9, PG13};
constexpr std::array<std::uint32_t, 2> kOtherStopPins = {PG14, PG15};
constexpr std::array<std::uint32_t, 4> kTemperaturePins = {PF4, PF5, PF6, PF7};
constexpr std::uint32_t kPowerDetectPin = PC0;

// Owner-selected hardware conversion values. Sensor homing establishes the
// repeatable zero; commanded angle remains step-derived and open-loop.
constexpr auto kPulsesPerDegree =
    parol6::calibration::kHardwarePulsesPerDegree;

AccelStepper stepper_j1(AccelStepper::DRIVER, PF13, PF12, 0, 0, false);
AccelStepper stepper_j2(AccelStepper::DRIVER, PG0, PG1, 0, 0, false);
AccelStepper stepper_j3(AccelStepper::DRIVER, PF11, PG3, 0, 0, false);
AccelStepper stepper_j4(AccelStepper::DRIVER, PG4, PC1, 0, 0, false);
AccelStepper stepper_j5(AccelStepper::DRIVER, PF9, PF10, 0, 0, false);
AccelStepper stepper_j6(AccelStepper::DRIVER, PC13, PF0, 0, 0, false);
std::array<AccelStepper*, 6> steppers = {
    &stepper_j1, &stepper_j2, &stepper_j3,
    &stepper_j4, &stepper_j5, &stepper_j6};

TMC2209Stepper driver_j3(PC6, PC6, kSenseResistorOhms, kTmcAddress);
TMC2209Stepper driver_j4(PC7, PC7, kSenseResistorOhms, kTmcAddress);
TMC2209Stepper driver_j5(PF2, PF2, kSenseResistorOhms, kTmcAddress);
TMC2209Stepper driver_j6(PE4, PE4, kSenseResistorOhms, kTmcAddress);
std::array<TMC2209Stepper*, 4> tmc_drivers = {
    &driver_j3, &driver_j4, &driver_j5, &driver_j6};

struct DebouncedInput {
  bool raw = true;
  bool stable = true;
  std::uint8_t count = 0;
};

enum class MotionKind : std::uint8_t { none, jog, hold, home, coordinated };
enum class HomePhase : std::uint8_t {
  none,
  initial_backoff,
  fast_seek,
  latch_backoff,
  backoff_margin,
  slow_seek,
  final_clear,
  post_home_standby,
};

struct MotionProfile {
  const char* name;
  float max_degrees_per_second;
  float acceleration_degrees_per_second2;
};

constexpr std::array<MotionProfile, 3> kProfiles = {{
    {"GENTLE", 6.0F, 12.0F},
    {"NORMAL", 18.0F, 45.0F},
    {"BRISK", 45.0F, 120.0F},
}};

struct MotionTask {
  bool running = false;
  MotionKind kind = MotionKind::none;
  HomePhase home_phase = HomePhase::none;
  std::size_t axis = 0U;
  bool positive = true;
  bool initial_axis_sensor = true;
  std::array<bool, 2> initial_other_stops{};
  std::int32_t requested_millidegrees = 0;
  const MotionProfile* profile = &kProfiles[0];
  std::uint32_t started_ms = 0U;
  std::uint32_t hold_deadline_ms = 0U;
  bool soft_limit_target = false;
  bool home_sensor_cleared = false;
};

struct CoordinatedTask {
  std::array<std::int32_t, 6> targets_millidegrees{};
  std::array<bool, 6> moving{};
  std::array<bool, 6> initial_sensors{};
  std::array<bool, 2> initial_other_stops{};
  std::uint32_t duration_ms = 0U;
};

std::array<DebouncedInput, 8> stop_states{};
std::array<bool, 2> servo_gate_open{};
std::array<bool, 2> servo_enable_active_low{};
std::array<bool, 6> home_configured{};
std::array<bool, 6> home_direction_positive{};
std::array<bool, 6> home_active_level{};
std::array<bool, 6> positive_direction_raw_positive{};
std::array<bool, 6> homed{};
std::array<bool, 6> manual_home_temporary{};
std::array<char, kLineCapacity + 1U> line{};
std::size_t line_length = 0U;
std::uint32_t command_token = 0U;
std::uint32_t last_sensor_sample_ms = 0U;
std::uint32_t last_host_contact_ms = 0U;
MotionTask motion{};
CoordinatedTask coordinated{};
bool coordinated_hold_active = false;
std::array<bool, 6> coordinated_hold_initial_sensors{};
std::array<bool, 2> coordinated_hold_initial_other_stops{};
bool motor_hold_active = false;
std::size_t motor_hold_axis = 0U;
bool motor_hold_initial_axis_sensor = true;
std::array<bool, 2> motor_hold_initial_other_stops{};

parol6::p6b1::FrameParser p6_parser{};
parol6::p6b1::SetpointQueue<kP6b1QueueCapacity> p6_queue{};
std::uint32_t p6_state = parol6::p6b1::idle;
std::uint32_t p6_faults = parol6::p6b1::no_fault;
std::uint32_t p6_last_host_sequence = 0U;
std::uint32_t p6_output_sequence = 1U;
std::uint32_t p6_last_contact_ms = 0U;
std::uint32_t p6_last_status_ms = 0U;
std::uint32_t p6_next_setpoint_us = 0U;
std::uint16_t p6_period_us = 10000U;
bool p6_handshake_complete = false;
bool p6_binary_session = false;
bool p6_motion_running = false;
bool p6_finish_requested = false;
std::uint32_t p6_queue_empty_since_ms = 0U;
bool p6_j1_auto_home = false;
bool p6_home_sequence_active = false;
std::array<std::size_t, 6> p6_home_order = {0U, 1U, 2U, 3U, 5U, 4U};
std::size_t p6_home_index = 0U;
std::array<bool, 6> p6_initial_sensors{};
std::array<bool, 2> p6_initial_other_stops{};
IWDG_HandleTypeDef hardware_watchdog{};
bool watchdog_ready = false;
parol6::calibration::Store calibration_store{};
parol6::calibration::CalibrationRecord calibration_record{};

void feed_watchdog() noexcept {
  if (watchdog_ready) HAL_IWDG_Refresh(&hardware_watchdog);
}

bool initialize_watchdog() noexcept {
  hardware_watchdog.Instance = IWDG1;
  hardware_watchdog.Init.Prescaler = IWDG_PRESCALER_32;
  hardware_watchdog.Init.Window = IWDG_WINDOW_DISABLE;
  hardware_watchdog.Init.Reload = 1999U;
  return HAL_IWDG_Init(&hardware_watchdog) == HAL_OK;
}

std::uint32_t make_token() noexcept {
  std::uint32_t value = HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2() ^
                        static_cast<std::uint32_t>(micros()) ^ command_token;
  value ^= value << 13U;
  value ^= value >> 17U;
  value ^= value << 5U;
  return value == 0U ? 0xC0110001U : value;
}

void rotate_token() noexcept { command_token = make_token(); }

void print_token(std::uint32_t value) {
  static constexpr char hex[] = "0123456789ABCDEF";
  for (int shift = 28; shift >= 0; shift -= 4) {
    Serial.write(hex[(value >> shift) & 0xFU]);
  }
}

void print_hex32(std::uint32_t value) { print_token(value); }

std::int32_t millidegrees_to_steps(std::size_t axis,
                                   std::int32_t millidegrees) {
  const std::int64_t scaled =
      static_cast<std::int64_t>(kPulsesPerDegree[axis]) * millidegrees;
  return static_cast<std::int32_t>((scaled >= 0 ? scaled + 500 : scaled - 500) /
                                   1000);
}

std::int32_t steps_to_millidegrees(std::size_t axis, long steps) {
  return static_cast<std::int32_t>(
      (static_cast<std::int64_t>(steps) * 1000) /
      static_cast<std::int64_t>(kPulsesPerDegree[axis]));
}

bool joint_flag(std::size_t axis, std::uint8_t flag) {
  return (calibration_record.joints[axis].flags & flag) != 0U;
}

void set_joint_flag(std::size_t axis, std::uint8_t flag, bool enabled) {
  auto& flags = calibration_record.joints[axis].flags;
  if (enabled) flags |= flag;
  else flags &= static_cast<std::uint8_t>(~flag);
}

void apply_j1_hardcoded_limits() {
  auto& joint = calibration_record.joints[0];
  joint.minimum_millidegrees = kJ1HardMinimumMilliDegrees;
  joint.maximum_millidegrees = kJ1HardMaximumMilliDegrees;
  joint.flags |= static_cast<std::uint8_t>(
      parol6::calibration::kMinimumSet |
      parol6::calibration::kMaximumSet);
}

void apply_j6_hardcoded_limits() {
  auto& joint = calibration_record.joints[5];
  joint.minimum_millidegrees = kJ6HardMinimumMilliDegrees;
  joint.maximum_millidegrees = kJ6HardMaximumMilliDegrees;
  joint.flags |= static_cast<std::uint8_t>(
      parol6::calibration::kMinimumSet |
      parol6::calibration::kMaximumSet);
}

void apply_hardcoded_limits() {
  apply_j1_hardcoded_limits();
  apply_j6_hardcoded_limits();
}

void load_calibration_runtime() {
  calibration_record = calibration_store.record();
  apply_hardcoded_limits();
  for (std::size_t axis = 0U; axis < 6U; ++axis) {
    home_configured[axis] =
        joint_flag(axis, parol6::calibration::kConfigured);
    home_direction_positive[axis] =
        joint_flag(axis, parol6::calibration::kHomeRawPositive);
    positive_direction_raw_positive[axis] =
        joint_flag(axis, parol6::calibration::kPositiveRawPositive);
    home_active_level[axis] =
        joint_flag(axis, parol6::calibration::kSensorActiveHigh);
  }
}

std::int32_t joint_position_millidegrees(std::size_t axis) {
  const std::int32_t raw =
      steps_to_millidegrees(axis, steppers[axis]->currentPosition());
  return positive_direction_raw_positive[axis] ? raw : -raw;
}

std::int32_t logical_to_raw_millidegrees(std::size_t axis,
                                         std::int32_t logical) {
  return positive_direction_raw_positive[axis] ? logical : -logical;
}

bool minimum_is_set(std::size_t axis) {
  return joint_flag(axis, parol6::calibration::kMinimumSet);
}

bool maximum_is_set(std::size_t axis) {
  return joint_flag(axis, parol6::calibration::kMaximumSet);
}

bool home_is_logical_positive(std::size_t axis) {
  return home_direction_positive[axis] ==
         positive_direction_raw_positive[axis];
}

bool has_automatic_home_boundary(std::size_t axis) {
  return axis == 1U || axis == 2U || axis == 3U;
}

bool automatic_home_boundary_is_minimum(std::size_t axis) {
  // J4 always clears its sensor in logical positive and defines that clear
  // position as its zero-degree minimum. J2/J3 retain learned direction logic.
  return axis == 3U || !home_is_logical_positive(axis);
}

bool logical_target_is_safe(std::size_t axis, std::int32_t target) {
  const auto& joint = calibration_record.joints[axis];
  return (!minimum_is_set(axis) || target >= joint.minimum_millidegrees) &&
         (!maximum_is_set(axis) || target <= joint.maximum_millidegrees) &&
         target >= -parol6::calibration::kAbsoluteAngleCeilingMilliDegrees &&
         target <= parol6::calibration::kAbsoluteAngleCeilingMilliDegrees;
}

void set_servo_enable(std::size_t axis, bool enabled) {
  if (axis > 1U || !servo_gate_open[axis]) {
    pinMode(kEnablePins[axis], INPUT);
    return;
  }
  pinMode(kEnablePins[axis], OUTPUT);
  const bool active_low = servo_enable_active_low[axis];
  digitalWrite(kEnablePins[axis], enabled == active_low ? LOW : HIGH);
}

void disable_axis(std::size_t axis) {
  if (axis < 2U) {
    set_servo_enable(axis, false);
    digitalWrite(kStepPins[axis], HIGH);
  } else {
    digitalWrite(kEnablePins[axis], HIGH);
    digitalWrite(kStepPins[axis], LOW);
  }
}

void enable_axis(std::size_t axis) {
  if (axis < 2U) set_servo_enable(axis, true);
  else digitalWrite(kEnablePins[axis], LOW);
}

void disable_all() {
  for (std::size_t axis = 0U; axis < 6U; ++axis) disable_axis(axis);
}

void configure_servo_interface(std::size_t axis, bool active_low) {
  servo_enable_active_low[axis] = active_low;
  servo_gate_open[axis] = true;
  pinMode(kStepPins[axis], OUTPUT);
  pinMode(kDirectionPins[axis], OUTPUT);
  digitalWrite(kStepPins[axis], HIGH);
  digitalWrite(kDirectionPins[axis], LOW);
  disable_axis(axis);
}

void configure_owner_servo_interfaces() {
  // J1/J2 were physically validated with the supplied 3.3 V push-pull
  // adapters and active-low enable. Keep their pins tri-stated through boot;
  // only the exact owner-profile P6B1 session clear may open these gates.
  configure_servo_interface(0U, true);
  configure_servo_interface(1U, true);
}

void sample_sensors();
void end_motion(const char* result);
void error(const char* code);
void service_coordinated();

void release_coordinated_hold(const char* result) {
  if (!coordinated_hold_active) return;
  disable_all();
  coordinated_hold_active = false;
  Serial.print("PAROL6_COORDINATED_HOLD_RELEASED result=");
  Serial.print(result);
  Serial.print(" driver_disabled=1 token=");
  print_token(command_token);
  Serial.print("\r\n");
}

void release_motor_hold(const char* result) {
  if (!motor_hold_active) return;
  const std::size_t axis = motor_hold_axis;
  disable_axis(axis);
  motor_hold_active = false;
  Serial.print("PAROL6_MOTOR_HOLD_RELEASED joint=J");
  Serial.print(axis + 1U);
  Serial.print(" result=");
  Serial.print(result);
  Serial.print(" driver_disabled=1 token=");
  print_token(command_token);
  Serial.print("\r\n");
}

void engage_motor_hold(std::size_t axis, const char* source) {
  // Stop at the exact commanded step reached by the hold-to-jog press before
  // changing the enable state.  This keeps the release atomic from the host's
  // point of view and prevents a gravity-loaded joint from being left
  // disabled between STOP and a separate hold command.
  const long position = steppers[axis]->currentPosition();
  steppers[axis]->setCurrentPosition(position);
  disable_all();
  sample_sensors();
  motor_hold_axis = axis;
  motor_hold_initial_axis_sensor = stop_states[axis].stable;
  motor_hold_initial_other_stops = {stop_states[6].stable, stop_states[7].stable};
  enable_axis(axis);
  delay(20);
  motor_hold_active = true;
  Serial.print("PAROL6_MOTOR_HOLD joint=J");
  Serial.print(axis + 1U);
  Serial.print(" result=engaged source=");
  Serial.print(source);
  Serial.print(" driver_disabled=0 token=");
  print_token(command_token);
  Serial.print("\r\n");
}

bool handoff_motor_hold_to_motion(std::size_t axis) {
  if (!motor_hold_active) return true;
  if (axis != motor_hold_axis) {
    error("motor_hold_active");
    return false;
  }
  // A held axis may resume only if its guarded inputs are still unchanged.
  // Keep the enable asserted for the handoff; the subsequent motion command
  // re-applies its profile and rotates the token after validation.
  sample_sensors();
  if (stop_states[motor_hold_axis].stable != motor_hold_initial_axis_sensor ||
      stop_states[6].stable != motor_hold_initial_other_stops[0] ||
      stop_states[7].stable != motor_hold_initial_other_stops[1]) {
    release_motor_hold("limit_abort");
    return false;
  }
  motor_hold_active = false;
  Serial.print("PAROL6_MOTOR_HOLD_RELEASED joint=J");
  Serial.print(axis + 1U);
  Serial.print(" result=handoff driver_disabled=0 token=");
  print_token(command_token);
  Serial.print("\r\n");
  return true;
}

void finish_hold_jog_release() {
  const std::size_t axis = motion.axis;
  sample_sensors();
  if (stop_states[axis].stable != motion.initial_axis_sensor ||
      stop_states[6].stable != motion.initial_other_stops[0] ||
      stop_states[7].stable != motion.initial_other_stops[1]) {
    end_motion("limit_abort");
    return;
  }
  // J1/J2 use Servo42C modules in the owner's open-loop configuration. Their
  // internal position-hold loop can hunt when left enabled at standstill, so
  // always remove torque after a hold-to-jog release. Gravity-load J2 must be
  // externally supported. TMC2209 joints retain the requested motor hold.
  if (axis < 2U) {
    end_motion("coast_release");
    return;
  }
  const long position = steppers[axis]->currentPosition();
  steppers[axis]->setCurrentPosition(position);
  motion = MotionTask{};
  engage_motor_hold(axis, "hold_release");
}

void sample_sensors() {
  const std::uint32_t now = millis();
  if (now == last_sensor_sample_ms) return;
  last_sensor_sample_ms = now;
  for (std::size_t index = 0U; index < stop_states.size(); ++index) {
    const std::uint32_t pin = index < 6U ? kHomePins[index]
                                         : kOtherStopPins[index - 6U];
    const bool next = digitalRead(pin) != LOW;
    auto& state = stop_states[index];
    state.raw = next;
    if (next == state.stable) state.count = 0U;
    else if (++state.count >= kDebounceSamples) {
      state.stable = next;
      state.count = 0U;
    }
  }
}

bool configure_tmc(std::size_t axis, std::uint8_t& connection,
                   std::uint8_t& version, std::uint32_t& status) {
  disable_axis(axis);
  auto& driver = *tmc_drivers[axis - 2U];
  driver.beginSerial(kTmcBaud);
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
  driver.rms_current(kRunCurrentMa[axis], 0.35F);
  driver.microsteps(kMicrosteps);
  driver.en_spreadCycle(false);
  driver.pwm_autoscale(true);
  driver.GSTAT(0b111U);
  const auto ifcnt_after = driver.IFCNT();
  version = driver.version();
  status = driver.DRV_STATUS();
  const bool hard_fault = driver.ot() || driver.s2ga() || driver.s2gb() ||
                          driver.s2vsa() || driver.s2vsb();
  return ifcnt_after != ifcnt_before && version == kExpectedTmcVersion &&
         !hard_fault;
}

void print_tmc_result(std::size_t axis, bool ready, std::uint8_t connection,
                      std::uint8_t version, std::uint32_t status) {
  if (p6_binary_session) return;
  Serial.print("PAROL6_DRIVER joint=J");
  Serial.print(axis + 1U);
  Serial.print(" ready=");
  Serial.print(ready ? 1 : 0);
  Serial.print(" connection=");
  Serial.print(connection);
  Serial.print(" version=0x");
  Serial.print(version, HEX);
  Serial.print(" status=0x");
  Serial.print(status, HEX);
  Serial.print(" current_ma=");
  Serial.print(kRunCurrentMa[axis]);
  Serial.print(" driver_disabled=1\r\n");
}

bool preflight_axis(std::size_t axis) {
  if (axis < 2U) return servo_gate_open[axis];
  std::uint8_t connection = 0U;
  std::uint8_t version = 0U;
  std::uint32_t status = 0U;
  const bool ready = configure_tmc(axis, connection, version, status);
  if (!ready) print_tmc_result(axis, false, connection, version, status);
  return ready;
}

void apply_profile(std::size_t axis, const MotionProfile& profile,
                   float speed_scale = 1.0F) {
  auto& stepper = *steppers[axis];
  float maximum_pulse_rate = profile.max_degrees_per_second * speed_scale *
                             kPulsesPerDegree[axis];
  if (axis < 2U && maximum_pulse_rate > kServoMaximumPulseRate[axis]) {
    maximum_pulse_rate = kServoMaximumPulseRate[axis];
  }
  stepper.setMaxSpeed(maximum_pulse_rate);
  float acceleration = profile.acceleration_degrees_per_second2 * speed_scale *
                       kPulsesPerDegree[axis];
  if (axis == 1U && acceleration > kJ2MaximumPulseAcceleration) {
    acceleration = kJ2MaximumPulseAcceleration;
  }
  stepper.setAcceleration(acceleration);
  stepper.setMinPulseWidth(axis < 2U ? kServoPulseWidthUs : kTmcPulseWidthUs);
}

void apply_hold_profile(std::size_t axis, std::int32_t speed_mdeg_per_second,
                        const MotionProfile& profile) {
  auto& stepper = *steppers[axis];
  const float degrees_per_second =
      static_cast<float>(speed_mdeg_per_second) / 1000.0F;
  float maximum_pulse_rate = degrees_per_second * kPulsesPerDegree[axis];
  if (axis < 2U && maximum_pulse_rate > kServoMaximumPulseRate[axis]) {
    maximum_pulse_rate = kServoMaximumPulseRate[axis];
  }
  stepper.setMaxSpeed(maximum_pulse_rate);
  float acceleration =
      profile.acceleration_degrees_per_second2 * kPulsesPerDegree[axis];
  if (axis == 1U && acceleration > kJ2MaximumPulseAcceleration) {
    acceleration = kJ2MaximumPulseAcceleration;
  }
  stepper.setAcceleration(acceleration);
  stepper.setMinPulseWidth(axis < 2U ? kServoPulseWidthUs : kTmcPulseWidthUs);
}

void print_ready() {
  Serial.print("PAROL6_MOTION_RC_READY version=" PAROL6_FIRMWARE_VERSION
               " board=OCTOPUS_PRO_V1_1_H723 joints=6 stops=8 adc=5 "
               "servo_signal=push_pull_3v3 servo_clock_max_hz=J1:500,J2:350 "
               "servo_pulse_us=1000 profiles=GENTLE,NORMAL,BRISK "
               "j2_lift_accel_max_pulses_s2=900 servo42c_ma=J1:2000,J2:2000 "
               "hold_speed_mdeg_s=3000-45000 hold_cap_mdeg=45000 "
               "motor_hold=host_supervised servo_hold=disabled "
               "coordinated_move=all_joints_shared_trapezoid "
               "coordinated_speed_cap_percent=80 coordinated_hold=host_supervised "
               "home_sequence=J2,J3,J4,J6,J5 "
               "j5_post_home_standby_mdeg=-130000 "
               "mechanical_home_map=J2:PG10,J3:PG12,J5:PG9 "
               "home_limits=J2:J3:auto_zero_boundary "
               "j4_home=positive_clear_then_zero_min "
               "home_style=standard_two_pass_adapted "
               "home_seek_max_mdeg=90000 "
               "home_initial_release_max_mdeg=30000 "
               "j1_home=sensor_or_manual_temporary "
               "j1_limits_mdeg=-230000:35000 "
               "j6_limits_mdeg=-180000:180000 "
               "limit_test=max,max_minus_10 motor_stop=software_stop "
               "calibration=dual_slot_crc32c soft_limits=firmware_enforced "
               "direction_discovery=raw_2deg "
               "manual_zero=j1_runtime_only "
               "driver_disabled=");
  Serial.print((motion.running || motor_hold_active || coordinated_hold_active)
                   ? 0
                   : 1);
  Serial.print(" token=");
  print_token(command_token);
  Serial.print("\r\n");
}

void print_sensors() {
  sample_sensors();
  Serial.print("PAROL6_SENSORS ms=");
  Serial.print(millis());
  for (std::size_t index = 0U; index < 8U; ++index) {
    Serial.print(index < 6U ? " J" : " STOP");
    Serial.print(index < 6U ? index + 1U : index);
    Serial.print('=');
    Serial.print(stop_states[index].raw ? 1 : 0);
    Serial.print('/');
    Serial.print(stop_states[index].stable ? 1 : 0);
  }
  for (std::size_t index = 0U; index < kTemperaturePins.size(); ++index) {
    Serial.print(" T");
    Serial.print(index);
    Serial.print('=');
    Serial.print(analogRead(kTemperaturePins[index]));
  }
  Serial.print(" PWR=");
  Serial.print(analogRead(kPowerDetectPin));
  Serial.print(" moving=");
  Serial.print(motion.running ? 1 : 0);
  Serial.print("\r\n");
}

void print_status() {
  Serial.print("PAROL6_STATUS moving=");
  Serial.print(motion.running ? 1 : 0);
  Serial.print(" mode=");
  Serial.print(coordinated_hold_active
                   ? "COORDINATED_HOLD"
                   : motor_hold_active
                   ? "HOLD_TORQUE"
                   : motion.kind == MotionKind::jog
                   ? "JOG"
                   : motion.kind == MotionKind::hold
                         ? "HOLD"
                         : motion.kind == MotionKind::home
                               ? "HOME"
                               : motion.kind == MotionKind::coordinated
                                     ? "COORDINATED"
                                     : "IDLE");
  Serial.print(" active=");
  if (coordinated_hold_active ||
      (motion.running && motion.kind == MotionKind::coordinated)) {
    Serial.print("ALL");
  } else if (motion.running || motor_hold_active) {
    Serial.print('J');
    Serial.print((motor_hold_active ? motor_hold_axis : motion.axis) + 1U);
  } else Serial.print("NONE");
  Serial.print(" held=");
  if (coordinated_hold_active) {
    Serial.print("ALL");
  } else if (motor_hold_active) {
    Serial.print('J');
    Serial.print(motor_hold_axis + 1U);
  } else {
    Serial.print("NONE");
  }
  for (std::size_t axis = 0U; axis < 6U; ++axis) {
    Serial.print(" J");
    Serial.print(axis + 1U);
    Serial.print("_mdeg=");
    Serial.print(joint_position_millidegrees(axis));
    Serial.print(" J");
    Serial.print(axis + 1U);
    Serial.print("_homed=");
    Serial.print(homed[axis] ? 1 : 0);
    Serial.print(" J");
    Serial.print(axis + 1U);
    Serial.print("_home_source=");
    Serial.print(!homed[axis] ? "NONE" :
                 manual_home_temporary[axis] ? "MANUAL_TEMP" : "SENSOR");
  }
  Serial.print(" driver_disabled=");
  Serial.print((motion.running || motor_hold_active || coordinated_hold_active)
                   ? 0
                   : 1);
  Serial.print(" token=");
  print_token(command_token);
  Serial.print("\r\n");
}

const char* calibration_storage_name() {
  switch (calibration_store.status()) {
    case parol6::calibration::StoreStatus::flash_selected:
      return "FLASH";
    case parol6::calibration::StoreStatus::io_error:
      return "IO_ERROR";
    case parol6::calibration::StoreStatus::factory_fallback:
      return "FACTORY_EMPTY";
  }
  return "UNKNOWN";
}

void print_joint_calibration(std::size_t axis) {
  if (p6_binary_session) return;
  const auto& joint = calibration_record.joints[axis];
  const bool positive_raw =
      joint_flag(axis, parol6::calibration::kPositiveRawPositive);
  Serial.print("PAROL6_CALIBRATION joint=J");
  Serial.print(axis + 1U);
  Serial.print(" configured=");
  Serial.print(joint_flag(axis, parol6::calibration::kConfigured) ? 1 : 0);
  Serial.print(" home_raw=");
  Serial.print(joint_flag(axis, parol6::calibration::kHomeRawPositive) ? '+' : '-');
  Serial.print(" positive_raw=");
  Serial.print(positive_raw ? '+' : '-');
  Serial.print(" negative_raw=");
  Serial.print(positive_raw ? '-' : '+');
  Serial.print(" active=");
  Serial.print(joint_flag(axis, parol6::calibration::kSensorActiveHigh)
                   ? "HIGH"
                   : "LOW");
  Serial.print(" min_set=");
  Serial.print(minimum_is_set(axis) ? 1 : 0);
  Serial.print(" min_mdeg=");
  Serial.print(joint.minimum_millidegrees);
  Serial.print(" max_set=");
  Serial.print(maximum_is_set(axis) ? 1 : 0);
  Serial.print(" max_mdeg=");
  Serial.print(joint.maximum_millidegrees);
  Serial.print(" limit_source=");
  Serial.print((axis == 0U || axis == 5U) ? "HARDCODED" : "CAPTURED");
  Serial.print(" pulses_per_degree=");
  Serial.print(joint.pulses_per_degree);
  Serial.print(" homed=");
  Serial.print(homed[axis] ? 1 : 0);
  Serial.print(" home_source=");
  Serial.print(!homed[axis] ? "NONE" :
               manual_home_temporary[axis] ? "MANUAL_TEMP" : "SENSOR");
  Serial.print("\r\n");
}

void print_calibration() {
  Serial.print("PAROL6_CALIBRATION_HEADER schema=");
  Serial.print(parol6::calibration::kSchemaVersion);
  Serial.print(" storage=");
  Serial.print(calibration_storage_name());
  Serial.print(" sequence=");
  Serial.print(calibration_record.sequence);
  Serial.print(" device_uid=");
  print_hex32(HAL_GetUIDw0());
  print_hex32(HAL_GetUIDw1());
  print_hex32(HAL_GetUIDw2());
  Serial.print(" home_angle_mdeg=0 angle_source=commanded_steps_open_loop\r\n");
  for (std::size_t axis = 0U; axis < 6U; ++axis) {
    print_joint_calibration(axis);
  }
  Serial.print("PAROL6_CALIBRATION_DONE token=");
  print_token(command_token);
  Serial.print("\r\n");
}

bool parse_axis(const char* text, std::size_t& axis) {
  if (text == nullptr || text[0] != 'J' || text[1] < '1' || text[1] > '6' ||
      text[2] != '\0') return false;
  axis = static_cast<std::size_t>(text[1] - '1');
  return true;
}

bool parse_token(const char* text, std::uint32_t& token) {
  if (text == nullptr || std::strlen(text) != 8U) return false;
  char* end = nullptr;
  token = std::strtoul(text, &end, 16);
  return end == text + 8U && *end == '\0';
}

const MotionProfile* parse_profile(const char* text) {
  if (text == nullptr) return nullptr;
  for (const auto& profile : kProfiles) {
    if (std::strcmp(text, profile.name) == 0) return &profile;
  }
  return nullptr;
}

void stop_stepper_now(std::size_t axis) {
  auto& stepper = *steppers[axis];
  const long position = stepper.currentPosition();
  disable_axis(axis);
  stepper.setCurrentPosition(position);
}

void end_motion(const char* result) {
  if (motion.kind == MotionKind::coordinated) {
    const bool complete = std::strcmp(result, "complete") == 0;
    if (complete) {
      sample_sensors();
      coordinated_hold_initial_sensors = coordinated.initial_sensors;
      for (std::size_t axis = 0U; axis < 6U; ++axis) {
        coordinated_hold_initial_sensors[axis] = stop_states[axis].stable;
      }
      coordinated_hold_initial_other_stops = {
          stop_states[6].stable, stop_states[7].stable};
      coordinated_hold_active = true;
    } else {
      disable_all();
      coordinated_hold_active = false;
    }
    if (!p6_binary_session) {
      Serial.print("PAROL6_COORDINATED_DONE result=");
      Serial.print(result);
      Serial.print(" hold=");
      Serial.print(complete ? 1 : 0);
      for (std::size_t axis = 0U; axis < 6U; ++axis) {
        Serial.print(" J");
        Serial.print(axis + 1U);
        Serial.print("_mdeg=");
        Serial.print(joint_position_millidegrees(axis));
      }
      Serial.print(" driver_disabled=");
      Serial.print(complete ? 0 : 1);
      Serial.print(" token=");
      print_token(command_token);
      Serial.print("\r\n");
    }
    motion = MotionTask{};
    coordinated = CoordinatedTask{};
    return;
  }
  const std::size_t axis = motion.axis;
  const MotionKind finished_kind = motion.kind;
  stop_stepper_now(axis);
  if (!p6_binary_session) {
    Serial.print(finished_kind == MotionKind::home ? "PAROL6_HOME" :
                                                     "PAROL6_MOTION_DONE");
    Serial.print(" joint=J");
    Serial.print(axis + 1U);
    Serial.print(" result=");
    Serial.print(result);
    Serial.print(" position_mdeg=");
    Serial.print(joint_position_millidegrees(axis));
    Serial.print(" driver_disabled=1 token=");
    print_token(command_token);
    Serial.print("\r\n");
  }
  motion = MotionTask{};
}

void abort_motion(const char* reason) {
  if (!motion.running) {
    disable_all();
    return;
  }
  end_motion(reason);
}

void error(const char* code) {
  abort_motion("protocol_abort");
  release_motor_hold("protocol_abort");
  release_coordinated_hold("protocol_abort");
  disable_all();
  if (p6_binary_session) return;
  Serial.print("PAROL6_ERROR code=");
  Serial.print(code);
  Serial.print(" driver_disabled=1 token=");
  print_token(command_token);
  Serial.print("\r\n");
}

void prepare_raw_move(std::size_t axis, std::int32_t raw_millidegrees,
                      const MotionProfile& profile,
                      float speed_scale = 1.0F) {
  auto& stepper = *steppers[axis];
  apply_profile(axis, profile, speed_scale);
  enable_axis(axis);
  delay(20);
  stepper.move(millidegrees_to_steps(axis, raw_millidegrees));
}

void prepare_logical_move(std::size_t axis,
                          std::int32_t logical_millidegrees,
                          const MotionProfile& profile,
                          float speed_scale = 1.0F) {
  prepare_raw_move(axis,
                   logical_to_raw_millidegrees(axis, logical_millidegrees),
                   profile, speed_scale);
}

void start_jog(std::size_t axis, bool positive, std::int32_t millidegrees,
               const MotionProfile& profile) {
  sample_sensors();
  motion.running = true;
  motion.kind = MotionKind::jog;
  motion.axis = axis;
  motion.positive = positive;
  motion.requested_millidegrees = millidegrees;
  motion.profile = &profile;
  motion.soft_limit_target = false;
  motion.initial_axis_sensor = stop_states[axis].stable;
  motion.initial_other_stops = {stop_states[6].stable, stop_states[7].stable};
  motion.started_ms = millis();
  const std::int32_t logical_delta = positive ? millidegrees : -millidegrees;
  prepare_logical_move(axis, logical_delta, profile);
  Serial.print("PAROL6_MOTION_STARTED joint=J");
  Serial.print(axis + 1U);
  Serial.print(" direction=");
  Serial.print(positive ? '+' : '-');
  Serial.print(" millidegrees=");
  Serial.print(millidegrees);
  Serial.print(" profile=");
  Serial.print(profile.name);
  Serial.print(" token=");
  print_token(command_token);
  Serial.print("\r\n");
}

void start_limit_test(std::size_t axis, std::int32_t target_millidegrees,
                      const char* target_name) {
  sample_sensors();
  const std::int32_t current = joint_position_millidegrees(axis);
  const std::int32_t delta = target_millidegrees - current;
  motion.running = true;
  motion.kind = MotionKind::jog;
  motion.axis = axis;
  motion.positive = delta >= 0;
  motion.requested_millidegrees = delta >= 0 ? delta : -delta;
  motion.profile = &kProfiles[0];
  motion.soft_limit_target = true;
  motion.initial_axis_sensor = stop_states[axis].stable;
  motion.initial_other_stops = {stop_states[6].stable, stop_states[7].stable};
  motion.started_ms = millis();
  prepare_logical_move(axis, delta, kProfiles[0]);
  Serial.print("PAROL6_LIMIT_TEST_STARTED joint=J");
  Serial.print(axis + 1U);
  Serial.print(" target=");
  Serial.print(target_name);
  Serial.print(" target_mdeg=");
  Serial.print(target_millidegrees);
  Serial.print(" profile=GENTLE token=");
  print_token(command_token);
  Serial.print("\r\n");
}

void start_raw_direction_jog(std::size_t axis, bool raw_positive) {
  sample_sensors();
  motion.running = true;
  motion.kind = MotionKind::jog;
  motion.axis = axis;
  motion.positive = raw_positive;
  motion.requested_millidegrees = kDirectionDiscoveryJogMilliDegrees;
  motion.profile = &kProfiles[0];
  motion.soft_limit_target = false;
  motion.initial_axis_sensor = stop_states[axis].stable;
  motion.initial_other_stops = {stop_states[6].stable, stop_states[7].stable};
  motion.started_ms = millis();
  prepare_raw_move(axis,
                   raw_positive ? kDirectionDiscoveryJogMilliDegrees
                                : -kDirectionDiscoveryJogMilliDegrees,
                   kProfiles[0], 0.5F);
  Serial.print("PAROL6_RAW_JOG_STARTED joint=J");
  Serial.print(axis + 1U);
  Serial.print(" raw_direction=");
  Serial.print(raw_positive ? '+' : '-');
  Serial.print(" millidegrees=");
  Serial.print(kDirectionDiscoveryJogMilliDegrees);
  Serial.print(" profile=GENTLE driver_enabled=1 token=");
  print_token(command_token);
  Serial.print("\r\n");
}

void start_hold(std::size_t axis, bool positive,
                std::int32_t speed_mdeg_per_second,
                const MotionProfile& profile) {
  sample_sensors();
  motion.running = true;
  motion.kind = MotionKind::hold;
  motion.axis = axis;
  motion.positive = positive;
  motion.soft_limit_target = false;
  const std::int32_t current = joint_position_millidegrees(axis);
  std::int32_t target = current +
      (positive ? kMaximumHoldTravelMilliDegrees
                : -kMaximumHoldTravelMilliDegrees);
  const auto& joint = calibration_record.joints[axis];
  if (target > parol6::calibration::kAbsoluteAngleCeilingMilliDegrees) {
    target = parol6::calibration::kAbsoluteAngleCeilingMilliDegrees;
    motion.soft_limit_target = true;
  }
  if (target < -parol6::calibration::kAbsoluteAngleCeilingMilliDegrees) {
    target = -parol6::calibration::kAbsoluteAngleCeilingMilliDegrees;
    motion.soft_limit_target = true;
  }
  if (positive && maximum_is_set(axis) && target > joint.maximum_millidegrees) {
    target = joint.maximum_millidegrees;
    motion.soft_limit_target = true;
  }
  if (!positive && minimum_is_set(axis) && target < joint.minimum_millidegrees) {
    target = joint.minimum_millidegrees;
    motion.soft_limit_target = true;
  }
  const std::int32_t logical_delta = target - current;
  motion.requested_millidegrees =
      logical_delta >= 0 ? logical_delta : -logical_delta;
  motion.profile = &profile;
  motion.initial_axis_sensor = stop_states[axis].stable;
  motion.initial_other_stops = {stop_states[6].stable, stop_states[7].stable};
  motion.started_ms = millis();
  motion.hold_deadline_ms = motion.started_ms + kHoldKeepaliveTimeoutMs;
  auto& stepper = *steppers[axis];
  apply_hold_profile(axis, speed_mdeg_per_second, profile);
  enable_axis(axis);
  delay(20);
  stepper.move(millidegrees_to_steps(
      axis, logical_to_raw_millidegrees(axis, logical_delta)));
  Serial.print("PAROL6_HOLD_STARTED joint=J");
  Serial.print(axis + 1U);
  Serial.print(" direction=");
  Serial.print(positive ? '+' : '-');
  Serial.print(" speed_mdeg_s=");
  Serial.print(speed_mdeg_per_second);
  Serial.print(" profile=");
  Serial.print(profile.name);
  Serial.print(" cap_mdeg=");
  Serial.print(motion.requested_millidegrees);
  Serial.print(" token=");
  print_token(command_token);
  Serial.print("\r\n");
}

bool home_sensor_active(std::size_t axis) {
  return stop_states[axis].stable == home_active_level[axis];
}

bool coordinated_home_sensor_guard_enabled(std::size_t axis) {
  if (axis == 0U) return p6_j1_auto_home;
  if (axis == 5U) return kJ6SensorHomeEnabled;
  return axis <= kJ5Axis;
}

bool home_boundary_is_minimum(std::size_t axis) {
  if (has_automatic_home_boundary(axis)) {
    return automatic_home_boundary_is_minimum(axis);
  }
  return !home_is_logical_positive(axis);
}

bool position_is_near_home_boundary(std::size_t axis,
                                    std::int32_t position_millidegrees) {
  const auto& joint = calibration_record.joints[axis];
  const std::int32_t boundary = home_boundary_is_minimum(axis)
      ? joint.minimum_millidegrees
      : joint.maximum_millidegrees;
  return labs(position_millidegrees - boundary) <=
         kHomeBoundaryGuardMilliDegrees;
}

bool home_sensor_transition_is_safe(std::size_t axis,
                                    bool initial_sensor,
                                    bool moving_positive,
                                    bool axis_is_moving,
                                    std::int32_t position_millidegrees) {
  if (!axis_is_moving || !homed[axis] ||
      !coordinated_home_sensor_guard_enabled(axis)) return false;
  const bool moving_away = moving_positive != home_is_logical_positive(axis);
  if (initial_sensor == home_active_level[axis] &&
      !home_sensor_active(axis) && moving_away) return true;
  if (initial_sensor != home_active_level[axis] &&
      home_sensor_active(axis) && !moving_away &&
      position_is_near_home_boundary(axis, position_millidegrees)) return true;
  return false;
}

bool guarded_axis_sensor_changed(std::size_t axis) {
  if (stop_states[axis].stable == motion.initial_axis_sensor) return false;

  // Permit leaving a referenced home switch, and permit entering it only in
  // the commanded home direction within two degrees of the calibrated
  // boundary. A switch transition anywhere else remains an immediate abort.
  if (home_sensor_transition_is_safe(
          axis, motion.initial_axis_sensor, motion.positive, true,
          joint_position_millidegrees(axis))) {
    motion.initial_axis_sensor = stop_states[axis].stable;
    return false;
  }
  return true;
}

void print_home_phase(std::size_t axis, const char* phase) {
  if (p6_binary_session) return;
  Serial.print("PAROL6_HOME_PHASE joint=J");
  Serial.print(axis + 1U);
  Serial.print(" phase=");
  Serial.print(phase);
  Serial.print(" sensor=");
  Serial.print(stop_states[axis].stable ? 1 : 0);
  Serial.print("\r\n");
}

void start_home_phase(HomePhase phase) {
  const std::size_t axis = motion.axis;
  motion.home_phase = phase;
  switch (phase) {
    case HomePhase::initial_backoff:
      print_home_phase(axis, "INITIAL_BACKOFF");
      prepare_raw_move(axis,
                       home_direction_positive[axis]
                           ? -kHomeInitialReleaseMilliDegrees
                           : kHomeInitialReleaseMilliDegrees,
                       kProfiles[0]);
      break;
    case HomePhase::fast_seek:
      print_home_phase(axis, "FAST_SEEK");
      prepare_raw_move(axis,
                       home_direction_positive[axis]
                           ? kHomeSeekMilliDegrees
                           : -kHomeSeekMilliDegrees,
                       kProfiles[0]);
      break;
    case HomePhase::latch_backoff:
      print_home_phase(axis, "LATCH_BACKOFF");
      prepare_raw_move(axis,
                       home_direction_positive[axis]
                           ? -kHomeBackoffMilliDegrees
                           : kHomeBackoffMilliDegrees,
                       kProfiles[0]);
      break;
    case HomePhase::backoff_margin:
      print_home_phase(axis, "BACKOFF_MARGIN");
      prepare_raw_move(axis,
                       home_direction_positive[axis]
                           ? -kHomeMarginMilliDegrees
                           : kHomeMarginMilliDegrees,
                       kProfiles[0], 0.5F);
      break;
    case HomePhase::slow_seek:
      print_home_phase(axis, "SLOW_SEEK");
      prepare_raw_move(axis,
                       home_direction_positive[axis]
                           ? kHomeLatchMilliDegrees
                           : -kHomeLatchMilliDegrees,
                       kProfiles[0], 0.35F);
      break;
    case HomePhase::final_clear:
      print_home_phase(axis, "FINAL_CLEAR_POSITIVE");
      prepare_logical_move(axis, kHomeInitialReleaseMilliDegrees,
                           kProfiles[0], 0.35F);
      break;
    case HomePhase::post_home_standby: {
      print_home_phase(axis, "POST_HOME_STANDBY_MINUS_130");
      motion.home_sensor_cleared = !home_sensor_active(axis);
      const std::int32_t delta =
          kJ5PostHomeStandbyMilliDegrees - joint_position_millidegrees(axis);
      prepare_logical_move(axis, delta, kProfiles[0]);
      break;
    }
    case HomePhase::none:
      break;
  }
}

void start_home(std::size_t axis) {
  sample_sensors();
  // A new homing attempt invalidates the previous reference until every
  // required phase, including a configured post-home move, completes.
  homed[axis] = false;
  manual_home_temporary[axis] = false;
  motion.running = true;
  motion.kind = MotionKind::home;
  motion.axis = axis;
  motion.profile = &kProfiles[0];
  motion.initial_other_stops = {stop_states[6].stable, stop_states[7].stable};
  motion.started_ms = millis();
  if (!p6_binary_session) {
    Serial.print("PAROL6_HOME_STARTED joint=J");
    Serial.print(axis + 1U);
    Serial.print(" seek_direction=");
    Serial.print(home_direction_positive[axis] ? '+' : '-');
    Serial.print(" active_level=");
    Serial.print(home_active_level[axis] ? 1 : 0);
    Serial.print(" max_seek_mdeg=");
    Serial.print(kHomeSeekMilliDegrees);
    Serial.print(" token=");
    print_token(command_token);
    Serial.print("\r\n");
  }
  start_home_phase(home_sensor_active(axis) ? HomePhase::initial_backoff
                                            : HomePhase::fast_seek);
}

bool persist_calibration();

bool apply_automatic_home_boundary(std::size_t axis) {
  if (!has_automatic_home_boundary(axis)) return true;
  auto& joint = calibration_record.joints[axis];
  const bool minimum = automatic_home_boundary_is_minimum(axis);
  const bool changed = minimum
      ? (!minimum_is_set(axis) || joint.minimum_millidegrees != 0)
      : (!maximum_is_set(axis) || joint.maximum_millidegrees != 0);
  if (minimum) {
    joint.minimum_millidegrees = 0;
    set_joint_flag(axis, parol6::calibration::kMinimumSet, true);
  } else {
    joint.maximum_millidegrees = 0;
    set_joint_flag(axis, parol6::calibration::kMaximumSet, true);
  }
  if (changed) {
    if (!persist_calibration()) return false;
    rotate_token();
  }
  if (!p6_binary_session) {
    Serial.print("PAROL6_HOME_LIMIT_SAVED joint=J");
    Serial.print(axis + 1U);
    Serial.print(" kind=");
    Serial.print(minimum ? "MIN" : "MAX");
    Serial.print(" position_mdeg=0 automatic=1 changed=");
    Serial.print(changed ? 1 : 0);
    Serial.print(" sequence=");
    Serial.print(calibration_record.sequence);
    Serial.print(" token=");
    print_token(command_token);
    Serial.print("\r\n");
    print_joint_calibration(axis);
  }
  return true;
}

void service_jog() {
  const std::size_t axis = motion.axis;
  if (guarded_axis_sensor_changed(axis) ||
      stop_states[6].stable != motion.initial_other_stops[0] ||
      stop_states[7].stable != motion.initial_other_stops[1]) {
    end_motion("limit_abort");
    return;
  }
  auto& stepper = *steppers[axis];
  stepper.run();
  if (stepper.distanceToGo() == 0) end_motion("complete");
}

void service_hold() {
  const std::size_t axis = motion.axis;
  if (guarded_axis_sensor_changed(axis) ||
      stop_states[6].stable != motion.initial_other_stops[0] ||
      stop_states[7].stable != motion.initial_other_stops[1]) {
    end_motion("limit_abort");
    return;
  }
  if (static_cast<std::int32_t>(millis() - motion.hold_deadline_ms) >= 0) {
    end_motion("hold_keepalive_timeout");
    return;
  }
  auto& stepper = *steppers[axis];
  stepper.run();
  if (stepper.distanceToGo() == 0) {
    end_motion(motion.soft_limit_target ? "soft_limit_reached"
                                        : "hold_travel_cap");
  }
}

void service_home() {
  const std::size_t axis = motion.axis;
  if (stop_states[6].stable != motion.initial_other_stops[0] ||
      stop_states[7].stable != motion.initial_other_stops[1]) {
    end_motion("aux_stop_abort");
    return;
  }
  auto& stepper = *steppers[axis];
  const bool active = home_sensor_active(axis);
  switch (motion.home_phase) {
    case HomePhase::initial_backoff:
      if (!active) {
        stop_stepper_now(axis);
        start_home_phase(HomePhase::fast_seek);
        return;
      }
      break;
    case HomePhase::fast_seek:
      if (active) {
        stop_stepper_now(axis);
        start_home_phase(HomePhase::latch_backoff);
        return;
      }
      break;
    case HomePhase::latch_backoff:
      if (!active) {
        stop_stepper_now(axis);
        start_home_phase(HomePhase::backoff_margin);
        return;
      }
      break;
    case HomePhase::backoff_margin:
      break;
    case HomePhase::slow_seek:
      if (active) {
        stop_stepper_now(axis);
        if (axis == 3U) {
          start_home_phase(HomePhase::final_clear);
          return;
        }
        stepper.setCurrentPosition(0L);
        manual_home_temporary[axis] = false;
        if (!apply_automatic_home_boundary(axis)) {
          homed[axis] = false;
          return;
        }
        if (axis == kJ5Axis) {
          if (!logical_target_is_safe(axis, kJ5PostHomeStandbyMilliDegrees)) {
            homed[axis] = false;
            end_motion("j5_standby_outside_soft_limits");
            return;
          }
          start_home_phase(HomePhase::post_home_standby);
          return;
        }
        homed[axis] = true;
        end_motion("complete");
        return;
      }
      break;
    case HomePhase::final_clear:
      if (!active) {
        stop_stepper_now(axis);
        stepper.setCurrentPosition(0L);
        homed[axis] = true;
        manual_home_temporary[axis] = false;
        if (!apply_automatic_home_boundary(axis)) {
          homed[axis] = false;
          return;
        }
        end_motion("complete");
        return;
      }
      break;
    case HomePhase::post_home_standby:
      if (!active) motion.home_sensor_cleared = true;
      else if (motion.home_sensor_cleared) {
        homed[axis] = false;
        end_motion("j5_sensor_retriggered_during_standby");
        return;
      } else if (joint_position_millidegrees(axis) <=
                 -kHomeInitialReleaseMilliDegrees) {
        homed[axis] = false;
        end_motion("j5_sensor_stuck_active_30deg");
        return;
      }
      break;
    case HomePhase::none:
      end_motion("home_state_fault");
      return;
  }
  stepper.run();
  if (stepper.distanceToGo() != 0) return;
  switch (motion.home_phase) {
    case HomePhase::initial_backoff:
      end_motion("sensor_stuck_active_30deg");
      break;
    case HomePhase::fast_seek:
      end_motion("sensor_not_found_90deg");
      break;
    case HomePhase::latch_backoff:
      end_motion("sensor_failed_to_clear");
      break;
    case HomePhase::backoff_margin:
      start_home_phase(HomePhase::slow_seek);
      break;
    case HomePhase::slow_seek:
      end_motion("latch_not_found_3deg");
      break;
    case HomePhase::final_clear:
      end_motion("j4_sensor_stuck_active_30deg");
      break;
    case HomePhase::post_home_standby:
      if (!motion.home_sensor_cleared) {
        homed[axis] = false;
        end_motion("j5_sensor_failed_to_clear");
        break;
      }
      homed[axis] = true;
      end_motion("complete");
      break;
    case HomePhase::none:
      end_motion("home_state_fault");
      break;
  }
}

void service_motion() {
  sample_sensors();
  if (coordinated_hold_active) {
    for (std::size_t axis = 0U; axis < 6U; ++axis) {
      if (stop_states[axis].stable != coordinated_hold_initial_sensors[axis]) {
        release_coordinated_hold("limit_abort");
        return;
      }
    }
    if (stop_states[6].stable != coordinated_hold_initial_other_stops[0] ||
        stop_states[7].stable != coordinated_hold_initial_other_stops[1]) {
      release_coordinated_hold("aux_stop_abort");
      return;
    }
    if (millis() - last_host_contact_ms > kMotorHoldTimeoutMs) {
      release_coordinated_hold("host_timeout");
    }
    return;
  }
  if (motor_hold_active) {
    if (stop_states[motor_hold_axis].stable != motor_hold_initial_axis_sensor ||
        stop_states[6].stable != motor_hold_initial_other_stops[0] ||
        stop_states[7].stable != motor_hold_initial_other_stops[1]) {
      release_motor_hold("limit_abort");
      return;
    }
    if (millis() - last_host_contact_ms > kMotorHoldTimeoutMs) {
      release_motor_hold("host_timeout");
    }
    return;
  }
  if (!motion.running) return;
  if (millis() - last_host_contact_ms > kHostMotionTimeoutMs) {
    end_motion("host_timeout");
    return;
  }
  if (motion.kind == MotionKind::jog) service_jog();
  else if (motion.kind == MotionKind::hold) service_hold();
  else if (motion.kind == MotionKind::home) service_home();
  else if (motion.kind == MotionKind::coordinated) service_coordinated();
}

bool persist_calibration() {
  if (!calibration_store.save(calibration_record)) {
    error("calibration_flash_write_failed");
    return false;
  }
  load_calibration_runtime();
  return true;
}

bool configure_joint_calibration(std::size_t axis, bool home_raw_positive,
                                 bool positive_raw_positive,
                                 bool sensor_active_high) {
  auto& joint = calibration_record.joints[axis];
  const std::uint8_t direction_mask =
      parol6::calibration::kConfigured |
      parol6::calibration::kHomeRawPositive |
      parol6::calibration::kPositiveRawPositive |
      parol6::calibration::kSensorActiveHigh;
  std::uint8_t requested = parol6::calibration::kConfigured;
  if (home_raw_positive) requested |= parol6::calibration::kHomeRawPositive;
  if (positive_raw_positive) {
    requested |= parol6::calibration::kPositiveRawPositive;
  }
  if (sensor_active_high) requested |= parol6::calibration::kSensorActiveHigh;
  const bool changed = (joint.flags & direction_mask) != requested;
  joint.flags = static_cast<std::uint8_t>(
      (joint.flags & ~direction_mask) | requested);
  if (changed && axis != 0U) {
    joint.flags &= static_cast<std::uint8_t>(
        ~(parol6::calibration::kMinimumSet |
          parol6::calibration::kMaximumSet));
    joint.minimum_millidegrees = 0;
    joint.maximum_millidegrees = 0;
  }
  if (axis == 0U) apply_j1_hardcoded_limits();
  else if (axis == 5U) apply_j6_hardcoded_limits();
  if (!persist_calibration()) return false;
  homed[axis] = false;
  manual_home_temporary[axis] = false;
  steppers[axis]->setCurrentPosition(0L);
  rotate_token();
  Serial.print("PAROL6_CALIBRATION_SAVED joint=J");
  Serial.print(axis + 1U);
  Serial.print(" home_raw=");
  Serial.print(home_raw_positive ? '+' : '-');
  Serial.print(" positive_raw=");
  Serial.print(positive_raw_positive ? '+' : '-');
  Serial.print(" negative_raw=");
  Serial.print(positive_raw_positive ? '-' : '+');
  Serial.print(" active=");
  Serial.print(sensor_active_high ? "HIGH" : "LOW");
  Serial.print(" limits_reset=");
  Serial.print(changed ? 1 : 0);
  Serial.print(" sequence=");
  Serial.print(calibration_record.sequence);
  Serial.print(" token=");
  print_token(command_token);
  Serial.print("\r\n");
  print_joint_calibration(axis);
  return true;
}

bool capture_joint_limit(std::size_t axis, bool minimum) {
  if (axis == 0U) {
    error("j1_limits_hardcoded");
    return false;
  }
  if (axis == 5U) {
    error("j6_limits_hardcoded");
    return false;
  }
  if (!homed[axis]) {
    error("axis_not_homed");
    return false;
  }
  if (has_automatic_home_boundary(axis) &&
      minimum == automatic_home_boundary_is_minimum(axis)) {
    error("home_boundary_is_automatic");
    return false;
  }
  const std::int32_t position = joint_position_millidegrees(axis);
  auto& joint = calibration_record.joints[axis];
  if ((minimum && position > 0) || (!minimum && position < 0)) {
    error(minimum ? "minimum_must_include_home" : "maximum_must_include_home");
    return false;
  }
  if (minimum && maximum_is_set(axis) &&
      position >= joint.maximum_millidegrees) {
    error("minimum_not_below_maximum");
    return false;
  }
  if (!minimum && minimum_is_set(axis) &&
      position <= joint.minimum_millidegrees) {
    error("maximum_not_above_minimum");
    return false;
  }
  if (minimum) {
    joint.minimum_millidegrees = position;
    set_joint_flag(axis, parol6::calibration::kMinimumSet, true);
  } else {
    joint.maximum_millidegrees = position;
    set_joint_flag(axis, parol6::calibration::kMaximumSet, true);
  }
  if (!persist_calibration()) return false;
  rotate_token();
  Serial.print("PAROL6_LIMIT_SAVED joint=J");
  Serial.print(axis + 1U);
  Serial.print(" kind=");
  Serial.print(minimum ? "MIN" : "MAX");
  Serial.print(" position_mdeg=");
  Serial.print(position);
  Serial.print(" sequence=");
  Serial.print(calibration_record.sequence);
  Serial.print(" token=");
  print_token(command_token);
  Serial.print("\r\n");
  print_joint_calibration(axis);
  return true;
}

bool reset_joint_calibration(std::size_t axis) {
  auto replacement = parol6::calibration::JointRecord{};
  replacement.pulses_per_degree = kPulsesPerDegree[axis];
  calibration_record.joints[axis] = replacement;
  if (axis == 0U) apply_j1_hardcoded_limits();
  else if (axis == 5U) apply_j6_hardcoded_limits();
  if (!persist_calibration()) return false;
  homed[axis] = false;
  manual_home_temporary[axis] = false;
  steppers[axis]->setCurrentPosition(0L);
  rotate_token();
  Serial.print("PAROL6_CALIBRATION_RESET joint=J");
  Serial.print(axis + 1U);
  Serial.print(" sequence=");
  Serial.print(calibration_record.sequence);
  Serial.print(" token=");
  print_token(command_token);
  Serial.print("\r\n");
  print_joint_calibration(axis);
  return true;
}

bool coordinated_sensor_change_is_safe(std::size_t axis) {
  if (stop_states[axis].stable == coordinated.initial_sensors[axis]) return true;
  const std::int32_t current = joint_position_millidegrees(axis);
  const std::int32_t delta = coordinated.targets_millidegrees[axis] - current;
  if (home_sensor_transition_is_safe(
          axis, coordinated.initial_sensors[axis], delta > 0, delta != 0,
          current)) {
    coordinated.initial_sensors[axis] = stop_states[axis].stable;
    return true;
  }
  return false;
}

void start_coordinated_move(
    const std::array<std::int32_t, 6>& targets_millidegrees,
    std::uint32_t duration_ms) {
  sample_sensors();
  coordinated = CoordinatedTask{};
  coordinated.targets_millidegrees = targets_millidegrees;
  coordinated.duration_ms = duration_ms;
  for (std::size_t axis = 0U; axis < 6U; ++axis) {
    coordinated.initial_sensors[axis] = stop_states[axis].stable;
  }
  coordinated.initial_other_stops = {
      stop_states[6].stable, stop_states[7].stable};

  const float duration_seconds = static_cast<float>(duration_ms) / 1000.0F;
  const float acceleration_seconds = duration_seconds / 4.0F;
  const float cruise_denominator = duration_seconds - acceleration_seconds;
  for (std::size_t axis = 0U; axis < 6U; ++axis) {
    auto& stepper = *steppers[axis];
    const long target_steps = millidegrees_to_steps(
        axis, logical_to_raw_millidegrees(axis, targets_millidegrees[axis]));
    const long distance_steps = labs(target_steps - stepper.currentPosition());
    coordinated.moving[axis] = distance_steps != 0L;
    if (!coordinated.moving[axis]) continue;
    const float maximum_step_rate =
        static_cast<float>(distance_steps) / cruise_denominator;
    const float step_acceleration = maximum_step_rate / acceleration_seconds;
    stepper.setMaxSpeed(maximum_step_rate);
    stepper.setAcceleration(step_acceleration);
    stepper.setMinPulseWidth(axis < 2U ? kServoPulseWidthUs : kTmcPulseWidthUs);
    enable_axis(axis);
    stepper.moveTo(target_steps);
  }
  delay(20);
  motion = MotionTask{};
  motion.running = true;
  motion.kind = MotionKind::coordinated;
  motion.started_ms = millis();
  Serial.print("PAROL6_COORDINATED_STARTED duration_ms=");
  Serial.print(duration_ms);
  Serial.print(" speed_cap_percent=10 token=");
  print_token(command_token);
  Serial.print("\r\n");
}

void service_coordinated() {
  if (stop_states[6].stable != coordinated.initial_other_stops[0] ||
      stop_states[7].stable != coordinated.initial_other_stops[1]) {
    end_motion("aux_stop_abort");
    return;
  }
  bool complete = true;
  for (std::size_t axis = 0U; axis < 6U; ++axis) {
    if (!coordinated.moving[axis]) continue;
    if (!coordinated_sensor_change_is_safe(axis)) {
      end_motion("limit_abort");
      return;
    }
    auto& stepper = *steppers[axis];
    stepper.run();
    if (stepper.distanceToGo() != 0L) complete = false;
  }
  if (complete) end_motion("complete");
}

std::uint32_t p6_next_output_sequence() {
  const std::uint32_t result = p6_output_sequence++;
  if (p6_output_sequence == 0U) p6_output_sequence = 1U;
  return result;
}

void p6_send_error(std::uint32_t rejected_sequence,
                   parol6::p6b1::ErrorCode code,
                   std::uint16_t detail = 0U) {
  std::array<std::uint8_t, 8> payload{};
  parol6::p6b1::write_u32(payload.data(), rejected_sequence);
  parol6::p6b1::write_u16(payload.data() + 4U,
                          static_cast<std::uint16_t>(code));
  parol6::p6b1::write_u16(payload.data() + 6U, detail);
  parol6::p6b1::send_packet(Serial, parol6::p6b1::MessageType::error,
                            p6_next_output_sequence(), payload.data(),
                            payload.size());
}

void p6_send_ack(std::uint32_t acknowledged_sequence) {
  std::array<std::uint8_t, 12> payload{};
  parol6::p6b1::write_u32(payload.data(), acknowledged_sequence);
  parol6::p6b1::write_u16(
      payload.data() + 4U, static_cast<std::uint16_t>(p6_queue.size()));
  parol6::p6b1::write_u16(
      payload.data() + 6U, static_cast<std::uint16_t>(p6_queue.capacity()));
  parol6::p6b1::write_u32(payload.data() + 8U, p6_state);
  parol6::p6b1::send_packet(Serial, parol6::p6b1::MessageType::ack,
                            p6_next_output_sequence(), payload.data(),
                            payload.size());
}

bool p6_all_homed() {
  for (const bool axis_homed : homed) {
    if (!axis_homed) return false;
  }
  return true;
}

std::int32_t p6_raw_steps(std::size_t axis, std::int32_t logical_steps) {
  return positive_direction_raw_positive[axis] ? logical_steps : -logical_steps;
}

std::int32_t p6_logical_steps(std::size_t axis, std::int32_t raw_steps) {
  return positive_direction_raw_positive[axis] ? raw_steps : -raw_steps;
}

std::int32_t p6_steps_to_millidegrees(std::size_t axis,
                                      std::int32_t logical_steps) {
  return static_cast<std::int32_t>(
      (static_cast<std::int64_t>(logical_steps) * 1000LL) /
      static_cast<std::int64_t>(kPulsesPerDegree[axis]));
}

void p6_latch_fault(std::uint32_t fault_code) {
  p6_faults |= fault_code;
  p6_queue.clear();
  p6_motion_running = false;
  p6_finish_requested = false;
  p6_home_sequence_active = false;
  abort_motion("p6b1_fault");
  release_motor_hold("p6b1_fault");
  release_coordinated_hold("p6b1_fault");
  disable_all();
  p6_state = parol6::p6b1::fault;
}

std::uint16_t p6_sensor_bits() {
  std::uint16_t bits = 0U;
  for (std::size_t index = 0U; index < 8U; ++index) {
    if (stop_states[index].stable) bits |= 1U << (7U - index);
  }
  return bits;
}

void p6_send_status() {
  std::array<std::uint8_t, 68> payload{};
  parol6::p6b1::write_u32(payload.data(), p6_last_host_sequence);
  parol6::p6b1::write_u16(
      payload.data() + 4U, static_cast<std::uint16_t>(p6_queue.size()));
  parol6::p6b1::write_u16(
      payload.data() + 6U, static_cast<std::uint16_t>(p6_queue.capacity()));
  std::uint32_t state = p6_state;
  if (p6_all_homed()) state |= parol6::p6b1::homed;
  parol6::p6b1::write_u32(payload.data() + 8U, state);
  parol6::p6b1::write_u32(payload.data() + 12U, p6_faults);
  for (std::size_t axis = 0U; axis < 6U; ++axis) {
    const auto logical_position = p6_logical_steps(
        axis, static_cast<std::int32_t>(steppers[axis]->currentPosition()));
    parol6::p6b1::write_i32(payload.data() + 16U + axis * 4U,
                            logical_position);
    parol6::p6b1::write_i32(
        payload.data() + 40U + axis * 4U,
        static_cast<std::int32_t>(fabsf(steppers[axis]->speed())));
  }
  parol6::p6b1::write_u16(payload.data() + 64U, p6_sensor_bits());
  parol6::p6b1::write_u16(payload.data() + 66U, 0U);
  parol6::p6b1::send_packet(Serial, parol6::p6b1::MessageType::status,
                            p6_next_output_sequence(), payload.data(),
                            payload.size());
}

void p6_capture_sensor_guards() {
  sample_sensors();
  for (std::size_t axis = 0U; axis < 6U; ++axis) {
    p6_initial_sensors[axis] = stop_states[axis].stable;
  }
  p6_initial_other_stops = {stop_states[6].stable, stop_states[7].stable};
}

bool p6_sensor_guards_safe(const parol6::p6b1::Setpoint& point) {
  sample_sensors();
  if (stop_states[6].stable != p6_initial_other_stops[0] ||
      stop_states[7].stable != p6_initial_other_stops[1]) return false;
  for (std::size_t axis = 0U; axis < 6U; ++axis) {
    if (stop_states[axis].stable == p6_initial_sensors[axis]) continue;
    const auto current = p6_logical_steps(
        axis, static_cast<std::int32_t>(steppers[axis]->currentPosition()));
    const auto delta = point.positions_steps[axis] - current;
    if (home_sensor_transition_is_safe(
            axis, p6_initial_sensors[axis], delta > 0, delta != 0,
            p6_steps_to_millidegrees(axis, current))) {
      p6_initial_sensors[axis] = stop_states[axis].stable;
      continue;
    }
    return false;
  }
  return true;
}

bool p6_validate_setpoint(const parol6::p6b1::Setpoint& point) {
  if (!p6_all_homed()) return false;
  for (std::size_t axis = 0U; axis < 6U; ++axis) {
    const auto target_mdeg =
        p6_steps_to_millidegrees(axis, point.positions_steps[axis]);
    if (!logical_target_is_safe(axis, target_mdeg)) return false;
    const auto rate_limit = static_cast<std::uint32_t>(
        kCoordinatedMaximumDegreesPerSecond[axis] *
        static_cast<float>(kPulsesPerDegree[axis]) + 1.0F);
    if (point.speeds_steps_s[axis] > rate_limit) return false;
  }
  return true;
}

bool p6_apply_setpoint(const parol6::p6b1::Setpoint& point) {
  if (!p6_validate_setpoint(point) || !p6_sensor_guards_safe(point)) {
    p6_latch_fault(parol6::p6b1::limit);
    return false;
  }
  for (std::size_t axis = 0U; axis < 6U; ++axis) {
    auto& stepper = *steppers[axis];
    const auto target = p6_raw_steps(axis, point.positions_steps[axis]);
    const auto delta = labs(target - stepper.currentPosition());
    if (delta == 0L) continue;
    const float hard_rate = axis < 2U
        ? kServoMaximumPulseRate[axis]
        : kCoordinatedMaximumDegreesPerSecond[axis] *
              static_cast<float>(kPulsesPerDegree[axis]);
    const float requested = static_cast<float>(point.speeds_steps_s[axis]);
    stepper.setMaxSpeed(fminf(hard_rate, fmaxf(1.0F, requested)));
    stepper.setAcceleration(
        kCoordinatedMaximumAccelerationDegreesPerSecond2[axis] *
        static_cast<float>(kPulsesPerDegree[axis]));
    stepper.setMinPulseWidth(axis < 2U ? kServoPulseWidthUs : kTmcPulseWidthUs);
    enable_axis(axis);
    stepper.moveTo(target);
  }
  return true;
}

void p6_start_next_home_axis() {
  while (p6_home_index < p6_home_order.size()) {
    const std::size_t axis = p6_home_order[p6_home_index];
    if (axis == 0U && !p6_j1_auto_home) {
      steppers[axis]->setCurrentPosition(0L);
      homed[axis] = true;
      manual_home_temporary[axis] = true;
      ++p6_home_index;
      continue;
    }
    if (axis == 5U && !kJ6SensorHomeEnabled) {
      // Temporary no-end-effector configuration: retain J6's calibrated
      // direction and fixed +/-180 degree containment, but define the current
      // startup position as zero without moving to its sensor.
      steppers[axis]->setCurrentPosition(0L);
      homed[axis] = true;
      manual_home_temporary[axis] = true;
      ++p6_home_index;
      continue;
    }
    if (!home_configured[axis] || !watchdog_ready || !preflight_axis(axis)) {
      p6_latch_fault(parol6::p6b1::limit);
      return;
    }
    start_home(axis);
    p6_state = parol6::p6b1::running | parol6::p6b1::motors_enabled;
    return;
  }
  p6_home_sequence_active = false;
  p6_state = parol6::p6b1::idle | parol6::p6b1::homed;
}

void p6_service_home_sequence() {
  if (!p6_home_sequence_active) return;
  const bool was_running = motion.running;
  service_motion();
  if (!was_running || motion.running) return;
  const std::size_t completed_axis = p6_home_order[p6_home_index];
  if (!homed[completed_axis]) {
    p6_latch_fault(parol6::p6b1::limit);
    return;
  }
  ++p6_home_index;
  p6_start_next_home_axis();
}

void p6_service_motion() {
  if (p6_home_sequence_active) {
    p6_service_home_sequence();
    return;
  }
  if (!p6_motion_running) return;
  const std::uint32_t now_us = micros();
  if (static_cast<std::int32_t>(now_us - p6_next_setpoint_us) >= 0) {
    parol6::p6b1::Setpoint point{};
    if (!p6_queue.pop(point)) {
      if (!p6_finish_requested) {
        if (p6_queue_empty_since_ms == 0U) p6_queue_empty_since_ms = millis();
        if (millis() - p6_queue_empty_since_ms > 30U) {
          p6_latch_fault(parol6::p6b1::queue_underrun);
          return;
        }
      }
      p6_next_setpoint_us = now_us + p6_period_us;
    } else {
      p6_queue_empty_since_ms = 0U;
      if (!p6_apply_setpoint(point)) return;
      p6_next_setpoint_us += p6_period_us;
    }
  }
  for (auto* stepper : steppers) stepper->run();
  if (p6_finish_requested && p6_queue.size() == 0U) {
    bool settled = true;
    for (auto* stepper : steppers) {
      if (stepper->distanceToGo() != 0L) settled = false;
    }
    if (settled) {
      p6_motion_running = false;
      p6_finish_requested = false;
      p6_queue_empty_since_ms = 0U;
      p6_state = parol6::p6b1::idle | parol6::p6b1::homed |
                 parol6::p6b1::motors_enabled;
    }
  }
}

void p6_handle_packet(const parol6::p6b1::PacketView& packet) {
  p6_last_contact_ms = millis();
  last_host_contact_ms = p6_last_contact_ms;

  if (packet.type == parol6::p6b1::MessageType::stop) {
    p6_last_host_sequence = packet.sequence;
    p6_queue.clear();
    p6_motion_running = false;
    p6_finish_requested = false;
    p6_home_sequence_active = false;
    abort_motion("p6b1_priority_stop");
    release_motor_hold("p6b1_priority_stop");
    release_coordinated_hold("p6b1_priority_stop");
    disable_all();
    p6_state = parol6::p6b1::stopped;
    p6_send_ack(packet.sequence);
    return;
  }

  if (packet.type == parol6::p6b1::MessageType::hello) {
    if (packet.payload_size != 12U) {
      p6_send_error(packet.sequence,
                    parol6::p6b1::ErrorCode::invalid_payload);
      return;
    }
    const std::uint32_t capabilities =
        parol6::p6b1::read_u32(packet.payload);
    const std::uint32_t profile_crc =
        parol6::p6b1::read_u32(packet.payload + 8U);
    const std::uint32_t missing =
        parol6::p6b1::kRequiredCapabilities & ~capabilities;
    if (missing != 0U || profile_crc != kP6b1ProfileCrc32c) {
      p6_faults |= parol6::p6b1::capability_mismatch;
      p6_state = parol6::p6b1::fault;
      p6_send_error(packet.sequence,
                    parol6::p6b1::ErrorCode::capability_mismatch,
                    static_cast<std::uint16_t>(missing));
      return;
    }
    p6_handshake_complete = true;
    p6_binary_session = true;
    p6_last_host_sequence = packet.sequence;
    std::array<std::uint8_t, 12> response{};
    parol6::p6b1::write_u32(response.data(),
                            parol6::p6b1::kRequiredCapabilities);
    parol6::p6b1::write_u16(
        response.data() + 4U, static_cast<std::uint16_t>(p6_queue.capacity()));
    parol6::p6b1::write_u16(response.data() + 6U, kP6b1WatchdogMs);
    parol6::p6b1::write_u32(response.data() + 8U, kP6b1ProfileCrc32c);
    parol6::p6b1::send_packet(Serial,
                              parol6::p6b1::MessageType::hello_ack,
                              p6_next_output_sequence(), response.data(),
                              response.size());
    return;
  }

  if (!p6_handshake_complete) {
    p6_send_error(packet.sequence,
                  parol6::p6b1::ErrorCode::capability_mismatch);
    return;
  }
  if (packet.sequence <= p6_last_host_sequence) {
    p6_faults |= parol6::p6b1::replay;
    p6_send_error(packet.sequence, parol6::p6b1::ErrorCode::replay);
    return;
  }
  p6_last_host_sequence = packet.sequence;

  if (packet.type == parol6::p6b1::MessageType::clear) {
    p6_queue.clear();
    p6_faults = parol6::p6b1::no_fault;
    p6_motion_running = false;
    p6_finish_requested = false;
    p6_home_sequence_active = false;
    abort_motion("p6b1_session_clear");
    release_motor_hold("p6b1_session_clear");
    release_coordinated_hold("p6b1_session_clear");
    disable_all();
    configure_owner_servo_interfaces();
    p6_state = parol6::p6b1::idle;
    p6_send_ack(packet.sequence);
    return;
  }
  if (p6_faults != parol6::p6b1::no_fault) {
    p6_send_error(packet.sequence,
                  parol6::p6b1::ErrorCode::fault_latched,
                  static_cast<std::uint16_t>(p6_faults & 0xFFFFU));
    return;
  }

  if (packet.type == parol6::p6b1::MessageType::set_j1_home_mode) {
    if (packet.payload_size != 1U || packet.payload[0] > 1U) {
      p6_send_error(packet.sequence,
                    parol6::p6b1::ErrorCode::invalid_payload);
      return;
    }
    p6_j1_auto_home = packet.payload[0] == 1U;
    p6_send_ack(packet.sequence);
    return;
  }
  if (packet.type == parol6::p6b1::MessageType::io) {
    p6_send_ack(packet.sequence);
    return;
  }
  if (packet.type == parol6::p6b1::MessageType::home) {
    if (p6_motion_running || motion.running) {
      p6_send_error(packet.sequence,
                    parol6::p6b1::ErrorCode::not_armed);
      return;
    }
    p6_queue.clear();
    p6_finish_requested = false;
    p6_home_sequence_active = true;
    p6_home_index = 0U;
    for (auto& axis_homed : homed) axis_homed = false;
    p6_start_next_home_axis();
    p6_send_ack(packet.sequence);
    return;
  }
  if (packet.type == parol6::p6b1::MessageType::enqueue) {
    if (packet.payload_size < 8U) {
      p6_send_error(packet.sequence,
                    parol6::p6b1::ErrorCode::invalid_payload);
      return;
    }
    const std::uint16_t period_us =
        parol6::p6b1::read_u16(packet.payload + 4U);
    const std::uint16_t count =
        parol6::p6b1::read_u16(packet.payload + 6U);
    if (count == 0U || period_us < 1000U || period_us > 50000U ||
        packet.payload_size != 8U + count * parol6::p6b1::kSetpointSize) {
      p6_send_error(packet.sequence,
                    parol6::p6b1::ErrorCode::invalid_payload);
      return;
    }
    if (p6_queue.size() + count > p6_queue.capacity()) {
      p6_latch_fault(parol6::p6b1::queue_overflow);
      p6_send_error(packet.sequence,
                    parol6::p6b1::ErrorCode::queue_overflow);
      return;
    }
    std::array<parol6::p6b1::Setpoint, 78> decoded{};
    if (count > decoded.size()) {
      p6_send_error(packet.sequence,
                    parol6::p6b1::ErrorCode::invalid_payload);
      return;
    }
    for (std::size_t index = 0U; index < count; ++index) {
      if (!parol6::p6b1::decode_setpoint(
              packet.payload + 8U + index * parol6::p6b1::kSetpointSize,
              parol6::p6b1::kSetpointSize, decoded[index]) ||
          !p6_validate_setpoint(decoded[index])) {
        p6_latch_fault(parol6::p6b1::limit);
        p6_send_error(packet.sequence,
                      parol6::p6b1::ErrorCode::invalid_payload);
        return;
      }
    }
    for (std::size_t index = 0U; index < count; ++index) {
      p6_queue.push(decoded[index]);
    }
    p6_queue_empty_since_ms = 0U;
    p6_period_us = period_us;
    p6_state = parol6::p6b1::armed | parol6::p6b1::motors_enabled;
    p6_send_ack(packet.sequence);
    return;
  }
  if (packet.type == parol6::p6b1::MessageType::start) {
    if (p6_queue.size() == 0U || !p6_all_homed()) {
      p6_send_error(packet.sequence, parol6::p6b1::ErrorCode::not_armed);
      return;
    }
    p6_capture_sensor_guards();
    p6_motion_running = true;
    p6_finish_requested = false;
    p6_queue_empty_since_ms = 0U;
    p6_next_setpoint_us = micros();
    p6_state = parol6::p6b1::running | parol6::p6b1::motors_enabled |
               parol6::p6b1::homed;
    p6_send_ack(packet.sequence);
    return;
  }
  if (packet.type == parol6::p6b1::MessageType::finish) {
    if (!p6_motion_running) {
      p6_send_error(packet.sequence, parol6::p6b1::ErrorCode::not_armed);
      return;
    }
    p6_finish_requested = true;
    p6_send_ack(packet.sequence);
    return;
  }
  p6_send_error(packet.sequence,
                parol6::p6b1::ErrorCode::invalid_payload);
}

void p6_service_protocol() {
  while (Serial.available() > 0) {
    const int value = Serial.read();
    if (value < 0) continue;
    parol6::p6b1::PacketView packet{};
    if (p6_parser.push(static_cast<std::uint8_t>(value), packet)) {
      p6_handle_packet(packet);
    }
  }
  if ((p6_motion_running || p6_home_sequence_active) &&
      millis() - p6_last_contact_ms > kP6b1WatchdogMs) {
    p6_latch_fault(parol6::p6b1::watchdog);
  }
  if (millis() - p6_last_status_ms >= kP6b1StatusPeriodMs) {
    p6_last_status_ms = millis();
    p6_send_status();
  }
}

bool parse_signed_millidegrees(const char* text, std::int32_t& value) {
  if (text == nullptr || *text == '\0') return false;
  char* end = nullptr;
  const long parsed = std::strtol(text, &end, 10);
  if (end == text || *end != '\0' ||
      parsed < -parol6::calibration::kAbsoluteAngleCeilingMilliDegrees ||
      parsed > parol6::calibration::kAbsoluteAngleCeilingMilliDegrees) {
    return false;
  }
  value = static_cast<std::int32_t>(parsed);
  return true;
}

void handle_line(char* command) {
  last_host_contact_ms = millis();
  if (std::strcmp(command, "IDENTIFY") == 0) {
    print_ready();
    return;
  }
  if (std::strcmp(command, "SENSORS") == 0) {
    print_sensors();
    return;
  }
  if (std::strcmp(command, "STATUS") == 0) {
    print_status();
    return;
  }
  if (std::strcmp(command, "CALIBRATION") == 0) {
    print_calibration();
    return;
  }
  if (std::strcmp(command, "PING") == 0) {
    Serial.print("PAROL6_PONG moving=");
    Serial.print(motion.running ? 1 : 0);
    Serial.print("\r\n");
    return;
  }
  if (std::strcmp(command, "STOP") == 0 || std::strcmp(command, "ABORT") == 0) {
    abort_motion("operator_stop");
    release_motor_hold("operator_stop");
    release_coordinated_hold("operator_stop");
    disable_all();
    Serial.print("PAROL6_STOPPED driver_disabled=1 token=");
    print_token(command_token);
    Serial.print("\r\n");
    return;
  }
  static constexpr char kCoordinatedHoldReleasePrefix[] =
      "COORD_HOLD_RELEASE ";
  if (std::strncmp(command, kCoordinatedHoldReleasePrefix,
                   sizeof(kCoordinatedHoldReleasePrefix) - 1U) == 0) {
    char copy[kLineCapacity + 1U]{};
    std::strncpy(copy,
                 command + sizeof(kCoordinatedHoldReleasePrefix) - 1U,
                 kLineCapacity);
    char* context = nullptr;
    const char* token_text = ::strtok_r(copy, " ", &context);
    const char* confirmation = ::strtok_r(nullptr, " ", &context);
    std::uint32_t supplied_token = 0U;
    if (!coordinated_hold_active ||
        !parse_token(token_text, supplied_token) ||
        supplied_token != command_token || confirmation == nullptr ||
        std::strcmp(confirmation,
                    "RELEASE_COORDINATED_HOLD_VERIFIED") != 0) {
      error("coordinated_hold_release_rejected");
      return;
    }
    release_coordinated_hold("operator_release");
    return;
  }
  static constexpr char kCoordinatedMovePrefix[] = "COORD_MOVE ";
  if (std::strncmp(command, kCoordinatedMovePrefix,
                   sizeof(kCoordinatedMovePrefix) - 1U) == 0) {
    if (motion.running || motor_hold_active) {
      error("motion_busy");
      return;
    }
    char copy[kLineCapacity + 1U]{};
    std::strncpy(copy, command + sizeof(kCoordinatedMovePrefix) - 1U,
                 kLineCapacity);
    char* context = nullptr;
    const char* token_text = ::strtok_r(copy, " ", &context);
    const char* duration_text = ::strtok_r(nullptr, " ", &context);
    std::uint32_t supplied_token = 0U;
    char* duration_end = nullptr;
    const unsigned long duration = duration_text == nullptr
        ? 0UL
        : std::strtoul(duration_text, &duration_end, 10);
    if (!parse_token(token_text, supplied_token) ||
        supplied_token != command_token || duration_text == nullptr ||
        duration_end == duration_text || *duration_end != '\0' ||
        duration < kMinimumCoordinatedDurationMs ||
        duration > kMaximumCoordinatedDurationMs) {
      error("bad_coordinated_envelope");
      return;
    }
    std::array<std::int32_t, 6> targets{};
    bool any_motion = false;
    for (std::size_t axis = 0U; axis < 6U; ++axis) {
      const char* target_text = ::strtok_r(nullptr, " ", &context);
      if (!parse_signed_millidegrees(target_text, targets[axis]) ||
          !home_configured[axis] || !homed[axis] ||
          !logical_target_is_safe(axis, targets[axis])) {
        error("coordinated_target_rejected");
        return;
      }
      any_motion = any_motion ||
          targets[axis] != joint_position_millidegrees(axis);
    }
    const char* confirmation = ::strtok_r(nullptr, " ", &context);
    if (!any_motion || confirmation == nullptr ||
        std::strcmp(confirmation, "COORDINATED_MOVE_VERIFIED") != 0 ||
        ::strtok_r(nullptr, " ", &context) != nullptr) {
      error("coordinated_confirmation_rejected");
      return;
    }
    const float duration_seconds = static_cast<float>(duration) / 1000.0F;
    const float acceleration_seconds = duration_seconds / 4.0F;
    const float cruise_denominator = duration_seconds - acceleration_seconds;
    for (std::size_t axis = 0U; axis < 6U; ++axis) {
      const float distance_degrees =
          static_cast<float>(labs(targets[axis] -
                                  joint_position_millidegrees(axis))) /
          1000.0F;
      if (distance_degrees == 0.0F) continue;
      const float maximum_speed = distance_degrees / cruise_denominator;
      const float acceleration = maximum_speed / acceleration_seconds;
      if (maximum_speed > kCoordinatedMaximumDegreesPerSecond[axis] ||
          acceleration >
              kCoordinatedMaximumAccelerationDegreesPerSecond2[axis]) {
        error("coordinated_rate_exceeds_owner_cap");
        return;
      }
      if (!watchdog_ready || !preflight_axis(axis)) {
        error(axis < 2U ? "servo_interface_unverified"
                        : "driver_preflight_failed");
        return;
      }
    }
    if (coordinated_hold_active) {
      sample_sensors();
      for (std::size_t axis = 0U; axis < 6U; ++axis) {
        if (stop_states[axis].stable !=
            coordinated_hold_initial_sensors[axis]) {
          error("coordinated_hold_limit_changed");
          return;
        }
      }
      coordinated_hold_active = false;
    }
    rotate_token();
    start_coordinated_move(targets, static_cast<std::uint32_t>(duration));
    return;
  }
  static constexpr char kHoldKeepalivePrefix[] = "HOLD_KEEPALIVE ";
  if (std::strncmp(command, kHoldKeepalivePrefix,
                   sizeof(kHoldKeepalivePrefix) - 1U) == 0) {
    std::uint32_t supplied_token = 0U;
    const char* token_text = command + sizeof(kHoldKeepalivePrefix) - 1U;
    if (!motion.running || motion.kind != MotionKind::hold ||
        !parse_token(token_text, supplied_token) || supplied_token != command_token) {
      error("hold_keepalive_rejected");
      return;
    }
    motion.hold_deadline_ms = millis() + kHoldKeepaliveTimeoutMs;
    Serial.print("PAROL6_HOLD_ALIVE joint=J");
    Serial.print(motion.axis + 1U);
    Serial.print("\r\n");
    return;
  }
  static constexpr char kHoldReleasePrefix[] = "HOLD_RELEASE ";
  if (std::strncmp(command, kHoldReleasePrefix,
                   sizeof(kHoldReleasePrefix) - 1U) == 0) {
    std::uint32_t supplied_token = 0U;
    char* context = nullptr;
    const char* token_text = command + sizeof(kHoldReleasePrefix) - 1U;
    char token_copy[kLineCapacity + 1U]{};
    std::strncpy(token_copy, token_text, kLineCapacity);
    const char* parsed_token = ::strtok_r(token_copy, " ", &context);
    const char* confirmation = ::strtok_r(nullptr, " ", &context);
    if (!motion.running || motion.kind != MotionKind::hold ||
        !parse_token(parsed_token, supplied_token) ||
        supplied_token != command_token || confirmation == nullptr ||
        std::strcmp(confirmation, "HOLD_POSITION_VERIFIED") != 0) {
      error("hold_release_rejected");
      return;
    }
    finish_hold_jog_release();
    return;
  }
  if (motion.running) {
    error("motion_busy");
    return;
  }
  if (coordinated_hold_active) {
    error("coordinated_hold_active");
    return;
  }

  char* context = nullptr;
  const char* verb = ::strtok_r(command, " ", &context);
  const char* axis_text = ::strtok_r(nullptr, " ", &context);
  std::size_t axis = 0U;
  if (!parse_axis(axis_text, axis)) {
    error("bad_axis");
    return;
  }

  const bool motion_handoff_request =
      motor_hold_active &&
      (std::strcmp(verb, "JOG") == 0 || std::strcmp(verb, "HOLD") == 0 ||
       std::strcmp(verb, "HOME") == 0 ||
       std::strcmp(verb, "LIMIT_TEST") == 0);
  const bool held_limit_capture =
      motor_hold_active && std::strcmp(verb, "CAL_LIMIT") == 0 &&
      axis == motor_hold_axis;
  if (motor_hold_active && !motion_handoff_request && !held_limit_capture &&
      std::strcmp(verb, "MOTOR_HOLD") != 0) {
    error("motor_hold_active");
    return;
  }
  if (motion_handoff_request && axis != motor_hold_axis) {
    error("motor_hold_active");
    return;
  }

  if (std::strcmp(verb, "MOTOR_HOLD") == 0) {
    const char* token_text = ::strtok_r(nullptr, " ", &context);
    const char* state = ::strtok_r(nullptr, " ", &context);
    const char* confirmation = ::strtok_r(nullptr, " ", &context);
    std::uint32_t supplied_token = 0U;
    if (!parse_token(token_text, supplied_token) || supplied_token != command_token ||
        state == nullptr || confirmation == nullptr) {
      error("motor_hold_rejected");
      return;
    }
    if (std::strcmp(state, "ON") == 0) {
      if (motion.running || motor_hold_active ||
          std::strcmp(confirmation, "HOLD_TORQUE_VERIFIED") != 0) {
        error("motor_hold_busy_or_unconfirmed");
        return;
      }
      if (axis < 2U) {
        error("servo_hold_disabled");
        return;
      }
      if (axis < 2U && !servo_gate_open[axis]) {
        error("servo_interface_unverified");
        return;
      }
      if (!homed[axis]) {
        error("axis_not_homed");
        return;
      }
      if (axis >= 2U && !preflight_axis(axis)) {
        error("driver_preflight_failed");
        return;
      }
      engage_motor_hold(axis, "explicit");
      return;
    }
    if (std::strcmp(state, "OFF") == 0 &&
        std::strcmp(confirmation, "HOLD_RELEASE_VERIFIED") == 0 &&
        motor_hold_active && motor_hold_axis == axis) {
      release_motor_hold("operator_release");
      return;
    }
    error("motor_hold_state_rejected");
    return;
  }
  if (std::strcmp(verb, "CHECK") == 0) {
    if (axis < 2U) {
      Serial.print("PAROL6_SERVO_GATE joint=J");
      Serial.print(axis + 1U);
      Serial.print(" ready=");
      Serial.print(servo_gate_open[axis] ? 1 : 0);
      Serial.print(" reason=");
      Serial.print(servo_gate_open[axis] ? "configured_push_pull_3v3" :
                                           "boot_gate_closed");
      Serial.print(" driver_disabled=1\r\n");
      return;
    }
    std::uint8_t connection = 0U;
    std::uint8_t version = 0U;
    std::uint32_t status = 0U;
    const bool ready = configure_tmc(axis, connection, version, status);
    print_tmc_result(axis, ready, connection, version, status);
    return;
  }

  const char* token_text = ::strtok_r(nullptr, " ", &context);
  std::uint32_t supplied_token = 0U;
  if (!parse_token(token_text, supplied_token) || supplied_token != command_token) {
    error("bad_token");
    return;
  }
  if (std::strcmp(verb, "SERVO_CONFIG") == 0) {
    const char* polarity = ::strtok_r(nullptr, " ", &context);
    const char* confirmation = ::strtok_r(nullptr, " ", &context);
    if (axis > 1U || polarity == nullptr || confirmation == nullptr ||
        std::strcmp(confirmation, "INTERFACE_VERIFIED") != 0 ||
        (std::strcmp(polarity, "ACTIVE_LOW") != 0 &&
         std::strcmp(polarity, "ACTIVE_HIGH") != 0)) {
      error("servo_gate_rejected");
      return;
    }
    configure_servo_interface(axis, std::strcmp(polarity, "ACTIVE_LOW") == 0);
    rotate_token();
    Serial.print("PAROL6_SERVO_CONFIGURED joint=J");
    Serial.print(axis + 1U);
    Serial.print(" enable=");
    Serial.print(polarity);
    Serial.print(" signal=PUSH_PULL_3V3 driver_disabled=1 token=");
    print_token(command_token);
    Serial.print("\r\n");
    return;
  }
  if (std::strcmp(verb, "MANUAL_HOME") == 0) {
    const char* confirmation = ::strtok_r(nullptr, " ", &context);
    if (axis != 0U) {
      error("manual_home_j1_only");
      return;
    }
    if (!home_configured[axis] || confirmation == nullptr ||
        std::strcmp(confirmation,
                    "SET_CURRENT_POSITION_ZERO_TEMPORARY") != 0) {
      error("manual_home_rejected");
      return;
    }
    disable_all();
    apply_j1_hardcoded_limits();
    if (!persist_calibration()) return;
    steppers[axis]->setCurrentPosition(0L);
    homed[axis] = true;
    manual_home_temporary[axis] = true;
    rotate_token();
    Serial.print("PAROL6_MANUAL_HOME joint=J1 result=complete "
                 "position_mdeg=0 limits_fixed=-230000:35000 temporary=1 "
                 "driver_disabled=1 sequence=");
    Serial.print(calibration_record.sequence);
    Serial.print(" token=");
    print_token(command_token);
    Serial.print("\r\n");
    print_joint_calibration(axis);
    return;
  }
  if (std::strcmp(verb, "CAL_CONFIG") == 0) {
    const char* home_direction = ::strtok_r(nullptr, " ", &context);
    const char* positive_direction = ::strtok_r(nullptr, " ", &context);
    const char* active = ::strtok_r(nullptr, " ", &context);
    const char* confirmation = ::strtok_r(nullptr, " ", &context);
    if (home_direction == nullptr || positive_direction == nullptr ||
        active == nullptr || confirmation == nullptr ||
        (std::strcmp(home_direction, "HOME_RAW_POS") != 0 &&
         std::strcmp(home_direction, "HOME_RAW_NEG") != 0) ||
        (std::strcmp(positive_direction, "POSITIVE_RAW_POS") != 0 &&
         std::strcmp(positive_direction, "POSITIVE_RAW_NEG") != 0) ||
        (std::strcmp(active, "ACTIVE_HIGH") != 0 &&
         std::strcmp(active, "ACTIVE_LOW") != 0) ||
        std::strcmp(confirmation, "SAVE_CALIBRATION_VERIFIED") != 0) {
      error("calibration_config_rejected");
      return;
    }
    configure_joint_calibration(
        axis, std::strcmp(home_direction, "HOME_RAW_POS") == 0,
        std::strcmp(positive_direction, "POSITIVE_RAW_POS") == 0,
        std::strcmp(active, "ACTIVE_HIGH") == 0);
    return;
  }
  if (std::strcmp(verb, "CAL_LIMIT") == 0) {
    const char* kind = ::strtok_r(nullptr, " ", &context);
    const char* confirmation = ::strtok_r(nullptr, " ", &context);
    if (kind == nullptr || confirmation == nullptr ||
        (std::strcmp(kind, "MIN") != 0 && std::strcmp(kind, "MAX") != 0) ||
        std::strcmp(confirmation, "CAPTURE_LIMIT_VERIFIED") != 0) {
      error("calibration_limit_rejected");
      return;
    }
    capture_joint_limit(axis, std::strcmp(kind, "MIN") == 0);
    return;
  }
  if (std::strcmp(verb, "CAL_RESET") == 0) {
    const char* confirmation = ::strtok_r(nullptr, " ", &context);
    if (confirmation == nullptr ||
        std::strcmp(confirmation, "RESET_JOINT_CALIBRATION_VERIFIED") != 0) {
      error("calibration_reset_rejected");
      return;
    }
    reset_joint_calibration(axis);
    return;
  }
  if (std::strcmp(verb, "RAW_JOG") == 0) {
    const char* direction = ::strtok_r(nullptr, " ", &context);
    const char* confirmation = ::strtok_r(nullptr, " ", &context);
    if (direction == nullptr || confirmation == nullptr ||
        (std::strcmp(direction, "+") != 0 &&
         std::strcmp(direction, "-") != 0) ||
        std::strcmp(confirmation, "DIRECTION_DISCOVERY_VERIFIED") != 0) {
      error("raw_jog_rejected");
      return;
    }
    if (homed[axis]) {
      error("raw_jog_requires_unhomed_axis");
      return;
    }
    if (!watchdog_ready || !preflight_axis(axis)) {
      error(axis < 2U ? "servo_interface_unverified"
                      : "driver_preflight_failed");
      return;
    }
    rotate_token();
    start_raw_direction_jog(axis, std::strcmp(direction, "+") == 0);
    return;
  }
  if (std::strcmp(verb, "LIMIT_TEST") == 0) {
    const char* destination = ::strtok_r(nullptr, " ", &context);
    const char* confirmation = ::strtok_r(nullptr, " ", &context);
    if (destination == nullptr || confirmation == nullptr ||
        (std::strcmp(destination, "MAX") != 0 &&
         std::strcmp(destination, "MAX_MINUS_10") != 0) ||
        std::strcmp(confirmation, "LIMIT_TEST_VERIFIED") != 0) {
      error("limit_test_rejected");
      return;
    }
    if (!home_configured[axis] || !homed[axis]) {
      error("limit_test_requires_homed_axis");
      return;
    }
    if (!maximum_is_set(axis)) {
      error("maximum_limit_not_set");
      return;
    }
    const auto& joint = calibration_record.joints[axis];
    const std::int32_t target =
        std::strcmp(destination, "MAX") == 0
            ? joint.maximum_millidegrees
            : joint.maximum_millidegrees - kLimitTestInsetMilliDegrees;
    if (!logical_target_is_safe(axis, target)) {
      error("limit_test_target_outside_limits");
      return;
    }
    if (!watchdog_ready || !preflight_axis(axis)) {
      error(axis < 2U ? "servo_interface_unverified"
                      : "driver_preflight_failed");
      return;
    }
    if (!handoff_motor_hold_to_motion(axis)) return;
    rotate_token();
    start_limit_test(axis, target, destination);
    return;
  }
  if (std::strcmp(verb, "JOG") == 0) {
    const char* direction = ::strtok_r(nullptr, " ", &context);
    const char* amount_text = ::strtok_r(nullptr, " ", &context);
    const char* profile_text = ::strtok_r(nullptr, " ", &context);
    const std::int32_t amount = amount_text == nullptr ? 0 : std::atoi(amount_text);
    const MotionProfile* profile = parse_profile(profile_text);
    if (direction == nullptr ||
        (std::strcmp(direction, "+") != 0 && std::strcmp(direction, "-") != 0) ||
        amount < 250 || amount > kMaximumJogMilliDegrees || profile == nullptr) {
      error("bad_jog_envelope");
      return;
    }
    if (!home_configured[axis]) {
      error("calibration_not_configured");
      return;
    }
    if (!homed[axis] &&
        (amount > 1000 || std::strcmp(profile->name, "GENTLE") != 0)) {
      error("unhomed_jog_requires_gentle_1deg");
      return;
    }
    const bool logical_positive = std::strcmp(direction, "+") == 0;
    const std::int32_t current = joint_position_millidegrees(axis);
    const std::int32_t target = current + (logical_positive ? amount : -amount);
    if (homed[axis] && !logical_target_is_safe(axis, target)) {
      error(logical_positive ? "maximum_soft_limit" : "minimum_soft_limit");
      return;
    }
    if (!watchdog_ready) {
      error("watchdog_not_ready");
      return;
    }
    if (!preflight_axis(axis)) {
      error(axis < 2U ? "servo_interface_unverified" : "driver_preflight_failed");
      return;
    }
    if (!handoff_motor_hold_to_motion(axis)) return;
    rotate_token();
    start_jog(axis, logical_positive, amount, *profile);
    return;
  }
  if (std::strcmp(verb, "HOLD") == 0) {
    const char* direction = ::strtok_r(nullptr, " ", &context);
    const char* speed_text = ::strtok_r(nullptr, " ", &context);
    const char* profile_text = ::strtok_r(nullptr, " ", &context);
    const std::int32_t speed = speed_text == nullptr ? 0 : std::atoi(speed_text);
    const MotionProfile* profile = parse_profile(profile_text);
    if (direction == nullptr ||
        (std::strcmp(direction, "+") != 0 && std::strcmp(direction, "-") != 0) ||
        speed < kMinimumHoldSpeedMilliDegreesPerSecond ||
        speed > kMaximumHoldSpeedMilliDegreesPerSecond || profile == nullptr) {
      error("bad_hold_envelope");
      return;
    }
    if (!home_configured[axis]) {
      error("calibration_not_configured");
      return;
    }
    if (!homed[axis]) {
      error("axis_not_homed");
      return;
    }
    const bool logical_positive = std::strcmp(direction, "+") == 0;
    const std::int32_t current = joint_position_millidegrees(axis);
    const auto& joint = calibration_record.joints[axis];
    if ((logical_positive &&
         ((maximum_is_set(axis) && current >= joint.maximum_millidegrees) ||
          current >= parol6::calibration::kAbsoluteAngleCeilingMilliDegrees)) ||
        (!logical_positive &&
         ((minimum_is_set(axis) && current <= joint.minimum_millidegrees) ||
          current <= -parol6::calibration::kAbsoluteAngleCeilingMilliDegrees))) {
      error(logical_positive ? "maximum_soft_limit" : "minimum_soft_limit");
      return;
    }
    if (!watchdog_ready || !preflight_axis(axis)) {
      error(axis < 2U ? "servo_interface_unverified" : "driver_preflight_failed");
      return;
    }
    if (!handoff_motor_hold_to_motion(axis)) return;
    rotate_token();
    start_hold(axis, logical_positive, speed, *profile);
    return;
  }
  if (std::strcmp(verb, "HOME") == 0) {
    const char* confirmation = ::strtok_r(nullptr, " ", &context);
    if (!home_configured[axis] || confirmation == nullptr ||
        std::strcmp(confirmation, "START") != 0) {
      error("home_not_configured");
      return;
    }
    if (!watchdog_ready || !preflight_axis(axis)) {
      error("home_preflight_failed");
      return;
    }
    // A TMC joint can remain energized after hold-to-jog release. Without
    // transferring that stationary hold here, service_motion() continues to
    // service the hold and never advances the newly-created home task.
    if (!handoff_motor_hold_to_motion(axis)) return;
    rotate_token();
    start_home(axis);
    return;
  }
  error("bad_command");
}

}  // namespace

void setup() {
  for (std::size_t axis = 2U; axis < 6U; ++axis) {
    digitalWrite(kEnablePins[axis], HIGH);
    digitalWrite(kStepPins[axis], LOW);
    digitalWrite(kDirectionPins[axis], LOW);
    pinMode(kEnablePins[axis], OUTPUT);
    pinMode(kStepPins[axis], OUTPUT);
    pinMode(kDirectionPins[axis], OUTPUT);
  }
  for (std::size_t axis = 0U; axis < 2U; ++axis) {
    pinMode(kEnablePins[axis], INPUT);
    pinMode(kStepPins[axis], INPUT);
    pinMode(kDirectionPins[axis], INPUT);
    steppers[axis]->setPinsInverted(false, true, false);
  }
  for (const auto pin : kHomePins) pinMode(pin, INPUT_PULLUP);
  for (const auto pin : kOtherStopPins) pinMode(pin, INPUT_PULLUP);
  analogReadResolution(12);
  Serial.begin(kBaud);
  watchdog_ready = initialize_watchdog();
  calibration_store.begin(feed_watchdog);
  load_calibration_runtime();
  command_token = make_token();
  last_host_contact_ms = millis();
  p6_last_contact_ms = millis();
  p6_last_status_ms = millis();
  for (std::uint8_t sample = 0U; sample < kDebounceSamples; ++sample) {
    sample_sensors();
    delay(1);
  }
  disable_all();
}

void loop() {
  feed_watchdog();
  sample_sensors();
  p6_service_protocol();
  p6_service_motion();
}
