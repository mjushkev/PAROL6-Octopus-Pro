"""Deterministic fake ESP/MCU path. It has no hardware I/O dependencies."""

from __future__ import annotations

from dataclasses import dataclass
import json

from parol6_protocol import (
    ControllerState,
    ErrorCode,
    Frame,
    FrameError,
    MessageType,
    ReplayDecision,
    ReplayWindow,
    decode_uart,
    encode_uart,
)

from .safety import SafetySupervisor
from .status import ControllerStatus
from .trajectory import TrajectoryBuffer, TrajectoryError, TrajectoryPoint


@dataclass(frozen=True, slots=True)
class LinkProfile:
    drop_every: int = 0
    duplicate_every: int = 0
    corrupt_every: int = 0

    def __post_init__(self) -> None:
        for name in ("drop_every", "duplicate_every", "corrupt_every"):
            if getattr(self, name) < 0:
                raise ValueError(f"{name} cannot be negative")


class FakeMCU:
    def __init__(self, *, commissioned: bool = False) -> None:
        self.safety = SafetySupervisor(commissioned=commissioned)
        self.safety.finish_boot()
        self._windows: dict[int, ReplayWindow] = {}
        self.status = ControllerStatus(controller_state=self.safety.state)
        self.executed_trajectory_ids: set[int] = set()
        self.current_steps = (10_240, -32_000, 57_905, 0, 0, 32_000)
        self.trajectory = TrajectoryBuffer()

    def connect(self, session_id: int) -> None:
        self.safety.connect_status_only(session_id)
        self._windows[session_id] = ReplayWindow()
        self._sync_status()

    def receive(self, packet: bytes) -> bytes:
        try:
            request = decode_uart(packet)
        except FrameError as exc:
            return encode_uart(self._nack(0, 0, ErrorCode.MALFORMED, exc.reason))
        window = self._windows.get(request.session_id)
        if window is None:
            return encode_uart(
                self._nack(request.session_id, request.sequence, ErrorCode.NOT_AUTHENTICATED, "session")
            )
        decision = window.check_and_mark(request.sequence)
        if decision is not ReplayDecision.ACCEPT:
            return encode_uart(
                self._nack(request.session_id, request.sequence, ErrorCode.REPLAY, decision.value)
            )
        response = self._handle(request)
        self._sync_status()
        return encode_uart(response)

    def _handle(self, request: Frame) -> Frame:
        try:
            message = MessageType(request.message_type)
        except ValueError:
            return self._nack(
                request.session_id, request.sequence, ErrorCode.MALFORMED, "unknown_message_type"
            )
        error = ErrorCode.OK
        detail = "ok"
        if message is MessageType.TAKE_CONTROL:
            if not self.safety.take_control(request.session_id):
                error, detail = ErrorCode.NO_CONTROL_LEASE, "lease_owned"
        elif message is MessageType.RELEASE_CONTROL:
            self.safety.disconnect()
        elif message is MessageType.MOTOR_ENABLE:
            if not self.safety.request_motor_enable(request.session_id):
                error = (
                    ErrorCode.NOT_COMMISSIONED
                    if not self.safety.commissioned
                    else ErrorCode.INVALID_STATE
                )
                detail = self.safety.state.name
        elif message is MessageType.MOTOR_OFF:
            self.safety.motor_off()
        elif message is MessageType.HOME_START:
            if not self.safety.start_homing(request.session_id):
                error, detail = ErrorCode.INVALID_STATE, self.safety.state.name
        elif message is MessageType.TRAJECTORY_BEGIN:
            if len(request.payload) != 8:
                error, detail = ErrorCode.MALFORMED, "trajectory_id_requires_u64"
            else:
                trajectory_id = int.from_bytes(request.payload, "little")
                try:
                    self.trajectory.begin(trajectory_id, self.current_steps)
                except TrajectoryError as exc:
                    error, detail = ErrorCode.OUT_OF_RANGE, str(exc)
        elif message is MessageType.TRAJECTORY_POINTS:
            try:
                document = json.loads(request.payload)
                for item in document["points"]:
                    point = TrajectoryPoint(
                        index=int(item["index"]),
                        duration_ms=int(item["duration_ms"]),
                        target_steps=tuple(int(value) for value in item["target_steps"]),
                    )
                    if len(point.target_steps) != 6:
                        raise TrajectoryError("target_steps_requires_six_axes")
                    self.trajectory.append(point)
            except (KeyError, TypeError, ValueError, json.JSONDecodeError, TrajectoryError) as exc:
                error, detail = ErrorCode.OUT_OF_RANGE, str(exc)
        elif message is MessageType.TRAJECTORY_COMMIT:
            if len(request.payload) != 8:
                error, detail = ErrorCode.MALFORMED, "trajectory_id_requires_u64"
            else:
                trajectory_id = int.from_bytes(request.payload, "little")
                if trajectory_id in self.executed_trajectory_ids:
                    error, detail = ErrorCode.REPLAY, "trajectory_already_executed"
                else:
                    try:
                        self.trajectory.validate_commit(trajectory_id)
                    except TrajectoryError as exc:
                        error, detail = ErrorCode.OUT_OF_RANGE, str(exc)
                    else:
                        if not self.safety.start_execution(request.session_id):
                            error, detail = ErrorCode.INVALID_STATE, self.safety.state.name
                        else:
                            self.executed_trajectory_ids.add(trajectory_id)
                            self.current_steps = self.trajectory.points[-1].target_steps
                            self.status.queue_points = len(self.trajectory.points)
                            self.status.queue_horizon_ms = self.trajectory.horizon_ms
                            self.trajectory.clear()
        elif message is MessageType.HEARTBEAT:
            return self._reply(request, MessageType.HEARTBEAT_REPLY, b"")
        elif message not in (MessageType.HELLO, MessageType.GET_DEVICE_INFO):
            error, detail = ErrorCode.INVALID_STATE, "not_implemented_in_simulator"
        if error is not ErrorCode.OK:
            return self._nack(request.session_id, request.sequence, error, detail)
        payload = json.dumps({"ok": True, "state": self.safety.state.name}).encode("utf-8")
        return self._reply(request, MessageType.EVENT, payload)

    def _reply(self, request: Frame, message_type: MessageType, payload: bytes) -> Frame:
        return Frame(
            message_type=message_type,
            payload=payload,
            session_id=request.session_id,
            sequence=request.sequence,
            acknowledgement=request.sequence,
            sender_time_us=request.sender_time_us,
        )

    def _nack(
        self, session_id: int, acknowledgement: int, error: ErrorCode, detail: str
    ) -> Frame:
        payload = json.dumps(
            {"error": error.name, "detail": detail, "retry": False},
            sort_keys=True,
        ).encode("utf-8")
        return Frame(
            message_type=MessageType.NACK,
            payload=payload,
            session_id=session_id,
            sequence=acknowledgement,
            acknowledgement=acknowledgement,
        )

    def _sync_status(self) -> None:
        self.status.controller_state = self.safety.state
        self.status.motor_power_requested = self.safety.outputs.contactor_request
        self.status.motor_power_verified = self.safety.motor_power_verified
        if self.safety.state is not ControllerState.EXECUTING:
            self.status.queue_points = 0
            self.status.queue_horizon_ms = 0
        self.status.fault_code = self.safety.fault_code


class FakeESPBridge:
    def __init__(self, mcu: FakeMCU, profile: LinkProfile | None = None) -> None:
        self.mcu = mcu
        self.profile = profile or LinkProfile()
        self.forward_count = 0
        self.drop_count = 0
        self.corrupt_count = 0
        self.duplicate_count = 0

    def forward(self, packet: bytes) -> list[bytes]:
        self.forward_count += 1
        n = self.forward_count
        if self.profile.drop_every and n % self.profile.drop_every == 0:
            self.drop_count += 1
            return []
        forwarded = packet
        if self.profile.corrupt_every and n % self.profile.corrupt_every == 0:
            self.corrupt_count += 1
            mutable = bytearray(packet)
            if len(mutable) > 2:
                mutable[len(mutable) // 2] ^= 0x01
            forwarded = bytes(mutable)
        responses = [self.mcu.receive(forwarded)]
        if self.profile.duplicate_every and n % self.profile.duplicate_every == 0:
            self.duplicate_count += 1
            responses.append(self.mcu.receive(forwarded))
        return responses


class SimulationClient:
    def __init__(self, bridge: FakeESPBridge, session_id: int = 1) -> None:
        self.bridge = bridge
        self.session_id = session_id
        self.sequence = 0
        self.bridge.mcu.connect(session_id)

    def send(self, message: MessageType, payload: bytes = b"") -> list[Frame]:
        self.sequence += 1
        request = Frame(
            message_type=message,
            payload=payload,
            session_id=self.session_id,
            sequence=self.sequence,
            sender_time_us=self.sequence * 10_000,
        )
        return [decode_uart(packet) for packet in self.bridge.forward(encode_uart(request))]
