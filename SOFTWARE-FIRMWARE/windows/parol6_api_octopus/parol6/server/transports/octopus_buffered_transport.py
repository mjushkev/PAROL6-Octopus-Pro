"""Guarded USB transport for P6B1 firmware on BTT Octopus Pro.

The class preserves the controller's existing transport interface while using
P6B1 internally. It will not report a connection until the firmware proves its
capabilities and exact owner-profile checksum.
"""

from __future__ import annotations

import logging
import time

import numpy as np
import serial

from parol6.hardware_profile import PROFILE
from parol6.protocol.octopus_buffered import (
    ControllerState,
    ErrorCode,
    Fault,
    Hello,
    MessageType,
    Packet,
    ProtocolError,
    REQUIRED_CAPABILITIES,
    Setpoint,
    SetpointBatch,
    Status,
    StreamDecoder,
    crc32c,
    decode_ack,
    decode_error,
)
from parol6.protocol.wire import CommandCode


logger = logging.getLogger(__name__)


class OctopusBufferedTransport:
    """Buffered absolute-step transport with fail-closed capability gating."""

    BATCH_POINTS = 12
    PERIOD_US = 10_000
    HANDSHAKE_TIMEOUT_S = 1.0
    HEARTBEAT_INTERVAL_S = 0.1
    MOTION_COMMANDS = (
        int(CommandCode.MOVE), int(CommandCode.JOG), int(CommandCode.TELEPORT)
    )

    def __init__(self, port: str | None = None, baudrate: int = 2_000_000, timeout: float = 0) -> None:
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.serial: serial.Serial | None = None
        self._connected = False
        self._decoder = StreamDecoder()
        self._sequence = 1
        self._tick = 0
        self._pending: list[Setpoint] = []
        self._started = False
        self._last_command = int(CommandCode.IDLE)
        self._last_home_mask = -1
        self._last_reconnect_attempt = 0.0
        self.reconnect_interval = 1.0
        self._latest_payload = bytearray(52)
        self._latest_view = memoryview(self._latest_payload)
        self._frame_version = 0
        self._frame_ts = 0.0
        self._last_status: Status | None = None
        self._last_sent_positions: tuple[int, int, int, int, int, int] | None = None
        self._queue_depth = 0
        self._queue_capacity = 0
        self._profile_crc = crc32c(PROFILE.path.read_bytes())
        self._last_tx_monotonic = 0.0

    def _next_packet(self, message_type: MessageType, payload: bytes = b"") -> Packet:
        packet = Packet(message_type, self._sequence, payload)
        self._sequence = (self._sequence + 1) & 0xFFFFFFFF
        if self._sequence == 0:
            self._sequence = 1
        return packet

    def _write_packet(self, message_type: MessageType, payload: bytes = b"") -> bool:
        if self.serial is None or not self.serial.is_open:
            return False
        try:
            self.serial.write(self._next_packet(message_type, payload).encode())
            self._last_tx_monotonic = time.monotonic()
            return True
        except (serial.SerialException, OSError) as exc:
            logger.error("P6B1 serial write failed: %s", exc)
            self.disconnect()
            return False

    def connect(self, port: str | None = None) -> bool:
        if port:
            self.port = port
        if not self.port:
            logger.warning("No Octopus serial port specified")
            return False
        self.disconnect()
        try:
            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=0.02,
                write_timeout=0.25,
            )
            if hasattr(self.serial, "reset_input_buffer"):
                self.serial.reset_input_buffer()
            self._decoder = StreamDecoder()
            request = Hello(REQUIRED_CAPABILITIES, 0, 250, self._profile_crc)
            hello_packet = self._next_packet(MessageType.HELLO, request.encode())
            self.serial.write(hello_packet.encode())
            deadline = time.perf_counter() + self.HANDSHAKE_TIMEOUT_S
            while time.perf_counter() < deadline:
                chunk = self.serial.read(4096)
                if not chunk:
                    continue
                for packet in self._decoder.feed(chunk):
                    if packet.message_type is MessageType.ERROR:
                        _, code, detail = decode_error(packet.payload)
                        raise ProtocolError(f"firmware rejected capability handshake: {code.name} ({detail})")
                    if packet.message_type is not MessageType.HELLO_ACK:
                        continue
                    response = Hello.decode(packet.payload)
                    missing = REQUIRED_CAPABILITIES & ~response.capabilities
                    if missing:
                        raise ProtocolError(f"firmware lacks required P6B1 capabilities: {missing!s}")
                    if response.profile_crc32c != self._profile_crc:
                        raise ProtocolError("firmware owner-profile checksum does not match Commander")
                    self._queue_capacity = response.queue_capacity
                    self._connected = True
                    self._last_tx_monotonic = time.monotonic()
                    logger.info(
                        "P6B1 firmware verified on %s (queue=%d watchdog=%dms profile=%08x)",
                        self.port, response.queue_capacity, response.watchdog_ms, response.profile_crc32c,
                    )
                    return True
            raise ProtocolError(
                "P6B1 handshake timed out; commissioning firmware 0.9.1 and upstream PAROL6 firmware are not compatible"
            )
        except (serial.SerialException, OSError, ProtocolError) as exc:
            logger.error("Octopus P6B1 connection refused: %s", exc)
            self.disconnect()
            return False

    def disconnect(self) -> None:
        self._connected = False
        self._pending.clear()
        self._started = False
        self._last_tx_monotonic = 0.0
        if self.serial is not None:
            try:
                if self.serial.is_open:
                    self.serial.close()
            except (serial.SerialException, OSError):
                pass
        self.serial = None

    def is_connected(self) -> bool:
        return bool(self._connected and self.serial is not None and self.serial.is_open)

    def auto_reconnect(self) -> bool:
        if self.is_connected() or not self.port:
            return False
        now = time.monotonic()
        if now - self._last_reconnect_attempt < self.reconnect_interval:
            return False
        self._last_reconnect_attempt = now
        return self.connect(self.port)

    @staticmethod
    def _pack_bits(values: np.ndarray) -> int:
        result = 0
        for index in range(min(8, len(values))):
            result |= int(bool(values[index])) << (7 - index)
        return result

    def write_frame(
        self,
        position_out: np.ndarray,
        speed_out: np.ndarray,
        command_out: int,
        affected_joint_out: np.ndarray,
        inout_out: np.ndarray,
        timeout_out: int,
        gripper_data_out: np.ndarray,
    ) -> bool:
        del timeout_out, gripper_data_out
        if not self.is_connected():
            return False

        command = int(command_out)
        if command == int(CommandCode.DISABLE):
            if self._last_command != command:
                self.priority_stop()
            self._last_command = command
            return True
        if self._last_command in self.MOTION_COMMANDS and command not in self.MOTION_COMMANDS:
            if not self._flush_pending():
                return False
            if self._started and not self._write_packet(MessageType.FINISH):
                return False
            self._started = False
        if command == int(CommandCode.HOME):
            home_mask = self._pack_bits(affected_joint_out)
            if self._last_command != command or self._last_home_mask != home_mask:
                self._pending.clear()
                self._started = False
                payload = bytes([home_mask])
                if not self._write_packet(MessageType.HOME, payload):
                    return False
                self._last_home_mask = home_mask
            self._last_command = command
            return self._heartbeat(self._pack_bits(inout_out))
        if command not in self.MOTION_COMMANDS:
            self._last_command = command
            return self._heartbeat(self._pack_bits(inout_out))

        point = Setpoint(
            tuple(int(value) for value in position_out[:6]),  # type: ignore[arg-type]
            tuple(abs(int(value)) for value in speed_out[:6]),  # type: ignore[arg-type]
            io_bits=self._pack_bits(inout_out),
            command=command,
        )
        self._pending.append(point)
        self._last_command = command
        if len(self._pending) < self.BATCH_POINTS:
            return True
        return self._flush_pending()

    def _heartbeat(self, io_bits: int) -> bool:
        """Keep long-running homing alive without weakening the 250 ms watchdog."""
        if time.monotonic() - self._last_tx_monotonic < self.HEARTBEAT_INTERVAL_S:
            return True
        return self._write_packet(MessageType.IO, int(io_bits & 0xFFFF).to_bytes(2, "little"))

    def _flush_pending(self) -> bool:
        if not self._pending:
            return True
        previous = self._last_sent_positions
        if previous is None and self._last_status is not None:
            previous = self._last_status.positions_steps
        if previous is None:
            previous = self._pending[0].positions_steps
        timed_points: list[Setpoint] = []
        for point in self._pending:
            derived = tuple(
                (abs(point.positions_steps[axis] - previous[axis]) * 1_000_000 + self.PERIOD_US - 1)
                // self.PERIOD_US
                for axis in range(6)
            )
            timed_points.append(
                Setpoint(
                    point.positions_steps,
                    tuple(max(point.speeds_steps_s[axis], derived[axis]) for axis in range(6)),  # type: ignore[arg-type]
                    point.io_bits,
                    point.command,
                )
            )
            previous = point.positions_steps
        points = tuple(timed_points)
        self._last_sent_positions = previous
        self._pending.clear()
        batch = SetpointBatch(self._tick, self.PERIOD_US, points)
        self._tick += len(points)
        if not self._write_packet(MessageType.ENQUEUE, batch.encode()):
            return False
        if not self._started:
            if not self._write_packet(MessageType.START):
                return False
            self._started = True
        return True

    def priority_stop(self) -> bool:
        """Drop host and firmware queues and request immediate motor stop."""
        self._pending.clear()
        self._started = False
        self._last_command = int(CommandCode.DISABLE)
        return self._write_packet(MessageType.STOP)

    def set_j1_home_mode(self, mode: str) -> bool:
        normalized = mode.strip().upper()
        if normalized not in ("MANUAL", "AUTO"):
            raise ValueError("J1 home mode must be MANUAL or AUTO")
        return self._write_packet(
            MessageType.SET_J1_HOME_MODE,
            bytes([0 if normalized == "MANUAL" else 1]),
        )

    def poll_read(self) -> bool:
        if not self.is_connected() or self.serial is None:
            return False
        try:
            waiting = int(getattr(self.serial, "in_waiting", 0))
            if waiting <= 0:
                return False
            chunk = self.serial.read(min(waiting, 4096))
            observed = False
            for packet in self._decoder.feed(chunk):
                if packet.message_type is MessageType.STATUS:
                    self._publish_status(Status.decode(packet.payload))
                    observed = True
                elif packet.message_type is MessageType.ACK:
                    _, self._queue_depth, self._queue_capacity, _ = decode_ack(packet.payload)
                elif packet.message_type is MessageType.ERROR:
                    rejected, code, detail = decode_error(packet.payload)
                    logger.error("P6B1 firmware error %s for sequence %d (detail=%d)", code.name, rejected, detail)
                    if code in (ErrorCode.CAPABILITY_MISMATCH, ErrorCode.FAULT_LATCHED):
                        self._connected = False
            return observed
        except (serial.SerialException, OSError, ProtocolError) as exc:
            logger.error("P6B1 serial read failed: %s", exc)
            self.disconnect()
            return False

    @staticmethod
    def _put_i24(out: bytearray, offset: int, value: int) -> None:
        value = max(-0x800000, min(0x7FFFFF, int(value))) & 0xFFFFFF
        out[offset] = (value >> 16) & 0xFF
        out[offset + 1] = (value >> 8) & 0xFF
        out[offset + 2] = value & 0xFF

    def _publish_status(self, status: Status) -> None:
        out = self._latest_payload
        for index, value in enumerate(status.positions_steps):
            self._put_i24(out, index * 3, value)
        for index, value in enumerate(status.speeds_steps_s):
            self._put_i24(out, 18 + index * 3, value)
        out[36] = 0xFC if status.state & ControllerState.HOMED else 0
        out[37] = status.sensor_bits & 0xFF
        out[38] = 0xFF if status.faults & (Fault.WATCHDOG | Fault.ESTOP) else 0
        out[39] = 0xFF if status.faults else 0
        out[40] = 0
        out[41] = self._queue_depth & 0xFF
        for index in range(42, 52):
            out[index] = 0
        self._last_status = status
        self._frame_version += 1
        self._frame_ts = time.time()

    def get_latest_frame_view(self) -> tuple[memoryview | None, int, float]:
        if self._frame_version == 0:
            return None, 0, 0.0
        return self._latest_view, self._frame_version, self._frame_ts
