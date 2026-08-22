# Quick Motor Step GUI

This small Windows panel controls the previously flashed PAROL6 Octopus motor
tester v1.0 on `MOTOR0` at 115200 baud.

- It performs the firmware `CHECK` before enabling motion buttons.
- It defaults to full 1.8-degree motor steps and can optionally convert
  explicit 1/16-microstep counts to the firmware's degree-based
  `MOVE` command.
- Each movement receives a fresh `ARM` command.
- `STOP AND DISABLE`, disconnect, and window close all send `STOP`.

Run `Launch Quick Motor Step GUI.cmd`. The default ten full motor steps equal
18 motor degrees, or 0.9 degrees at a 20:1 gearbox output. A standard
200-step/revolution motor therefore makes one complete motor revolution when
the GUI is set to 200 full steps.

This is isolated bench-test software, not finished robot-control firmware.
Secure the motor and gearbox, keep tools clear during motion, and remove all
power before changing motor or driver wiring.
