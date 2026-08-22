Import("env")

from pathlib import Path
import re
import subprocess


EXPECTED_VECTOR_ADDRESS = 0x08020000
PHYSICAL_FLASH_BASE = 0x08000000
PHYSICAL_FLASH_END = 0x08080000
SERVICE_CORE_STORAGE_START = 0x08040000
FORBIDDEN_UNDEFINED_SYMBOLS = (
    "pinMode",
    "digitalWrite",
    "analogWrite",
    "HardwareSerial",
    "Servo",
)
FORBIDDEN_SOURCE_TOKENS = (
    "pinMode(",
    "digitalWrite(",
    "analogWrite(",
    "tone(",
    "HAL_GPIO_Init(",
    "LL_GPIO_Init(",
)


def verify_firmware(source, target, env):
    del source
    elf_path = Path(str(target[0]))
    project_dir = Path(env.subst("$PROJECT_DIR"))
    toolchain_bin = (
        Path(env.subst("$PROJECT_PACKAGES_DIR"))
        / "toolchain-gccarmnoneeabi"
        / "bin"
    )
    objdump = toolchain_bin / "arm-none-eabi-objdump.exe"
    nm = toolchain_bin / "arm-none-eabi-nm.exe"

    section_output = subprocess.check_output(
        [str(objdump), "-h", str(elf_path)], text=True, encoding="utf-8"
    )
    vector_match = re.search(
        r"^\s*\d+\s+\.isr_vector\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)",
        section_output,
        re.MULTILINE,
    )
    if not vector_match:
        raise RuntimeError("Could not locate .isr_vector in firmware ELF")
    vector_address = int(vector_match.group(1), 16)
    if vector_address != EXPECTED_VECTOR_ADDRESS:
        raise RuntimeError(
            f"Unsafe vector address 0x{vector_address:08X}; "
            f"expected 0x{EXPECTED_VECTOR_ADDRESS:08X}"
        )

    for match in re.finditer(
        r"^\s*\d+\s+\S+\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)",
        section_output,
        re.MULTILINE,
    ):
        size = int(match.group(1), 16)
        for address_text in match.group(2, 3):
            address = int(address_text, 16)
            if PHYSICAL_FLASH_BASE <= address < PHYSICAL_FLASH_END:
                section_end = address + size
                if address < EXPECTED_VECTOR_ADDRESS:
                    raise RuntimeError(
                        f"Firmware section overlaps the retained bootloader: "
                        f"0x{address:08X}..0x{section_end:08X}"
                    )
                if section_end > PHYSICAL_FLASH_END:
                    raise RuntimeError(
                        f"Firmware section exceeds STM32H723ZE flash: "
                        f"0x{address:08X}..0x{section_end:08X}"
                    )

                if (
                    env.subst("$PIOENV") == "octopus_h723_service_core"
                    and section_end > SERVICE_CORE_STORAGE_START
                ):
                    raise RuntimeError(
                        f"Service-core section overlaps reserved storage: "
                        f"0x{address:08X}..0x{section_end:08X}"
                    )

    undefined_output = subprocess.check_output(
        [str(nm), "-u", "-C", str(elf_path)], text=True, encoding="utf-8"
    )
    found = [name for name in FORBIDDEN_UNDEFINED_SYMBOLS if name in undefined_output]
    if found:
        raise RuntimeError(f"Forbidden output-capable API imported: {', '.join(found)}")

    application_sources = [
        path
        for folder in (project_dir / "src", project_dir / "include")
        for path in folder.glob("**/*")
        if path.suffix in {".c", ".cpp", ".h", ".hpp"}
    ]
    for path in application_sources:
        content = path.read_text(encoding="utf-8")
        for token in FORBIDDEN_SOURCE_TOKENS:
            if token in content:
                raise RuntimeError(
                    f"Forbidden output API present in {path.relative_to(project_dir)}: "
                    f"{token}"
                )

    if env.subst("$PIOENV") in {
        "octopus_h723_safe_core",
        "octopus_h723_service_core",
    }:
        symbols = subprocess.check_output(
            [str(nm), "-C", str(elf_path)], text=True, encoding="utf-8"
        )
        for symbol in ("HAL_IWDG_Init", "HAL_IWDG_Refresh"):
            if symbol not in symbols:
                raise RuntimeError(f"Safe-core watchdog symbol absent: {symbol}")

    if env.subst("$PIOENV") == "octopus_h723_service_core":
        symbols = subprocess.check_output(
            [str(nm), "-C", str(elf_path)], text=True, encoding="utf-8"
        )
        for symbol in (
            "HAL_FLASH_Program",
            "HAL_FLASHEx_Erase",
            "parol6::binary::decode_body",
        ):
            if symbol not in symbols:
                raise RuntimeError(f"Service-core required symbol absent: {symbol}")

    print(
        "PAROL6 firmware verification passed: "
        "vector=0x08020000, application GPIO-output APIs absent"
    )


env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", verify_firmware)
