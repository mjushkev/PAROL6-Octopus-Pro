from __future__ import annotations

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "firmware" / "octopus_h723"


class SafeCoreFirmwareSourceTests(unittest.TestCase):
    def test_safe_core_is_a_separate_non_default_image(self) -> None:
        ini = (FIRMWARE / "platformio.ini").read_text(encoding="utf-8")
        self.assertIn("default_envs = octopus_h723_identity", ini)
        self.assertIn("[env:octopus_h723_safe_core]", ini)
        self.assertIn('PAROL6_FIRMWARE_VERSION="0.2.0-safe-core"', ini)

    def test_safe_core_application_has_no_output_api(self) -> None:
        source_files = [
            FIRMWARE / "src" / "safe_core_main.cpp",
            FIRMWARE / "src" / "safe_core.cpp",
            FIRMWARE / "include" / "safe_core.hpp",
        ]
        forbidden = (
            "pinMode(",
            "digitalWrite(",
            "analogWrite(",
            "tone(",
            "HAL_GPIO_Init(",
            "LL_GPIO_Init(",
            "HardwareSerial",
            "Servo",
        )
        combined = "\n".join(path.read_text(encoding="utf-8") for path in source_files)
        for token in forbidden:
            self.assertNotIn(token, combined)
        self.assertIn("outputs=disabled", combined)
        self.assertIn("motion=disabled", combined)

    def test_hardware_and_control_watchdogs_are_bounded(self) -> None:
        header = (FIRMWARE / "include" / "safe_core.hpp").read_text(encoding="utf-8")
        main = (FIRMWARE / "src" / "safe_core_main.cpp").read_text(encoding="utf-8")
        self.assertIn("kControlHeartbeatTimeoutMs = 300", header)
        self.assertIn("kHardwareWatchdogTimeoutUs = 2000000", main)
        self.assertIn("HAL_IWDG_Init", main)
        self.assertIn("HAL_IWDG_Refresh", main)
        self.assertIn("test_watchdog_fail_closed", (FIRMWARE / "src" / "safe_core.cpp").read_text(encoding="utf-8"))

    def test_config_and_event_algorithms_have_compile_time_tests(self) -> None:
        header = (FIRMWARE / "include" / "safe_core.hpp").read_text(encoding="utf-8")
        source = (FIRMWARE / "src" / "safe_core.cpp").read_text(encoding="utf-8")
        self.assertIn("kEventCapacity = 32", header)
        self.assertIn("hardware_outputs_enabled == 0U", header)
        self.assertIn("record.crc32c == config_crc32c(record)", header)
        self.assertIn("static_assert(test_config_selection())", source)
        self.assertIn("static_assert(test_event_log_bounds())", source)

    def test_motion_and_power_commands_remain_rejected(self) -> None:
        protocol = (FIRMWARE / "src" / "safe_core_protocol.cpp").read_text(encoding="utf-8")
        main = (FIRMWARE / "src" / "safe_core_main.cpp").read_text(encoding="utf-8")
        self.assertIn('parse("MOVE") == Request::rejected', protocol)
        self.assertIn('parse("MOTOR_ENABLE") == Request::rejected', protocol)
        self.assertNotIn("begin_control_session(", main)
        self.assertIn("motion_commands=none", main)


if __name__ == "__main__":
    unittest.main()
