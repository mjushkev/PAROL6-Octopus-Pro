from __future__ import annotations

import numpy as np

from waldo_commander.services.urdf_scene.config import RobotAppearanceMode
from waldo_commander.services.urdf_scene.tcp_controls_mixin import TCPControlsMixin


class _Ball:
    def __init__(self) -> None:
        self.x = 0.0
        self.y = 0.0
        self.z = 0.0
        self.rotations: list[tuple[float, float, float]] = []
        self.spaces: list[str] = []

    def move(self, x: float, y: float, z: float) -> None:
        self.x, self.y, self.z = x, y, z

    def rotate(self, rx: float, ry: float, rz: float, *, order: str) -> None:
        assert order == "XYZ"
        self.rotations.append((rx, ry, rz))

    def set_transform_space(self, space: str) -> None:
        self.spaces.append(space)


class _Solver:
    @staticmethod
    def forward_kinematics(angles: list[float] | np.ndarray) -> np.ndarray:
        # Keep XYZ fixed so the test specifically covers a wrist-only rotation.
        return np.asarray([1.0, 2.0, 3.0, 0.0, 0.0, float(angles[5])])


class _Scene(TCPControlsMixin):
    pass


def _editing_scene() -> tuple[_Scene, _Ball]:
    scene = _Scene()
    scene._init_tcp_controls_state()
    scene.joint_names = [f"L{i}" for i in range(1, 7)]
    scene._appearance_mode = RobotAppearanceMode.EDITING
    scene._editing_angles = [0.0] * 6
    ball = _Ball()
    scene._tcp_ball = ball
    scene._ik_solver = _Solver()
    return scene, ball


def test_tcp_pointer_rotation_updates_when_position_is_unchanged() -> None:
    scene, ball = _editing_scene()
    scene._update_tcp_ball_position()
    assert ball.rotations[-1] == (0.0, 0.0, 0.0)

    scene._editing_angles[5] = 0.75
    scene._update_tcp_ball_position()

    assert (ball.x, ball.y, ball.z) == (1.0, 2.0, 3.0)
    assert ball.rotations[-1] == (0.0, 0.0, 0.75)


def test_tcp_pointer_space_is_retained_and_applied() -> None:
    scene, ball = _editing_scene()
    scene.scene = object()

    scene.set_tcp_transform_space("local")
    assert scene._tcp_transform_space == "local"
    assert ball.spaces == []

    scene._tcp_transform_enabled = True
    scene.set_tcp_transform_space("world")
    assert scene._tcp_transform_space == "world"
    assert ball.spaces == ["world"]
