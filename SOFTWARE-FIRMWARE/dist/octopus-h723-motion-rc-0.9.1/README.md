# PAROL6 Octopus H723 motion RC 0.9.1

- Board: BTT Octopus Pro V1.1 STM32H723ZE
- SD-card file: `firmware.bin`
- SHA-256: `5FAE34E6BA1C6BFC4D8645B8EDBA8F32AB63A3F588B42B21814F9D33EBB16300`
- Calibration storage: retained in the controller's dual CRC-protected slots

This release preserves the owner-verified directions, homing behavior, pulse
conversions, joint limits, synchronized motion, and safety interlocks from
motion RC 0.9.0. After J5 completes its two-pass sensor home at 0 degrees, it
moves gently to -130 degrees and reports homing complete only after the sensor
clears and the standby position is reached.

Keep the physical E-stop reachable and the J5 travel path clear. Begin with an
unloaded, single-joint J5 home test before using Home All.
