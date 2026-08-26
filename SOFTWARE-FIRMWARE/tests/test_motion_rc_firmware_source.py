from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "firmware" / "octopus_h723_motion_rc"


class MotionRc09FirmwareSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = (PROJECT / "src" / "main.cpp").read_text(encoding="utf-8")
        cls.ini = (PROJECT / "platformio.ini").read_text(encoding="utf-8")

    def test_release_is_bootloader_bounded_and_versioned(self) -> None:
        self.assertIn("board_build.flash_offset = 0x20000", self.ini)
        self.assertIn("board_upload.maximum_size = 262144", self.ini)
        self.assertIn("0.9.0-motion-rc", self.ini)

    def test_coordinated_command_requires_all_homed_and_safe_targets(self) -> None:
        self.assertIn('kCoordinatedMovePrefix[] = "COORD_MOVE "', self.source)
        self.assertIn("!home_configured[axis] || !homed[axis]", self.source)
        self.assertIn("!logical_target_is_safe(axis, targets[axis])", self.source)
        self.assertIn("COORDINATED_MOVE_VERIFIED", self.source)
        self.assertIn("coordinated_target_rejected", self.source)

    def test_initial_coordinated_rates_are_bounded_to_ten_percent(self) -> None:
        self.assertIn(
            "kCoordinatedMaximumDegreesPerSecond", self.source
        )
        self.assertIn(
            "kCoordinatedMaximumAccelerationDegreesPerSecond2", self.source
        )
        self.assertIn("speed_cap_percent=10", self.source)
        self.assertIn("coordinated_rate_exceeds_10_percent_cap", self.source)
        self.assertIn("kMaximumCoordinatedDurationMs = 60000U", self.source)

    def test_coordinated_motion_and_pose_hold_are_host_supervised(self) -> None:
        self.assertIn("MotionKind::coordinated", self.source)
        self.assertIn("kHostMotionTimeoutMs = 2000U", self.source)
        self.assertIn("kMotorHoldTimeoutMs = 2000U", self.source)
        self.assertIn("PAROL6_COORDINATED_DONE", self.source)
        self.assertIn("PAROL6_COORDINATED_HOLD_RELEASED", self.source)
        self.assertIn("RELEASE_COORDINATED_HOLD_VERIFIED", self.source)
        self.assertIn('release_coordinated_hold("host_timeout")', self.source)
        self.assertIn('release_coordinated_hold("operator_stop")', self.source)

    def test_all_switch_transitions_remain_guarded(self) -> None:
        self.assertIn("coordinated_sensor_change_is_safe", self.source)
        self.assertIn("coordinated.initial_other_stops", self.source)
        self.assertIn('end_motion("limit_abort")', self.source)
        self.assertIn('end_motion("aux_stop_abort")', self.source)

    def test_manual_and_automatic_j1_home_paths_are_both_retained(self) -> None:
        self.assertIn('std::strcmp(verb, "MANUAL_HOME")', self.source)
        self.assertIn("SET_CURRENT_POSITION_ZERO_TEMPORARY", self.source)
        self.assertIn('std::strcmp(verb, "HOME")', self.source)
        self.assertIn("j1_home=sensor_or_manual_temporary", self.source)
        self.assertNotIn("j1_home_hard_locked", self.source)


if __name__ == "__main__":
    unittest.main()
