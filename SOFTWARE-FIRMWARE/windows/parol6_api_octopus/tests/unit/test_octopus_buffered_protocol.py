from __future__ import annotations

import pytest

from parol6.protocol.octopus_buffered import (
    Capability, ControllerState, ErrorCode, Fault, Hello, MessageType, Packet,
    ProtocolError, REQUIRED_CAPABILITIES, Setpoint, SetpointBatch, Status,
    StreamDecoder, crc32c, decode_error,
)
from parol6.protocol.octopus_simulator import BufferedControllerSimulator


PROFILE_CRC = 0x12345678


def point(value: int) -> Setpoint:
    return Setpoint((value,) * 6, (100,) * 6, io_bits=3)


def hello(sequence: int = 1) -> Packet:
    return Packet(
        MessageType.HELLO, sequence,
        Hello(REQUIRED_CAPABILITIES, 128, 250, PROFILE_CRC).encode(),
    )


def decode_one(frames: list[bytes]) -> Packet:
    assert len(frames) == 1
    return Packet.decode(frames[0])


def test_crc32c_known_vector() -> None:
    assert crc32c(b"123456789") == 0xE3069283


def test_packet_and_payload_round_trip() -> None:
    original = SetpointBatch(42, 10_000, (point(10), point(-20)))
    packet = Packet(MessageType.ENQUEUE, 7, original.encode(), flags=3)
    decoded = Packet.decode(packet.encode())
    assert decoded == packet
    assert SetpointBatch.decode(decoded.payload) == original


def test_status_round_trip() -> None:
    status = Status(
        5, 8, 128, ControllerState.RUNNING | ControllerState.MOTORS_ENABLED,
        Fault.NONE, (1, 2, 3, 4, 5, 6), (6, 5, 4, 3, 2, 1), 0b10101, 0b11,
    )
    assert Status.decode(status.encode()) == status


def test_stream_decoder_handles_splits_noise_and_corruption() -> None:
    good = Packet(MessageType.START, 9).encode()
    bad = bytearray(Packet(MessageType.CLEAR, 8).encode())
    bad[-1] ^= 0xFF
    decoder = StreamDecoder()
    assert decoder.feed(b"noise" + bytes(bad) + good[:5]) == []
    assert decoder.feed(good[5:]) == [Packet(MessageType.START, 9)]
    assert decoder.bad_frames == 1
    assert decoder.discarded_bytes >= 5


def test_packet_rejects_crc_damage() -> None:
    encoded = bytearray(Packet(MessageType.STOP, 1).encode())
    encoded[8] ^= 1
    with pytest.raises(ProtocolError, match="CRC32C"):
        Packet.decode(encoded)


def test_simulator_requires_exact_owner_profile() -> None:
    simulator = BufferedControllerSimulator(profile_crc32c=PROFILE_CRC)
    wrong = Hello(REQUIRED_CAPABILITIES, 128, 250, 0).encode()
    response = decode_one(simulator.receive(Packet(MessageType.HELLO, 1, wrong).encode()))
    assert response.message_type is MessageType.ERROR
    assert decode_error(response.payload)[1] is ErrorCode.CAPABILITY_MISMATCH
    assert simulator.status().faults & Fault.CAPABILITY_MISMATCH


def test_simulator_rejects_replay_and_overflow() -> None:
    simulator = BufferedControllerSimulator(queue_capacity=2, profile_crc32c=PROFILE_CRC)
    assert decode_one(simulator.receive(hello().encode())).message_type is MessageType.HELLO_ACK
    batch = SetpointBatch(0, 10_000, (point(1), point(2)))
    enqueue = Packet(MessageType.ENQUEUE, 2, batch.encode())
    assert decode_one(simulator.receive(enqueue.encode())).message_type is MessageType.ACK
    assert decode_error(decode_one(simulator.receive(enqueue.encode())).payload)[1] is ErrorCode.REPLAY

    simulator.receive(Packet(MessageType.CLEAR, 3).encode())
    oversized = SetpointBatch(0, 10_000, (point(1), point(2), point(3)))
    overflow = decode_one(simulator.receive(Packet(MessageType.ENQUEUE, 4, oversized.encode()).encode()))
    assert decode_error(overflow.payload)[1] is ErrorCode.QUEUE_OVERFLOW
    assert simulator.status().faults & Fault.QUEUE_OVERFLOW


def test_simulator_executes_then_faults_on_underrun() -> None:
    simulator = BufferedControllerSimulator(profile_crc32c=PROFILE_CRC, watchdog_ms=1000)
    simulator.receive(hello().encode())
    simulator.receive(Packet(MessageType.ENQUEUE, 2, SetpointBatch(0, 10_000, (point(10), point(20))).encode()).encode())
    simulator.receive(Packet(MessageType.START, 3).encode())
    simulator.advance(20)
    assert simulator.status().positions_steps == (20,) * 6
    simulator.advance(10)
    assert simulator.status().faults & Fault.QUEUE_UNDERRUN
    assert simulator.status().state is ControllerState.FAULT


def test_finish_drains_queue_and_holds_without_underrun() -> None:
    simulator = BufferedControllerSimulator(profile_crc32c=PROFILE_CRC, watchdog_ms=1000)
    simulator.receive(hello().encode())
    simulator.receive(Packet(MessageType.ENQUEUE, 2, SetpointBatch(0, 10_000, (point(10), point(20))).encode()).encode())
    simulator.receive(Packet(MessageType.START, 3).encode())
    simulator.receive(Packet(MessageType.FINISH, 4).encode())
    simulator.advance(30)
    status = simulator.status()
    assert status.positions_steps == (20,) * 6
    assert status.faults is Fault.NONE
    assert status.state & ControllerState.IDLE
    assert status.state & ControllerState.MOTORS_ENABLED


def test_priority_stop_is_always_accepted_and_clears_queue() -> None:
    simulator = BufferedControllerSimulator(profile_crc32c=PROFILE_CRC)
    simulator.receive(hello().encode())
    simulator.receive(Packet(MessageType.ENQUEUE, 2, SetpointBatch(0, 10_000, (point(1), point(2))).encode()).encode())
    simulator.receive(Packet(MessageType.START, 3).encode())
    response = decode_one(simulator.receive(Packet(MessageType.STOP, 3).encode()))
    assert response.message_type is MessageType.ACK
    status = simulator.status()
    assert status.state is ControllerState.STOPPED
    assert status.queue_depth == 0
    assert not (status.state & ControllerState.MOTORS_ENABLED)


def test_watchdog_latches_fault_and_drops_motion() -> None:
    simulator = BufferedControllerSimulator(profile_crc32c=PROFILE_CRC, watchdog_ms=25)
    simulator.receive(hello().encode())
    batch = SetpointBatch(0, 10_000, tuple(point(i) for i in range(10)))
    simulator.receive(Packet(MessageType.ENQUEUE, 2, batch.encode()).encode())
    simulator.receive(Packet(MessageType.START, 3).encode())
    simulator.advance(30)
    assert simulator.status().faults & Fault.WATCHDOG
    assert simulator.status().queue_depth == 0


def test_capability_mask_contains_required_safety_features() -> None:
    assert REQUIRED_CAPABILITIES & Capability.PRIORITY_STOP
    assert REQUIRED_CAPABILITIES & Capability.QUEUE_WATCHDOG
    assert REQUIRED_CAPABILITIES & Capability.J1_MANUAL_AUTO_HOME
    assert REQUIRED_CAPABILITIES & Capability.GRACEFUL_FINISH_HOLD
