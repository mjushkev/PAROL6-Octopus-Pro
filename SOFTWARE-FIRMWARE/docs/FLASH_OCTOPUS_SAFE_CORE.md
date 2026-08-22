# Flash the output-disabled safe-core image

This procedure is prepared for the next owner-authorized physical step. Do not
perform it merely because the image exists.

## Image

- Version: `0.2.0-safe-core`
- File: `dist/octopus-h723-safe-core-0.2.0/firmware.bin`
- Size: 30,392 bytes
- SHA-256: `25B176BA79B029E0D82FD4489A94DA84D734C8C08BA25A0DC1DFB9478F5BA2E4`
- Application origin: `0x08020000`
- Outputs/motion: disabled

## Required state

- Obtain explicit owner authorization for this flash.
- Unplug the 24 V PSU from the wall.
- Keep Octopus POWER, MOTOR-POWER, and BED-POWER inputs absent.
- Keep all actuator branch power absent and ESP disconnected.
- Unplug USB before touching the microSD card.

Stop if any item is not true.

## Install and verify

1. Copy the checksum-verified image to the root of the FAT32 microSD card as
   `firmware.bin`.
2. Safely eject the card, insert it into the unpowered Octopus, reconnect USB,
   and wait 15 seconds.
3. Unplug USB, return the card to the computer, and require the file to be
   renamed to `FIRMWARE.CUR` with the exact size and SHA-256 above.
4. Remove the card and reconnect USB.
5. Run `tools/commissioning/verify-safe-core.ps1` on COM4.

The verifier checks identity, `NOT_COMMISSIONED` state, watchdog startup,
configuration selection, heartbeat response, output-disabled reporting, and
fail-closed rejection of `MOVE`.

Do not connect 24 V after this test. The image is not motion capable.
