# Implementation status

This ledger distinguishes implemented software from hardware-dependent exit
criteria. “Blocked” means the plan deliberately requires evidence or tooling
that is not present; it does not authorize bypassing the gate.

| Phase | Status | Implemented now | Remaining exit work |
| --- | --- | --- | --- |
| 0 Repository/evidence | IN PROGRESS | Nested project, source locks, decisions, gate ledger, traceability, safety notes, wiring tutorial; owner reports wiring complete, main 24 V removed by E-stop, single POWER input, direct USB, and ESP deferred; identity verified on COM4 | Remaining HV-01/HV-02/HV-04-HV-07 evidence, reviewed as-built schematic, exact connector/fuse/wire list, power budget, qualified safety review |
| 1 Protocol/simulators | IN PROGRESS | Python codec, fixed-buffer H723 C++ codec, golden vectors, CRC32C/COBS/HMAC, replay defense, PCAP, fake ESP/MCU, bounded trajectories, 1M fuzz cases; service-core binary handler built offline | Physical C++/Python conformance on COM4; fuller latency/reorder simulator; authenticated control-session handshake |
| 2 H723 safe core | IN PROGRESS / GATED | `0.2.0-safe-core` installed and verified on COM4; separate output-disabled `0.3.0-service-core` built reproducibly with canonical USB messages, IWDG, CRC-checked dual-sector config/event persistence, reserved flash boundaries, runtime self-test, and fail-closed command rejection | Explicit authorization to flash/initialize storage; post-flash binary protocol and persistence verification; deliberate watchdog-reset timing test; complete HV-01 before any output-capable firmware; logic-analyzer evidence |
| 3 Step/TMC/homing | IN PROGRESS / GATED | `0.8.1-calibration-rc` builds on the proven motion timing and adds fixed 2° pre-configuration raw-direction tests, guarded J1-J6 sensor homing, a relative 0° latch, saved logical direction mapping, captured firmware soft limits, and CRC32C dual-slot persistence | Flash and identify 0.8.1; verify each joint's raw home direction, logical positive direction, sensor polarity, slow latch, minimum, and maximum one axis at a time; export and review the completed calibration JSON |
| 4 ESP/OLED | DEFERRED | Fake bridge remains for protocol/fault simulation only | Optional owner-resumed phase; no ESP hardware or USART2 wiring in initial build |
| 5 Windows backend | IN PROGRESS | Simulation-only status/safety/transport skeleton | Import pinned API/waldoctl forks, parity transport, support bundle, complete integration matrix |
| 6 Commander | IN PROGRESS | Hosted Web Serial joint setup app implements one session-level motion enable, a single selected-joint workbench, fixed raw-direction discovery, configurable home/logical directions, retained min/max capture and reset, all-joint homing, JSON export, and live sensors/positions; J1/J2 encoder integration is explicitly dormant | Hardware HIL of raw-direction moves, calibration writes, reboot retention, every homing path, every soft-limit stop, JSON review, open-loop missed-step/load characterization, and offline Windows packaging |
| 7 Offline release | NOT STARTED | Reproducibility script placeholders only | Runtime/wheelhouse, installer/portable, recovery, clean-VM tests |
| 8 Commissioning | IN PROGRESS / GATED | `0.8.1-calibration-rc` and the simplified Site provide a test-direction/save/home/jog-capture workflow, retained flash configuration, all-joint reset controls, and final JSON handoff while preserving the proven Servo42C timing, TMC currents, heartbeat, sensor, and STOP paths | Install 0.8.1; calibrate one supported joint at a time; power-cycle and verify retention; run Home All only after all six records are complete; provide the exported JSON for final-software integration |
| 9 Release/handoff | NOT STARTED | None | All earlier exit criteria and Definition of Done |

## Current safe-use boundary

The retained identity and safe-core artifacts remain rollback images. The
commissioning image is deliberately motion-capable but bounded and gated; it
is not production control firmware and must be used only through the documented
one-joint physical commissioning sequence. No ESP image or installer is
produced. `RELEASE_MANIFEST.json`
intentionally declares `release_ready: false` and
`hardware_outputs_enabled: false`.
