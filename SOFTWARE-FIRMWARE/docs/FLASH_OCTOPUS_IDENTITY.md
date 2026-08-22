# Flash the USB-only identity firmware

This procedure installs an outputs-disabled diagnostic image on the Octopus
Pro V1.1 H723 while preserving the board's SD bootloader.

## Required state

- Explicit owner authorization is recorded in the task conversation.
- The 24 V PSU is unplugged from the wall.
- Octopus `POWER`, `MOTOR-POWER`, and `BED-POWER` inputs are physically absent.
- Actuator branch power is absent.
- ESP power and USART2 are disconnected.
- USB is unplugged whenever the microSD card or a jumper is touched.
- The MCU-power-from-USB jumper remains in the already proven USB-only state.

Stop if any requirement is not true.

## Prepare the card

1. Use a microSD card of 32 GB or smaller, formatted FAT32.
2. Copy the verified build artifact to the card root.
3. Name it exactly `firmware.bin`.
4. Safely eject the card from Windows.

The source artifact is
`firmware/octopus_h723/.pio/build/octopus_h723_identity/firmware.bin`.

## Install

1. Unplug USB from the Octopus.
2. Insert the prepared microSD card into the Octopus.
3. Do not touch any other jumper or connector.
4. Reconnect USB-C to the Octopus.
5. Wait 15 seconds without opening the COM port.
6. Unplug USB.
7. Remove the microSD card and inspect it on Windows. A successful BTT
   bootloader update normally renames `firmware.bin` to `FIRMWARE.CUR`.
8. Safely eject the card, leave it removed, and reconnect Octopus USB-C.

## Verify

Run `tools/commissioning/identify-octopus.ps1`. It sends only `IDENTIFY` and
requires the returned line to contain all of:

- `firmware=safe_identity`
- `board=BTT_OCTOPUS_PRO_V1_1_H723ZE`
- `outputs=disabled`
- `motion=disabled`

Do not apply 24 V after this test. This identity image is not motion firmware.

## Rollback

The previous application image has not been captured and cannot be restored
from this repository. The retained SD bootloader remains the recovery path for
a future known-good application. Direct DFU is intentionally not used because
BTT documents that it overwrites the SD bootloader.
