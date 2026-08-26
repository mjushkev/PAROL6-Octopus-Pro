# Implementation status

This ledger distinguishes implemented software from hardware-dependent exit
criteria. “Blocked” means the plan deliberately requires evidence or tooling
that is not present; it does not authorize bypassing the gate.

| Phase | Status | Implemented now | Remaining exit work |
| --- | --- | --- | --- |
| 0 Repository/evidence | IN PROGRESS | Source locks, decisions, gate ledger, safety notes and as-built wiring knowledge; owner reports wiring complete, main 24 V removed by E-stop, single POWER input, direct USB, ESP deferred, and all six joints tested | Formal reviewed as-built schematic, power budget, qualified safety review and remaining evidence closures |
| 1 Protocol/simulators | IN PROGRESS | Python codec, fixed-buffer H723 codec, golden vectors, CRC32C/COBS/HMAC, replay defense, simulators, calibrated trajectory planner and bounded queue | Physical binary-protocol conformance and fuller adverse-link integration |
| 2 H723 safe core | COMPLETE AS ROLLBACK | Output-disabled identity, safe-core and service-core artifacts are retained; watchdog and flash-boundary verification remains automated | No further work unless recovery is required |
| 3 Step/TMC/homing | OWNER-VALIDATED | Installed `0.8.12-calibration-rc`; owner reports all joints functional/calibrated. Exact directions, limits, pulses-per-degree, active levels, J2/J3 active-start release, J4 positive clear, J1 manual fallback and J6 cable bound are captured in the production calibration JSON | Repair and electrically validate the J1 sensor before selecting automatic J1 home |
| 4 ESP/OLED | DEFERRED | Fake bridge remains for protocol/fault simulation only | Optional owner-resumed phase; no ESP hardware or USART2 wiring in initial build |
| 5 Windows backend | IN PROGRESS | Canonical calibration loader, validated angle/raw-step conversion, smooth synchronized and multi-waypoint program planners, deterministic 0.9.1 USB line parser, direct pyserial transport, simulation, dry-run/operator command builder and 83 tests | Add the background session service around the transport and package it for offline Windows use |
| 6 Commander | STAGE 3 FUNCTIONAL | Operator-first Web Serial Commander with individual home/jog, persistent J1 Manual/Auto switch, synchronized poses, local multi-waypoint programs, per-step bounded speed/dwell, repeat/reorder, JSON import/export, continuous inter-waypoint hold, fail-closed program cancellation, motor stop, J5 post-home −130° standby, and retained Setup/Wiring views | Add Cartesian/kinematics and tool controls in the next capability stage; then complete offline packaging and loaded endurance validation |
| 7 Offline release | NOT STARTED | Reproducibility script placeholders only | Runtime/wheelhouse, installer/portable, recovery, clean-VM tests |
| 8 Commissioning | OWNER FUNCTIONAL VALIDATION COMPLETE | Calibration is complete and the owner reports 0.9.1 homing, calibrated joint motion, synchronized motion and J5 −130° post-home standby working | Record formal loaded thermal, repeatability and endurance results before release |
| 9 Release/handoff | NOT STARTED | None | All earlier exit criteria and Definition of Done |

## Current safe-use boundary

The retained identity, safe-core, and `0.8.12-calibration-rc` artifacts are
rollback images. Installed `0.9.1-motion-rc` is output-capable and has passed
owner-reported functional motion checks, but deliberately remains a release
candidate until loaded thermal, repeatability and endurance evidence is
recorded. No ESP image or offline installer is produced. `RELEASE_MANIFEST.json`
therefore continues to declare `release_ready: false`.
