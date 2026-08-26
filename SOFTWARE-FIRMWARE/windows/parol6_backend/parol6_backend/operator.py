from __future__ import annotations

from dataclasses import dataclass, field

from .calibration import J1HomeMode, RobotCalibration, load_default_calibration
from .trajectory import PlannedTrajectory, TrajectoryPlanner


class OperatorError(ValueError):
    pass


@dataclass(slots=True)
class OperatorSession:
    calibration: RobotCalibration = field(default_factory=load_default_calibration)
    j1_home_mode: J1HomeMode | None = None
    homed: dict[str, bool] = field(default_factory=lambda: {f"J{i}": False for i in range(1, 7)})
    positions_deg: list[float] = field(default_factory=lambda: [0.0] * 6)
    motion_enabled: bool = False
    planner: TrajectoryPlanner = field(init=False)

    def __post_init__(self) -> None:
        if self.j1_home_mode is None:
            self.j1_home_mode = self.calibration.j1_home_mode_default
        self.planner = TrajectoryPlanner(self.calibration)

    def set_j1_home_mode(self, mode: J1HomeMode | str) -> None:
        selected = J1HomeMode(mode)
        if selected is J1HomeMode.AUTO and not self.calibration.j1_auto_home_available:
            raise OperatorError("j1_auto_home_not_available")
        self.j1_home_mode = selected
        self.homed["J1"] = False

    def build_home_command(self, joint: str, token: str) -> str:
        if joint not in self.calibration.by_id:
            raise OperatorError("unknown_joint")
        if joint == "J1" and self.j1_home_mode is J1HomeMode.MANUAL:
            return f"MANUAL_HOME J1 {token} SET_CURRENT_POSITION_ZERO_TEMPORARY"
        return f"HOME {joint} {token} START"

    def build_jog_command(
        self, joint: str, token: str, direction: str, millidegrees: int
    ) -> str:
        if not self.motion_enabled:
            raise OperatorError("motion_not_enabled")
        if not self.homed.get(joint, False):
            raise OperatorError("joint_not_homed")
        if direction not in ("+", "-") or not 1 <= millidegrees <= 10_000:
            raise OperatorError("invalid_jog")
        axis = int(joint[1]) - 1
        target = self.positions_deg[axis] + (millidegrees / 1_000) * (1 if direction == "+" else -1)
        self.calibration.by_id[joint].validate_angle(target)
        return f"JOG {joint} {token} {direction} {millidegrees} GENTLE"

    def plan_pose(
        self,
        target_deg: tuple[float, float, float, float, float, float],
        *,
        speed_percent: int | None = None,
    ) -> PlannedTrajectory:
        if not self.motion_enabled:
            raise OperatorError("motion_not_enabled")
        if not all(self.homed.values()):
            raise OperatorError("all_joints_must_be_homed")
        return self.planner.plan_pose(
            tuple(self.positions_deg), target_deg, speed_percent=speed_percent
        )

    def build_coordinated_move_command(
        self, token: str, trajectory: PlannedTrajectory
    ) -> str:
        targets = " ".join(str(round(angle * 1_000)) for angle in trajectory.target_deg)
        return (
            f"COORD_MOVE {token} {trajectory.duration_ms} {targets} "
            "COORDINATED_MOVE_VERIFIED"
        )
