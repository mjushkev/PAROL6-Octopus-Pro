import unittest

import _bootstrap

from parol6_backend import J1HomeMode, OperatorError, OperatorSession


class OperatorTests(unittest.TestCase):
    def test_j1_home_mode_switch_changes_command_without_changing_calibration(self) -> None:
        session = OperatorSession()
        self.assertEqual(
            session.build_home_command("J1", "1234ABCD"),
            "MANUAL_HOME J1 1234ABCD SET_CURRENT_POSITION_ZERO_TEMPORARY",
        )
        session.set_j1_home_mode(J1HomeMode.AUTO)
        self.assertEqual(session.build_home_command("J1", "1234ABCD"), "HOME J1 1234ABCD START")
        self.assertEqual(session.calibration.by_id["J1"].minimum_deg, -230.0)

    def test_jog_and_pose_require_homing_and_respect_limits(self) -> None:
        session = OperatorSession()
        session.motion_enabled = True
        with self.assertRaisesRegex(OperatorError, "joint_not_homed"):
            session.build_jog_command("J2", "1234ABCD", "+", 1000)
        session.homed = {f"J{i}": True for i in range(1, 7)}
        self.assertIn("JOG J2", session.build_jog_command("J2", "1234ABCD", "+", 1000))
        plan = session.plan_pose((1, 1, 1, 1, -1, 1), speed_percent=10)
        command = session.build_coordinated_move_command("1234ABCD", plan)
        self.assertTrue(command.startswith("COORD_MOVE 1234ABCD"))
