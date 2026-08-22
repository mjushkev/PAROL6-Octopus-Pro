import unittest

import _bootstrap

from parol6_backend import SafetySupervisor
from parol6_protocol import ControllerState


class SafetyTests(unittest.TestCase):
    def assert_outputs_safe(self, safety: SafetySupervisor) -> None:
        self.assertFalse(safety.outputs.contactor_request)
        self.assertFalse(safety.outputs.step_outputs_enabled)
        self.assertFalse(safety.outputs.driver_enables_active)
        self.assertFalse(safety.outputs.gripper_pwm_enabled)

    def test_uncommissioned_boot_and_enable_fail_closed(self) -> None:
        safety = SafetySupervisor(commissioned=False)
        safety.finish_boot()
        self.assertEqual(safety.state, ControllerState.NOT_COMMISSIONED)
        self.assert_outputs_safe(safety)
        safety.take_control(1)
        self.assertFalse(safety.request_motor_enable(1))
        self.assert_outputs_safe(safety)

    def test_commissioned_simulated_sequence_requires_explicit_actions(self) -> None:
        safety = SafetySupervisor(commissioned=True)
        safety.finish_boot()
        self.assertEqual(safety.state, ControllerState.DISARMED)
        self.assertTrue(safety.take_control(10))
        self.assertTrue(safety.request_motor_enable(10))
        self.assertEqual(safety.state, ControllerState.ARMING)
        self.assertFalse(safety.outputs.step_outputs_enabled)
        safety.update_contactor_feedback(True)
        self.assertEqual(safety.state, ControllerState.UNHOMED)
        self.assertTrue(safety.start_homing(10))
        safety.finish_homing()
        self.assertEqual(safety.state, ControllerState.READY)
        self.assertTrue(safety.start_execution(10))
        self.assertEqual(safety.state, ControllerState.EXECUTING)

    def test_disconnect_never_restores_motion_or_homing(self) -> None:
        safety = SafetySupervisor(commissioned=True)
        safety.finish_boot()
        safety.take_control(3)
        safety.request_motor_enable(3)
        safety.update_contactor_feedback(True)
        safety.start_homing(3)
        safety.finish_homing()
        safety.disconnect()
        self.assertEqual(safety.state, ControllerState.DISARMED)
        self.assertFalse(safety.homed)
        self.assertIsNone(safety.control_session)
        self.assert_outputs_safe(safety)

    def test_estop_latches_and_forces_every_output_off(self) -> None:
        safety = SafetySupervisor(commissioned=True)
        safety.finish_boot()
        safety.take_control(2)
        safety.request_motor_enable(2)
        safety.estop()
        self.assertEqual(safety.state, ControllerState.ESTOP_LATCHED)
        self.assert_outputs_safe(safety)


if __name__ == "__main__":
    unittest.main()

