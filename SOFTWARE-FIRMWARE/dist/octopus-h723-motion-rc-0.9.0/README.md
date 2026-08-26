# PAROL6 Octopus H723 motion RC 0.9.0

- Board: BTT Octopus Pro V1.1 STM32H723ZE
- SD-card file: `firmware.bin`
- SHA-256: `52936AA4BE448BB0BCF05682F69D800D0B1C399EBC55D1E6BCE911A34647BB57`
- Calibration storage: retained in the controller's dual CRC-protected slots

This release keeps the owner-verified directions, homing behavior, pulse
conversions, and joint limits from calibration RC 0.8.12. It adds bounded
six-joint synchronized moves, an initial 10% coordinated speed ceiling, and a
USB-supervised final pose hold. Unexpected home/auxiliary switch transitions,
loss of host contact, protocol errors, and `STOP` disable the drivers.

J1 supports both temporary manual zero and automatic two-pass sensor homing.
The operator software defaults to manual J1 home until its sensor is repaired
and verified. Keep the physical E-stop reachable and begin with an unloaded,
clear-path test.
