"""Simulation-first PAROL6 backend components."""

from .safety import OutputInterlocks, SafetySupervisor
from .simulation import FakeESPBridge, FakeMCU, LinkProfile, SimulationClient
from .motion_config import ENCODER_INTEGRATION_ENABLED, SERVO42C_MODE
from .status import ControllerStatus, EncoderMode, EncoderTelemetry
from .trajectory import (
    PlannedTrajectory,
    TrajectoryBuffer,
    TrajectoryError,
    TrajectoryPlanner,
    TrajectoryPoint,
)
from .calibration import (
    CalibrationError,
    J1HomeMode,
    JointCalibration,
    RobotCalibration,
    default_calibration_path,
    load_calibration,
    load_default_calibration,
)
from .operator import OperatorError, OperatorSession

__all__ = [
    "ControllerStatus",
    "ENCODER_INTEGRATION_ENABLED",
    "EncoderMode",
    "EncoderTelemetry",
    "FakeESPBridge",
    "FakeMCU",
    "LinkProfile",
    "OutputInterlocks",
    "SafetySupervisor",
    "SERVO42C_MODE",
    "SimulationClient",
    "TrajectoryBuffer",
    "TrajectoryError",
    "TrajectoryPoint",
    "PlannedTrajectory",
    "TrajectoryPlanner",
    "CalibrationError",
    "J1HomeMode",
    "JointCalibration",
    "RobotCalibration",
    "default_calibration_path",
    "load_calibration",
    "load_default_calibration",
    "OperatorError",
    "OperatorSession",
]
