"""Versioned buffered wire protocol for the owner-configured Octopus controller.

P6B1 is deliberately separate from both the upstream PAROL6 52-byte protocol
and the commissioning firmware's ASCII command language. A host must complete
the HELLO capability exchange before it can arm or enqueue motion.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum, IntFlag
import struct
from typing import Iterable


MAGIC = b"P6B1"
VERSION = 1
MAX_PAYLOAD = 4096
JOINT_COUNT = 6

_HEADER = struct.Struct("<4sBBHIH")
_CRC = struct.Struct("<I")
_SETPOINT = struct.Struct("<6i6I HBB")
_BATCH_HEADER = struct.Struct("<IHH")
_HELLO = struct.Struct("<IHHI")
_STATUS = struct.Struct("<IHHII6i6iHH")
_ACK = struct.Struct("<IHHI")
_ERROR = struct.Struct("<IHH")


class MessageType(IntEnum):
    HELLO = 1
    HELLO_ACK = 2
    ENQUEUE = 3
    ACK = 4
    STATUS = 5
    START = 6
    STOP = 7
    CLEAR = 8
    HOME = 9
    SET_J1_HOME_MODE = 10
    IO = 11
    FINISH = 12
    ERROR = 255


class Capability(IntFlag):
    BUFFERED_ABSOLUTE_STEPS = 1 << 0
    CRC32C = 1 << 1
    PRIORITY_STOP = 1 << 2
    QUEUE_WATCHDOG = 1 << 3
    OWNER_PROFILE = 1 << 4
    J1_MANUAL_AUTO_HOME = 1 << 5
    GRACEFUL_FINISH_HOLD = 1 << 6


REQUIRED_CAPABILITIES = (
    Capability.BUFFERED_ABSOLUTE_STEPS
    | Capability.CRC32C
    | Capability.PRIORITY_STOP
    | Capability.QUEUE_WATCHDOG
    | Capability.OWNER_PROFILE
    | Capability.J1_MANUAL_AUTO_HOME
    | Capability.GRACEFUL_FINISH_HOLD
)


class ControllerState(IntFlag):
    IDLE = 1 << 0
    ARMED = 1 << 1
    RUNNING = 1 << 2
    STOPPED = 1 << 3
    FAULT = 1 << 4
    HOMED = 1 << 5
    MOTORS_ENABLED = 1 << 6


class Fault(IntFlag):
    NONE = 0
    BAD_FRAME = 1 << 0
    BAD_VERSION = 1 << 1
    CAPABILITY_MISMATCH = 1 << 2
    REPLAY = 1 << 3
    QUEUE_OVERFLOW = 1 << 4
    QUEUE_UNDERRUN = 1 << 5
    WATCHDOG = 1 << 6
    LIMIT = 1 << 7
    ESTOP = 1 << 8


class ErrorCode(IntEnum):
    BAD_FRAME = 1
    BAD_VERSION = 2
    CAPABILITY_MISMATCH = 3
    REPLAY = 4
    QUEUE_OVERFLOW = 5
    NOT_ARMED = 6
    INVALID_PAYLOAD = 7
    FAULT_LATCHED = 8


class ProtocolError(ValueError):
    """A P6B1 frame is malformed or fails integrity checks."""


def crc32c(data: bytes | bytearray | memoryview, crc: int = 0) -> int:
    """Return the Castagnoli CRC-32C used by P6B1."""
    value = crc ^ 0xFFFFFFFF
    for byte in data:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ (0x82F63B78 if value & 1 else 0)
    return value ^ 0xFFFFFFFF


@dataclass(frozen=True)
class Packet:
    message_type: MessageType
    sequence: int
    payload: bytes = b""
    flags: int = 0

    def encode(self) -> bytes:
        if not 0 <= self.sequence <= 0xFFFFFFFF:
            raise ProtocolError("sequence must fit uint32")
        if len(self.payload) > MAX_PAYLOAD:
            raise ProtocolError(f"payload exceeds {MAX_PAYLOAD} bytes")
        header = _HEADER.pack(
            MAGIC, VERSION, int(self.message_type), self.flags & 0xFFFF,
            self.sequence, len(self.payload)
        )
        body = header + self.payload
        return body + _CRC.pack(crc32c(body))

    @classmethod
    def decode(cls, frame: bytes | bytearray | memoryview) -> "Packet":
        data = bytes(frame)
        minimum = _HEADER.size + _CRC.size
        if len(data) < minimum:
            raise ProtocolError("truncated frame")
        magic, version, raw_type, flags, sequence, payload_len = _HEADER.unpack_from(data)
        if magic != MAGIC:
            raise ProtocolError("bad magic")
        if version != VERSION:
            raise ProtocolError(f"unsupported protocol version {version}")
        if payload_len > MAX_PAYLOAD:
            raise ProtocolError("declared payload is too large")
        expected_len = _HEADER.size + payload_len + _CRC.size
        if len(data) != expected_len:
            raise ProtocolError("frame length does not match payload length")
        expected_crc = _CRC.unpack_from(data, expected_len - _CRC.size)[0]
        actual_crc = crc32c(data[:-_CRC.size])
        if actual_crc != expected_crc:
            raise ProtocolError("CRC32C mismatch")
        try:
            message_type = MessageType(raw_type)
        except ValueError as exc:
            raise ProtocolError(f"unknown message type {raw_type}") from exc
        return cls(message_type, sequence, data[_HEADER.size : -_CRC.size], flags)


class StreamDecoder:
    """Incremental decoder that resynchronizes at the next P6B1 magic word."""

    def __init__(self) -> None:
        self._buffer = bytearray()
        self.discarded_bytes = 0
        self.bad_frames = 0

    def feed(self, data: bytes | bytearray | memoryview) -> list[Packet]:
        self._buffer.extend(data)
        packets: list[Packet] = []
        minimum = _HEADER.size + _CRC.size
        while True:
            magic_at = self._buffer.find(MAGIC)
            if magic_at < 0:
                keep = min(len(self._buffer), len(MAGIC) - 1)
                self.discarded_bytes += len(self._buffer) - keep
                if keep:
                    del self._buffer[:-keep]
                else:
                    self._buffer.clear()
                break
            if magic_at:
                self.discarded_bytes += magic_at
                del self._buffer[:magic_at]
            if len(self._buffer) < minimum:
                break
            _, _, _, _, _, payload_len = _HEADER.unpack_from(self._buffer)
            if payload_len > MAX_PAYLOAD:
                self.bad_frames += 1
                del self._buffer[0]
                continue
            total = _HEADER.size + payload_len + _CRC.size
            if len(self._buffer) < total:
                break
            candidate = bytes(self._buffer[:total])
            try:
                packets.append(Packet.decode(candidate))
                del self._buffer[:total]
            except ProtocolError:
                self.bad_frames += 1
                del self._buffer[0]
        return packets


@dataclass(frozen=True)
class Setpoint:
    positions_steps: tuple[int, int, int, int, int, int]
    speeds_steps_s: tuple[int, int, int, int, int, int]
    io_bits: int = 0
    command: int = 0

    def encode(self) -> bytes:
        if len(self.positions_steps) != JOINT_COUNT or len(self.speeds_steps_s) != JOINT_COUNT:
            raise ProtocolError("setpoint requires exactly six joints")
        if any(speed < 0 or speed > 0xFFFFFFFF for speed in self.speeds_steps_s):
            raise ProtocolError("setpoint speed must fit uint32")
        return _SETPOINT.pack(
            *self.positions_steps, *self.speeds_steps_s,
            self.io_bits & 0xFFFF, self.command & 0xFF, 0
        )

    @classmethod
    def decode(cls, data: bytes | bytearray | memoryview) -> "Setpoint":
        if len(data) != _SETPOINT.size:
            raise ProtocolError("invalid setpoint length")
        values = _SETPOINT.unpack(data)
        return cls(tuple(values[:6]), tuple(values[6:12]), values[12], values[13])  # type: ignore[arg-type]


@dataclass(frozen=True)
class SetpointBatch:
    start_tick: int
    period_us: int
    setpoints: tuple[Setpoint, ...]

    def encode(self) -> bytes:
        if not self.setpoints:
            raise ProtocolError("setpoint batch cannot be empty")
        if not 1000 <= self.period_us <= 50000:
            raise ProtocolError("setpoint period must be between 1 ms and 50 ms")
        payload = _BATCH_HEADER.pack(self.start_tick, self.period_us, len(self.setpoints))
        payload += b"".join(point.encode() for point in self.setpoints)
        if len(payload) > MAX_PAYLOAD:
            raise ProtocolError("setpoint batch exceeds maximum payload")
        return payload

    @classmethod
    def decode(cls, payload: bytes | bytearray | memoryview) -> "SetpointBatch":
        if len(payload) < _BATCH_HEADER.size:
            raise ProtocolError("truncated setpoint batch")
        start_tick, period_us, count = _BATCH_HEADER.unpack_from(payload)
        expected = _BATCH_HEADER.size + count * _SETPOINT.size
        if count == 0 or len(payload) != expected:
            raise ProtocolError("setpoint batch count/length mismatch")
        points = tuple(
            Setpoint.decode(payload[offset : offset + _SETPOINT.size])
            for offset in range(_BATCH_HEADER.size, expected, _SETPOINT.size)
        )
        return cls(start_tick, period_us, points)


@dataclass(frozen=True)
class Hello:
    capabilities: Capability
    queue_capacity: int
    watchdog_ms: int
    profile_crc32c: int

    def encode(self) -> bytes:
        return _HELLO.pack(int(self.capabilities), self.queue_capacity, self.watchdog_ms, self.profile_crc32c)

    @classmethod
    def decode(cls, payload: bytes | bytearray | memoryview) -> "Hello":
        if len(payload) != _HELLO.size:
            raise ProtocolError("invalid HELLO payload")
        capabilities, capacity, watchdog_ms, profile_crc = _HELLO.unpack(payload)
        return cls(Capability(capabilities), capacity, watchdog_ms, profile_crc)


@dataclass(frozen=True)
class Status:
    acknowledged_sequence: int
    queue_depth: int
    queue_capacity: int
    state: ControllerState
    faults: Fault
    positions_steps: tuple[int, int, int, int, int, int]
    speeds_steps_s: tuple[int, int, int, int, int, int]
    sensor_bits: int
    io_bits: int

    def encode(self) -> bytes:
        return _STATUS.pack(
            self.acknowledged_sequence, self.queue_depth, self.queue_capacity,
            int(self.state), int(self.faults), *self.positions_steps,
            *self.speeds_steps_s, self.sensor_bits, self.io_bits
        )

    @classmethod
    def decode(cls, payload: bytes | bytearray | memoryview) -> "Status":
        if len(payload) != _STATUS.size:
            raise ProtocolError("invalid STATUS payload")
        values = _STATUS.unpack(payload)
        return cls(
            values[0], values[1], values[2], ControllerState(values[3]), Fault(values[4]),
            tuple(values[5:11]), tuple(values[11:17]), values[17], values[18]  # type: ignore[arg-type]
        )


def encode_ack(acknowledged_sequence: int, queue_depth: int, queue_capacity: int, state: ControllerState) -> bytes:
    return _ACK.pack(acknowledged_sequence, queue_depth, queue_capacity, int(state))


def decode_ack(payload: bytes | bytearray | memoryview) -> tuple[int, int, int, ControllerState]:
    if len(payload) != _ACK.size:
        raise ProtocolError("invalid ACK payload")
    sequence, depth, capacity, state = _ACK.unpack(payload)
    return sequence, depth, capacity, ControllerState(state)


def encode_error(rejected_sequence: int, code: ErrorCode, detail: int = 0) -> bytes:
    return _ERROR.pack(rejected_sequence, int(code), detail & 0xFFFF)


def decode_error(payload: bytes | bytearray | memoryview) -> tuple[int, ErrorCode, int]:
    if len(payload) != _ERROR.size:
        raise ProtocolError("invalid ERROR payload")
    sequence, code, detail = _ERROR.unpack(payload)
    return sequence, ErrorCode(code), detail


def make_batch(start_tick: int, period_us: int, points: Iterable[Setpoint]) -> SetpointBatch:
    return SetpointBatch(start_tick, period_us, tuple(points))
