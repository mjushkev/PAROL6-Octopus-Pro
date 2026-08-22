from __future__ import annotations

import json
from pathlib import Path
import struct
import unittest

import _bootstrap

from parol6_protocol import Frame, MessageType, decode_uart, encode_uart


ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "firmware" / "octopus_h723"


class ServiceCoreFirmwareSourceTests(unittest.TestCase):
    def test_service_core_is_separate_and_not_the_default_image(self) -> None:
        ini = (FIRMWARE / "platformio.ini").read_text(encoding="utf-8")
        self.assertIn("default_envs = octopus_h723_identity", ini)
        self.assertIn("[env:octopus_h723_service_core]", ini)
        self.assertIn('PAROL6_FIRMWARE_VERSION="0.3.0-service-core"', ini)
        self.assertIn("board_upload.maximum_size = 262144", ini)

    def test_application_and_storage_have_non_overlapping_flash_sectors(self) -> None:
        header = (FIRMWARE / "include" / "persistent_store.hpp").read_text(
            encoding="utf-8"
        )
        expected = {
            "kApplicationOrigin": "0x08020000U",
            "kApplicationLimit": "0x08040000U",
            "kSlotAAddress": "0x08040000U",
            "kSlotBAddress": "0x08060000U",
            "kStorageEnd": "0x08080000U",
            "kSectorBytes": "0x00020000U",
        }
        for name, value in expected.items():
            self.assertIn(f"{name} = {value}", header)

        verifier = (FIRMWARE / "scripts" / "verify_firmware.py").read_text(
            encoding="utf-8"
        )
        self.assertIn("SERVICE_CORE_STORAGE_START = 0x08040000", verifier)
        self.assertIn("address < EXPECTED_VECTOR_ADDRESS", verifier)
        self.assertIn("section_end > SERVICE_CORE_STORAGE_START", verifier)

    def test_service_image_has_no_application_output_api(self) -> None:
        source_files = [
            FIRMWARE / "src" / "service_core_main.cpp",
            FIRMWARE / "src" / "safe_core.cpp",
            FIRMWARE / "src" / "binary_protocol.cpp",
            FIRMWARE / "src" / "persistent_store.cpp",
            FIRMWARE / "include" / "safe_core.hpp",
            FIRMWARE / "include" / "binary_protocol.hpp",
            FIRMWARE / "include" / "persistent_store.hpp",
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
        self.assertIn("all power and motion outputs compiled out", combined)
        self.assertIn("ErrorCode::NOT_COMMISSIONED", combined)
        self.assertIn("request.header.flags & static_cast<std::uint8_t>(~kKnownFlags)", combined)

    def test_protocol_runtime_self_test_uses_the_checked_in_golden_vector(self) -> None:
        vectors = json.loads(
            (ROOT / "shared" / "test_vectors" / "protocol_v1.json").read_text(
                encoding="utf-8"
            )
        )
        heartbeat_hex = vectors["vectors"][0]["uart_hex"]
        source = (FIRMWARE / "src" / "binary_protocol.cpp").read_text(
            encoding="utf-8"
        )
        source_bytes = bytes(
            int(value, 16)
            for value in __import__("re").findall(
                r"0x([0-9A-Fa-f]{2})", source.split("kGoldenHeartbeatUart", 1)[1].split("}};", 1)[0]
            )
        )
        self.assertEqual(source_bytes.hex(), heartbeat_hex)
        self.assertIn("if (!protocol_self_test_ready)", (FIRMWARE / "src" / "service_core_main.cpp").read_text(encoding="utf-8"))

    def test_fixed_response_payload_layouts_match_schema(self) -> None:
        schema = json.loads((ROOT / "config" / "protocol.yaml").read_text(encoding="utf-8"))
        self.assertEqual(struct.calcsize("<BBBBIIIIIHBB"), schema["payloads"]["HEARTBEAT_REPLY"]["size"])
        self.assertEqual(struct.calcsize("<HHHHIIIIII32s"), schema["payloads"]["DEVICE_INFO"]["size"])
        self.assertEqual(struct.calcsize("<BBHI"), schema["payloads"]["NACK"]["size"])

        packet = encode_uart(Frame(message_type=MessageType.HEARTBEAT, sequence=7))
        decoded = decode_uart(packet)
        self.assertEqual(decoded.sequence, 7)
        self.assertEqual(decoded.payload, b"")

    def test_flash_records_are_one_aligned_flashword_and_fail_closed(self) -> None:
        header = (FIRMWARE / "include" / "persistent_store.hpp").read_text(
            encoding="utf-8"
        )
        source = (FIRMWARE / "src" / "persistent_store.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("static_assert(sizeof(ConfigRecord) == kFlashWordBytes)", header)
        self.assertIn("static_assert(sizeof(EventRecord) == kFlashWordBytes)", header)
        self.assertIn("hardware_outputs_enabled == 0U", header)
        self.assertIn("record.crc32c == config_crc32c(record)", header)
        self.assertIn("static_assert(storage_algorithm_tests())", source)
        self.assertIn("erase_sector(target) || !program_flashword(target, &next)", source)
        self.assertIn("SCB_InvalidateDCache_by_Addr", source)
        self.assertIn("increment_nonzero(0xFFFFFFFFU) == 1U", source)
        self.assertIn("status_ = StoreStatus::io_error", source)


if __name__ == "__main__":
    unittest.main()
