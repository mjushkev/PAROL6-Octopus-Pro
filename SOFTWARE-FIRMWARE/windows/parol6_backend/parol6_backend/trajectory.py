from __future__ import annotations

from dataclasses import dataclass

MAX_POINTS = 512
MIN_START_HORIZON_MS = 150
MAX_HORIZON_MS = 1_000
MAX_STEP_RATES = (15_000, 25_000, 32_000, 10_000, 10_000, 27_000)
INITIAL_STEP_LIMITS = (
    (-14_000, 14_000),
    (-51_587, -1_200),
    (34_700, 92_605),
    (-7_500, 7_500),
    (-6_400, 6_400),
    (0, 64_000),
)


class TrajectoryError(ValueError):
    pass


@dataclass(frozen=True, slots=True)
class TrajectoryPoint:
    index: int
    duration_ms: int
    target_steps: tuple[int, int, int, int, int, int]


class TrajectoryBuffer:
    def __init__(self) -> None:
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
        for axis, (target, prior, limits, max_rate) in enumerate(
            zip(point.target_steps, previous, INITIAL_STEP_LIMITS, MAX_STEP_RATES, strict=True)
        ):
            if not limits[0] <= target <= limits[1]:
                raise TrajectoryError(f"j{axis + 1}_position_out_of_range")
            maximum_delta = max_rate * point.duration_ms / 1_000
            if abs(target - prior) > maximum_delta:
                raise TrajectoryError(f"j{axis + 1}_rate_out_of_range")
        self.points.append(point)

    def validate_commit(self, trajectory_id: int) -> None:
        if trajectory_id != self.trajectory_id:
            raise TrajectoryError("trajectory_id_mismatch")
        if self.horizon_ms < MIN_START_HORIZON_MS:
            raise TrajectoryError("initial_horizon_too_short")

