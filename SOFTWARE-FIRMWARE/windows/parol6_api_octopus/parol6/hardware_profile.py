"""Owner-selected Octopus Pro hardware and calibration profile.

The measured joint angles are the public Commander coordinate system.  The
official PAROL6 URDF uses different zeros on several axes, so this module also
creates a mapped URDF for visualization and kinematics.  The mapping is derived
from the official homing offsets and remains simulation-only until validated
against measured physical TCP poses.
"""

from __future__ import annotations

import hashlib
import json
import os
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from scipy.spatial.transform import Rotation

PROFILE_SCHEMA = "parol6.production-calibration.v1"
EXPECTED_ROBOT_ID = "PAROL6-MATTJ-001"
EXPECTED_BOARD = "BTT_OCTOPUS_PRO_V1_1_H723ZE"
BUNDLED_PROFILE_SHA256 = "5eb94dc1c1e3e0693488b989b6d1ac021f8c9ed31dd3710330f9879b0577a584"

# q_urdf = MODEL_SIGN * q_owner + MODEL_ZERO_OFFSET_DEG
# J1 assumes the owner-selected manual zero is the standard 90-degree standby.
# J2/J3 use the stock lower switch coordinates. J4/J5/J6 are derived from the
# official post-latch homing offsets and stock standby coordinates.
MODEL_SIGN = np.array([1.0, 1.0, 1.0, -1.0, -1.0, -1.0], dtype=np.float64)
MODEL_ZERO_OFFSET_DEG = np.array(
    [90.0, -145.0088, 107.866, 143.4375, -125.15625, 90.5625],
    dtype=np.float64,
)
MODEL_MAPPING_STATUS = "derived_pending_physical_pose_validation"

# Owner-selected 50% commissioning stage. J1/J2 remain capped at their
# separately proven Servo42C pulse rates; J3-J6 use 50% of the reviewed rates.
COMMISSIONING_MAX_DEG_S = np.array([4.0, 1.0, 22.5, 22.5, 22.5, 22.5])
COMMISSIONING_MAX_DEG_S2 = np.array([8.0, 2.5, 60.0, 60.0, 60.0, 60.0])


@dataclass(frozen=True)
class JointProfile:
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


@dataclass(frozen=True)
class HardwareProfile:
    path: Path
    robot_id: str
    board: str
    device_uid: str
    j1_home_mode_default: str
    j1_auto_home_available: bool
    home_order: tuple[str, ...]
    joints: tuple[JointProfile, ...]

    @property
    def limits_deg(self) -> np.ndarray:
        return np.asarray(
            [[joint.minimum_deg, joint.maximum_deg] for joint in self.joints],
            dtype=np.float64,
        )

    @property
    def pulses_per_degree(self) -> np.ndarray:
        return np.asarray([joint.pulses_per_degree for joint in self.joints], dtype=np.float64)

    @property
    def standby_deg(self) -> np.ndarray:
        result = np.zeros(6, dtype=np.float64)
        for index, joint in enumerate(self.joints):
            if joint.post_home_standby_deg is not None:
                result[index] = joint.post_home_standby_deg
        return result


def _bundled_profile_path() -> Path:
    return Path(__file__).resolve().parent / "profiles" / "robot.mattj.calibrated.json"


def _profile_path() -> Path:
    configured = os.getenv("PAROL6_HARDWARE_PROFILE", "").strip()
    if configured:
        return Path(configured).expanduser().resolve()
    return _bundled_profile_path()


def load_profile(path: Path | None = None) -> HardwareProfile:
    source = (path or _profile_path()).resolve()
    raw_bytes = source.read_bytes()
    data = json.loads(raw_bytes)
    if data.get("schema") != PROFILE_SCHEMA:
        raise ValueError(f"unsupported PAROL6 profile schema in {source}")
    if data.get("robot_id") != EXPECTED_ROBOT_ID:
        raise ValueError(f"wrong robot profile: expected {EXPECTED_ROBOT_ID}")
    controller = data.get("controller", {})
    if controller.get("board") != EXPECTED_BOARD:
        raise ValueError(f"wrong controller board in {source}")
    if source == _bundled_profile_path() and hashlib.sha256(raw_bytes).hexdigest() != BUNDLED_PROFILE_SHA256:
        raise ValueError("bundled owner profile checksum mismatch")

    raw_joints = data.get("joints", [])
    if [joint.get("joint") for joint in raw_joints] != [f"J{i}" for i in range(1, 7)]:
        raise ValueError("profile must contain J1 through J6 in order")
    joints = tuple(JointProfile(**joint) for joint in raw_joints)
    for joint in joints:
        if joint.minimum_deg >= joint.maximum_deg:
            raise ValueError(f"{joint.joint} minimum must be less than maximum")
        if joint.pulses_per_degree <= 0:
            raise ValueError(f"{joint.joint} pulses_per_degree must be positive")
    if joints[5].minimum_deg != -180.0 or joints[5].maximum_deg != 180.0:
        raise ValueError("J6 must remain cable-limited to -180..180 degrees")

    motion = data.get("motion_model", {})
    return HardwareProfile(
        path=source,
        robot_id=data["robot_id"],
        board=controller["board"],
        device_uid=controller.get("device_uid", ""),
        j1_home_mode_default=motion.get("j1_home_mode_default", "MANUAL"),
        j1_auto_home_available=bool(motion.get("j1_auto_home_available", False)),
        home_order=tuple(motion.get("home_order", [])),
        joints=joints,
    )


PROFILE = load_profile()


def owner_to_model_rad(q_owner_rad: np.ndarray) -> np.ndarray:
    return MODEL_SIGN * np.asarray(q_owner_rad, dtype=np.float64) + np.deg2rad(MODEL_ZERO_OFFSET_DEG)


def model_to_owner_rad(q_model_rad: np.ndarray) -> np.ndarray:
    return MODEL_SIGN * (np.asarray(q_model_rad, dtype=np.float64) - np.deg2rad(MODEL_ZERO_OFFSET_DEG))


def _numbers(text: str | None) -> np.ndarray:
    return np.asarray([float(value) for value in (text or "0 0 0").split()], dtype=float)


def build_mapped_urdf(source_path: str | Path) -> str:
    """Create the owner-coordinate URDF in the user runtime cache."""
    source = Path(source_path).resolve()
    cache_root = Path(os.getenv("PAROL6_RUNTIME_CACHE", Path.home() / ".cache" / "parol6-waldo"))
    cache_root.mkdir(parents=True, exist_ok=True)
    target = cache_root / "PAROL6_MATTJ.urdf"
    source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
    profile_hash = hashlib.sha256(PROFILE.path.read_bytes()).hexdigest()
    marker = f"source={source_hash};profile={profile_hash};mapping=v1"
    if target.exists() and marker in target.read_text(encoding="utf-8", errors="ignore"):
        return str(target)

    tree = ET.parse(source)
    root = tree.getroot()
    root.set("name", PROFILE.robot_id)
    root.insert(0, ET.Comment(f"Generated owner-coordinate model; {marker}; {MODEL_MAPPING_STATUS}"))
    by_name = {joint.get("name"): joint for joint in root.findall("joint")}
    for index in range(6):
        node = by_name[f"L{index + 1}"]
        origin = node.find("origin")
        axis = node.find("axis")
        limit = node.find("limit")
        if origin is None or axis is None or limit is None:
            raise ValueError(f"official URDF joint L{index + 1} is incomplete")
        xyz = _numbers(origin.get("xyz"))
        rpy = _numbers(origin.get("rpy"))
        axis_values = _numbers(axis.get("xyz"))
        base_rotation = Rotation.from_euler("xyz", rpy).as_matrix()
        offset_rotation = Rotation.from_rotvec(
            axis_values / np.linalg.norm(axis_values) * np.deg2rad(MODEL_ZERO_OFFSET_DEG[index])
        ).as_matrix()
        mapped_rpy = Rotation.from_matrix(base_rotation @ offset_rotation).as_euler("xyz")
        origin.set("xyz", " ".join(f"{value:.12g}" for value in xyz))
        origin.set("rpy", " ".join(f"{value:.12g}" for value in mapped_rpy))
        axis.set("xyz", " ".join(f"{value * MODEL_SIGN[index]:.12g}" for value in axis_values))
        limit.set("lower", f"{np.deg2rad(PROFILE.joints[index].minimum_deg):.12g}")
        limit.set("upper", f"{np.deg2rad(PROFILE.joints[index].maximum_deg):.12g}")
    tree.write(target, encoding="utf-8", xml_declaration=True)
    return str(target)
