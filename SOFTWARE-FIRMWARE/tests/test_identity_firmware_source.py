from __future__ import annotations

import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "firmware" / "octopus_h723"


class IdentityFirmwareSourceTests(unittest.TestCase):
    def test_board_target_and_bootloader_offset_are_bounded(self) -> None:
        board = json.loads(
            (FIRMWARE / "boards" / "octopus_pro_v1_1_h723.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual(board["build"]["mcu"], "stm32h723zet6")
        self.assertEqual(board["upload"]["maximum_size"], 524288)
        self.assertEqual(board["build"]["flash_offset"], "0x20000")
        self.assertEqual(board["upload"]["offset_address"], "0x08020000")

    def test_application_has_no_gpio_output_api(self) -> None:
        source = (FIRMWARE / "src" / "main.cpp").read_text(encoding="utf-8")
        for forbidden in (
            "pinMode(",
            "digitalWrite(",
            "analogWrite(",
            "tone(",
            "HardwareSerial",
            "Servo",
        ):
            self.assertNotIn(forbidden, source)

    def test_only_identity_commands_are_named(self) -> None:
        protocol = (FIRMWARE / "src" / "identity_protocol.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn('parse("IDENTIFY")', protocol)
        self.assertIn('parse("STATUS")', protocol)
        self.assertIn('parse("HELP")', protocol)
        self.assertIn('parse("MOVE") == Request::rejected', protocol)

    def test_usb_only_status_is_explicit(self) -> None:
        source = (FIRMWARE / "src" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn("outputs=disabled", source)
        self.assertIn("motion=disabled", source)
        self.assertIn("actuator_power=required_off", source)


if __name__ == "__main__":
    unittest.main()
