# Direct USB logic bring-up

**Status:** Preparation checklist only. No actuator power, motion, or attached-
robot flashing is authorized.

The owner selected a direct Windows PC to Octopus Pro V1.1 H723 USB connection
for initial development. The ESP8266, its 5 V supply, and Octopus USART2 remain
disconnected. The power button is unfinished, so the PSU path is not used for
this procedure.

## What this first session can accomplish

- Confirm the exact Octopus `V1.1` and `STM32H723` identity.
- Confirm the PC, cable, USB-C connector, MCU/VUSB jumper, and Windows device
  enumeration work.
- Record the device name, COM port or DFU identity, USB VID/PID, and photos.

It cannot yet run the robot. This repository has no flashable H723 application
firmware. Do not flash Marlin, Klipper, another Octopus build, or an experimental
binary as a substitute.

## Required physical state

1. Support the arm in a gravity-safe pose and clear people from its workspace.
2. Unplug AC mains and disconnect every bench supply, battery, and ESP USB
   cable. Verify the 24 V bus measures 0 V.
3. Remove the pluggable `POWER` and `MOTOR-POWER` screw-terminal blocks from
   the Octopus. Keep `BED-POWER` empty. This prevents PC USB from coexisting
   with an unfinished external power path.
4. Remove all actuator branch fuses or otherwise positively isolate J1-J6 and
   the gripper power branches. Do not rely on software or the unfinished power
   button.
5. Leave the ESP unpowered and disconnect PD5, PD6, 5 V, and GND at the
   Octopus UART2 header.
6. Photograph the board top/bottom, `V1.1` mark, full `STM32H723` marking, USB
   area, and present jumper positions.

## USB connection

1. With USB still unplugged, identify the MCU/VUSB power jumper from the
   official BIGTREETECH Octopus Pro documentation and the board markings.
2. Fit that jumper only in the documented USB-power position. Do not move any
   other jumper and do not infer orientation from a different Octopus version.
3. Connect a known-good USB **data** cable directly from the Windows PC to the
   Octopus USB-C device port. Do not use the neighboring USB-A port. Avoid an
   unpowered hub for this first test.
4. Watch for heat, odor, smoke, repeated USB disconnect sounds, or abnormal
   LEDs. Disconnect immediately if any appear.
5. In Windows Device Manager, record exactly what appears: category, device
   name, COM number if present, hardware IDs/VID/PID, and driver status.
6. Do not open a motion program, enable a driver, home an axis, or send an
   undocumented command. Enumeration is the stopping point.
7. Disconnect USB before touching a jumper, terminal, driver, or sensor plug.

BIGTREETECH states that the Octopus can be powered through USB-C only when the
MCU power jumper is installed; otherwise the main input must power the board.
It also documents USB enumeration as a virtual serial device when application
firmware supports it. USB enumeration alone does not identify or validate the
currently installed firmware.

## Evidence to record in `HARDWARE_VERIFICATION.md`

- Date, operator, and meter used to verify both external power inputs at 0 V.
- Photos listed above and the exact jumper location.
- USB cable identifier and whether it is known to carry data.
- Windows device name, COM port, VID/PID/hardware IDs, and a screenshot.
- Whether the board remained cool and stable for five minutes.
- Any existing firmware name/version shown by the device; write `UNKNOWN` if
  it cannot be proven.

After this evidence is reviewed, Phase 2 can implement the safe H723 USB CDC
firmware with every motor enable, STEP output, gripper PWM, and contactor request
inactive through reset and boot. Flashing that firmware onto the attached robot
still requires explicit owner authorization for the physical action.
