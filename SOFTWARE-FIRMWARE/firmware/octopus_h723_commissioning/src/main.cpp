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
constexpr std::uint32_t kMotorHoldTimeoutMs = 2000U;
constexpr std::uint32_t kHoldKeepaliveTimeoutMs = 400U;
constexpr std::int32_t kMaximumJogMilliDegrees = 10000;
constexpr std::int32_t kDirectionDiscoveryJogMilliDegrees = 2000;
constexpr std::int32_t kMaximumHoldTravelMilliDegrees = 45000;
constexpr std::int32_t kMinimumHoldSpeedMilliDegreesPerSecond = 3000;
constexpr std::int32_t kMaximumHoldSpeedMilliDegreesPerSecond = 45000;
constexpr std::int32_t kHomeSeekMilliDegrees = 30000;
constexpr std::int32_t kHomeInitialReleaseMilliDegrees = 30000;
constexpr std::int32_t kHomeBackoffMilliDegrees = 5000;
constexpr std::int32_t kHomeMarginMilliDegrees = 500;
constexpr std::int32_t kHomeLatchMilliDegrees = 3000;
constexpr std::int32_t kJ1HardMinimumMilliDegrees = -230000;
constexpr std::int32_t kJ1HardMaximumMilliDegrees = 35000;

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

enum class MotionKind : std::uint8_t { none, jog, hold, home };
enum class HomePhase : std::uint8_t {
  none,
  initial_backoff,
  fast_seek,
  latch_backoff,
  backoff_margin,
  slow_seek,
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
bool motor_hold_active = false;
std::size_t motor_hold_axis = 0U;
bool motor_hold_initial_axis_sensor = true;
std::array<bool, 2> motor_hold_initial_other_stops{};
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

void load_calibration_runtime() {
  calibration_record = calibration_store.record();
  apply_j1_hardcoded_limits();
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
  return axis == 1U || axis == 2U;  // J2 and J3 rest at their home switches.
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

void sample_sensors();
void end_motion(const char* result);
void error(const char* code);

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
               "j2_lift_accel_max_pulses_s2=900 j2_servo_ma=local_1600_initial "
               "hold_speed_mdeg_s=3000-45000 hold_cap_mdeg=45000 "
               "motor_hold=host_supervised servo_hold=disabled "
               "home_sequence=J2,J3,J4,J6,J5 "
               "mechanical_home_map=J2:PG10,J3:PG12,J5:PG9 "
               "home_limits=J2:J3:auto_zero_boundary "
               "home_initial_release_max_mdeg=30000 "
               "j1_home=sensor_or_manual_temporary "
               "j1_limits_mdeg=-230000:35000 "
               "calibration=dual_slot_crc32c soft_limits=firmware_enforced "
               "direction_discovery=raw_2deg "
               "manual_zero=j1_runtime_only "
               "driver_disabled=");
  Serial.print(motion.running ? 0 : 1);
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
  Serial.print(motor_hold_active
                   ? "HOLD_TORQUE"
                   : motion.kind == MotionKind::jog
                   ? "JOG"
                   : motion.kind == MotionKind::hold
                         ? "HOLD"
                         : motion.kind == MotionKind::home ? "HOME" : "IDLE");
  Serial.print(" active=");
  if (motion.running || motor_hold_active) {
    Serial.print('J');
    Serial.print((motor_hold_active ? motor_hold_axis : motion.axis) + 1U);
  } else Serial.print("NONE");
  Serial.print(" held=");
  if (motor_hold_active) {
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
  Serial.print((motion.running || motor_hold_active) ? 0 : 1);
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
  Serial.print(axis == 0U ? "HARDCODED" : "CAPTURED");
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
  const std::size_t axis = motion.axis;
  const MotionKind finished_kind = motion.kind;
  stop_stepper_now(axis);
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
  disable_all();
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

bool guarded_axis_sensor_changed(std::size_t axis) {
  if (stop_states[axis].stable == motion.initial_axis_sensor) return false;

  // J2/J3 normally begin at the active home switch. Once referenced, permit
  // exactly the active-to-clear transition while jogging away from home. Any
  // transition toward home, or any later re-trigger, remains a motion abort.
  const bool moving_away = motion.positive != home_is_logical_positive(axis);
  if (has_automatic_home_boundary(axis) && homed[axis] && moving_away &&
      motion.initial_axis_sensor == home_active_level[axis] &&
      !home_sensor_active(axis)) {
    motion.initial_axis_sensor = stop_states[axis].stable;
    return false;
  }
  return true;
}

void print_home_phase(std::size_t axis, const char* phase) {
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
    case HomePhase::none:
      break;
  }
}

void start_home(std::size_t axis) {
  sample_sensors();
  motion.running = true;
  motion.kind = MotionKind::home;
  motion.axis = axis;
  motion.profile = &kProfiles[0];
  motion.initial_other_stops = {stop_states[6].stable, stop_states[7].stable};
  motion.started_ms = millis();
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
  start_home_phase(home_sensor_active(axis) ? HomePhase::initial_backoff
                                            : HomePhase::fast_seek);
}

bool persist_calibration();

bool apply_automatic_home_boundary(std::size_t axis) {
  if (!has_automatic_home_boundary(axis)) return true;
  auto& joint = calibration_record.joints[axis];
  const bool minimum = !home_is_logical_positive(axis);
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
      end_motion("sensor_not_found_30deg");
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
    case HomePhase::none:
      end_motion("home_state_fault");
      break;
  }
}

void service_motion() {
  sample_sensors();
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
  if (!homed[axis]) {
    error("axis_not_homed");
    return false;
  }
  if (has_automatic_home_boundary(axis) &&
      minimum == !home_is_logical_positive(axis)) {
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
    disable_all();
    Serial.print("PAROL6_STOPPED driver_disabled=1 token=");
    print_token(command_token);
    Serial.print("\r\n");
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
      (std::strcmp(verb, "JOG") == 0 || std::strcmp(verb, "HOLD") == 0);
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
    servo_enable_active_low[axis] = std::strcmp(polarity, "ACTIVE_LOW") == 0;
    servo_gate_open[axis] = true;
    pinMode(kStepPins[axis], OUTPUT);
    pinMode(kDirectionPins[axis], OUTPUT);
    digitalWrite(kStepPins[axis], HIGH);
    digitalWrite(kDirectionPins[axis], LOW);
    disable_axis(axis);
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
  for (std::uint8_t sample = 0U; sample < kDebounceSamples; ++sample) {
    sample_sensors();
    delay(1);
  }
  disable_all();
}

void loop() {
  feed_watchdog();
  sample_sensors();
  service_motion();
  while (Serial.available() > 0) {
    const int value = Serial.read();
    if (value < 0) continue;
    const char character = static_cast<char>(value);
    if (character == '\r') continue;
    if (character == '\n') {
      line[line_length] = '\0';
      handle_line(line.data());
      line_length = 0U;
      continue;
    }
    if (line_length >= kLineCapacity) {
      line_length = 0U;
      error("line_too_long");
      continue;
    }
    line[line_length++] = character;
  }
}
