import unittest

import _bootstrap

from parol6_backend import CalibrationError, J1HomeMode, load_default_calibration


class CalibrationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.calibration = load_default_calibration()

    def test_owner_verified_calibration_is_complete_and_exact(self) -> None:
        self.assertEqual(self.calibration.device_uid, "0028002F3233510536303431")
        self.assertEqual(self.calibration.source_flash_sequence, 60)
        self.assertEqual(self.calibration.j1_home_mode_default, J1HomeMode.MANUAL)
        self.assertTrue(self.calibration.j1_auto_home_available)
        expected = {
            "J1": (114, "-", "+", -230.0, 35.0),
            "J2": (356, "-", "+", 0.0, 119.536),
            "J3": (161, "+", "-", 0.0, 90.329),
            "J4": (36, "-", "+", 0.0, 232.694),
            "J5": (36, "+", "+", -254.25, 0.0),
            "J6": (89, "-", "+", -180.0, 180.0),
        }
        for joint, values in expected.items():
            item = self.calibration.by_id[joint]
            self.assertEqual(
                (item.pulses_per_degree, item.home_raw, item.positive_raw,
                 item.minimum_deg, item.maximum_deg),
                values,
            )
        self.assertEqual(self.calibration.by_id["J5"].post_home_standby_deg, -130.0)
        for joint in ("J1", "J2", "J3", "J4", "J6"):
            self.assertIsNone(self.calibration.by_id[joint].post_home_standby_deg)

    def test_j3_direction_and_pose_limits_are_enforced(self) -> None:
        self.assertEqual(self.calibration.by_id["J3"].angle_to_raw_steps(10), -1610)
        with self.assertRaisesRegex(CalibrationError, "J4_angle_out_of_range"):
            self.calibration.validate_pose((0, 0, 0, -0.001, 0, 0))
