from __future__ import annotations

import xml.etree.ElementTree as ET

import numpy as np

from parol6 import PAROL6_ROBOT
from parol6.client.dry_run_client import DryRunRobotClient
from parol6.hardware_profile import (
    COMMISSIONING_MAX_DEG_S,
    COMMISSIONING_MAX_DEG_S2,
    MODEL_ZERO_OFFSET_DEG,
    PROFILE,
    build_mapped_urdf,
)
from pinokin import Robot


def test_owner_profile_is_loaded_exactly() -> None:
    assert PROFILE.robot_id == "PAROL6-MATTJ-001"
    assert PROFILE.board == "BTT_OCTOPUS_PRO_V1_1_H723ZE"
    assert PROFILE.home_order == ("J1", "J2", "J3", "J4", "J6", "J5")
    assert PROFILE.j1_home_mode_default == "MANUAL"
    assert PROFILE.j1_auto_home_available
    np.testing.assert_allclose(
        PROFILE.limits_deg,
        [
            [-230.0, 35.0],
            [0.0, 119.536],
            [0.0, 90.329],
            [0.0, 232.694],
            [-254.25, 0.0],
            [-180.0, 180.0],
        ],
    )
    np.testing.assert_array_equal(PROFILE.pulses_per_degree, [114, 356, 161, 36, 36, 89])
    np.testing.assert_allclose(PROFILE.standby_deg, [0, 0, 0, 0, -130, 0])
    assert PROFILE.initial_speed_cap_percent == 80
    np.testing.assert_allclose(COMMISSIONING_MAX_DEG_S, [4, 1, 36, 36, 36, 36])
    np.testing.assert_allclose(COMMISSIONING_MAX_DEG_S2, [8, 2.5, 96, 96, 96, 96])


def test_effective_step_conversion_matches_measured_profile() -> None:
    effective_ppd = PAROL6_ROBOT.joint.ratio / PAROL6_ROBOT.degree_per_step_constant
    np.testing.assert_allclose(effective_ppd, PROFILE.pulses_per_degree, atol=1e-12)


def test_mapped_urdf_zero_matches_official_model_offsets() -> None:
    mapped_path = build_mapped_urdf(PAROL6_ROBOT._stock_urdf_path)
    mapped = Robot(mapped_path)
    official = Robot(PAROL6_ROBOT._stock_urdf_path)
    np.testing.assert_allclose(
        mapped.fkine(np.zeros(6)),
        official.fkine(np.deg2rad(MODEL_ZERO_OFFSET_DEG)),
        atol=2e-10,
    )


def test_mapped_urdf_exposes_owner_limits_and_never_continuous_j6() -> None:
    mapped_path = build_mapped_urdf(PAROL6_ROBOT._stock_urdf_path)
    root = ET.parse(mapped_path).getroot()
    joints = {node.get("name"): node for node in root.findall("joint")}
    limits = []
    for index in range(6):
        node = joints[f"L{index + 1}"]
        assert node.get("type") == "revolute"
        limit = node.find("limit")
        assert limit is not None
        limits.append([np.rad2deg(float(limit.get("lower"))), np.rad2deg(float(limit.get("upper")))])
    np.testing.assert_allclose(limits, PROFILE.limits_deg, atol=1e-9)


def test_owner_coordinate_movej_plans_end_to_end() -> None:
    target_deg = np.array([-5.0, 3.0, 2.0, 4.0, -135.0, 5.0])
    result = DryRunRobotClient().move_j(angles=target_deg.tolist(), speed=0.5, r=0)
    assert result is not None
    assert result.error is None
    assert result.duration > 0.0
    assert result.joint_trajectory_rad is not None
    np.testing.assert_allclose(result.joint_trajectory_rad[0], np.deg2rad(PROFILE.standby_deg))
    np.testing.assert_allclose(result.end_joints_rad, np.deg2rad(target_deg))
    assert result.tcp_poses.shape[1] == 6
