"""Deterministic P6B1 controller simulator used for safety/fault testing."""

from __future__ import annotations

from collections import deque

from .octopus_buffered import (
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
    encode_ack,
    encode_error,
)


class BufferedControllerSimulator:
    """Small state machine mirroring the safety-critical firmware contract."""

    def __init__(self, *, queue_capacity: int = 128, watchdog_ms: int = 250, profile_crc32c: int = 0) -> None:
        self.queue_capacity = queue_capacity
        self.watchdog_ms = watchdog_ms
        self.profile_crc32c = profile_crc32c
        self.capabilities = REQUIRED_CAPABILITIES
        self.decoder = StreamDecoder()
        self.queue: deque[Setpoint] = deque()
        self.state = ControllerState.IDLE
        self.faults = Fault.NONE
        self.positions = (0, 0, 0, 0, 0, 0)
        self.speeds = (0, 0, 0, 0, 0, 0)
        self.sensor_bits = 0
        self.io_bits = 0
        self.last_sequence: int | None = None
        self.last_host_activity_ms = 0
        self.now_ms = 0
        self.period_us = 10_000
        self._sequence_out = 1
        self.handshake_complete = False
        self.finish_requested = False

    def _packet(self, message_type: MessageType, payload: bytes = b"") -> bytes:
        packet = Packet(message_type, self._sequence_out, payload)
        self._sequence_out = (self._sequence_out + 1) & 0xFFFFFFFF
        return packet.encode()

    def _error(self, request: Packet, code: ErrorCode, detail: int = 0) -> bytes:
        return self._packet(MessageType.ERROR, encode_error(request.sequence, code, detail))

    def _ack(self, request: Packet) -> bytes:
        payload = encode_ack(request.sequence, len(self.queue), self.queue_capacity, self.state)
        return self._packet(MessageType.ACK, payload)

    def receive(self, data: bytes | bytearray | memoryview) -> list[bytes]:
        """Process arbitrary serial chunks and return encoded response frames."""
        before_bad = self.decoder.bad_frames
        packets = self.decoder.feed(data)
        if self.decoder.bad_frames != before_bad:
            self.faults |= Fault.BAD_FRAME
        return [self._handle(packet) for packet in packets]

    def _handle(self, packet: Packet) -> bytes:
        self.last_host_activity_ms = self.now_ms

        # STOP is accepted before handshake, during faults, and if replayed.
        if packet.message_type is MessageType.STOP:
            self.queue.clear()
            self.finish_requested = False
            self.speeds = (0, 0, 0, 0, 0, 0)
            self.state = ControllerState.STOPPED
            self.last_sequence = packet.sequence
            return self._ack(packet)

        if packet.message_type is MessageType.HELLO:
            try:
                hello = Hello.decode(packet.payload)
            except ProtocolError:
                return self._error(packet, ErrorCode.INVALID_PAYLOAD)
            missing = REQUIRED_CAPABILITIES & ~hello.capabilities
            if missing or hello.profile_crc32c != self.profile_crc32c:
                self.faults |= Fault.CAPABILITY_MISMATCH
                self.state = ControllerState.FAULT
                return self._error(packet, ErrorCode.CAPABILITY_MISMATCH, int(missing) & 0xFFFF)
            self.handshake_complete = True
            self.last_sequence = packet.sequence
            response = Hello(self.capabilities, self.queue_capacity, self.watchdog_ms, self.profile_crc32c)
            return self._packet(MessageType.HELLO_ACK, response.encode())

        if not self.handshake_complete:
            return self._error(packet, ErrorCode.CAPABILITY_MISMATCH)
        if self.last_sequence is not None and packet.sequence <= self.last_sequence:
            self.faults |= Fault.REPLAY
            return self._error(packet, ErrorCode.REPLAY)
        self.last_sequence = packet.sequence

        if packet.message_type is MessageType.CLEAR:
            self.queue.clear()
            self.finish_requested = False
            self.faults = Fault.NONE
            self.state = ControllerState.IDLE
            return self._ack(packet)
        if self.faults:
            return self._error(packet, ErrorCode.FAULT_LATCHED)

        if packet.message_type is MessageType.ENQUEUE:
            try:
                batch = SetpointBatch.decode(packet.payload)
            except ProtocolError:
                self.faults |= Fault.BAD_FRAME
                self.state = ControllerState.FAULT
                return self._error(packet, ErrorCode.INVALID_PAYLOAD)
            if len(self.queue) + len(batch.setpoints) > self.queue_capacity:
                self.faults |= Fault.QUEUE_OVERFLOW
                self.state = ControllerState.FAULT
                return self._error(packet, ErrorCode.QUEUE_OVERFLOW)
            self.period_us = batch.period_us
            self.queue.extend(batch.setpoints)
            self.state = ControllerState.ARMED | ControllerState.MOTORS_ENABLED
            return self._ack(packet)

        if packet.message_type is MessageType.START:
            if not self.queue:
                return self._error(packet, ErrorCode.NOT_ARMED)
            self.state = ControllerState.RUNNING | ControllerState.MOTORS_ENABLED
            self.finish_requested = False
            return self._ack(packet)
        if packet.message_type is MessageType.FINISH:
            if not self.state & (ControllerState.ARMED | ControllerState.RUNNING):
                return self._error(packet, ErrorCode.NOT_ARMED)
            self.finish_requested = True
            return self._ack(packet)
        if packet.message_type in (MessageType.IO, MessageType.SET_J1_HOME_MODE):
            return self._ack(packet)
        return self._error(packet, ErrorCode.INVALID_PAYLOAD)

    def advance(self, elapsed_ms: int) -> None:
        """Advance simulated firmware time and execute queued setpoints."""
        if elapsed_ms < 0:
            raise ValueError("elapsed_ms cannot be negative")
        target = self.now_ms + elapsed_ms
        step_ms = max(1, self.period_us // 1000)
        while self.now_ms + step_ms <= target:
            self.now_ms += step_ms
            if self.state & ControllerState.RUNNING:
                if self.queue:
                    point = self.queue.popleft()
                    self.positions = point.positions_steps
                    self.speeds = point.speeds_steps_s
                    self.io_bits = point.io_bits
                elif self.finish_requested:
                    self.finish_requested = False
                    self.speeds = (0, 0, 0, 0, 0, 0)
                    self.state = ControllerState.IDLE | ControllerState.MOTORS_ENABLED
                else:
                    self.faults |= Fault.QUEUE_UNDERRUN
                    self.speeds = (0, 0, 0, 0, 0, 0)
                    self.state = ControllerState.FAULT
            if (
                self.state & (ControllerState.ARMED | ControllerState.RUNNING)
                and self.now_ms - self.last_host_activity_ms > self.watchdog_ms
            ):
                self.queue.clear()
                self.speeds = (0, 0, 0, 0, 0, 0)
                self.faults |= Fault.WATCHDOG
                self.state = ControllerState.FAULT
        self.now_ms = target

    def status(self) -> Status:
        return Status(
            self.last_sequence or 0,
            len(self.queue), self.queue_capacity, self.state, self.faults,
            self.positions, self.speeds, self.sensor_bits, self.io_bits,
        )
