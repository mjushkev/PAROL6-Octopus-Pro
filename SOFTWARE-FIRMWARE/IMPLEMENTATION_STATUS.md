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
| 5 Windows backend | IN PROGRESS | Canonical calibration loader, validated angle/raw-step conversion, smooth synchronized planner, simulation, dry-run/operator command builder and tests | Bind the production backend to the direct-USB transport and package it for offline Windows use |
| 6 Commander | MOTION RC COMPLETE / HIL PENDING | Operator-first Web Serial UI with individual home/jog, persistent J1 Manual/Auto switch, speed controls, dry-run synchronized pose entry, supervised pose hold, motor stop, J5 post-home −130° standby, and retained Setup/Wiring views | Flash 0.9.1 and validate home, J5 standby travel, tiny single-axis jog, stop, hold release, then a small synchronized move on hardware |
| 7 Offline release | NOT STARTED | Reproducibility script placeholders only | Runtime/wheelhouse, installer/portable, recovery, clean-VM tests |
| 8 Commissioning | CALIBRATION COMPLETE / MOTION RC PENDING | Calibration and functional joint testing are complete by owner report; 0.9.1 firmware/package and operator UI are built | Perform the staged 0.9.1 hardware acceptance sequence and record loaded thermal/repeatability results |
| 9 Release/handoff | NOT STARTED | None | All earlier exit criteria and Definition of Done |

## Current safe-use boundary

The retained identity, safe-core, and `0.8.12-calibration-rc` artifacts are
rollback images. `0.9.1-motion-rc` is output-capable and deliberately remains a
release candidate until staged hardware acceptance succeeds. No ESP image or
offline installer is produced. `RELEASE_MANIFEST.json` intentionally declares
`release_ready: false` and records that motion-capable outputs now exist.
