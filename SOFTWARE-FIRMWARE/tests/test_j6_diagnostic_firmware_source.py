from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "firmware" / "octopus_h723_j6_diag"


class J6DiagnosticFirmwareSourceTests(unittest.TestCase):
    def test_diagnostic_is_isolated_from_safe_core_project(self) -> None:
        self.assertTrue((PROJECT / "platformio.ini").is_file())
        safe_ini = (ROOT / "firmware" / "octopus_h723" / "platformio.ini").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("j6_diag", safe_ini)

    def test_only_official_motor5_signals_are_named(self) -> None:
        source = (PROJECT / "src" / "main.cpp").read_text(encoding="utf-8")
        for pin in ("PC13", "PF0", "PF1", "PE4"):
            self.assertIn(pin, source)
        for other_joint in ("J1", "J2", "J3", "J4", "J5"):
            self.assertNotIn(other_joint, source)

    def test_motion_envelope_is_small_and_one_shot(self) -> None:
        source = (PROJECT / "src" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn("kRunCurrentMa = 250U", source)
        self.assertIn("kMaximumPulses = 1600U", source)
        self.assertIn("kPulseHalfPeriodUs = 1000U", source)
        self.assertIn("jog_used = true", source)
        self.assertIn("code=jog_already_used", source)
        self.assertIn("received_token != arm_token", source)

    def test_enable_defaults_disabled_and_watchdog_is_required(self) -> None:
        source = (PROJECT / "src" / "main.cpp").read_text(encoding="utf-8")
        self.assertLess(
            source.index("digitalWrite(kEnablePin, HIGH)"),
            source.index("pinMode(kEnablePin, OUTPUT)"),
        )
        self.assertIn("hardware_watchdog.Init.Reload = 999U", source)
        self.assertIn("code=watchdog_not_ready", source)
        self.assertIn("driver.ot()", source)
        self.assertIn("driver.s2ga()", source)

    def test_driver_configuration_is_pinned_and_conservative(self) -> None:
        ini = (PROJECT / "platformio.ini").read_text(encoding="utf-8")
        source = (PROJECT / "src" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn("teemuatlut/TMCStepper@0.7.3", ini)
        self.assertIn("kSenseResistorOhms = 0.11F", source)
        self.assertIn("kExpectedTmcVersion = 0x21U", source)
        self.assertIn(
            "TMC2209Stepper driver(kDriverUartPin, kDriverUartPin", source
        )
        self.assertIn("driver.beginSerial(115200)", source)
        self.assertNotIn("HardwareSerial driver_uart", source)
        self.assertIn("ifcnt_after != ifcnt_before", source)
        self.assertIn("driver.rms_current(kRunCurrentMa, 0.35F)", source)


if __name__ == "__main__":
    unittest.main()
