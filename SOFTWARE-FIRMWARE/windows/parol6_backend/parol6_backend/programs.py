from __future__ import annotations

from dataclasses import dataclass

from .calibration import RobotCalibration, load_default_calibration
from .trajectory import PlannedTrajectory, TrajectoryPlanner


class ProgramError(ValueError):
    pass


@dataclass(frozen=True, slots=True)
class ProgramWaypoint:
    name: str
    pose_deg: tuple[float, float, float, float, float, float]
    speed_percent: int = 10
    dwell_ms: int = 0


@dataclass(frozen=True, slots=True)
class MotionProgram:
    name: str
    waypoints: tuple[ProgramWaypoint, ...]
    repeat_count: int = 1


@dataclass(frozen=True, slots=True)
class PlannedProgramStep:
    cycle: int
    waypoint_index: int
    waypoint: ProgramWaypoint
    trajectory: PlannedTrajectory


@dataclass(frozen=True, slots=True)
class PlannedProgram:
    name: str
    steps: tuple[PlannedProgramStep, ...]
    total_motion_ms: int
    total_dwell_ms: int

    @property
    def total_duration_ms(self) -> int:
        return self.total_motion_ms + self.total_dwell_ms


class ProgramPlanner:
    MAX_WAYPOINTS = 32
    MAX_REPEAT_COUNT = 20
    MAX_DWELL_MS = 60_000

    def __init__(self, calibration: RobotCalibration | None = None) -> None:
        self.calibration = calibration or load_default_calibration()
        self.trajectory_planner = TrajectoryPlanner(self.calibration)

    def plan(
        self,
        program: MotionProgram,
        start_deg: tuple[float, float, float, float, float, float],
    ) -> PlannedProgram:
        name = program.name.strip()
        if not name or len(name) > 80:
            raise ProgramError("program_name_required")
        if not 1 <= len(program.waypoints) <= self.MAX_WAYPOINTS:
            raise ProgramError("program_requires_1_to_32_waypoints")
        if not 1 <= program.repeat_count <= self.MAX_REPEAT_COUNT:
            raise ProgramError("repeat_count_out_of_range")
        self.calibration.validate_pose(start_deg)

        current = start_deg
        steps: list[PlannedProgramStep] = []
        total_motion_ms = 0
        total_dwell_ms = 0
        for cycle in range(program.repeat_count):
            for index, waypoint in enumerate(program.waypoints):
                waypoint_name = waypoint.name.strip()
                if not waypoint_name or len(waypoint_name) > 80:
                    raise ProgramError(f"waypoint_{index + 1}_name_required")
                if not 0 <= waypoint.dwell_ms <= self.MAX_DWELL_MS:
                    raise ProgramError(f"waypoint_{index + 1}_dwell_out_of_range")
                trajectory = self.trajectory_planner.plan_pose(
                    current,
                    waypoint.pose_deg,
                    speed_percent=waypoint.speed_percent,
                )
                steps.append(PlannedProgramStep(cycle, index, waypoint, trajectory))
                total_motion_ms += trajectory.duration_ms
                total_dwell_ms += waypoint.dwell_ms
                current = waypoint.pose_deg
        return PlannedProgram(
            name=name,
            steps=tuple(steps),
            total_motion_ms=total_motion_ms,
            total_dwell_ms=total_dwell_ms,
        )
