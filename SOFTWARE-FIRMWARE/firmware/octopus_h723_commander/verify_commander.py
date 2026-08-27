Import("env")

from pathlib import Path
import re
from elftools.elf.elffile import ELFFile

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
    with elf_path.open("rb") as elf_file:
        elf = ELFFile(elf_file)
        vector = elf.get_section_by_name(".isr_vector")
        if vector is None or int(vector["sh_addr"]) != APPLICATION_ORIGIN:
            raise RuntimeError("Commander vector is not at 0x08020000")
        for section in elf.iter_sections():
            address = int(section["sh_addr"])
            size = int(section["sh_size"])
            if 0x08000000 <= address < 0x08080000 and (
                address < APPLICATION_ORIGIN or address + size > APPLICATION_LIMIT
            ):
                raise RuntimeError("Commander flash section outside sector 1")
        symbol_table = elf.get_section_by_name(".symtab")
        symbol_names = {symbol.name for symbol in symbol_table.iter_symbols()} if symbol_table else set()
    application = (project_dir / "src" / "main.cpp").read_text(encoding="utf-8")
    storage = (project_dir / "src" / "calibration_store.cpp").read_text(encoding="utf-8")
    storage_header = (project_dir / "include" / "calibration_store.hpp").read_text(encoding="utf-8")
    used_pins = set(re.findall(r"\bP[A-K](?:[0-9]|1[0-5])\b", application))
    if used_pins != ALLOWED_GPIO_PINS:
        raise RuntimeError(f"motion RC pin set changed: {sorted(used_pins)}")
    protocol = (project_dir / "src" / "p6b1_protocol.cpp").read_text(encoding="utf-8")
    protocol_header = (project_dir / "include" / "p6b1_protocol.hpp").read_text(encoding="utf-8")
    platform_config = (project_dir / "platformio.ini").read_text(encoding="utf-8")
    if 'PAROL6_FIRMWARE_VERSION="1.0.0-commander-rc5"' not in platform_config:
        raise RuntimeError("Commander version macro is not pinned")
    for invariant in (
        "0U, 0U, 700U, 700U, 700U, 450U", "kMaximumJogMilliDegrees = 10000",
        "kDirectionDiscoveryJogMilliDegrees = 2000",
        "kServoMaximumPulseRate = {500.0F, 350.0F}",
        "kJ2MaximumPulseAcceleration = 900.0F", "kServoPulseWidthUs = 1000U",
        "kJ6SensorHomeEnabled = false",
        "kHostMotionTimeoutMs = 2000U", "j1_home=sensor_or_manual_temporary",
        "kHoldKeepaliveTimeoutMs = 400U", "kMotorHoldTimeoutMs = 2000U",
        "kMaximumHoldTravelMilliDegrees = 45000",
        "kMaximumHoldSpeedMilliDegreesPerSecond = 45000", "hold_keepalive_timeout",
        "kLimitTestInsetMilliDegrees = 10000", "LIMIT_TEST_VERIFIED",
        "kMinimumCoordinatedDurationMs = 500U",
        "kMaximumCoordinatedDurationMs = 60000U",
        "kCoordinatedMaximumDegreesPerSecond",
        "kCoordinatedMaximumAccelerationDegreesPerSecond2",
        "COORDINATED_MOVE_VERIFIED", "RELEASE_COORDINATED_HOLD_VERIFIED",
        "PAROL6_COORDINATED_STARTED", "PAROL6_COORDINATED_DONE",
        "coordinated_rate_exceeds_10_percent_cap",
        "kHomeSeekMilliDegrees = 90000", "sensor_not_found_90deg",
        "kHomeInitialReleaseMilliDegrees = 30000", "sensor_stuck_active_30deg",
        "j4_sensor_stuck_active_30deg", "FINAL_CLEAR_POSITIVE",
        "kJ5PostHomeStandbyMilliDegrees = -130000",
        "POST_HOME_STANDBY_MINUS_130", "j5_sensor_failed_to_clear",
        "j5_sensor_retriggered_during_standby", "j5_sensor_stuck_active_30deg",
        "automatic_home_boundary_is_minimum",
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
        "kP6b1ProfileCrc32c = 0xB39A8973U",
        "kP6b1QueueCapacity = 512U",
        "kP6b1WatchdogMs = 250U",
        "configure_owner_servo_interfaces",
        'abort_motion("p6b1_session_clear")',
        "p6_latch_fault(parol6::p6b1::queue_underrun)",
        "p6_handle_packet", "p6_service_protocol", "p6_service_motion",
        "p6_j1_auto_home", "p6_home_order = {0U, 1U, 2U, 3U, 5U, 4U}",
    ):
        if invariant not in application:
            raise RuntimeError(f"Commander safety invariant absent: {invariant}")
    for invariant in (
        "kMagic = {'P', '6', 'B', '1'}", "kMaximumPayload = 4096U",
        "priority_stop = 1U << 2U", "queue_watchdog = 1U << 3U",
        "graceful_finish_hold = 1U << 6U", "finish = 12U",
        "j1_manual_auto_home = 1U << 5U", "SetpointQueue",
    ):
        if invariant not in protocol_header:
            raise RuntimeError(f"P6B1 header invariant absent: {invariant}")
    for invariant in (
        "0x82F63B78U", "expected_crc != actual_crc", "output.write",
        "decode_setpoint", "payload_size > kMaximumPayload",
    ):
        if invariant not in protocol:
            raise RuntimeError(f"P6B1 codec invariant absent: {invariant}")
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
    for required in (
        "HAL_IWDG_Init", "HAL_IWDG_Refresh", "HAL_FLASH_Program", "HAL_FLASHEx_Erase",
        "_ZN12AccelStepper3runEv", "_ZN14TMC2209Stepper4IOINEv",
    ):
        if required not in symbol_names:
            raise RuntimeError(f"Commander required symbol absent: {required}")
    firmware_bytes = elf_path.read_bytes()
    for required in (b"PAROL6 Commander 1.0",):
        if required not in firmware_bytes:
            raise RuntimeError(f"Commander identity string absent: {required!r}")
    print("PAROL6 Commander firmware verification passed: P6B1 CRC/sequence queue, 250 ms watchdog, owner profile lock, calibrated homing, and fixed flash boundary")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", verify_firmware)
