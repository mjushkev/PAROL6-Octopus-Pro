# Octopus Pro V1.1 H723 firmware

This directory contains two deliberately output-disabled firmware images:

- `octopus_h723_identity` is the installed and hardware-verified `0.1.0`
  recovery/identity milestone. It remains the default build.
- `octopus_h723_safe_core` is the unflashed `0.2.0` development image. It adds
  watchdog supervision, CRC-checked dual-slot selection logic, a bounded RAM
  event log, and USB diagnostics while still containing no output capability.

## Safety boundary

- Target: BIGTREETECH Octopus Pro V1.1, STM32H723ZET6.
- Application origin: `0x08020000`, after the board's 128 KiB bootloader.
- USB: CDC on PA11/PA12.
- No GPIO pin is configured as an output by application code.
- No STEP, DIR, driver-enable, heater, fan, relay, servo, UART, ADC, SD, or
  actuator-power peripheral is initialized.
- The identity image accepts `IDENTIFY`, `STATUS`, and `HELP`. The safe-core
  image additionally accepts `HEARTBEAT` and `DIAGNOSTICS`. Every other
  non-empty line is rejected.
- This image does not close HV-01 and does not authorize 24 V or actuator
  power.

The firmware emits a `PAROL6_SAFE_ID_READY` banner when the host asserts USB
CDC DTR. `IDENTIFY` and `STATUS` both report `outputs=disabled` and
`motion=disabled`.

The safe-core dual-slot algorithm and event ring are implemented and
compile-time tested, but STM32 flash persistence is intentionally not connected
yet. Its diagnostics honestly report `config_storage=algorithm_only` and
`event_storage=ram_only`.

## Build

From this directory:

```powershell
& 'C:\Users\mattj\Documents\PAROL 6\tmp\pio-env\Scripts\platformio.exe' run -e octopus_h723_identity
```

The build is pinned in `platformio.ini`. The application vector table must be
at `0x08020000`; `scripts/verify_firmware.py` enforces this and checks that the
application imports no Arduino GPIO-output API.

Build the safe-core package with:

```powershell
powershell -ExecutionPolicy Bypass -File ..\..\scripts\build-octopus-safe-core.ps1
```

This performs two clean builds, checks the locked hash, and prepares a package
without flashing it.

## Update method

Use the bootloader-preserving microSD procedure in
`../../docs/FLASH_OCTOPUS_IDENTITY.md`. Do not use direct USB DFU for this
milestone: BTT documents that DFU overwrites the installed SD bootloader.

This is experimental diagnostic firmware, not a certified safety controller or
a motion release.
