# Flash the output-disabled USB service core

This procedure is prepared for a later, explicitly authorized physical step.
Building the image does not authorize flashing it. The currently installed
`0.2.0-safe-core` remains the verified hardware baseline.

## Image

- Version: `0.3.0-service-core`
- File: `dist/octopus-h723-service-core-0.3.0/firmware.bin`
- Size: 41,292 bytes
- SHA-256: `3C176B9D75EE10711DB818E37B65F547089A30EC7E5BB4EE4096DB0ADB1B4FB1`
- Application region: `0x08020000..0x0803FFFF` (sector 1)
- Persistent slots: `0x08040000..0x0807FFFF` (sectors 2 and 3)
- Outputs/motion: compiled out

On its first boot the image writes a CRC-protected factory-safe record and a
boot event into an erased persistent slot. That internal-flash write is part of
the future authorization decision; it has not happened yet.

## Required physical state

- Obtain explicit owner authorization for this exact flash and first-boot
  storage initialization.
- Unplug the 24 V PSU from the wall.
- Keep Octopus POWER, MOTOR-POWER, and BED-POWER inputs absent.
- Keep every actuator branch unpowered and keep the ESP disconnected.
- Unplug USB before touching the microSD card.

Stop if any item is not true. Do not infer safety from the firmware being
output-disabled; the experimental robot remains uncommissioned.

## Install and verify

1. Verify the packaged SHA-256 above, then copy the image to the root of the
   FAT32 microSD card as `firmware.bin`.
2. Safely eject the card, insert it into the unpowered Octopus, reconnect only
   USB, and wait 15 seconds.
3. Unplug USB, return the card to the computer, and require the file to have
   been renamed to `FIRMWARE.CUR` with the exact size and SHA-256 above.
4. Remove the card and reconnect only USB.
5. Install `tools/commissioning/requirements.txt` into a Python environment,
   then run `python tools/commissioning/verify-service-core.py --port COM4`.
6. Save the complete verifier output as dated evidence.

The verifier checks binary COBS/CRC framing, response acknowledgements, version
and flash boundaries, watchdog and persistent-storage readiness, the
`NOT_COMMISSIONED` state, `outputs_enabled=0`, replay rejection, and rejection
of `MOTOR_ENABLE`.

Do not connect 24 V after this test. This image is not motion capable.
