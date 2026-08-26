from __future__ import annotations

from dataclasses import dataclass
import math

from .calibration import RobotCalibration, load_default_calibration

MAX_POINTS = 512
MIN_START_HORIZON_MS = 150
MAX_HORIZON_MS = 60_000

# Production ceilings. The initial 10% hardware stage yields approximately
# J1 4 deg/s, J2 1 deg/s, and J3-J6 4.5 deg/s.
MAX_JOINT_SPEED_DPS = (40.0, 10.0, 45.0, 45.0, 45.0, 45.0)
MAX_JOINT_ACCEL_DPS2 = (80.0, 25.0, 120.0, 120.0, 120.0, 120.0)


class TrajectoryError(ValueError):
    pass


@dataclass(frozen=True, slots=True)
class TrajectoryPoint:
    index: int
    duration_ms: int
    target_steps: tuple[int, int, int, int, int, int]


@dataclass(frozen=True, slots=True)
class PlannedTrajectory:
    start_deg: tuple[float, float, float, float, float, float]
    target_deg: tuple[float, float, float, float, float, float]
    speed_percent: int
    duration_ms: int
    points: tuple[TrajectoryPoint, ...]


class TrajectoryBuffer:
    def __init__(self, calibration: RobotCalibration | None = None) -> None:
        self.calibration = calibration or load_default_calibration()
        self.trajectory_id: int | None = None
        self.start_steps: tuple[int, int, int, int, int, int] | None = None
        self.points: list[TrajectoryPoint] = []

    @property
    def horizon_ms(self) -> int:
        return sum(point.duration_ms for point in self.points)

    def clear(self) -> None:
        self.trajectory_id = None
        self.start_steps = None
        self.points.clear()

    def begin(self, trajectory_id: int, start_steps: tuple[int, int, int, int, int, int]) -> None:
        if not 1 <= trajectory_id <= 0xFFFFFFFFFFFFFFFF:
            raise TrajectoryError("trajectory_id_out_of_range")
        if len(start_steps) != 6:
            raise TrajectoryError("start_steps_requires_six_axes")
        self.clear()
        self.trajectory_id = trajectory_id
        self.start_steps = start_steps

    def append(self, point: TrajectoryPoint) -> None:
        if self.trajectory_id is None or self.start_steps is None:
            raise TrajectoryError("trajectory_not_started")
        if len(self.points) >= MAX_POINTS:
            raise TrajectoryError("queue_full")
        if point.index != len(self.points):
            raise TrajectoryError("non_monotonic_point_index")
        if not 1 <= point.duration_ms <= 250:
            raise TrajectoryError("point_duration_out_of_range")
        if self.horizon_ms + point.duration_ms > MAX_HORIZON_MS:
            raise TrajectoryError("horizon_too_large")
        previous = self.start_steps if not self.points else self.points[-1].target_steps
        speed_scale = self.calibration.initial_speed_cap_percent / 100.0
        for axis, (target, prior, joint, max_speed) in enumerate(
            zip(
                point.target_steps,
                previous,
                self.calibration.joints,
                MAX_JOINT_SPEED_DPS,
                strict=True,
            )
        ):
            lower, upper = joint.raw_step_limits
            if not lower <= target <= upper:
                raise TrajectoryError(f"j{axis + 1}_position_out_of_range")
            maximum_delta = (
                max_speed * speed_scale * joint.pulses_per_degree * point.duration_ms / 1_000
            )
            if abs(target - prior) > math.ceil(maximum_delta) + 1:
                raise TrajectoryError(f"j{axis + 1}_rate_out_of_range")
        self.points.append(point)

    def validate_commit(self, trajectory_id: int) -> None:
        if trajectory_id != self.trajectory_id:
            raise TrajectoryError("trajectory_id_mismatch")
        if self.horizon_ms < MIN_START_HORIZON_MS:
            raise TrajectoryError("initial_horizon_too_short")


class TrajectoryPlanner:
    def __init__(self, calibration: RobotCalibration | None = None) -> None:
        self.calibration = calibration or load_default_calibration()

    def plan_pose(
        self,
        start_deg: tuple[float, float, float, float, float, float],
        target_deg: tuple[float, float, float, float, float, float],
        *,
        speed_percent: int | None = None,
        segment_ms: int = 50,
    ) -> PlannedTrajectory:
        self.calibration.validate_pose(start_deg)
        self.calibration.validate_pose(target_deg)
        cap = self.calibration.initial_speed_cap_percent
        speed_percent = cap if speed_percent is None else int(speed_percent)
        if not 1 <= speed_percent <= cap:
            raise TrajectoryError(f"speed_percent_exceeds_current_cap:{speed_percent}:{cap}")
        if not 10 <= segment_ms <= 250:
            raise TrajectoryError("segment_duration_out_of_range")
        scale = speed_percent / 100.0
        duration_s = MIN_START_HORIZON_MS / 1_000
        for delta, max_speed, max_accel in zip(
            (abs(target - start) for start, target in zip(start_deg, target_deg, strict=True)),
            MAX_JOINT_SPEED_DPS,
            MAX_JOINT_ACCEL_DPS2,
            strict=True,
        ):
            if delta == 0:
                continue
            # Quintic smoothstep peak factors: velocity 1.875, acceleration <5.8.
            duration_s = max(
                duration_s,
                1.875 * delta / (max_speed * scale),
                math.sqrt(5.8 * delta / (max_accel * scale)),
            )
        minimum_duration_ms = max(
            MIN_START_HORIZON_MS,
            math.ceil(duration_s * 1_000 / segment_ms) * segment_ms,
        )
        if minimum_duration_ms > MAX_HORIZON_MS:
            raise TrajectoryError("planned_trajectory_too_long")
        # Preserve the requested resolution for short moves, but coarsen long
        # moves just enough to stay inside the fixed-capacity queue.
        effective_segment_ms = max(
            segment_ms, math.ceil(minimum_duration_ms / MAX_POINTS)
        )
        if effective_segment_ms > 250:
            raise TrajectoryError("planned_trajectory_exceeds_queue_capacity")
        point_count = math.ceil(minimum_duration_ms / effective_segment_ms)
        duration_ms = point_count * effective_segment_ms
        points: list[TrajectoryPoint] = []
        for index in range(point_count):
            u = (index + 1) / point_count
            blend = 10 * u**3 - 15 * u**4 + 6 * u**5
            pose = tuple(
                start + (target - start) * blend
                for start, target in zip(start_deg, target_deg, strict=True)
            )
            points.append(
                TrajectoryPoint(
                    index,
                    effective_segment_ms,
                    self.calibration.pose_to_raw_steps(pose),
                )
            )
        return PlannedTrajectory(
            start_deg=start_deg,
            target_deg=target_deg,
            speed_percent=speed_percent,
            duration_ms=duration_ms,
            points=tuple(points),
        )
