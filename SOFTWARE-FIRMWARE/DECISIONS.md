# Architecture decisions

## ADR-001: Keep implementation separate from the official checkout

- **Status:** Accepted
- **Decision:** `SOFTWARE-FIRMWARE` is a nested, independently versioned
  project. The parent checkout remains the immutable official source snapshot
  at `77597de127a844990965189f0e6062e2551a2842`.
- **Reason:** The parent working tree contains owner-managed mechanical assets
  and historical tools. Isolation prevents accidental modification or an
  implied fork of unmodified upstream manufacturing data.

## ADR-002: Simulation-only first milestone

- **Status:** Accepted
- **Decision:** Implement protocol and deterministic fake transports before
  any H723 or ESP hardware output code.
- **Safety impact:** No produced artifact can energize a motor, contactor, or
  gripper. Hardware-facing phases remain blocked by HV-01 through HV-07.

## ADR-003: Canonical explicit little-endian wire format

- **Status:** Accepted
- **Decision:** Use the plan's COBS-delimited body, CRC32C, strict maximum
  lengths, explicit little-endian fields, and a 64-packet replay window.
- **Reason:** A single codec and golden-vector set can be shared by Python,
  H723, and ESP implementations without raw structure casts.

## ADR-004: Standard-library Python core

- **Status:** Accepted for the first milestone
- **Decision:** The protocol and simulators depend only on Python 3.12's
  standard library.
- **Reason:** This gives deterministic offline tests before the pinned runtime
  and wheelhouse are assembled in Phase 7.

## ADR-005: Unknown hardware facts fail closed

- **Status:** Accepted
- **Decision:** Hardware gate values are `UNVERIFIED` until measurement evidence
  is attached. No provisional pin polarity, voltage, current, home direction,
  Servo42C response, or gripper endpoint can enable an output.

## ADR-006: Direct USB is the initial primary transport

- **Status:** Accepted by owner, 2026-08-17
- **Decision:** The Windows PC connects directly to the Octopus Pro USB-C
  device port. ESP8266 hardware, its 5 V supply, and USART2 remain disconnected.
  Phase 4 is deferred optional work. The H723 implements the canonical protocol
  over USB CDC first; the backend defaults to USB.
- **Safety impact:** USB may be used first only for controller identity and
  logic-only firmware work with the Octopus `POWER` and `MOTOR-POWER` plugs
  removed. The incomplete power-button circuit blocks PSU and actuator power.
  USB does not bypass HV-01, output-safe firmware requirements, or the physical
  E-stop/contactor gates.

## ADR-007: Preserve the BTT bootloader for identity firmware

- **Status:** Accepted with owner authorization, 2026-08-17
- **Decision:** Build the identity-only STM32H723ZE application for the
  official 128 KiB bootloader offset and install it through microSD. Do not use
  direct USB DFU for this milestone because BTT documents that DFU overwrites
  the SD recovery bootloader.
- **Scope:** This exception permits only the `safe_identity` image, whose
  application configures no GPIO output and accepts no motion command. It does
  not close HV-01 or authorize 24 V, actuator power, or motion firmware.

## ADR-008: Reserve H723 sectors 2 and 3 for fail-safe persistence

- **Status:** Accepted for the unflashed service-core build, 2026-08-17
- **Decision:** Keep the BTT bootloader in sector 0, constrain the application
  to sector 1 (`0x08020000..0x0803FFFF`), and dedicate sectors 2 and 3 to
  alternating config/event slots. A new config is CRC-validated and programmed
  into the inactive sector before the previous valid sector can be reused.
- **Safety impact:** Only an uncommissioned, hardware-output-disabled config is
  accepted. Invalid, partial, unsafe, or absent records fail closed. Flashing
  and the first-boot storage write still require explicit owner authorization;
  this decision does not authorize 24 V or motion.
