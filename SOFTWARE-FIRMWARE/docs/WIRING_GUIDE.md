# PAROL6 Matt J - full wiring tutorial

**Revision:** 0.3 - owner-reported wired and motion-proven, USB-first, 2026-08-17
**As-built status:** Owner reports all wiring complete, the E-stop removes main
24 V, only the Octopus `POWER` input is used, and the J6 bounded test passed.
The ESP is intentionally deferred in favor of direct PC-to-Octopus USB. This
has not yet been independently inspected or reconciled to the connector record.
**Safe boundary:** This tutorial covers planning, low-voltage harnessing,
termination, and unpowered verification. It does not authorize mains work,
energizing an actuator, flashing attached hardware, or moving the robot.

> [!CAUTION]
> This is experimental robotic hardware, not a certified machine or safety
> system. Unexpected motion, pinch/crush, heat, electrical, and stored-energy
> hazards remain. A qualified person must finalize mains wiring, protective
> earth, the DC-rated contactor/E-stop circuit, conductor sizing, fuses, and
> enclosure. Never work on wiring with power applied.

## 1. Read this before touching a wire

The approved target differs from the stock PAROL6 electrical design:

- Controller: BIGTREETECH Octopus Pro V1.1 with STM32H723.
- J1/J2: MKS SERVO42C boards at the motors; Octopus MOTOR0/MOTOR1 supply only
  logic signals through a purpose-built keyed interface. No plug-in drivers and
  no use of the MOTOR0/MOTOR1 A/B motor terminals.
- J3-J6: BTT TMC2209 V1.3 modules in MOTOR2-MOTOR5.
- Home inputs: J1/J4/J6 inductive sensors through a verified isolated 24 V to
  3.3 V interface; J2/J3/J5 mechanical NC switches directly to 3.3 V endstop
  inputs.
- Communications: direct PC-to-Octopus USB-C is the initial primary path.
  ESP8266/USART2 is deferred and must remain disconnected. Servo42C telemetry
  remains on a separately qualified USART3 arrangement.
- E-stop: owner reports it removes the main 24 V supply. USB data may remain
  connected, but the VUSB jumper stays removed whenever external 24 V is used.
- Gripper: the final plan calls for a positional servo, but the actual device
  is not yet identified. Do not wire the gripper yet.

Do **not** copy the terminal map on page 85 of the 2023 PAROL6 manual. It is for
the original PAROL6 control board with TMC5160 drivers and 24 V limit inputs,
not this Octopus/TMC2209/Servo42C build. The manual remains useful for physical
sensor locations and wrist-to-base cable routing.

## 2. Mandatory stop points

| Stop | You may proceed only when | Until then |
| --- | --- | --- |
| S0 - identify hardware | Section 3 photo/label packet is complete | Do not cut final wire lengths or set driver current |
| S1 - approve electrical design | HV-01 through HV-07 evidence and the as-built schematic are reviewed | No power of any kind |
| S2 - unpowered inspection | Every check in Section 15 passes | Do not insert actuator fuses |
| S3 - logic-only test | Firmware safe-output tests and logic measurements pass | Motor bus remains disconnected |
| S4 - contactor test | Exact contactor/coil/suppressor circuit is approved | Actuator branch fuses remain removed |
| S5 - actuator test | Phase 2/3 firmware and HIL exit criteria pass | No motor or gripper power |

If a step says **VERIFY**, record the result in `HARDWARE_VERIFICATION.md`.
Never replace a measurement with a wire-color assumption.

## 3. First task: hardware identification packet

Before routing conductors, photograph the following in good light with all text
legible. Include one overall photo and one close-up of every connector side.

1. Octopus top, bottom, `V1.1` marking, and full STM32 chip marking.
2. Both Servo42C boards: front, back, side terminals, firmware screen/menu, and
   any `V1.0`, `OC`, `485`, or other suffix.
3. All four TMC2209 boards, showing `V1.3`, top/bottom orientation, and solder
   bridges.
4. Every motor label, associated with its physical joint J1-J6.
5. All six home devices and their cable labels/colors.
6. Both sides and terminal legends of the four-channel optocoupler module.
7. ESP8266/OLED board front/back, ESP module marking, USB connector, and pin
   legends.
8. PSU complete label; every DC output terminal; any mains enclosure/entry.
9. E-stop contact blocks and terminal numbers; contactor/relay label, coil
   voltage, main-contact DC rating, and auxiliary contacts.
10. Every fuse holder/fuse, DC-DC converter, connector, fan, thermistor, and the
    exact gripper/servo label.

Also record:

- Whether the J1 mechanical rotation blocker is installed.
- Whether covers can be removed without disturbing belts or bearings.
- Existing cable passages and the free inside diameter of tight passages.
- Whether a Servo42C is already physically mounted to J1/J2.

### Known configuration conflict to resolve

The retained project knowledge lists J3-J5 as `5-17HS15-1504S-X1` and J6 as
`17HE08-1004S`; the later final plan lists J3-J5 as `17HS16-2004S` and J6 as
`17HS08-1004S`. The later plan is the software target, but **the installed motor
labels win** for current, wire, and thermal design. The older knowledge also
mentions an MG90S continuous-rotation gripper while the final plan requires a
positional 180-degree servo. The exact installed gripper decides the circuit and
software behavior.

## 4. Tools and materials

Required before starting:

- True-RMS digital multimeter with continuity, resistance, and DC voltage.
- Current-limited bench supply for component qualification; not for operating
  the assembled arm.
- Proper ferrule and connector crimp tools; terminal extraction tools.
- Flexible stranded copper wire with documented temperature/current rating.
- Twisted pair or paired conductors for UART and sensitive signals.
- Numbered heat-shrink or durable wire labels at both ends.
- Keyed locking connectors, strain relief, cable braid/tape, and grommets.
- Insulated fuse distribution and covered terminal blocks.
- ESD precautions for the Octopus and TMC modules.
- A wiring notebook or the record tables in this document.

Do not select final conductor gauges or fuse ratings from a generic chart. They
depend on HV-07: actual motor current, bundle derating, flex, connector ratings,
length, PSU fault current, and fuse time-current curves.

## 5. Make the robot safe for routing

1. Disconnect and remove every power source, USB cable, battery, and fuse.
2. Lock the arm in a supported, gravity-safe pose. Do not rely on unpowered
   motors or friction to hold J2/J3.
3. Mark the current relationship of covers, pulleys, and cable passages.
4. Remove only the covers needed for a continuous wrist-to-base route.
5. If the completed assembly blocks cable routing, partially disassemble it in
   the official sequence. Do not force a fish tape around bearings, belts, or a
   sharp printed edge.
6. Confirm the J1 rotation blocker exists before any cable enters the base.

The official assembly sequence is wrist/J6 -> forearm/J5 -> elbow/J4 -> upper
arm/J3 -> shoulder/J2/J1 -> base. Because the robot was assembled unwired,
partial disassembly is likely necessary. Routing in the opposite direction can
trap conductors or produce an unserviceable bundle.

## 6. Harness naming and labeling

Use these circuit IDs at both ends. Add connector pin numbers after the exact
connectors are selected.

| ID | Route | Conductors/function | Status |
| --- | --- | --- | --- |
| J6-M | J6 motor to Octopus MOTOR5 | Two measured coil pairs, four conductors | Route after motor ID |
| J6-H | J6 inductive sensor to optocoupler | +24 V, 0 V, NPN signal | Route after sensor ID |
| J5-M | J5 motor to Octopus MOTOR4 | Two measured coil pairs, four conductors | Route after motor ID |
| J5-H | J5 mechanical NC switch to STOP1 | COM/GND and NC/signal | Owner-verified as-built assignment; may route unpowered |
| J4-M | J4 motor to Octopus MOTOR3 | Two measured coil pairs, four conductors | Route after motor ID |
| J4-H | J4 inductive sensor to optocoupler | +24 V, 0 V, NPN signal | Route after sensor ID |
| J3-M | J3 motor to Octopus MOTOR2 | Two measured coil pairs, four conductors | Route after motor ID |
| J3-H | J3 mechanical NC switch to STOP4 | COM/GND and NC/signal | Owner-verified as-built assignment; may route unpowered |
| J2-P | Switched bus to J2 Servo42C | +24 V and 0 V, individually fused | Gauge/fuse blocked by HV-07 |
| J2-C | Octopus interface to J2 Servo42C | COM, STEP, DIR, EN | Interface blocked by HV-02 |
| J2-T | Servo42C telemetry | TX, RX, logic GND | Blocked by HV-02 |
| J2-H | J2 mechanical NC switch to STOP2 | COM/GND and NC/signal | Owner-verified as-built assignment; may route unpowered |
| J1-P | Switched bus to J1 Servo42C | +24 V and 0 V, individually fused | Gauge/fuse blocked by HV-07 |
| J1-C | Octopus interface to J1 Servo42C | COM, STEP, DIR, EN | Interface blocked by HV-02 |
| J1-T | Servo42C telemetry | TX, RX, logic GND | Blocked by HV-02 |
| J1-H | J1 inductive sensor to optocoupler | +24 V, 0 V, NPN signal | Route after sensor ID |
| TOOL | Wrist tool to base | PWM, +5 V, 0 V; add spare conductor | Blocked by HV-05 |
| NTCn | Temperature point to Octopus | Dedicated two-wire sensor pair | Sensor type must be recorded |

Leave a service loop at each moving joint without allowing it to enter a belt,
gear, bearing, or hard stop. Move each joint slowly by hand through only its
safe mechanical range while observing the loose bundle. Do not use J6 or J1 as
continuous-rotation joints.

## 7. Cable routing order

Route one labeled group at a time:

1. From wrist: J6-M, J6-H, TOOL, and any wrist NTC/spare pair.
2. Add at forearm: J5-M and J5-H.
3. Add at elbow: J4-M and J4-H.
4. Add at upper arm: J3-M and J3-H.
5. Add at shoulder/base: J2 and J1 circuits.
6. Separate motor-power/phase conductors from UART, sensor, encoder, and NTC
   conductors wherever the geometry permits.
7. Bundle progressively with soft harness tape; do not cinch tightly at flex
   points.
8. Protect every printed-edge transition with a grommet or abrasion sleeve.
9. Leave at least the official manual's approximate 8 cm service length beyond
   the base, then increase it if the new terminal layout needs more.
10. Photograph every hidden route before replacing covers.

Do not reuse the original manual's two board-route-hole grouping blindly; the
Servo42C power/control/telemetry bundle is different from the original four-wire
J1/J2 motor harness.

## 8. Low-voltage power architecture

The qualified electrician supplies a protected, enclosed 24 V DC output and
protective earth arrangement. From the DC side onward, the target topology is:

```text
24 V PSU +
  +-- main fuse --> E-stop switching --> SWITCHED 24 V BUS --> Octopus POWER VIN
  |                                                 +--> J1 Servo42C branch fuse
  |                                                 +--> J2 Servo42C branch fuse
  |                                                 `--> gripper branch (deferred)
  +-- [DEFERRED] ESP fuse --> dedicated 5 V buck --> ESP 5 V input
  +-- fan fuse --> dedicated/approved supply --> enclosure fan (voltage TBD)
  `-- any implemented switching/contactor control must remain independently reviewed

24 V PSU 0 V
  `-- covered star distribution --> Octopus grounds, buck grounds, Servo42C grounds
```

Rules:

- The owner-selected as-built configuration uses only Octopus `POWER` for the
  controller and installed TMC drivers. `MOTOR-POWER`, `BED-POWER`, and the bed
  output remain empty.
- The source-selection jumper for each installed TMC2209 slot must select
  `POWER`. Verify orientation against
  the official V1.1 pin image with a meter; do not infer left/right after the
  board is mounted.
- Do not bridge `POWER` to the unused `MOTOR-POWER` or `BED-POWER` inputs.
- Logic and actuator 0 V share a deliberate star/common reference required by
  this design; do not use the robot structure as a current return.
- The owner reports the E-stop now removes main 24 V. The switching device and
  contacts must be correctly DC rated for the actual load and inrush.
- The HE3/PB11 output is only a low-side coil-enable path after coil current,
  MOSFET rating, suppression, and reset behavior are reviewed.
- Fit the manufacturer-approved coil suppressor at the coil. Diode polarity is
  critical for a DC coil.
- Do not connect AC mains from this tutorial. Mains entry, switchgear,
  protective earth, and PSU primary wiring require a qualified person.

### Octopus screw terminals

Using the official V1.1 pin view, this build populates only `POWER` (`+` and
`GND`). `MOTOR-POWER` remains empty. Before inserting wire:

1. Identify the terminal by board marking and the official image.
2. Meter continuity from the selected ground terminal to board logic ground.
3. With all sources disconnected, verify there is no short across either input.
4. Terminate flexible wire with correctly sized ferrules; no solder-tinned ends
   under screw terminals.
5. Tug-test each conductor and fit a transparent terminal cover where possible.

## 9. Octopus connector allocation

This is the design target from the final plan. HV-01 must confirm it on the
owner's exact board before termination.

| Function | Octopus location | MCU signal | Connection rule |
| --- | --- | --- | --- |
| J1 control | MOTOR0 logic socket | PF13 STEP, PF12 DIR, PF14 EN | Keyed interface only; no driver or A/B terminal |
| J2 control | MOTOR1 logic socket | PG0 STEP, PG1 DIR, PF15 EN | Keyed interface only; no driver or A/B terminal |
| J3 motor/driver | MOTOR2 | PF11/PG3/PG5, UART PC6 | TMC2209 + measured coil pairs |
| J4 motor/driver | MOTOR3 | PG4/PC1/PA2, UART PC7 | V1.1 uses PA2 enable |
| J5 motor/driver | MOTOR4 | PF9/PF10/PG2, UART PF2 | TMC2209 + measured coil pairs |
| J6 motor/driver | MOTOR5 | PC13/PF0/PF1, UART PE4 | TMC2209 + measured coil pairs |
| J1 home | STOP0 | PG6 | Optocoupler Q1 only |
| J2 home | STOP2 | PG10 | Mechanical NC dry contact; owner-verified as built |
| J3 home | STOP4 | PG12 | Mechanical NC dry contact; owner-verified as built |
| J4 home | STOP3 | PG11 | Optocoupler Q2 only |
| J5 home | STOP1 | PG9 | Mechanical NC dry contact; owner-verified as built |
| J6 home | STOP5 | PG13 | Optocoupler Q3 only |
| Contactor feedback | STOP7 | PG15 | Optocoupler Q4 only |
| USB PC link | Board USB-C device port | PA11 D- / PA12 D+ | Primary control/service transport; use a data cable |
| ESP link | Raspberry Pi header | PD5 TX / PD6 RX | DEFERRED; leave UART2 and ESP power disconnected |
| Servo telemetry | USART3/SPI3 header | PD8 TX / PD9 RX | Only after Servo42C qualification |
| Gripper PWM | Probe servo signal | PB6 | Signal only; do not use header 5 V for servo power |
| Motor-bus sense | Power-Det | PC0 | Protected external divider only |
| NTC 0-3 | T0-T3 | PF4-PF7 | Known 100 kOhm NTCs only |
| Contactor inhibit | HE3 | PB11 | Reviewed coil circuit only |
| DO1/DO2 | FAN0/FAN1 | PA8/PE5 | Low-side outputs; loads/flyback/fuses documented |

STOP connectors on the official V1.1 image are three-pin `signal / GND / 5 V`
groups. For the mechanical switches, use only the verified signal and GND pins;
leave 5 V unconnected. Never apply 24 V to a STOP pin.

## 10. Install J3-J6 TMC2209 drivers

Do this only with every power source and USB cable removed.

1. Identify each module as BTT TMC2209 V1.3. Photograph both sides.
2. Install a non-shorting heatsink as BTT directs; keep it clear of pins.
3. On the Octopus, select UART mode for MOTOR2-MOTOR5 using the slot jumper
   positions verified from the V1.1 schematic.
4. Leave every DIAG-to-endstop/sensorless-homing jumper removed. Physical home
   sensors are authoritative.
5. Set each driver-voltage source jumper to the populated `POWER` rail.
6. Orient each module by matching **VM to VM, GND to GND, STEP to STEP, and DIR
   to DIR**. Do not orient it from the potentiometer or heatsink position.
7. Insert modules only in MOTOR2, MOTOR3, MOTOR4, and MOTOR5.
8. Leave MOTOR0 and MOTOR1 empty.
9. Do not adjust Vref as a substitute for firmware/UART configuration. Initial
   current settings remain software-gated and depend on the actual motor labels.

For each J3-J6 motor, find its two coil pairs with the ohmmeter:

1. Disconnect the motor from everything.
2. Test wire pairs. A coil pair shows low, finite resistance; unrelated wires
   show open circuit.
3. Record pair A and pair B without assigning direction polarity yet.
4. Connect one complete pair to the slot's A1/A2 output and the other complete
   pair to B1/B2. On the official pin view the motor connector is labeled
   `A1 A2 B2 B1`; verify the mounted-board viewing direction before crimping.
5. Never combine one wire from each coil into a phase pair.

Changing the order within one complete pair reverses direction and is handled
only during mechanically isolated, low-energy commissioning. Wire color is not
evidence of a phase pair.

## 11. J1/J2 Servo42C wiring

### Do not terminate this section until HV-02

The boards may be standard TTL, open-collector (`OC`), or RS-485 variants, and
their input circuit differs. The available reference manual also distinguishes
V1.0/V1.1 hardware and firmware. First identify the exact suffix and firmware.

The common architecture is:

- The motor's two measured coil pairs connect locally to Servo42C `A+/A-` and
  `B+/B-`.
- Servo42C `V+` and `Gnd` receive individually fused switched 24 V.
- Octopus MOTOR0/MOTOR1 motor A/B terminals remain unused.
- `STEP`, `DIR`, `EN`, and `COM` use a keyed interface board, reset-safe pull
  states, series resistance/buffering as approved, and test points.
- UART/RS-485 telemetry uses the interface appropriate to the exact module; it
  is not assumed from the connector shape.

For a verified TTL Servo42C, the reference connector names are `3V3, G, TX,
RX`. The module's `3V3` pin is an output/reference and must remain unconnected;
do not tie two module regulators together. Cross TX to RX, connect logic ground,
and verify idle voltage before connecting the Octopus. A shared addressed bus is
permitted only after address and bus-contention tests.

The final interface drawing must record:

| Joint | Servo42C terminal | Octopus/interface destination | Verified voltage/polarity |
| --- | --- | --- | --- |
| J1 | V+ / Gnd | Fused switched bus / star 0 V | UNVERIFIED |
| J1 | Com / En / Stp / Dir | MOTOR0 keyed interface | UNVERIFIED |
| J1 | telemetry | Leave disconnected for current CR_OPEN configuration | DISABLED BY OWNER |
| J2 | V+ / Gnd | Fused switched bus / star 0 V | UNVERIFIED |
| J2 | Com / En / Stp / Dir | MOTOR1 keyed interface | UNVERIFIED |
| J2 | telemetry | Leave disconnected for current CR_OPEN configuration | DISABLED BY OWNER |

Do not use loose Dupont leads in the completed robot. Do not power a Servo42C
until magnet alignment, phase pairing, motor current, firmware menu, enable
polarity, and reset-safe behavior are recorded.

## 12. Home sensors and contactor feedback

### Mechanical NC switches: J2, J3, J5

With power disconnected:

1. Identify the switch `COM`, `NC`, and `NO` markings with the meter.
2. Verify COM-NC is closed at rest and opens when the mechanism actuates.
3. Connect COM to Octopus GND and NC to the assigned STOP signal.
4. Insulate NO and leave it unused.
5. Route as a paired cable away from motor phases.
6. Record both normal and actuated resistance at the base end.

Owner-verified as-built assignments: J2 -> STOP2, J3 -> STOP4, J5 -> STOP1.

### NPN-NO inductive sensors: J1, J4, J6

These must never connect directly to the Octopus. Bench-qualify the exact
four-channel isolated module first with its output disconnected from Octopus.

Expected sensor-side circuit, subject to the printed terminal legend:

```text
sensor brown --> protected +24 V sensor supply
sensor blue  --> sensor-side 0 V
sensor black --> optocoupler channel input negative/sink terminal
channel input positive --> protected +24 V sensor supply
```

Expected Octopus-side circuit, only after measuring both states:

```text
module output VCC --> Octopus 3.3 V
module output GND --> Octopus logic GND
Q1 --> STOP0 / J1
Q2 --> STOP3 / J4
Q3 --> STOP5 / J6
Q4 --> STOP7 / contactor auxiliary feedback
```

Acceptance before connection:

- Confirm the module is the 24 V-input/3.3 V-output variant.
- With 24 V only on the isolated input side, measure output-side pin voltages
  inactive and active using a separate current-limited 3.3 V source.
- Confirm no output exceeds the H723 input range.
- Record whether active is high or low.
- Confirm input/output isolation is not defeated by an accidental common.

The contactor auxiliary contact is treated like the fourth isolated input. It
must prove the physical contactor state; the software command alone is not
feedback.

## 13. Direct USB primary link; ESP8266 deferred

The PC connects to the Octopus USB-C **device** connector, not the adjacent
USB-A host/OTG connector. Keep the VUSB jumper removed whenever external 24 V
is connected. The `0.5.0-commissioning` image and GUI provide bounded,
token-gated single-joint tests plus debounced digital and raw analog readings.
They do not provide coordinated motion, homing, or automatic movement.

### Deferred ESP8266 bridge

The owner deferred this path on 2026-08-17. Leave ESP power and the Octopus
USART2 pins disconnected. If Phase 4 is resumed later, permanent wiring remains
blocked by HV-03 because GPIO15 is a boot-strap pin and the integrated OLED
board's actual pin use must be identified.

Target data connections after verification:

| Octopus | ESP target | Rule |
| --- | --- | --- |
| PD5 / USART2 TX | GPIO13 / UART0 RX after swap | Cross TX to RX, 3.3 V only |
| PD6 / USART2 RX | GPIO15 / UART0 TX after swap | Cross RX to TX, preserve boot state |
| GND | GND | Required signal reference |

Power the ESP from its own fused 5 V buck through one approved input. Do not
power it from an Octopus GPIO/header rail, and prevent USB-C/backfeed when a PC
is attached for programming. Use a short ground-paired UART cable separated
from motor phases. Final series resistance is chosen from signal-integrity
measurement, not guessed.

## 14. Gripper, voltage sense, temperature, fan, and spare I/O

### Gripper - leave disconnected

Do not wire until the exact model proves whether it is positional or continuous
rotation and its allowed voltage/stall current are measured. The positional
target circuit is:

```text
switched 24 V bus -> branch fuse -> 5 V buck sized above measured stall current
5 V / 0 V -> servo power
Octopus PB6 -> protected PWM signal
servo 0 V -> designed logic star reference
```

Never power the servo from the Octopus Probe 5 V pin or the ESP regulator.

### Motor-bus voltage input - do not connect yet

The plan proposes a protected 100 kOhm/10 kOhm divider with filtering to PC0,
but component voltage ratings, ADC protection, ground reference, and a two-point
calibration must be reviewed first. A bare resistor pair dangling from 24 V is
not acceptable.

### Temperature inputs

Only known, documented 100 kOhm NTCs may connect to T0-T3. Record each sensor's
beta or Steinhart-Hart data and cable ID. A detached/open/shorted sensor must be
detectable. Proposed locations are J1 motor/Servo42C, J2 motor/Servo42C, the
hottest J3/J4 or nearby PLA+ point, and TMC enclosure/heatsink air.

### Fan

Verify whether the actual enclosure fan is 5 V, 12 V, or 24 V before connecting
it. Because the owner-selected E-stop removes main 24 V, do not describe any
fan on that supply as always-on. Never rely on the upstream BOM name when the
installed label is available.

### Generic outputs

FAN0/FAN1 are low-side outputs, not voltage-free relay contacts. Do not connect
a load until its voltage/current, fuse, flyback path, and desired fail-off state
are documented.

## 15. Unpowered inspection and continuity record

Complete every line before logic power. Record actual measurements, not just a
check mark.

| Test | Required result | Measured/result |
| --- | --- | --- |
| All power removed | No PSU, USB, bench supply, or stored bus voltage | PENDING |
| J1 blocker | Installed and mechanically effective | PENDING |
| Harness abrasion | No conductor touches sharp edge/moving belt/gear | PENDING |
| Strain relief | Every moving transition and base exit supported | PENDING |
| J1-J6 coil pairs | Two finite-resistance pairs per motor, recorded | PENDING |
| Cross-coil isolation | Open between unrelated phase pairs | PENDING |
| Motor to structure | Open circuit | PENDING |
| Mechanical switches | NC at rest, open when actuated | PENDING |
| Inductive circuits | Isolated and not connected to MCU yet | PENDING |
| Positive-to-ground resistance | No short on logic or switched bus | PENDING |
| Switched/unswitched positive | No unintended continuity | PENDING |
| 24 V to MCU signals | Open; no direct path | PENDING |
| Octopus driver orientation | VM/GND/STEP/DIR checked per slot | PENDING |
| Driver source jumpers | MOTOR2-MOTOR5 select POWER | OWNER VERIFIED; J6 MOTION PASS |
| DIAG jumpers | Removed for MOTOR2-MOTOR5 | PENDING |
| MOTOR0/MOTOR1 | No plug-in driver; A/B outputs unused | PENDING |
| BED-POWER/bed/heaters | Unused and insulated | PENDING |
| Fuse values | Approved against wire/connector/load and recorded | PENDING |
| Polarity | Every connector checked end-to-end | PENDING |
| Connector retention | Every crimp/ferrule passes tug test | PENDING |

Create an as-built connector table before closing the base:

| Connector ID | Pin | Signal | From | To | Wire label/color | Verified by |
| --- | ---: | --- | --- | --- | --- | --- |
| Example only | 1 | J5 switch NC | J5-H | STOP1 signal | Record actual | PENDING |

## 16. First-power sequence - future hold point

Do not perform this sequence until the as-built photos, measurements, schematic,
and HV-01 through HV-07 have been reviewed and the safe firmware phase permits
it.

1. Remove all actuator branch fuses and physically disconnect the switched
   motor bus from loads.
2. Use a current-limited source to test each DC-DC converter independently with
   dummy loads.
3. Apply logic power only; verify Octopus 24 V input, 5 V, and 3.3 V rails.
4. Confirm zero STEP transitions, driver enables inactive, gripper PWM off, and
   HE3/contactor request off through reset/boot.
5. Test each home input manually at logic level; inductive channels remain a
   bench/HIL test until HV-04 passes.
6. Test direct USB communications without any actuator supply. ESP testing is deferred.
7. Test the contactor coil with all actuator branch fuses removed and verify
   auxiliary feedback, E-stop dropout, and no automatic re-energization.
8. Only after firmware/HIL approval, add one actuator branch at a time under
   mechanical support and the Phase 8 10% commissioning limits.

## 17. What to send for the next wiring checkpoint

Send the Section 3 photo packet plus these measurements:

- Resistance of both coil pairs for J1-J6.
- Mechanical switch COM/NC/NO truth table.
- PSU label and measured output with no robot attached, if already safely
  measured by a qualified person.
- Contactor coil resistance and full datasheet/part number; do not infer DC
  contact ratings from an AC marking.
- Optocoupler input/output truth-table setup and measurements.
- Actual available wire sizes, insulation ratings, connector families, fuse
  holders, and fuse values.

After review, this document will be revised with the exact wire/fuse schedule,
  connector part numbers, Servo42C interface schematic, and signed-off as-built
  pin table.

## 18. Source boundary

- Official local PAROL6 checkout at
  `77597de127a844990965189f0e6062e2551a2842`: mechanical routing and sensor
  locations only where compatible with this build.
- PAROL6 assembly manual v1.2 rev1 (2023-10-27): wrist-to-base order, cable
  passages, sensor physical locations, and J1 blocker warning.
- BTT Octopus Pro repository at
  `60a01f412959b62c349ba00da15b45232b7d90c5`: V1.1 schematic and official
  pin image.
- BTT TMC2209 V1.3 repository/manual: pin identity, UART capability, electrical
  limits, and installation precautions.
- MKS Servo42C repository at
  `31471153111fc991fb6f4e6cab2690912b2f79a5`: variant-specific reference only;
  actual board and firmware evidence remain authoritative.
