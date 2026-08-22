Import("env")

from pathlib import Path
import re
import subprocess


APPLICATION_ORIGIN = 0x08020000
APPLICATION_LIMIT = 0x08040000
ALLOWED_GPIO_PINS = {"PC13", "PF0", "PF1", "PE4"}


def verify_firmware(source, target, env):
    del source
    elf_path = Path(str(target[0]))
    project_dir = Path(env.subst("$PROJECT_DIR"))
    toolchain = (
        Path(env.subst("$PROJECT_PACKAGES_DIR"))
        / "toolchain-gccarmnoneeabi"
        / "bin"
    )
    objdump = toolchain / "arm-none-eabi-objdump.exe"
    nm = toolchain / "arm-none-eabi-nm.exe"
    strings = toolchain / "arm-none-eabi-strings.exe"

    sections = subprocess.check_output(
        [str(objdump), "-h", str(elf_path)], text=True, encoding="utf-8"
    )
    vector = re.search(
        r"^\s*\d+\s+\.isr_vector\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)",
        sections,
        re.MULTILINE,
    )
    if not vector or int(vector.group(1), 16) != APPLICATION_ORIGIN:
        raise RuntimeError("J6 diagnostic vector is not at 0x08020000")
    for match in re.finditer(
        r"^\s*\d+\s+\S+\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)",
        sections,
        re.MULTILINE,
    ):
        size = int(match.group(1), 16)
        for address_text in match.group(2, 3):
            address = int(address_text, 16)
            if 0x08000000 <= address < 0x08080000:
                if address < APPLICATION_ORIGIN or address + size > APPLICATION_LIMIT:
                    raise RuntimeError(
                        f"J6 diagnostic flash section outside sector 1: "
                        f"0x{address:08X}..0x{address + size:08X}"
                    )

    application = (project_dir / "src" / "main.cpp").read_text(encoding="utf-8")
    used_pins = set(re.findall(r"\bP[A-K](?:[0-9]|1[0-5])\b", application))
    if used_pins != ALLOWED_GPIO_PINS:
        raise RuntimeError(
            f"J6 diagnostic pin set changed: {sorted(used_pins)}; "
            f"expected {sorted(ALLOWED_GPIO_PINS)}"
        )
    for forbidden in ("analogWrite(", "tone(", "HAL_GPIO_Init(", "LL_GPIO_Init("):
        if forbidden in application:
            raise RuntimeError(f"Forbidden J6 diagnostic output API: {forbidden}")
    for invariant in (
        "kMaximumPulses = 1600U",
        "kRunCurrentMa = 250U",
        "TMC2209Stepper driver(kDriverUartPin, kDriverUartPin",
        "driver.beginSerial(115200)",
        "jog_used = true",
        "disable_driver();",
    ):
        if invariant not in application:
            raise RuntimeError(f"J6 diagnostic safety invariant absent: {invariant}")

    symbols = subprocess.check_output(
        [str(nm), "-C", str(elf_path)], text=True, encoding="utf-8"
    )
    for required in (
        "HAL_IWDG_Init",
        "HAL_IWDG_Refresh",
        "TMC2209Stepper::IOIN",
        "SoftwareSerial::SoftwareSerial",
    ):
        if required not in symbols:
            raise RuntimeError(f"J6 diagnostic required symbol absent: {required}")
    firmware_strings = subprocess.check_output(
        [str(strings), str(elf_path)], text=True, encoding="utf-8"
    )
    for required in (
        "0.4.2-j6-diag",
        "one_jog_per_boot",
        "driver_disabled",
    ):
        if required not in firmware_strings:
            raise RuntimeError(f"J6 diagnostic identity string absent: {required}")

    print(
        "PAROL6 J6 diagnostic verification passed: sector1 only, "
        "pins=PC13/PF0/PF1/PE4, one bounded jog per boot"
    )


env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", verify_firmware)
