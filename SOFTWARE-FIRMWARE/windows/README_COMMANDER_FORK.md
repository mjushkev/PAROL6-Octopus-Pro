# PAROL6-MATTJ Waldo Commander fork

This folder contains the production Commander migration for the owner-selected
PAROL6 build. It is based on Waldo Commander `3de942af856f1727eaa01dc8f8d81d7cf564cb6d`
and PAROL6 Python API `829c2c73051c18d9cbf2e4cb07508a1557f63294`.

The current milestone provides the complete Waldo UI and controller simulator
using the measured owner profile:

- BTT Octopus Pro V1.1 H723ZE profile and robot identity
- J1..J6 limits from `config/robot.mattj.calibrated.json`
- measured pulses-per-degree: 114, 356, 161, 36, 36, 89
- owner-confirmed directions and homing metadata
- J1 manual-home default with auto-home availability retained
- J5 post-home standby at -130 degrees
- J6 hard cable limit of -180 to +180 degrees
- owner-coordinate URDF mapping for Waldo visualization, FK, and IK
- click-selectable colors and opacity for individual disconnected model parts,
  including separate covers and housings where the URDF mesh preserves them
- responsive controls that keep all six jog rows and the E-stop visible and
  prevent horizontal scrolling in the Settings panel
- portable Windows builds of TOPP-RA and Pinokin APIs

## Model appearance

Open **Settings → Model Appearance**. Click a visible component on the 3D
robot (or choose it from the grouped selector), then set its color and opacity.
Selections can target one component, a whole link, or the entire robot. Changes
persist between launches. **Make solid** sets the selected opacity to 100%; the
reset buttons restore either the selected part or the whole robot.

The visual splitter operates only on disconnected shells in the displayed STL.
It does not alter the URDF joints, motion planning, limits, or collision model.

## Start the simulator

Run `Install-PAROL6-Commander.ps1` once, then run
`Start-PAROL6-Commander.ps1`. The app opens at <http://127.0.0.1:8080/>.

## Hardware acceptance mode

The launcher now has a hardware acceptance mode:

```powershell
.\Start-PAROL6-Commander.ps1 -Mode Hardware -ComPort COM4
```

It only connects to Commander firmware that proves the P6B1 capabilities and
the exact owner-profile checksum. Commissioning firmware 0.9.1 and official
upstream firmware are rejected before any motion command is accepted. The
firmware keeps an independent 10% speed cap, calibrated joint limits, sensor
guards, queue watchdog, CRC and sequence checks, graceful finish-and-hold, and
priority STOP.

This is an acceptance build, not a completed physical release. Windows uses
the portable kinematics runtime, which does not provide the upstream collision
checker. The UI therefore shows a persistent hardware warning. Initial tests
must be no-tool, low speed, one joint at a time, with a clear work area and the
main-power E-stop immediately reachable.

Release status:

1. complete: buffered Octopus protocol and firmware;
2. complete: matching host transport, simulator, and fault-injection tests;
3. complete: J1 manual/automatic selector and calibrated homing integration;
4. pending physical test: owner-to-URDF zero-pose confirmation;
5. pending: collision checking for the mapped Windows robot model;
6. pending physical test: no-tool hardware acceptance with rollback ready.

Firmware 0.9.1 remains the known-good rollback image throughout this work.
