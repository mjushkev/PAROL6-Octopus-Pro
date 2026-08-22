from __future__ import annotations

from dataclasses import dataclass, field

from parol6_protocol import ControllerState


@dataclass(slots=True)
class OutputInterlocks:
    contactor_request: bool = False
    step_outputs_enabled: bool = False
    driver_enables_active: bool = False
    gripper_pwm_enabled: bool = False

    def force_safe(self) -> None:
        self.contactor_request = False
        self.step_outputs_enabled = False
        self.driver_enables_active = False
        self.gripper_pwm_enabled = False


@dataclass(slots=True)
class SafetySupervisor:
    commissioned: bool = False
    state: ControllerState = ControllerState.BOOT_SELF_TEST
    outputs: OutputInterlocks = field(default_factory=OutputInterlocks)
    motor_power_verified: bool = False
    homed: bool = False
    fault_code: str | None = None
    control_session: int | None = None

    def finish_boot(self) -> None:
        self.outputs.force_safe()
        self.motor_power_verified = False
        self.homed = False
        self.control_session = None
        self.state = (
            ControllerState.DISARMED if self.commissioned else ControllerState.NOT_COMMISSIONED
        )

    def connect_status_only(self, session_id: int) -> None:
        if not 1 <= session_id <= 0xFFFFFFFF:
            raise ValueError("session_id must be nonzero")
        self.disconnect()
        self.control_session = None

    def take_control(self, session_id: int) -> bool:
        if self.control_session not in (None, session_id):
            return False
        self.control_session = session_id
        return True

    def request_motor_enable(self, session_id: int) -> bool:
        if not self.commissioned:
            self.state = ControllerState.NOT_COMMISSIONED
            self.outputs.force_safe()
            return False
        if self.control_session != session_id or self.state != ControllerState.DISARMED:
            return False
        self.outputs.contactor_request = True
        self.state = ControllerState.ARMING
        return True

    def update_contactor_feedback(self, closed: bool) -> None:
        self.motor_power_verified = closed
        if self.state == ControllerState.ARMING and closed:
            self.outputs.driver_enables_active = True
            self.state = ControllerState.UNHOMED
        elif not closed and self.state not in (
            ControllerState.BOOT_SELF_TEST,
            ControllerState.NOT_COMMISSIONED,
            ControllerState.DISARMED,
        ):
            self.protective_stop("MOTOR_POWER_LOST")

    def start_homing(self, session_id: int) -> bool:
        if self.control_session != session_id or self.state != ControllerState.UNHOMED:
            return False
        self.outputs.step_outputs_enabled = True
        self.state = ControllerState.HOMING
        return True

    def finish_homing(self) -> None:
        if self.state != ControllerState.HOMING:
            raise RuntimeError("homing is not active")
        self.outputs.step_outputs_enabled = False
        self.homed = True
        self.state = ControllerState.READY

    def start_execution(self, session_id: int) -> bool:
        if self.control_session != session_id or self.state != ControllerState.READY or not self.homed:
            return False
        self.outputs.step_outputs_enabled = True
        self.state = ControllerState.EXECUTING
        return True

    def motor_off(self) -> None:
        self.outputs.force_safe()
        self.motor_power_verified = False
        self.homed = False
        self.state = (
            ControllerState.DISARMED if self.commissioned else ControllerState.NOT_COMMISSIONED
        )

    def protective_stop(self, reason: str) -> None:
        self.outputs.step_outputs_enabled = False
        self.outputs.driver_enables_active = False
        self.homed = False
        self.fault_code = reason
        self.state = ControllerState.PROTECTIVE_STOP

    def estop(self) -> None:
        self.outputs.force_safe()
        self.motor_power_verified = False
        self.homed = False
        self.fault_code = "ESTOP"
        self.state = ControllerState.ESTOP_LATCHED

    def disconnect(self) -> None:
        self.outputs.force_safe()
        self.motor_power_verified = False
        self.homed = False
        self.control_session = None
        self.state = (
            ControllerState.DISARMED if self.commissioned else ControllerState.NOT_COMMISSIONED
        )

