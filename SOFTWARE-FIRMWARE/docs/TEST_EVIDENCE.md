# Test evidence

## 2026-08-09 — first simulation milestone

- Python: bundled Codex Python runtime (Python 3.12-compatible).
- Unit/integration: 24 tests passed.
- Property loops inside the unit suite: 10,000 randomized COBS payloads and
  5,000 randomized full UART/TCP frames.
- Standalone deterministic fuzz campaign: 1,000,000 cases, comprising 250,000
  accepted canonical round trips and 750,000 rejected corrupt, truncated, or
  random inputs; zero crash and zero known-malformed acceptance.
- Immutable upstream verification: Waldo Commander, PAROL6 Python API, and
  waldoctl commits and Git trees match `UPSTREAM_MANIFEST.json`.

Not run in this milestone: host C++ protocol conformance (no Windows host C++
compiler available), H723/ESP HIL, physical motion, thermal/soak, UI browser,
or clean Windows installer tests.

## 2026-08-17 — H723 identity-only firmware build

- Target: STM32H723ZET6, 512 KiB flash, 128 KiB bootloader offset.
- Toolchain: PlatformIO 6.1.19, ST STM32 platform 19.7.1, STM32duino 2.12.0,
  GCC Arm 12.3.1.
- ELF `.isr_vector`: `0x08020000`.
- Application verifier: no `pinMode`, `digitalWrite`, `analogWrite`, tone,
  hardware UART, or Servo API use by application code.
- Parser compile-time assertions: `IDENTIFY`, `STATUS`, and `HELP` accepted;
  `MOVE`, lowercase input, empty input, and overlong input fail closed as
  specified.
- Usage: 27,496 bytes flash and 6,972 bytes RAM.
- Two clean builds produced the same firmware SHA-256:
  `25A16D997590C00BC158DF40061970802BEBB5B8A1845C57A2F8734748FC94BD`.
- Physical flash: PASS through the retained BTT microSD bootloader; card renamed
  to `FIRMWARE.CUR` with the expected artifact hash.
- Post-flash USB: PASS on COM4 with the `PAROL6 H723 Safe Identity` descriptor.
- Runtime identity/status: PASS; both report outputs and motion disabled.
- Fail-closed command check: PASS; `MOVE` returned `command_rejected`.

Detailed output is retained in
`docs/evidence/2026-08-17_identity_firmware_build.txt` and
`docs/evidence/2026-08-17_identity_firmware_hardware_verification.txt`.

## 2026-08-17 — H723 output-disabled safe-core build

- Version: `0.2.0-safe-core`; separate from and not replacing the installed
  identity image.
- Added compile-time tests for CRC-checked dual-slot selection, corrupted-slot
  fallback, unsafe-output configuration rejection, event-ring rollover, and
  300 ms heartbeat timeout behavior.
- Added STM32 HAL independent watchdog initialization and refresh with a
  nominal two-second interval; hardware startup is exposed in diagnostics for
  post-flash verification.
- USB line diagnostics accept only `IDENTIFY`, `STATUS`, `HEARTBEAT`,
  `DIAGNOSTICS`, and `HELP`; `MOVE` and `MOTOR_ENABLE` remain rejected.
- ELF verifier passed at `0x08020000`, found the watchdog symbols, and found no
  application GPIO-output API.
- Usage: 29,644 bytes flash and 7,280 bytes RAM; binary size 30,392 bytes.
- Two clean builds produced SHA-256
  `25B176BA79B029E0D82FD4489A94DA84D734C8C08BA25A0DC1DFB9478F5BA2E4`.
- Python suite: 33 tests passed.
- Physical flash/HIL: PASS through the retained SD bootloader and on COM4;
  identity, status, diagnostics, heartbeat, watchdog-ready reporting, and
  fail-closed `MOVE` rejection verified.
- Persistence boundary: dual-slot selection is implemented, but the STM32 flash
  adapter is pending; event storage is RAM-only.

Detailed output is retained in
`docs/evidence/2026-08-17_safe_core_firmware_build.txt`, SHA-256
`641B780CD3383CF9AB8D55610D1C81CA2E13E4CA449CDF082580D11E6FBC87A9`.

Post-flash output is retained in
`docs/evidence/2026-08-17_safe_core_hardware_verification.txt`, SHA-256
`D659E07EDC04FCEC4994204A49626C357087B51760EBECC8A46DC9F70242D700`.

## 2026-08-17 — H723 output-disabled service-core offline build

- Version: `0.3.0-service-core`; it is separate from the installed and verified
  `0.2.0-safe-core` image and was not flashed in this milestone.
- Added a fixed-buffer H723 implementation of canonical little-endian frames,
  COBS/CRC32C USB framing, a 64-frame replay window, fixed diagnostic/NACK
  payloads, and a boot-time golden-vector/CRC/replay self-test.
- Added 32-byte H723 flashword records and dual 128 KiB sectors for
  CRC-checked uncommissioned config plus append-only events. Rotation commits
  the inactive config before abandoning the previous valid sector.
- Linker map constrains the application to `0x08020000..0x0803FFFF`; the ELF
  verifier rejects any overlap with persistent storage at `0x08040000`.
- Usage: 40,520 bytes linked flash, 19,928 bytes RAM; binary size 41,292 bytes.
- Python suite: 44 tests passed, including interrupted erase/program/commit
  recovery checkpoints and exact fixed payload sizes.
- Two clean builds produced SHA-256
  `3C176B9D75EE10711DB818E37B65F547089A30EC7E5BB4EE4096DB0ADB1B4FB1`.
- Physical flash, first-boot storage initialization, COM4 binary conformance,
  and physical power-cut recovery: NOT RUN; explicit authorization required.

Detailed output is retained in
`docs/evidence/2026-08-17_service_core_firmware_build.txt`, SHA-256
`98A1724FE37BC9363DAE7314CA45D9A56B252694D290133D1FFF0AC021C58753`.
