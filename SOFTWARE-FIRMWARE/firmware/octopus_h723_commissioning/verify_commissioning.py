Import("env")

from pathlib import Path
import re
import subprocess

APPLICATION_ORIGIN = 0x08020000
APPLICATION_LIMIT = 0x08040000
ALLOWED_GPIO_PINS = {
    "PF13", "PF12", "PF14", "PG0", "PG1", "PF15",
    "PF11", "PG3", "PG5", "PC6", "PG4", "PC1", "PA2", "PC7",
    "PF9", "PF10", "PG2", "PF2", "PC13", "PF0", "PF1", "PE4",
    "PG6", "PG9", "PG10", "PG11", "PG12", "PG13", "PG14", "PG15",
    "PF4", "PF5", "PF6", "PF7", "PC0",
}

def verify_firmware(source, target, env):
    del source
    elf_path = Path(str(target[0]))
    project_dir = Path(env.subst("$PROJECT_DIR"))
    toolchain = Path(env.subst("$PROJECT_PACKAGES_DIR")) / "toolchain-gccarmnoneeabi" / "bin"
    objdump = toolchain / "arm-none-eabi-objdump.exe"
    nm = toolchain / "arm-none-eabi-nm.exe"
    strings = toolchain / "arm-none-eabi-strings.exe"
    sections = subprocess.check_output([str(objdump), "-h", str(elf_path)], text=True)
    vector = re.search(r"^\s*\d+\s+\.isr_vector\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)", sections, re.MULTILINE)
    if not vector or int(vector.group(1), 16) != APPLICATION_ORIGIN:
        raise RuntimeError("motion RC vector is not at 0x08020000")
    for match in re.finditer(r"^\s*\d+\s+\S+\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)", sections, re.MULTILINE):
        size = int(match.group(1), 16)
        for address_text in match.group(2, 3):
            address = int(address_text, 16)
            if 0x08000000 <= address < 0x08080000 and (address < APPLICATION_ORIGIN or address + size > APPLICATION_LIMIT):
                raise RuntimeError("motion RC flash section outside sector 1")
    application = (project_dir / "src" / "main.cpp").read_text(encoding="utf-8")
    storage = (project_dir / "src" / "calibration_store.cpp").read_text(encoding="utf-8")
    storage_header = (project_dir / "include" / "calibration_store.hpp").read_text(encoding="utf-8")
    used_pins = set(re.findall(r"\bP[A-K](?:[0-9]|1[0-5])\b", application))
    if used_pins != ALLOWED_GPIO_PINS:
        raise RuntimeError(f"motion RC pin set changed: {sorted(used_pins)}")
    for invariant in (
        "0U, 0U, 700U, 700U, 700U, 450U", "kMaximumJogMilliDegrees = 10000",
        "kDirectionDiscoveryJogMilliDegrees = 2000",
        "kServoMaximumPulseRate = {500.0F, 350.0F}",
        "kJ2MaximumPulseAcceleration = 900.0F", "kServoPulseWidthUs = 1000U",
        "kHostMotionTimeoutMs = 2000U", "j1_home=sensor_or_manual_temporary",
        "kHoldKeepaliveTimeoutMs = 400U", "kMotorHoldTimeoutMs = 2000U",
        "kMaximumHoldTravelMilliDegrees = 45000",
        "kMaximumHoldSpeedMilliDegreesPerSecond = 45000", "hold_keepalive_timeout",
        "kHomeSeekMilliDegrees = 30000", "sensor_not_found_30deg",
        "kHomeInitialReleaseMilliDegrees = 30000", "sensor_stuck_active_30deg",
        "INTERFACE_VERIFIED", "SAVE_CALIBRATION_VERIFIED", "HOLD_POSITION_VERIFIED",
        "hold_release_rejected", "disable_all();",
        "handoff_motor_hold_to_motion", "result=handoff driver_disabled=0",
        "PAROL6_MOTION_STARTED", "PAROL6_MOTION_DONE", "PAROL6_HOME",
        "CAL_CONFIG", "CAL_LIMIT", "CAL_RESET", "RAW_JOG", "MANUAL_HOME",
        "SET_CURRENT_POSITION_ZERO_TEMPORARY", "PAROL6_MANUAL_HOME",
        "manual_home_j1_only", "manual_home_temporary",
        "DIRECTION_DISCOVERY_VERIFIED", "PAROL6_RAW_JOG_STARTED",
        "raw_jog_requires_unhomed_axis", "logical_target_is_safe",
        "maximum_soft_limit", "minimum_soft_limit", "print_calibration",
        "kJ1HardMinimumMilliDegrees = -230000",
        "kJ1HardMaximumMilliDegrees = 35000", "apply_j1_hardcoded_limits",
        "has_automatic_home_boundary", "apply_automatic_home_boundary",
        "guarded_axis_sensor_changed", "home_boundary_is_automatic",
        "PG6, PG10, PG12, PG11, PG9, PG13",
    ):
        if invariant not in application:
            raise RuntimeError(f"motion RC safety invariant absent: {invariant}")
    for forbidden in ("analogWrite(", "tone(", "HAL_GPIO_Init(", "LL_GPIO_Init("):
        if forbidden in application:
            raise RuntimeError(f"forbidden motion RC output API: {forbidden}")
    if "j1_home_hard_locked" in application:
        raise RuntimeError("J1 homing unexpectedly remains locked")
    for invariant in (
        "kSlotAAddress = 0x08040000U", "kSlotBAddress = 0x08060000U",
        "kStorageEnd = 0x08080000U", "sizeof(CalibrationRecord) == 128U",
        "kCalibrationMagic", "kHardwarePulsesPerDegree",
    ):
        if invariant not in storage_header:
            raise RuntimeError(f"calibration storage invariant absent: {invariant}")
    for invariant in (
        "HAL_FLASH_Program", "HAL_FLASHEx_Erase", "record_crc32c",
        "CompatibilityConfigRecord", "record_valid(verified)",
    ):
        if invariant not in storage:
            raise RuntimeError(f"calibration persistence invariant absent: {invariant}")
    symbols = subprocess.check_output([str(nm), "-C", str(elf_path)], text=True)
    for required in ("HAL_IWDG_Init", "HAL_IWDG_Refresh", "HAL_FLASH_Program", "HAL_FLASHEx_Erase", "AccelStepper::run()", "TMC2209Stepper::IOIN"):
        if required not in symbols:
            raise RuntimeError(f"motion RC required symbol absent: {required}")
    firmware_strings = subprocess.check_output([str(strings), str(elf_path)], text=True)
    for required in ("0.8.10-calibration-rc", "servo_signal=push_pull_3v3", "servo_clock_max_hz=J1:500,J2:350", "servo_pulse_us=1000", "j2_lift_accel_max_pulses_s2=900", "j2_servo_ma=local_1600_initial", "motor_hold=host_supervised", "servo_hold=disabled", "home_sequence=J2,J3,J4,J6,J5", "mechanical_home_map=J2:PG10,J3:PG12,J5:PG9", "home_limits=J2:J3:auto_zero_boundary", "home_initial_release_max_mdeg=30000", "j1_home=sensor_or_manual_temporary", "j1_limits_mdeg=-230000:35000", "j6_limits_mdeg=-180000:180000", "j1_limits_hardcoded", "j6_limits_hardcoded", "limits_fixed=-230000:35000", "manual_zero=j1_runtime_only", "calibration=dual_slot_crc32c", "soft_limits=firmware_enforced", "direction_discovery=raw_2deg", "PAROL6_MANUAL_HOME", "PAROL6_RAW_JOG_STARTED", "PAROL6_HOLD_STARTED", "PAROL6_MOTOR_HOLD", "PAROL6_HOME_LIMIT_SAVED", "coast_release", "servo_hold_disabled", "PAROL6_STATUS", "PAROL6_CALIBRATION", "operator_stop"):
        if required not in firmware_strings:
            raise RuntimeError(f"motion RC identity string absent: {required}")
    print("PAROL6 calibration RC verification passed: J1 temporary manual zero, retained sensor homing, dual-slot calibration, captured soft limits, and preserved motion interlocks")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", verify_firmware)
