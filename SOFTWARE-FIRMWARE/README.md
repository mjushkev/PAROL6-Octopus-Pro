# PAROL6 Matt J controller stack

This directory is the implementation project for the approved
[software/firmware plan](FINAL_SOFTWARE_FIRMWARE_IMPLEMENTATION_PLAN.md). It is
separate from the pinned official PAROL6 checkout in the parent directory.

## Current milestone

The installed H723 image is the USB-only, output-disabled
`0.2.0-safe-core`, verified on COM4. A separate `0.3.0-service-core` image adds
the canonical binary protocol and dual-sector internal-flash persistence, but
has only been built and tested offline; it has not been flashed. Neither image
can generate a STEP pulse, energize a driver, drive the motor contactor, or
output a gripper PWM signal.

All real hardware outputs remain blocked by the hardware-verification gates in
[`docs/HARDWARE_VERIFICATION.md`](docs/HARDWARE_VERIFICATION.md). This project
is experimental and is not a certified safety system. Read the parent
repository's `SAFETY_WARNING_AND_DISCLAIMER.md` before physical work.

The owner now reports the robot wired except for the power button. That report
is not inspection evidence, so the hardware gates remain closed. Initial
development uses direct PC-to-Octopus USB with the ESP deferred. Follow the
[`USB logic bring-up`](docs/USB_LOGIC_BRINGUP.md) and
[`wiring tutorial`](docs/WIRING_GUIDE.md); both stop before actuator power or
motion.

## Run the simulation checks

On Windows:

```powershell
.\scripts\test-all.ps1
```

Or with Python 3.12 or newer:

```powershell
python -m unittest discover -s tests -v
```

No network or attached robot is required.
