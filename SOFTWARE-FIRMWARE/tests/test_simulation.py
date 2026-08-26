import json
import unittest

import _bootstrap

from parol6_backend import (
    ENCODER_INTEGRATION_ENABLED,
    SERVO42C_MODE,
    EncoderMode,
    FakeESPBridge,
    FakeMCU,
    LinkProfile,
    SimulationClient,
)
from parol6_protocol import ControllerState, MessageType


class SimulationTests(unittest.TestCase):
    def test_uncommissioned_controller_refuses_motor_enable(self) -> None:
        mcu = FakeMCU(commissioned=False)
        client = SimulationClient(FakeESPBridge(mcu), session_id=17)
        client.send(MessageType.TAKE_CONTROL)
        response = client.send(MessageType.MOTOR_ENABLE)[0]
        payload = json.loads(response.payload)
        self.assertEqual(payload["error"], "NOT_COMMISSIONED")
        self.assertEqual(mcu.safety.state, ControllerState.NOT_COMMISSIONED)
        self.assertFalse(mcu.safety.outputs.contactor_request)

    def test_duplicate_transport_frame_cannot_execute_twice(self) -> None:
        mcu = FakeMCU(commissioned=True)
        bridge = FakeESPBridge(mcu, LinkProfile(duplicate_every=6))
        client = SimulationClient(bridge, session_id=4)
        client.send(MessageType.TAKE_CONTROL)
        client.send(MessageType.MOTOR_ENABLE)
        mcu.safety.update_contactor_feedback(True)
        client.send(MessageType.HOME_START)
        mcu.safety.finish_homing()
        client.send(MessageType.TRAJECTORY_BEGIN, (55).to_bytes(8, "little"))
        points = {
            "points": [
                {"index": 0, "duration_ms": 100, "target_steps": [10, 10, -10, 10, -10, 10]},
                {"index": 1, "duration_ms": 100, "target_steps": [20, 20, -20, 20, -20, 20]},
            ]
        }
        client.send(MessageType.TRAJECTORY_POINTS, json.dumps(points).encode())
        responses = client.send(MessageType.TRAJECTORY_COMMIT, (55).to_bytes(8, "little"))
        self.assertEqual(len(responses), 2)
        self.assertEqual(mcu.executed_trajectory_ids, {55})
        duplicate_payload = json.loads(responses[1].payload)
        self.assertEqual(duplicate_payload["error"], "REPLAY")

    def test_short_trajectory_horizon_is_rejected(self) -> None:
        mcu = FakeMCU(commissioned=True)
        client = SimulationClient(FakeESPBridge(mcu), session_id=12)
        client.send(MessageType.TAKE_CONTROL)
        client.send(MessageType.MOTOR_ENABLE)
        mcu.safety.update_contactor_feedback(True)
        client.send(MessageType.HOME_START)
        mcu.safety.finish_homing()
        client.send(MessageType.TRAJECTORY_BEGIN, (9).to_bytes(8, "little"))
        points = {
            "points": [
                {"index": 0, "duration_ms": 50, "target_steps": [1, 1, -1, 1, -1, 1]}
            ]
        }
        client.send(MessageType.TRAJECTORY_POINTS, json.dumps(points).encode())
        response = client.send(MessageType.TRAJECTORY_COMMIT, (9).to_bytes(8, "little"))[0]
        self.assertEqual(json.loads(response.payload)["detail"], "initial_horizon_too_short")
        self.assertEqual(mcu.safety.state, ControllerState.READY)

    def test_corruption_is_rejected_without_state_change(self) -> None:
        mcu = FakeMCU(commissioned=True)
        bridge = FakeESPBridge(mcu, LinkProfile(corrupt_every=1))
        client = SimulationClient(bridge, session_id=9)
        response = client.send(MessageType.TAKE_CONTROL)[0]
        payload = json.loads(response.payload)
        self.assertEqual(payload["error"], "MALFORMED")
        self.assertEqual(mcu.safety.state, ControllerState.DISARMED)
        self.assertIsNone(mcu.safety.control_session)

    def test_commanded_angles_are_primary_and_encoder_code_remains_dormant(self) -> None:
        mcu = FakeMCU()
        mcu.status.commanded_joint_deg[:] = [1, 2, 3, 4, 5, 6]
        self.assertEqual(mcu.status.angles, [1, 2, 3, 4, 5, 6])
        self.assertEqual(len(mcu.status.encoders), 2)
        self.assertFalse(ENCODER_INTEGRATION_ENABLED)
        self.assertEqual(SERVO42C_MODE, "CR_OPEN")
        self.assertTrue(all(encoder.mode is EncoderMode.DISABLED for encoder in mcu.status.encoders))
        self.assertTrue(all(not encoder.valid for encoder in mcu.status.encoders))


if __name__ == "__main__":
    unittest.main()
