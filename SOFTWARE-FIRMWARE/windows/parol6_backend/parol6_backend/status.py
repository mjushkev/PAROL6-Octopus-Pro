from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum

from parol6_protocol import ControllerState

from .motion_config import ENCODER_INTEGRATION_ENABLED


class EncoderMode(str, Enum):
    DISABLED = "DISABLED"
    LIVE = "LIVE"
    IDLE_ONLY = "IDLE_ONLY"
    STALE = "STALE"
    UNAVAILABLE = "UNAVAILABLE"


@dataclass(slots=True)
class EncoderTelemetry:
    raw: int | None = None
    motor_deg: float | None = None
    estimated_joint_deg: float | None = None
    following_error_deg: float | None = None
    age_ms: float | None = None
    valid: bool = False
    mode: EncoderMode = EncoderMode.UNAVAILABLE


def _new_encoder_telemetry() -> EncoderTelemetry:
    """Keep the telemetry object available while making it inert by default."""

    mode = EncoderMode.UNAVAILABLE if ENCODER_INTEGRATION_ENABLED else EncoderMode.DISABLED
    return EncoderTelemetry(mode=mode)


@dataclass(slots=True)
class ControllerStatus:
    commanded_joint_deg: list[float] = field(default_factory=lambda: [0.0] * 6)
    commanded_joint_speed_rad_s: list[float] = field(default_factory=lambda: [0.0] * 6)
    encoders: list[EncoderTelemetry] = field(
        default_factory=lambda: [_new_encoder_telemetry(), _new_encoder_telemetry()]
    )
    controller_state: ControllerState = ControllerState.BOOT_SELF_TEST
    motor_power_requested: bool = False
    motor_power_verified: bool = False
    queue_points: int = 0
    queue_horizon_ms: int = 0
    fault_code: str | None = None

    @property
    def angles(self) -> list[float]:
        """Backward-compatible primary field: commanded output-joint degrees."""

        return list(self.commanded_joint_deg)
