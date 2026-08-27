"""Create the Octopus SD-card binary without invoking a blocked objcopy EXE."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from elftools.elf.elffile import ELFFile


ORIGIN = 0x08020000
LIMIT = 0x08040000


def elf_to_binary(elf_path: Path) -> bytes:
    chunks: list[tuple[int, bytes]] = []
    with elf_path.open("rb") as source:
        elf = ELFFile(source)
        vector = elf.get_section_by_name(".isr_vector")
        if vector is None or int(vector["sh_addr"]) != ORIGIN:
            raise RuntimeError("application vector is not at 0x08020000")
        for segment in elf.iter_segments():
            if segment["p_type"] != "PT_LOAD" or int(segment["p_filesz"]) == 0:
                continue
            address = int(segment["p_paddr"])
            data = segment.data()
            if not (ORIGIN <= address < LIMIT):
                continue
            if address + len(data) > LIMIT:
                raise RuntimeError("load segment exceeds application sector")
            chunks.append((address, data))
    if not chunks:
        raise RuntimeError("ELF has no flash load segments")
    end = max(address + len(data) for address, data in chunks)
    output = bytearray(b"\xFF" * (end - ORIGIN))
    for address, data in chunks:
        offset = address - ORIGIN
        output[offset : offset + len(data)] = data
    initial_stack = int.from_bytes(output[:4], "little")
    reset_vector = int.from_bytes(output[4:8], "little")
    if not (0x20000000 <= initial_stack < 0x40000000):
        raise RuntimeError("invalid initial stack pointer in generated image")
    if not (ORIGIN <= (reset_vector & ~1) < LIMIT):
        raise RuntimeError("invalid reset vector in generated image")
    return bytes(output)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    args = parser.parse_args()
    binary = elf_to_binary(args.elf.resolve())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(binary)
    digest = hashlib.sha256(binary).hexdigest()
    manifest = {
        "schema": "parol6.firmware-release.v1",
        "firmware": "1.0.0-commander-rc5",
        "board": "BTT_OCTOPUS_PRO_V1_1_H723ZE",
        "application_origin": "0x08020000",
        "owner_profile_crc32c": "0xB39A8973",
        "size_bytes": len(binary),
        "sha256": digest,
        "sd_filename": "firmware.bin",
        "rollback": "0.9.1-motion-rc",
        "physical_validation": "required_before_release",
    }
    args.manifest.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Created {args.output} ({len(binary)} bytes, sha256={digest})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
