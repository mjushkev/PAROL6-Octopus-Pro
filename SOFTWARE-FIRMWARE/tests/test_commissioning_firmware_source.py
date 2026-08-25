from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "firmware" / "octopus_h723_commissioning"


class MotionRcFirmwareSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = (PROJECT / "src" / "main.cpp").read_text(encoding="utf-8")
        cls.storage = (PROJECT / "src" / "calibration_store.cpp").read_text(
            encoding="utf-8"
        )
        cls.storage_header = (
            PROJECT / "include" / "calibration_store.hpp"
        ).read_text(encoding="utf-8")
        cls.ini = (PROJECT / "platformio.ini").read_text(encoding="utf-8")

    def test_project_is_bootloader_bounded_and_versioned(self) -> None:
        self.assertIn("board_build.flash_offset = 0x20000", self.ini)
        self.assertIn("board_upload.maximum_size = 262144", self.ini)
        self.assertIn("0.8.11-calibration-rc", self.ini)

    def test_all_owner_selected_joint_and_sensor_pins_are_present(self) -> None:
        for pin in (
            "PF13", "PF12", "PF14", "PG0", "PG1", "PF15",
            "PF11", "PG3", "PG5", "PC6", "PG4", "PC1", "PA2", "PC7",
            "PF9", "PF10", "PG2", "PF2", "PC13", "PF0", "PF1", "PE4",
            "PG6", "PG9", "PG10", "PG11", "PG12", "PG13", "PG14", "PG15",
        ):
            self.assertIn(pin, self.source)

    def test_accelerated_jog_is_bounded_and_token_bound(self) -> None:
        self.assertIn("AccelStepper::DRIVER", self.source)
        self.assertIn("setAcceleration", self.source)
        self.assertIn("kMaximumJogMilliDegrees = 10000", self.source)
        self.assertIn("amount < 250", self.source)
        self.assertIn("supplied_token != command_token", self.source)
        for profile in ("GENTLE", "NORMAL", "BRISK"):
            self.assertIn(profile, self.source)

    def test_motion_remains_host_supervised_and_single_axis(self) -> None:
        self.assertIn("kHostMotionTimeoutMs = 2000U", self.source)
        self.assertIn("host_timeout", self.source)
        self.assertIn("motion_busy", self.source)
        self.assertIn("operator_stop", self.source)
        self.assertIn("disable_axis(axis);", self.source)
        self.assertIn("limit_abort", self.source)

    def test_raw_direction_discovery_is_small_gentle_and_unhomed_only(self) -> None:
        self.assertIn("kDirectionDiscoveryJogMilliDegrees = 2000", self.source)
        self.assertIn('std::strcmp(verb, "RAW_JOG")', self.source)
        self.assertIn("DIRECTION_DISCOVERY_VERIFIED", self.source)
        self.assertIn("PAROL6_RAW_JOG_STARTED", self.source)
        self.assertIn("raw_jog_requires_unhomed_axis", self.source)
        self.assertIn("prepare_raw_move", self.source)

    def test_hold_to_jog_is_speed_bounded_and_release_supervised(self) -> None:
        self.assertIn("kMinimumHoldSpeedMilliDegreesPerSecond = 3000", self.source)
        self.assertIn("kMaximumHoldSpeedMilliDegreesPerSecond = 45000", self.source)
        self.assertIn("kMaximumHoldTravelMilliDegrees = 45000", self.source)
        self.assertIn("kHoldKeepaliveTimeoutMs = 400U", self.source)
        self.assertIn("HOLD_KEEPALIVE", self.source)
        self.assertIn("HOLD_RELEASE", self.source)
        self.assertIn("HOLD_POSITION_VERIFIED", self.source)
        self.assertIn("hold_keepalive_timeout", self.source)
        self.assertIn("hold_travel_cap", self.source)

    def test_stationary_motor_hold_is_token_bound_and_supervised(self) -> None:
        self.assertIn("kMotorHoldTimeoutMs = 2000U", self.source)
        self.assertIn('std::strcmp(verb, "MOTOR_HOLD")', self.source)
        self.assertIn("HOLD_TORQUE_VERIFIED", self.source)
        self.assertIn("HOLD_RELEASE_VERIFIED", self.source)
        self.assertIn("PAROL6_MOTOR_HOLD_RELEASED", self.source)
        self.assertIn("handoff_motor_hold_to_motion", self.source)
        self.assertIn("result=handoff driver_disabled=0", self.source)
        self.assertIn("motor_hold_active", self.source)
        self.assertIn('std::strcmp(verb, "MOTOR_HOLD") != 0', self.source)
        self.assertIn('std::strcmp(verb, "JOG") == 0', self.source)
        self.assertIn("motor_hold_busy_or_unconfirmed", self.source)
        self.assertIn('engage_motor_hold(axis, "hold_release")', self.source)

    def test_homing_is_guarded_for_all_six_joints(self) -> None:
        self.assertNotIn("j1_home_hard_locked", self.source)
        self.assertIn("SAVE_CALIBRATION_VERIFIED", self.source)
        self.assertIn('std::strcmp(verb, "CAL_CONFIG")', self.source)
        self.assertIn("kHomeSeekMilliDegrees = 30000", self.source)
        self.assertIn("sensor_not_found_30deg", self.source)
        self.assertIn("INITIAL_BACKOFF", self.source)
        self.assertIn("SLOW_SEEK", self.source)

    def test_home_hands_stationary_hold_to_motion_state_machine(self) -> None:
        home_handler = self.source[self.source.index('if (std::strcmp(verb, "HOME") == 0)'):]
        handoff = home_handler.index("handoff_motor_hold_to_motion(axis)")
        start = home_handler.index("start_home(axis)")
        self.assertLess(handoff, start)

    def test_j1_temporary_manual_zero_is_non_motion_and_runtime_only(self) -> None:
        self.assertIn('std::strcmp(verb, "MANUAL_HOME")', self.source)
        self.assertIn("SET_CURRENT_POSITION_ZERO_TEMPORARY", self.source)
        self.assertIn("manual_home_j1_only", self.source)
        self.assertIn("PAROL6_MANUAL_HOME", self.source)
        self.assertIn("manual_home_temporary", self.source)
        self.assertIn("manual_zero=j1_runtime_only", self.source)
        self.assertIn("limits_fixed=-230000:35000 temporary=1", self.source)

    def test_joint_directions_and_soft_limits_are_firmware_enforced(self) -> None:
        self.assertIn("positive_direction_raw_positive", self.source)
        self.assertIn("logical_to_raw_millidegrees", self.source)
        self.assertIn("logical_target_is_safe", self.source)
        self.assertIn("maximum_soft_limit", self.source)
        self.assertIn("minimum_soft_limit", self.source)
        self.assertIn('std::strcmp(verb, "CAL_LIMIT")', self.source)
        self.assertIn('std::strcmp(verb, "CAL_RESET")', self.source)
        self.assertIn("axis_not_homed", self.source)

    def test_j6_limits_are_hardcoded_to_plus_minus_180_degrees(self) -> None:
        self.assertIn("kJ6HardMinimumMilliDegrees = -180000", self.source)
        self.assertIn("kJ6HardMaximumMilliDegrees = 180000", self.source)
        self.assertIn("apply_j6_hardcoded_limits", self.source)
        self.assertIn("j6_limits_hardcoded", self.source)
        self.assertIn('"j6_limits_mdeg=-180000:180000 "', self.source)

    def test_limit_test_targets_are_homed_bounded_and_stoppable(self) -> None:
        self.assertIn("kLimitTestInsetMilliDegrees = 10000", self.source)
        self.assertIn('std::strcmp(verb, "LIMIT_TEST")', self.source)
        self.assertIn("LIMIT_TEST_VERIFIED", self.source)
        self.assertIn("limit_test_requires_homed_axis", self.source)
        self.assertIn("logical_target_is_safe(axis, target)", self.source)
        self.assertIn("PAROL6_LIMIT_TEST_STARTED", self.source)
        self.assertIn('std::strcmp(command, "STOP")', self.source)

    def test_home_and_limit_test_can_take_over_same_axis_motor_hold(self) -> None:
        handoff_gate = self.source[self.source.index("const bool motion_handoff_request"):]
        self.assertIn('std::strcmp(verb, "HOME") == 0', handoff_gate)
        self.assertIn('std::strcmp(verb, "LIMIT_TEST") == 0', handoff_gate)

    def test_calibration_is_crc_checked_and_dual_slot_persistent(self) -> None:
        for value in (
            "kSlotAAddress = 0x08040000U",
            "kSlotBAddress = 0x08060000U",
            "kStorageEnd = 0x08080000U",
            "kCalibrationMagic",
            "kHardwarePulsesPerDegree",
            "sizeof(CalibrationRecord) == 128U",
        ):
            self.assertIn(value, self.storage_header)
        for value in (
            "record_crc32c",
            "HAL_FLASH_Program",
            "HAL_FLASHEx_Erase",
            "record_valid(verified)",
            "CompatibilityConfigRecord",
        ):
            self.assertIn(value, self.storage)

    def test_driver_current_faults_and_servo_signal_mode_are_preserved(self) -> None:
        self.assertIn("0U, 0U, 700U, 700U, 700U, 450U", self.source)
        self.assertIn("driver.rms_current(kRunCurrentMa[axis], 0.35F)", self.source)
        self.assertIn("kServoMaximumPulseRate = {500.0F, 350.0F}", self.source)
        self.assertIn("kJ2MaximumPulseAcceleration = 900.0F", self.source)
        self.assertIn("axis == 1U && acceleration > kJ2MaximumPulseAcceleration", self.source)
        self.assertIn("kServoPulseWidthUs = 1000U", self.source)
        self.assertIn("axis < 2U ? kServoPulseWidthUs : kTmcPulseWidthUs", self.source)
        self.assertIn("kExpectedTmcVersion = 0x21U", self.source)
        self.assertIn("ifcnt_after != ifcnt_before", self.source)
        self.assertIn("signal=PUSH_PULL_3V3", self.source)
        self.assertIn("INTERFACE_VERIFIED", self.source)


if __name__ == "__main__":
    unittest.main()
