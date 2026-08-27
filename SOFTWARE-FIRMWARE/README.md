# PAROL6 Matt J controller stack

This directory is the implementation project for the approved
[software/firmware plan](FINAL_SOFTWARE_FIRMWARE_IMPLEMENTATION_PLAN.md). It is
separate from the pinned official PAROL6 checkout in the parent directory.

## Current milestone

The owner reports all six joints functional and calibrated, and has installed
and functionally validated `0.9.1-motion-rc`. The authoritative,
machine-readable calibration is
[`config/robot.mattj.calibrated.json`](config/robot.mattj.calibrated.json): it
records every pulse conversion, direction, home behavior, and joint limit.
J1 defaults to a temporary manual zero until its sensor is repaired; automatic
J1 sensor homing remains available as an operator-selected mode.

`0.9.1-motion-rc` preserves the calibration firmware's
home state machine and flash records, and adds a token-bound, synchronized
six-joint move with a 50% J3-J6 speed/acceleration ceiling, protected J1/J2 Servo42C caps, firmware soft
limits, switch guards, host timeout, supervised pose hold, and a guarded J5
post-home move from the 0° latch position to −130° standby. The owner reports
the installed image, homing, joint motion, synchronized motion and J5 standby
behavior working. Formal loaded thermal/repeatability/endurance evidence is
still required. The rollback image remains `0.8.12-calibration-rc`.

The hosted Web Serial app now opens on the Stage 3 Robot Commander with
per-joint home/jog controls, a persistent Manual/Auto J1 home switch, dry-run
synchronized poses, and local multi-waypoint programs. Programs support pose
capture, editing, bounded per-step speed/wait time, repeat/reorder, JSON
import/export and a software motor stop. They retain coordinated holding torque
between waypoints, clear on any stop/disconnect/error, and never resume
automatically. Calibration/service and wiring views remain available. Direct
USB is still the only control transport; the ESP is deferred. See
[`docs/STAGE3_COMMANDER_PROGRAMS.md`](docs/STAGE3_COMMANDER_PROGRAMS.md).

This is experimental machinery, not a certified safety system. Software stop
and firmware limits do not replace the physical E-stop, guarded workspace, or
safe mechanical support. Read the parent repository's
`SAFETY_WARNING_AND_DISCLAIMER.md` before physical work.

## Run the simulation checks

On Windows:

```powershell
.\scripts\test-all.ps1
```

Or with Python 3.12 or newer:

```powershell
python -m unittest discover -s tests -v
```

No network or attached robot is required.
