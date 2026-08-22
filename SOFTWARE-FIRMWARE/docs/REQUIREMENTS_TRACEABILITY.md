# Requirements traceability

| Requirement | Implementation artifact | Verification | Status |
| --- | --- | --- | --- |
| No motion on boot/reset/reconnect | `parol6_backend.safety`, fake MCU | `test_safety.py`, `test_simulation.py` | Implemented in simulation |
| Versioned COBS/CRC protocol | `parol6_protocol.frame`, H723 `binary_protocol` | Golden vectors, runtime self-test, `test_protocol.py`, service-core ELF build | Implemented in Python and unflashed H723 service core |
| Reject duplicate/replayed commands | `parol6_protocol.replay`, H723 `ReplayWindow` | `test_replay.py`, integration duplicate test, runtime self-test, prepared COM4 verifier | Implemented in simulation and unflashed H723 service core |
| Single control lease | Fake MCU session owner | Integration lease tests | Implemented in simulation |
| Commanded joint angle remains primary | `ControllerStatus.commanded_joint_deg` | Status model test | Implemented in simulation |
| J1/J2 encoder data is separate/honest | Optional telemetry with validity/mode; runtime gate is disabled | Status model test | Implemented in simulation; dormant by owner selection |
| J3-J6 do not fabricate encoders | Two-element encoder arrays only | Status model test | Implemented in simulation |
| Motor/gripper outputs gated by evidence | `SafetySupervisor` commissioning flag | Safety tests | Implemented in simulation |
| H723 deterministic firmware | `firmware/octopus_h723` identity, safe-core, and service-core images | Compile-time config/event/watchdog assertions, ELF vector/API/storage-boundary verifier, reproducible builds, recovery-model tests | Safe core installed/verified; service core built but unflashed; motion remains gated |
| Direct PC-to-Octopus USB transport | H723 USB CDC firmware and commissioning verifiers | Identity and safe-core post-flash responses passed; binary service-core verifier prepared | Safe-core USB verified; service-core HIL pending authorization; HV-01 incomplete |
| Power-loss-tolerant config/event storage | H723 `PersistentStore`, sectors 2-3 | CRC/static assertions, interrupted-commit recovery model, post-link application/storage boundary check | Implemented and built offline; physical flash/power-cut evidence pending |
| ESP bridge/OLED | Phase 4 | Fault/soak/HIL | Deferred optional phase by owner |
| Windows hybrid backend parity | Phase 5 | API and integration suite | Simulation skeleton only |
| Waldo Commander UI | Phase 6 | Browser/UI tests | Baseline not yet integrated |
| Offline installer/recovery | Phase 7 | Clean offline Windows VM | Not started |
| Physical commissioning | Phase 8 | Section 14 acceptance matrix | Prohibited without authorization/evidence |
| Full wiring tutorial | `docs/WIRING_GUIDE.md` | Independent electrical review, completed connector table, continuity record | Drafted; component-specific values gated |
