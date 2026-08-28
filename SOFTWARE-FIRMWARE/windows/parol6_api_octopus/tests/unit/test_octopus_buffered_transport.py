from __future__ import annotations

from dataclasses import replace

import numpy as np

from parol6.hardware_profile import PROFILE
from parol6.protocol.octopus_buffered import ControllerState, Fault, Setpoint, crc32c
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


def test_transport_clears_latched_fault_before_reporting_connected(monkeypatch) -> None:
    profile_crc = crc32c(PROFILE.path.read_bytes())
    simulator = BufferedControllerSimulator(profile_crc32c=profile_crc)
    simulator.faults = Fault.WATCHDOG
    simulator.state = ControllerState.FAULT
    install_fake(monkeypatch, simulator)

    transport = OctopusBufferedTransport(port="FAKE")

    assert transport.connect()
    assert transport.is_connected()
    assert simulator.faults is Fault.NONE
    assert simulator.status().queue_depth == 0
    assert simulator.status().state is ControllerState.IDLE


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


def test_running_stream_refills_before_queue_can_underrun(monkeypatch) -> None:
    profile_crc = crc32c(PROFILE.path.read_bytes())
    simulator = BufferedControllerSimulator(profile_crc32c=profile_crc)
    install_fake(monkeypatch, simulator)
    transport = OctopusBufferedTransport(port="FAKE")
    assert transport.connect()

    affected = np.ones(8, dtype=np.uint8)
    io = np.zeros(8, dtype=np.uint8)
    gripper = np.zeros(6, dtype=np.int32)
    for index in range(transport.BATCH_POINTS):
        assert transport.write_frame(
            np.full(6, index, dtype=np.int32),
            np.full(6, 100, dtype=np.int32),
            int(CommandCode.MOVE),
            affected,
            io,
            0,
            gripper,
        )
    assert simulator.status().queue_depth == transport.BATCH_POINTS

    simulator.advance(transport.REFILL_POINTS * 10)
    assert simulator.status().queue_depth == (
        transport.BATCH_POINTS - transport.REFILL_POINTS
    )
    for offset in range(transport.REFILL_POINTS - 1):
        assert transport.write_frame(
            np.full(6, 20 + offset, dtype=np.int32),
            np.full(6, 100, dtype=np.int32),
            int(CommandCode.MOVE),
            affected,
            io,
            0,
            gripper,
        )
    assert simulator.status().queue_depth == 8
    assert transport.write_frame(
        np.full(6, 30, dtype=np.int32),
        np.full(6, 100, dtype=np.int32),
        int(CommandCode.MOVE),
        affected,
        io,
        0,
        gripper,
    )
    assert simulator.status().queue_depth == transport.BATCH_POINTS
    assert simulator.status().faults is Fault.NONE


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


def test_joint_sensor_bits_never_alias_the_physical_estop() -> None:
    simulator = BufferedControllerSimulator(profile_crc32c=crc32c(PROFILE.path.read_bytes()))
    transport = OctopusBufferedTransport(port="FAKE")

    base_status = simulator.status()
    # Native P6B1 bit 3 is J5 because sensor bits are packed MSB-first. Test
    # clear, J5-only, and every-home-input-active states.
    for sensor_bits in (0x00, 0x08, 0xFF):
        transport._publish_status(replace(base_status, sensor_bits=sensor_bits))

        # The legacy adapter byte reports E-stop released independently of
        # every joint home input. With the owner's main-power E-stop, losing
        # power is represented by loss of the controller connection.
        assert transport._latest_payload[37] == 0x08


def test_timing_quantization_never_exceeds_firmware_speed_limits() -> None:
    transport = OctopusBufferedTransport(port="FAKE")
    previous = (0, 0, 0, 0, 0, 0)
    # At 100 Hz, these deltas derive to 500 steps/s. That is above J1/J2's
    # configured validator limits even though the planner's average trajectory
    # remains within its calibrated degrees-per-second cap.
    point = Setpoint(
        positions_steps=(5, 5, 5, 5, 5, 5),
        speeds_steps_s=(200, 200, 200, 200, 200, 200),
        io_bits=0,
        command=int(CommandCode.MOVE),
    )

    bounded = transport._bounded_timed_setpoint(point, previous)

    assert bounded.speeds_steps_s[0] == transport.SPEED_LIMITS_STEPS_S[0]
    assert bounded.speeds_steps_s[1] == transport.SPEED_LIMITS_STEPS_S[1]
    assert bounded.speeds_steps_s[2:] == (500, 500, 500, 500)
    assert all(
        speed <= limit
        for speed, limit in zip(
            bounded.speeds_steps_s, transport.SPEED_LIMITS_STEPS_S, strict=True
        )
    )
