# J6-only first-motor diagnostic

This temporary image is output-capable. It is not general robot firmware and
must not be installed or run until the checklist below is explicitly confirmed.

## Fixed test envelope

- Board/slot: Octopus Pro V1.1, MOTOR5 only
- Signals verified from the official V1.1 schematic: STEP PC13, DIR PF0,
  active-low EN PF1, TMC UART PE4
- Driver: BTT TMC2209 V1.3, UART address 0, measured schematic sense
  resistors `0R11`
- Current: 250 mA RMS; hold multiplier 0.35
- Motion: 1,600 microsteps at 16 microsteps/full-step and 500 steps/s
- Expected J6 output with the documented 10:1 reduction: about 18 degrees
- Limits: one jog per MCU boot; no code for J1-J5; driver disabled on boot,
  idle, rejection, and completion; one-second independent watchdog

## Before the image may be installed

1. PSU unplugged and USB unplugged while touching wiring or microSD.
2. Complete the power-button/E-stop/contactor circuit. Do not bypass the
   button or contactor. The owner-configured E-stop removes main 24 V power.
3. Remove J1/J2 actuator branch fuses and unplug J3/J4/J5 motor connectors so
   J6 is the only connected motor load.
4. Verify a TMC2209 V1.3 is correctly oriented in MOTOR5, its UART jumper is
   fitted, its source jumper selects the populated main `POWER` input, and its
   heatsink cannot short module pins.
5. With J6 disconnected from MOTOR5, measure and record two similar finite
   coil-pair resistances, open circuit between pairs, and open circuit from all
   four phases to the robot frame. Reconnect only with all power absent.
6. Verify 24 V polarity at the disconnected Octopus plugs before reconnecting.
   Verify the E-stop removes main 24 V and does not automatically re-energize.
7. Support the arm, clear people/tools from every pinch zone, give J6 at least
   20 degrees of cable-safe travel both ways, and keep a hand on the E-stop.

## Image and execution gates

- Artifact: `dist/octopus-h723-j6-diag-0.4.2/firmware.bin`
- Size: 46,852 bytes
- SHA-256: `D955E4F5C69670E1F78433B6FE836329AD14A37D92E0EEAB1C4E444C8E4FC530`

The microSD procedure is the same bootloader-preserving process used for the
safe core. First run `verify-j6-diagnostic.ps1` without `-Execute`; this performs
UART identity/configuration/fault checks without STEP pulses. The actual jog
also requires `-Execute` and the exact local confirmation phrase. Immediately
drop the E-stop for any unexpected sound, direction, motion, heat, or smell.

After the observation, remove MOTOR-POWER and restore the output-disabled safe
core before making wiring changes or proceeding to another axis.
