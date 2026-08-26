from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
import json
from pathlib import Path
from typing import Any


class CalibrationError(ValueError):
    pass


class J1HomeMode(str, Enum):
    MANUAL = "MANUAL"
    AUTO = "AUTO"


@dataclass(frozen=True, slots=True)
class JointCalibration:
    joint: str
    name: str
    motor_slot: str
    driver: str
    pulses_per_degree: int
    home_raw: str
    positive_raw: str
    sensor_active_level: str
    home_behavior: str
    minimum_deg: float
    maximum_deg: float
    post_home_standby_deg: float | None = None

    def validate_angle(self, angle_deg: float) -> None:
        if not self.minimum_deg <= angle_deg <= self.maximum_deg:
            raise CalibrationError(
                f"{self.joint}_angle_out_of_range:{angle_deg}:"
                f"{self.minimum_deg}:{self.maximum_deg}"
            )

    def angle_to_raw_steps(self, angle_deg: float) -> int:
        self.validate_angle(angle_deg)
        logical_steps = round(angle_deg * self.pulses_per_degree)
        return logical_steps if self.positive_raw == "+" else -logical_steps

    def raw_steps_to_angle(self, steps: int) -> float:
        logical_steps = steps if self.positive_raw == "+" else -steps
        return logical_steps / self.pulses_per_degree

    @property
    def raw_step_limits(self) -> tuple[int, int]:
        endpoints = (
            self.angle_to_raw_steps(self.minimum_deg),
            self.angle_to_raw_steps(self.maximum_deg),
        )
        return min(endpoints), max(endpoints)


@dataclass(frozen=True, slots=True)
class RobotCalibration:
    schema: str
    robot_id: str
    device_uid: str
    source_flash_sequence: int
    validated_through_firmware: str
    initial_speed_cap_percent: int
    j1_home_mode_default: J1HomeMode
    j1_auto_home_available: bool
    home_order: tuple[str, ...]
    joints: tuple[JointCalibration, ...]

    @property
    def by_id(self) -> dict[str, JointCalibration]:
        return {joint.joint: joint for joint in self.joints}

    def validate_pose(self, pose_deg: tuple[float, float, float, float, float, float]) -> None:
        if len(pose_deg) != 6:
            raise CalibrationError("pose_requires_six_joints")
        for joint, angle in zip(self.joints, pose_deg, strict=True):
            joint.validate_angle(float(angle))

    def pose_to_raw_steps(
        self, pose_deg: tuple[float, float, float, float, float, float]
    ) -> tuple[int, int, int, int, int, int]:
        self.validate_pose(pose_deg)
        return tuple(
            joint.angle_to_raw_steps(float(angle))
            for joint, angle in zip(self.joints, pose_deg, strict=True)
        )  # type: ignore[return-value]


def _required(document: dict[str, Any], key: str) -> Any:
    if key not in document:
        raise CalibrationError(f"missing_{key}")
    return document[key]


def load_calibration(path: str | Path) -> RobotCalibration:
    document = json.loads(Path(path).read_text(encoding="utf-8"))
    if document.get("schema") != "parol6.production-calibration.v1":
        raise CalibrationError("unsupported_calibration_schema")
    controller = _required(document, "controller")
    motion = _required(document, "motion_model")
    raw_joints = _required(document, "joints")
    if not isinstance(raw_joints, list) or len(raw_joints) != 6:
        raise CalibrationError("calibration_requires_six_joints")
    joints = tuple(JointCalibration(**item) for item in raw_joints)
    expected_ids = tuple(f"J{index}" for index in range(1, 7))
    if tuple(joint.joint for joint in joints) != expected_ids:
        raise CalibrationError("joint_order_must_be_J1_through_J6")
    for joint in joints:
        if joint.pulses_per_degree <= 0:
            raise CalibrationError(f"{joint.joint}_invalid_pulses_per_degree")
        if joint.home_raw not in ("+", "-") or joint.positive_raw not in ("+", "-"):
            raise CalibrationError(f"{joint.joint}_invalid_direction")
        if joint.sensor_active_level not in ("LOW", "HIGH"):
            raise CalibrationError(f"{joint.joint}_invalid_sensor_level")
        if joint.minimum_deg >= joint.maximum_deg:
            raise CalibrationError(f"{joint.joint}_invalid_limits")
        if joint.post_home_standby_deg is not None:
            joint.validate_angle(joint.post_home_standby_deg)
    home_order = tuple(motion["home_order"])
    if set(home_order) != set(expected_ids) or len(home_order) != 6:
        raise CalibrationError("home_order_requires_each_joint_once")
    speed_cap = int(motion["initial_speed_cap_percent"])
    if not 1 <= speed_cap <= 100:
        raise CalibrationError("invalid_initial_speed_cap")
    mode = J1HomeMode(motion["j1_home_mode_default"])
    return RobotCalibration(
        schema=document["schema"],
        robot_id=str(_required(document, "robot_id")),
        device_uid=str(controller["device_uid"]),
        source_flash_sequence=int(controller["source_flash_sequence"]),
        validated_through_firmware=str(controller["validated_through_firmware"]),
        initial_speed_cap_percent=speed_cap,
        j1_home_mode_default=mode,
        j1_auto_home_available=bool(motion["j1_auto_home_available"]),
        home_order=home_order,
        joints=joints,
    )


def default_calibration_path() -> Path:
    return Path(__file__).resolve().parents[3] / "config" / "robot.mattj.calibrated.json"


def load_default_calibration() -> RobotCalibration:
    return load_calibration(default_calibration_path())
