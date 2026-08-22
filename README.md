# PAROL6 Octopus Pro build

This repository tracks my build of the [PAROL6 desktop robot arm](https://github.com/Source-Robotics/PAROL6-Desktop-robot-arm), adapted around a BIGTREETECH Octopus Pro V1.1 H723 controller.

It keeps the original mechanical files, BOM, assembly guide, and URDF alongside the controller software I am developing for this specific robot. The software is simulation-first and the hardware bring-up is deliberately gated; this is a workshop project, not a finished product or certified safety system.

![PAROL6 desktop robot arm](Images/img3.png)

## This build

- **Controller:** BIGTREETECH Octopus Pro V1.1 H723
- **J1–J2:** `3-17HE19-2004S` motors with MKS SERVO42C boards in local `CR_OPEN` mode
- **J3–J5:** `5-17HS15-1504S-X1` motors with TMC2209 drivers
- **J6:** `17HE08-1004S` motor with a TMC2209 driver
- **Host:** Windows PC for the UI, kinematics, trajectories, and supervision
- **Printed parts:** SUNLU PLA+ 2.0 on a Bambu Lab P2S with a 0.4 mm nozzle
- **Gripper:** deferred from the current software release

These choices differ from the upstream PAROL6 controller, drivers, host, and PETG print guidance. The exact configuration and known source conflicts are recorded in [PAROL6_PROJECT_KNOWLEDGE.md](PAROL6_PROJECT_KNOWLEDGE.md).

## What is here

| Path | Contents |
| --- | --- |
| [`SOFTWARE-FIRMWARE/`](SOFTWARE-FIRMWARE/) | Octopus H723 firmware, host-side Python packages, protocol, tests, commissioning docs, and wiring app |
| [`STL/`](STL/) | Printable robot parts and mounting-plate files |
| [`BOM/`](BOM/) | Current upstream bill of materials and reference images |
| [`Building instructions/`](Building%20instructions/) | Upstream assembly manual |
| [`PAROL6_URDF/`](PAROL6_URDF/) | ROS model and meshes |
| [`quick_motor_step_gui/`](quick_motor_step_gui/) | Small Windows bench tool for bounded motor stepping |
| [`PAROL6 control board main software/`](PAROL6%20control%20board%20main%20software/) | Upstream STM32F446/TMC5160 firmware, retained as reference only |

## Start with simulation

The host-side checks need Python 3.12 or newer and do not require a connected robot:

```powershell
cd SOFTWARE-FIRMWARE
.\scripts\bootstrap-dev.ps1
.\scripts\test-all.ps1
```

The wiring and commissioning app needs Node.js 22.13 or newer:

```powershell
cd SOFTWARE-FIRMWARE\wiring-app
npm ci
npm run dev
```

Firmware build and flashing instructions live in [`SOFTWARE-FIRMWARE/docs/`](SOFTWARE-FIRMWARE/docs/). Do not flash a board or energize an actuator until the relevant hardware-verification gates are complete.

## Project status

The installed H723 image is the USB-only, output-disabled `0.2.0-safe-core`. The `0.3.0-service-core` image has been built and tested offline but has not been flashed. Current limits and evidence are maintained in [`SOFTWARE-FIRMWARE/IMPLEMENTATION_STATUS.md`](SOFTWARE-FIRMWARE/IMPLEMENTATION_STATUS.md) and [`SOFTWARE-FIRMWARE/docs/TEST_EVIDENCE.md`](SOFTWARE-FIRMWARE/docs/TEST_EVIDENCE.md).

## Safety

Robots can move unexpectedly and create electrical, heat, pinch, crush, and stored-energy hazards. Use an emergency stop, physical guarding, conservative software limits, and qualified electrical and robotics practices. Read [SAFETY_WARNING_AND_DISCLAIMER.md](SAFETY_WARNING_AND_DISCLAIMER.md) before building, wiring, flashing, or operating anything in this repository.

## Upstream and license

This work is based on Source Robotics' PAROL6 repository at commit [`77597de`](https://github.com/Source-Robotics/PAROL6-Desktop-robot-arm/commit/77597de127a844990965189f0e6062e2551a2842). The main project is distributed under GPLv3; see [LICENSE](LICENSE). Some upstream artifacts carry separate or more restrictive terms, including the URDF package and the STEP/control-board materials described by the upstream manual. Check the notice attached to an artifact before reusing it.
