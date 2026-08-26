from __future__ import annotations

from dataclasses import dataclass, field
import re
from typing import Any, Callable


SUPPORTED_FIRMWARE = "0.9.1-motion-rc"
TOKEN_RE = re.compile(r"\btoken=([0-9A-F]{8})\b")
POSITION_RE = re.compile(r"\b(J[1-6])_mdeg=(-?[0-9]+)\b")
HOMED_RE = re.compile(r"\b(J[1-6])_homed=([01])\b")


class DirectUsbError(RuntimeError):
    pass


@dataclass(slots=True)
class MotionRcSnapshot:
    connected: bool = False
    firmware: str | None = None
    token: str | None = None
    positions_deg: dict[str, float] = field(
        default_factory=lambda: {f"J{index}": 0.0 for index in range(1, 7)}
    )
    homed: dict[str, bool] = field(
        default_factory=lambda: {f"J{index}": False for index in range(1, 7)}
    )
    moving: bool = False
    held: str = "NONE"
    fault: str | None = None
    controller_epoch: int = 0

    @property
    def firmware_ready(self) -> bool:
        return self.firmware == SUPPORTED_FIRMWARE


class MotionRcLineParser:
    """Deterministic parser for the deployed 0.9.1 USB line protocol."""

    def __init__(self) -> None:
        self.snapshot = MotionRcSnapshot()

    def connected(self) -> None:
        self.snapshot.connected = True
        self.snapshot.fault = None

    def disconnected(self) -> None:
        epoch = self.snapshot.controller_epoch + 1
        self.snapshot = MotionRcSnapshot(controller_epoch=epoch)

    def feed(self, line: str) -> MotionRcSnapshot:
        line = line.strip()
        if not line:
            return self.snapshot
        token = TOKEN_RE.search(line)
        if token:
            self.snapshot.token = token.group(1)
        if line.startswith("PAROL6_MOTION_RC_READY"):
            version = re.search(r"\bversion=([^ ]+)", line)
            self.snapshot.firmware = version.group(1) if version else None
            self.snapshot.connected = True
            self.snapshot.moving = False
            self.snapshot.held = "NONE"
            self.snapshot.fault = None
        if line.startswith("PAROL6_STATUS"):
            for joint, value in POSITION_RE.findall(line):
                self.snapshot.positions_deg[joint] = int(value) / 1_000
            for joint, value in HOMED_RE.findall(line):
                self.snapshot.homed[joint] = value == "1"
            moving = re.search(r"\bmoving=([01])\b", line)
            held = re.search(r"\bheld=([^ ]+)", line)
            if moving:
                self.snapshot.moving = moving.group(1) == "1"
            if held:
                self.snapshot.held = held.group(1)
        elif line.startswith(("PAROL6_HOME_STARTED", "PAROL6_MOTION_STARTED", "PAROL6_HOLD_STARTED", "PAROL6_COORDINATED_STARTED")):
            self.snapshot.moving = True
            self.snapshot.held = "NONE"
            self.snapshot.fault = None
        elif line.startswith("PAROL6_COORDINATED_DONE"):
            self.snapshot.moving = False
            self.snapshot.held = "ALL" if "result=complete" in line and "hold=1" in line else "NONE"
            for joint, value in POSITION_RE.findall(line):
                self.snapshot.positions_deg[joint] = int(value) / 1_000
            if "result=complete" not in line:
                self.snapshot.fault = self._result(line)
        elif line.startswith("PAROL6_HOME joint="):
            self.snapshot.moving = False
            joint = re.search(r"\bjoint=(J[1-6])", line)
            if joint and "result=complete" in line:
                self.snapshot.homed[joint.group(1)] = True
            elif "result=complete" not in line:
                self.snapshot.fault = self._result(line)
        elif line.startswith("PAROL6_COORDINATED_HOLD_RELEASED"):
            self.snapshot.held = "NONE"
        elif line.startswith("PAROL6_STOPPED"):
            self.snapshot.moving = False
            self.snapshot.held = "NONE"
            self.snapshot.fault = "operator_stop"
        elif line.startswith("PAROL6_ERROR"):
            code = re.search(r"\bcode=([^ ]+)", line)
            self.snapshot.moving = False
            self.snapshot.held = "NONE"
            self.snapshot.fault = code.group(1) if code else "unknown_error"
        return self.snapshot

    @staticmethod
    def _result(line: str) -> str:
        result = re.search(r"\bresult=([^ ]+)", line)
        return result.group(1) if result else "unknown_result"


class DirectUsbTransport:
    """Small pyserial adapter; serial is imported only for real hardware use."""

    def __init__(
        self,
        port: str,
        *,
        baudrate: int = 3_000_000,
        serial_factory: Callable[..., Any] | None = None,
    ) -> None:
        self.port = port
        self.baudrate = baudrate
        self.serial_factory = serial_factory
        self.serial: Any | None = None

    def connect(self) -> None:
        if self.serial is not None:
            raise DirectUsbError("usb_already_connected")
        factory = self.serial_factory
        if factory is None:
            try:
                from serial import Serial  # type: ignore[import-not-found]
            except ImportError as exc:
                raise DirectUsbError("pyserial_not_installed") from exc
            factory = Serial
        self.serial = factory(
            self.port,
            self.baudrate,
            timeout=0.2,
            write_timeout=0.2,
        )

    def send(self, command: str) -> None:
        if self.serial is None:
            raise DirectUsbError("usb_not_connected")
        if not command or "\n" in command or "\r" in command or len(command) > 127:
            raise DirectUsbError("invalid_usb_command")
        self.serial.write((command + "\n").encode("ascii"))
        self.serial.flush()

    def read_line(self) -> str | None:
        if self.serial is None:
            raise DirectUsbError("usb_not_connected")
        raw = self.serial.readline()
        if not raw:
            return None
        try:
            return raw.decode("ascii").strip()
        except UnicodeDecodeError as exc:
            raise DirectUsbError("non_ascii_controller_response") from exc

    def disconnect(self) -> None:
        serial = self.serial
        self.serial = None
        if serial is None:
            return
        try:
            serial.write(b"STOP\n")
            serial.flush()
        finally:
            serial.close()
