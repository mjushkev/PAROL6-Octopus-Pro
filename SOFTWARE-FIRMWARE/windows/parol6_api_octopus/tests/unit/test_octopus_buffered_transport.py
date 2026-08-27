from __future__ import annotations

import numpy as np

from parol6.hardware_profile import PROFILE
from parol6.protocol.octopus_buffered import ControllerState, Fault, crc32c
from parol6.protocol.octopus_simulator import BufferedControllerSimulator
from parol6.protocol.wire import CommandCode
from parol6.server.transports.octopus_buffered_transport import OctopusBufferedTransport


class FakeSerial:
    def __init__(self, controller: BufferedControllerSimulator) -> None:
        self.controller = controller
        self.is_open = True
        self.rx = bytearray()

    @property
    def in_waiting(self) -> int:
        return len(self.rx)

    def reset_input_buffer(self) -> None:
        self.rx.clear()

    def write(self, data: bytes | bytearray | memoryview) -> int:
        for response in self.controller.receive(data):
            self.rx.extend(response)
        return len(data)

    def read(self, size: int) -> bytes:
        result = bytes(self.rx[:size])
        del self.rx[:size]
        return result

    def close(self) -> None:
        self.is_open = False


def install_fake(monkeypatch, simulator: BufferedControllerSimulator) -> FakeSerial:
    fake = FakeSerial(simulator)
    monkeypatch.setattr(
        "parol6.server.transports.octopus_buffered_transport.serial.Serial",
        lambda **_kwargs: fake,
    )
    return fake


def test_transport_refuses_wrong_profile_firmware(monkeypatch) -> None:
    simulator = BufferedControllerSimulator(profile_crc32c=0)
    install_fake(monkeypatch, simulator)
    transport = OctopusBufferedTransport(port="FAKE")
    assert not transport.connect()
    assert not transport.is_connected()


def test_transport_handshake_batches_motion_and_priority_stops(monkeypatch) -> None:
    profile_crc = crc32c(PROFILE.path.read_bytes())
    simulator = BufferedControllerSimulator(profile_crc32c=profile_crc)
    install_fake(monkeypatch, simulator)
    transport = OctopusBufferedTransport(port="FAKE")
    assert transport.connect()

    affected = np.ones(8, dtype=np.uint8)
    io = np.zeros(8, dtype=np.uint8)
    gripper = np.zeros(6, dtype=np.int32)
    for index in range(transport.BATCH_POINTS):
        positions = np.full(6, index * 100, dtype=np.int32)
        speeds = np.full(6, 200, dtype=np.int32)
        assert transport.write_frame(
            positions, speeds, int(CommandCode.MOVE), affected, io, 0, gripper
        )

    assert simulator.status().queue_depth == transport.BATCH_POINTS
    assert simulator.status().state & ControllerState.RUNNING
    idle = np.zeros(6, dtype=np.int32)
    assert transport.write_frame(
        idle, idle, int(CommandCode.IDLE), affected, io, 0, gripper
    )
    simulator.advance((transport.BATCH_POINTS + 1) * 10)
    assert simulator.status().faults is Fault.NONE
    assert simulator.status().state & ControllerState.IDLE
    assert simulator.status().state & ControllerState.MOTORS_ENABLED
    assert transport.priority_stop()
    assert simulator.status().queue_depth == 0
    assert simulator.status().state is ControllerState.STOPPED


def test_idle_frame_sends_watchdog_heartbeat(monkeypatch) -> None:
    profile_crc = crc32c(PROFILE.path.read_bytes())
    simulator = BufferedControllerSimulator(profile_crc32c=profile_crc)
    install_fake(monkeypatch, simulator)
    transport = OctopusBufferedTransport(port="FAKE")
    assert transport.connect()
    sequence_before = simulator.last_sequence
    transport._last_tx_monotonic = 0.0
    zeros = np.zeros(8, dtype=np.uint8)
    assert transport.write_frame(
        np.zeros(6), np.zeros(6), int(CommandCode.IDLE), zeros, zeros, 0, np.zeros(6)
    )
    assert simulator.last_sequence != sequence_before
