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
| 5 Windows backend | COMMANDER RC | Owner-profile loader, mapped URDF/FK/IK, TOPP-RA planning, P6B1 Octopus transport, exact profile handshake, 120 ms motion buffering, graceful finish-and-hold, 100 ms heartbeat, priority STOP, simulator/fault injection and one-click installation | Physical USB/HIL acceptance; restore a Windows collision checker before unrestricted Cartesian hardware operation |
| 6 Commander | COMMANDER RC | Full Waldo Commander fork with the standard editor, program playback, joint/Cartesian controls, I/O/tool surfaces, visualization, custom robot identity/limits/directions, persistent J1 Manual/Auto selector, calibrated home semantics, hardware warning, software E-stop, responsive jog/settings layout, and persistent click-selectable colors/opacity for disconnected robot mesh components | Confirm owner-to-URDF zero pose physically; no-tool acceptance, then tool and program validation |
| 7 Offline release | IN PROGRESS | One-click Windows installer/launcher, pinned source locks, pure-Python Windows dependencies, reproducible H723 build and SD-card binary manifest | Offline wheelhouse/portable bundle and clean-VM test |
| 8 Commissioning | OWNER FUNCTIONAL VALIDATION COMPLETE | Calibration is complete and the owner reports 0.9.1 homing, calibrated joint motion, synchronized motion and J5 −130° post-home standby working | Record formal loaded thermal, repeatability and endurance results before release |
| 9 Release/handoff | NOT STARTED | None | All earlier exit criteria and Definition of Done |

## Current safe-use boundary

The retained identity, safe-core, and `0.8.12-calibration-rc` artifacts are
rollback images. Installed `0.9.1-motion-rc` is output-capable and has passed
owner-reported functional motion checks, but deliberately remains a release
candidate until loaded thermal, repeatability and endurance evidence is
recorded. Commander `1.0.0-commander-rc1` compiles and passes static,
protocol, transport, profile and UI simulation checks, but has not been flashed
or physically exercised. Firmware 0.9.1 remains the rollback image. The ESP
path remains deferred and `RELEASE_MANIFEST.json` continues to declare
`release_ready: false`.
