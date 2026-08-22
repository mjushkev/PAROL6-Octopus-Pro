# Changelog

## Unreleased

### Added

- `0.8.2-calibration-rc` adds a J1-only, non-motion temporary manual-zero
  command for operation while the J1 home sensor is unavailable. It disables
  every driver, assigns the current J1 step position to 0°, clears stale J1
  limits, identifies the runtime source as `MANUAL_TEMP`, and never persists a
  false homed state across reboot. The Site gives J1 a short manual-position
  workflow, records the temporary method in JSON, and homes only J2-J6 in its
  grouped sensor-homing action.
- `0.8.1-calibration-rc` adds a pre-configuration `RAW_JOG` direction test:
  each press moves one unhomed joint exactly 2° in a physical motor direction,
  independent of the saved logical mapping. The redesigned Site uses one
  setup-motion enable, one selected-joint workbench, automatic J1/J2 interface
  setup, a four-step direction/home/limits/done flow, and continuous hold jog
  after homing. Firmware still owns the one-axis, token, timeout, sensor,
  travel-cap, driver-disable, and STOP interlocks.
- `0.8.0-calibration-rc` adds J1-J6 sensor homing at a relative 0° reference,
  retained per-joint home-seek direction, logical positive/negative direction,
  sensor polarity, and captured minimum/maximum angles. Calibration is stored
  in CRC32C-checked inactive-slot-first flash records, and captured limits are
  enforced by firmware for tap and hold motion. The Site adds a one-joint
  calibration workflow, per-joint reset, Home All after calibration, and a
  checksummed JSON export for final-software implementation.
- `0.7.5-motion-rc` lets a held joint accept the next same-joint `JOG` or
  `HOLD` command without a separate motor-hold release. The firmware checks
  the held-axis and guarded sensor baselines before the handoff; different
  joints, sensor transitions, STOP, heartbeat loss, and focus loss remain
  driver-disable paths. Current and speed remain at the approved
  commissioning starts until load and thermal evidence supports staged tuning.
- `0.7.4-motion-rc` makes a normal hold-to-jog release stop at the exact
  commanded position and atomically engage stationary holding torque. Focus
  loss, sensor transitions, heartbeat loss, and STOP remain fail-safe driver
  disable paths; the new `HOLD_RELEASE` command is token- and confirmation-bound.
- `0.7.3-motion-rc` adds token-bound, one-axis stationary motor hold with
  host-heartbeat, sensor-transition, STOP, and explicit-release safeguards.
  J1/J2 remain open-loop Servo42C with dormant encoder integration.
- `0.7.2-motion-rc` restores the physically proven 1,000 microsecond
  active-low Servo42C clock pulse and temporarily caps J1/J2 at 500 clock
  pulses/second after 0.7.1 produced buzzing, jerking, and unreliable Tap moves.
- `0.7.1-motion-rc` corrects the temporary low-torque TMC2209 setting by
  applying the approved commissioning starts: 700 mA RMS on J3-J5 and 450 mA
  RMS on J6. Per-move UART readback and fault preflight remain mandatory.
- `0.7.0-motion-rc` with 3-45 degree/second adjustable press-and-hold jogging,
  400 ms dedicated hold supervision, a 45-degree per-press travel cap, faster
  acceleration profiles, and the upstream-derived serialized homing order
  J2 → J3 → J4 → J6 → J5. J1 homing remains hard-locked.
- Motion-control Site controls for speed selection, pointer/keyboard hold and
  release, automatic STOP on focus loss, and one-button guarded J2-J6 homing.

- `0.6.0-motion-rc` H723 firmware with nonblocking acceleration-limited jogs,
  `GENTLE`/`NORMAL`/`BRISK` profiles, repeated 0.25-10 degree relative moves,
  two-second host supervision, tokenless STOP, live relative positions, and
  bounded clear/seek/backoff/slow-latch homing for J2-J6. J1 homing is rejected
  in firmware until its sensor passes.
- Motion-control Site with six joint cards, global jog increment/profile,
  boot-local Servo42C arming, live home inputs, guarded homing setup, prominent
  STOP, controller logs, and three sequential one-axis test programs.

- Reproducible `0.5.0-commissioning` H723 firmware for all six joints, with
  J3-J6 TMC2209 UART preflight, guarded J1/J2 Servo42C setup, token-bound
  1/5/10-degree single-joint moves, one move per joint per boot, limit aborts,
  watchdog coverage, automatic driver disable, debounced STOP0-STOP7 readings,
  and raw T0-T3/POWER-DET observations.
- Browser-based direct-USB commissioning console with firmware identity checks,
  live sensor and limit display, TMC preflight controls, per-joint bounded jogs,
  explicit safety acknowledgements, a boot-only J1/J2 interface gate, and a
  timestamped test log.
- Retained evidence for the successful J6 bounded motion observation.

- Phase 0 repository scaffold, immutable source lock, decisions, hardware gate
  ledger, traceability matrix, and safety documentation.
- Phase 1 simulation-only protocol core with COBS, CRC32C, strict framing,
  replay-window enforcement, authentication helpers, golden vectors, and a
  deterministic fake ESP/MCU stack.
- Explicit output interlock model that defaults every physical output off and
  refuses arming while hardware commissioning is incomplete.
- Bounded simulated trajectory validation, HMAC message tags, PCAP capture and
  replay support, and a completed one-million-input fuzz campaign.
- Pre-wiring tutorial for the owner-reported fully assembled/unwired robot,
  including routing order, connector allocation, power-domain topology,
  driver/sensor instructions, continuity records, and explicit hardware gates.
- Owner-selected direct USB primary transport, deferred ESP/USART2 path,
  USB-only logic bring-up checklist, and updated wiring/progress records.
- Windows USB enumeration evidence for the attached Octopus Pro V1.1 H723:
  CDC on COM4 with VID 0483 and PID 5740; firmware identity remains unverified.
- Passive COM4 observation evidence: the port opened successfully, no bytes
  were transmitted, and no unsolicited firmware banner appeared in 10 seconds.
- Outputs-disabled Octopus Pro V1.1 H723ZE USB CDC identity firmware with a
  pinned reproducible build, `0x08020000` vector verification, compile-time
  command-parser checks, bootloader-preserving microSD guide, and Windows
  identity verifier.
- Successful identity-image microSD installation and USB verification on COM4,
  including the expected descriptor, exact identity/status responses, and
  fail-closed rejection of `MOVE`.
- Separate output-disabled H723 safe-core image with independent watchdog,
  heartbeat timeout logic, CRC-checked dual-slot selection, bounded event log,
  USB diagnostics, reproducible packaging, and a prepared post-flash verifier.
- Successful safe-core microSD installation and COM4 verification, including
  watchdog-ready/config diagnostics, heartbeat response, `NOT_COMMISSIONED`
  state, and fail-closed rejection of `MOVE`.
- Separate, unflashed `0.3.0-service-core` image with fixed-buffer canonical
  binary USB framing, CRC32C/COBS runtime self-test, 64-frame replay rejection,
  fixed diagnostic/NACK payloads, and compiled-out motion/power outputs.
- H723 dual-sector flash persistence for safe configuration and append-only
  events, with 32-byte aligned records, CRC validation, inactive-slot-first
  commits, interrupted-write recovery tests, and a hard application/storage
  linker boundary at `0x08040000`.
- Reproducible service-core packaging plus a read-only post-flash verifier for
  watchdog/storage readiness, flash boundaries, replay rejection, and blocked
  `MOTOR_ENABLE`.
- Separate, unflashed J6-only diagnostic image for the first low-energy motor
  observation: exact MOTOR5 pins, TMC2209 UART/version/fault validation, 250 mA
  RMS, 160 microsteps, arm token, one jog per boot, automatic disable, IWDG,
  reproducible packaging, and an execution-gated COM4 verifier.
