"""Portable implementation of Pinokin's public kinematics API.

This module is used when Windows application policy cannot load the upstream
unsigned extension.  FK follows URDF semantics, Jacobians are evaluated with
central differences, and IK uses SciPy's bounded least-squares solver.  The
collision class deliberately refuses to start: physical execution must never
silently lose collision checking.
"""

from __future__ import annotations

import enum
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from scipy.optimize import least_squares
from scipy.spatial.transform import Rotation


def _values(text: str | None, default: tuple[float, ...]) -> np.ndarray:
    if not text:
        return np.asarray(default, dtype=float)
    return np.asarray([float(value) for value in text.split()], dtype=float)


def _origin_matrix(node: ET.Element | None) -> np.ndarray:
    matrix = np.eye(4, dtype=float)
    if node is None:
        return matrix
    matrix[:3, 3] = _values(node.get("xyz"), (0.0, 0.0, 0.0))
    roll, pitch, yaw = _values(node.get("rpy"), (0.0, 0.0, 0.0))
    matrix[:3, :3] = Rotation.from_euler("xyz", [roll, pitch, yaw]).as_matrix()
    return matrix


def _axis_rotation(axis: np.ndarray, angle: float) -> np.ndarray:
    matrix = np.eye(4, dtype=float)
    length = np.linalg.norm(axis)
    if length:
        matrix[:3, :3] = Rotation.from_rotvec(axis / length * angle).as_matrix()
    return matrix


@dataclass(frozen=True)
class _Joint:
    name: str
    kind: str
    parent: str
    child: str
    origin: np.ndarray
    axis: np.ndarray
    lower: float
    upper: float
    velocity: float


class Robot:
    def __init__(self, urdf_path: str, ee_frame: str = "") -> None:
        self._urdf_path = str(urdf_path)
        self._load(Path(urdf_path).read_text(encoding="utf-8"), ee_frame)

    @staticmethod
    def from_urdf_string(urdf_string: str, ee_frame: str = "") -> "Robot":
        instance = Robot.__new__(Robot)
        instance._urdf_path = ""
        instance._load(urdf_string, ee_frame)
        return instance

    def _load(self, urdf_string: str, ee_frame: str) -> None:
        root = ET.fromstring(urdf_string)
        self._name = root.get("name", "robot")
        all_joints: list[_Joint] = []
        child_links: set[str] = set()
        parent_links: set[str] = set()
        for node in root.findall("joint"):
            parent_node = node.find("parent")
            child_node = node.find("child")
            if parent_node is None or child_node is None:
                continue
            parent = parent_node.get("link", "")
            child = child_node.get("link", "")
            child_links.add(child)
            parent_links.add(parent)
            limit = node.find("limit")
            axis_node = node.find("axis")
            kind = node.get("type", "fixed")
            continuous = kind == "continuous"
            all_joints.append(
                _Joint(
                    name=node.get("name", child),
                    kind=kind,
                    parent=parent,
                    child=child,
                    origin=_origin_matrix(node.find("origin")),
                    axis=_values(axis_node.get("xyz") if axis_node is not None else None, (1.0, 0.0, 0.0)),
                    lower=-np.inf if continuous else float(limit.get("lower", "0") if limit is not None else 0.0),
                    upper=np.inf if continuous else float(limit.get("upper", "0") if limit is not None else 0.0),
                    velocity=float(limit.get("velocity", "inf") if limit is not None else np.inf),
                )
            )

        roots = parent_links - child_links
        base = next(iter(roots), all_joints[0].parent if all_joints else "world")
        leaves = child_links - parent_links
        self._ee_frame = ee_frame or next(iter(leaves), all_joints[-1].child)
        by_child = {joint.child: joint for joint in all_joints}
        chain: list[_Joint] = []
        cursor = self._ee_frame
        while cursor != base and cursor in by_child:
            joint = by_child[cursor]
            chain.append(joint)
            cursor = joint.parent
        chain.reverse()
        self._chain = chain
        self._actuated = [joint for joint in chain if joint.kind in {"revolute", "continuous", "prismatic"}]
        self._tool = np.eye(4, dtype=float)

    def _transform(self, q) -> np.ndarray:
        q = np.asarray(q, dtype=float)
        if q.shape != (len(self._actuated),):
            raise ValueError(f"expected {len(self._actuated)} joint positions")
        result = np.eye(4, dtype=float)
        q_index = 0
        for joint in self._chain:
            result = result @ joint.origin
            if joint.kind in {"revolute", "continuous"}:
                result = result @ _axis_rotation(joint.axis, q[q_index])
                q_index += 1
            elif joint.kind == "prismatic":
                translation = np.eye(4, dtype=float)
                translation[:3, 3] = joint.axis * q[q_index]
                result = result @ translation
                q_index += 1
        return result @ self._tool

    def fkine(self, q):
        return self._transform(q)

    def fkine_into(self, q, out) -> None:
        out[:] = self._transform(q)

    def jacob0(self, q):
        q = np.asarray(q, dtype=float)
        transform = self._transform(q)
        jacobian = np.empty((6, len(q)), dtype=float)
        epsilon = 1e-7
        for index in range(len(q)):
            plus = q.copy()
            minus = q.copy()
            plus[index] += epsilon
            minus[index] -= epsilon
            t_plus = self._transform(plus)
            t_minus = self._transform(minus)
            jacobian[:3, index] = (t_plus[:3, 3] - t_minus[:3, 3]) / (2.0 * epsilon)
            relative = t_minus[:3, :3].T @ t_plus[:3, :3]
            local_omega = Rotation.from_matrix(relative).as_rotvec() / (2.0 * epsilon)
            jacobian[3:, index] = transform[:3, :3] @ local_omega
        return jacobian

    def jacob0_into(self, q, out) -> None:
        out[:] = self.jacob0(q)

    def jacobe(self, q):
        transform = self._transform(q)
        world = self.jacob0(q)
        rotation = transform[:3, :3].T
        result = world.copy()
        result[:3] = rotation @ world[:3]
        result[3:] = rotation @ world[3:]
        return result

    def batch_fk(self, joint_positions):
        return [self._transform(row) for row in np.asarray(joint_positions, dtype=float)]

    @property
    def name(self):
        return self._name

    @property
    def nq(self):
        return len(self._actuated)

    @property
    def lower_limits(self):
        return np.asarray([joint.lower for joint in self._actuated], dtype=float)

    @property
    def upper_limits(self):
        return np.asarray([joint.upper for joint in self._actuated], dtype=float)

    @property
    def velocity_limits(self):
        return np.asarray([joint.velocity for joint in self._actuated], dtype=float)

    @property
    def qlim(self):
        return np.vstack((self.lower_limits, self.upper_limits))

    def set_ee_frame(self, name: str) -> None:
        if name != self._ee_frame:
            raise NotImplementedError("changing the end-effector frame requires reloading the URDF")

    def set_tool_transform(self, T_tool) -> None:
        array = np.asarray(T_tool, dtype=float)
        if array.shape != (4, 4):
            raise ValueError("tool transform must be 4x4")
        self._tool = array.copy()

    def clear_tool_transform(self) -> None:
        self._tool = np.eye(4, dtype=float)

    @property
    def has_tool_transform(self):
        return not np.allclose(self._tool, np.eye(4))


class Method(enum.Enum):
    GN = "GN"
    NR = "NR"
    LM = "LM"


class Damping(enum.Enum):
    Chan = "Chan"
    Wampler = "Wampler"
    Sugihara = "Sugihara"


@dataclass
class BatchResult:
    joint_positions: np.ndarray
    valid: list[bool]

    @property
    def all_valid(self) -> bool:
        return all(self.valid)


class IKSolver:
    def __init__(
        self,
        robot: Robot,
        method: Method = Method.LM,
        damping: Damping = Damping.Sugihara,
        tol: float = 1e-6,
        lm_lambda: float = 1.0,
        max_iter: int = 30,
        max_restarts: int = 100,
        enforce_limits: bool = True,
    ) -> None:
        self.robot = robot
        self.tol = float(tol)
        self.max_iter = int(max_iter)
        self.max_restarts = int(max_restarts)
        self.enforce_limits = bool(enforce_limits)
        self._weights = np.ones(6, dtype=float)
        self._q = np.zeros(robot.nq, dtype=float)
        self._success = False
        self._residual = np.inf
        self._iterations = 0
        self._restarts = 0

    def set_we(self, we) -> None:
        weights = np.asarray(we, dtype=float)
        if weights.shape != (6,):
            raise ValueError("task weights must contain six values")
        self._weights = weights.copy()

    def _error(self, q: np.ndarray, target: np.ndarray) -> np.ndarray:
        actual = self.robot.fkine(q)
        translation = actual[:3, 3] - target[:3, 3]
        rotation = Rotation.from_matrix(target[:3, :3].T @ actual[:3, :3]).as_rotvec()
        return np.concatenate((translation, rotation)) * self._weights

    def solve(self, Tep, q0=None) -> bool:
        target = np.asarray(Tep, dtype=float)
        initial = np.zeros(self.robot.nq, dtype=float) if q0 is None else np.asarray(q0, dtype=float).copy()
        lower = self.robot.lower_limits if self.enforce_limits else np.full(self.robot.nq, -np.inf)
        upper = self.robot.upper_limits if self.enforce_limits else np.full(self.robot.nq, np.inf)
        initial = np.clip(initial, lower, upper)
        starts = [initial]
        rng = np.random.default_rng(0)
        finite_lower = np.where(np.isfinite(lower), lower, -np.pi)
        finite_upper = np.where(np.isfinite(upper), upper, np.pi)
        starts.extend(rng.uniform(finite_lower, finite_upper) for _ in range(max(0, self.max_restarts)))

        best = None
        for restart, start in enumerate(starts):
            result = least_squares(
                self._error,
                start,
                args=(target,),
                bounds=(lower, upper),
                xtol=self.tol,
                ftol=self.tol,
                gtol=self.tol,
                max_nfev=max(20, self.max_iter),
            )
            residual = float(np.linalg.norm(self._error(result.x, target)))
            if best is None or residual < best[0]:
                best = (residual, result, restart)
            if residual <= self.tol:
                break
        assert best is not None
        self._residual, result, self._restarts = best
        self._q = np.asarray(result.x, dtype=float)
        self._iterations = int(result.nfev)
        self._success = bool(result.success and self._residual <= max(self.tol * 10.0, 1e-5))
        return self._success

    def batch_ik(self, poses, q_start, stop_on_failure=False):
        current = np.asarray(q_start, dtype=float).copy()
        rows: list[np.ndarray] = []
        valid: list[bool] = []
        for pose in poses:
            ok = self.solve(pose, current)
            valid.append(ok)
            rows.append(self.q.copy())
            if ok:
                current = self.q.copy()
            elif stop_on_failure:
                break
        return BatchResult(np.asarray(rows), valid)

    @property
    def q(self):
        return self._q

    @property
    def success(self):
        return self._success

    @property
    def residual(self):
        return self._residual

    @property
    def iterations(self):
        return self._iterations

    @property
    def restarts(self):
        return self._restarts


class CollisionChecker:
    def __init__(self, *args, **kwargs) -> None:
        raise RuntimeError(
            "The portable Pinokin runtime does not provide collision checking. "
            "Use PAROL6_COLLISION_CHECK=0 only for simulation; physical control remains blocked."
        )
