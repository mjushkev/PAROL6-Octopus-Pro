# PAROL6 retained project knowledge

## Purpose and provenance

This file is the durable, project-local reference produced after a complete
review of the official Source Robotics PAROL6 repository. It is an orientation
and conflict map, not a replacement for the source files.

- Upstream: `https://github.com/Source-Robotics/PAROL6-Desktop-robot-arm.git`
- Branch: `main`
- Commit: `77597de127a844990965189f0e6062e2551a2842`
- Commit date: `2026-07-07T12:27:20+02:00`
- Commit subject: `BOM update`
- Nearest description: `V1-38-g77597de`
- Local acquisition date: 2026-07-21, America/Toronto
- Audit: 7,849 tracked files; 7,670 text files; 641,352 text lines; 179 binary
  files; zero read errors.
- PDF audit: all 101 pages of the assembly manual plus the one-page BOM,
  print-table, and mounting-plate PDFs were text-extracted and visually
  reviewed.
- Binary audit: all five XLSX workbooks were scanned cell-by-cell; all 48 STL
  meshes were parsed facet-by-facet for geometry bounds; repository images were
  visually reviewed; the ST-Link ZIP archive was inventoried.

The complete source remains in this checkout, so future work can retrieve exact
details without depending on conversational memory.

## Source precedence and version boundaries

Use this order unless a task explicitly targets an older artifact:

1. Current source or current Markdown file at the pinned commit.
2. Current official documentation linked from the repository.
3. Current STL/CAD/manufacturing file for geometry.
4. Assembly manual version 1.2 rev1, dated 2023-10-27, for assembly sequence and
   pictorial routing.
5. `BOM/BOM_PDF_Legacy.pdf` only for legacy comparison.
6. `PAROL6 control board test code` only for board diagnostics, never as the
   production motion-control authority.

The assembly manual explicitly says that the GitHub BOM, STL files, printing
table, and documentation are the more current external references. Do not
assume values in the 2023 manual override current firmware or the current BOM.

## Owner-selected build configuration

This section is authoritative for the robot being built in this project. These
are deliberate owner selections and take precedence over upstream PAROL6
hardware and printing choices whenever they conflict. Preserve the upstream
values elsewhere in this document for comparison and design adaptation.

### Electronic hardware

| Function | Selected hardware |
| --- | --- |
| Main controller | BIGTREETECH Octopus Pro V1.1 H723 |
| Host computer | Owner's personal computer |
| Host-computer role | GUI, inverse kinematics, trajectory generation, and other high-level processing |
| Joint 1 motor | NEMA 17 `3-17HE19-2004S` |
| Joint 1 motor control | MKS SERVO42C with magnetic encoder feedback |
| Joint 2 motor | NEMA 17 `3-17HE19-2004S` |
| Joint 2 motor control | MKS SERVO42C with magnetic encoder feedback |
| Joint 3 motor | NEMA 17 `5-17HS15-1504S-X1` |
| Joint 3 driver | BIGTREETECH TMC2209 |
| Joint 4 motor | NEMA 17 `5-17HS15-1504S-X1` |
| Joint 4 driver | BIGTREETECH TMC2209 |
| Joint 5 motor | NEMA 17 `5-17HS15-1504S-X1` |
| Joint 5 driver | BIGTREETECH TMC2209 |
| Joint 6 motor | NEMA 17 `17HE08-1004S` |
| Joint 6 driver | BIGTREETECH TMC2209 |
| Gripper | Deferred from the current control-software scope; planned MG90S 360-degree servo motor |

### Owner-verified as-built home-input routing

The assembled robot's three mechanical home-switch cables are intentionally
handled in firmware with this verified physical routing. This overrides older
plan and guide tables that assigned J2/J3/J5 sequentially to STOP1/STOP2/STOP4.

| Physical joint switch | Octopus connector | MCU pin | Firmware logical input |
| --- | --- | --- | --- |
| J2 | STOP2 | PG10 | J2 |
| J3 | STOP4 | PG12 | J3 |
| J5 | STOP1 | PG9 | J5 |

Owner observation on 2026-08-25 before remapping: physical J2 appeared as J3,
physical J3 appeared as J5, and physical J5 appeared as J2. Firmware 0.8.5 and
later apply the inverse mapping so the host software sees the correct joint.

**Owner homing amendment — 2026-08-25:** J2 and J3 normally rest with their
mechanical home switches active. Firmware 0.8.6 and later must not accept an
active input alone as proof of home. It performs an active-start release,
bounded seek, backoff, and slow re-latch. The repeatable latch edge is 0 degrees
and automatically becomes the home-side soft limit for J2/J3. The operator
captures only the opposite travel limit. The calibration JSON retains the raw
home direction, raw logical-positive direction, derived joint-space home
direction, and automatic home-limit side.

Firmware 0.8.7 increases the active-start release ceiling from 5 degrees to 30
degrees. This is not a commanded fixed travel: motion remains opposite the
configured home direction and stops immediately when the debounced input
clears. Reaching the ceiling while still active stops homing with a fault.

This is a hybrid motor-control arrangement: J1-J2 use MKS SERVO42C hardware
with magnetic encoder feedback, while J3-J6 use TMC2209 drivers. The J1-J2
encoders close the loop at the motor shaft; they can improve motor-position
accuracy and missed-step detection/correction, but they do not directly measure
the joint output after the gearbox, belt, couplers, or backlash. Their exact
revision, encoder alignment/calibration procedure, step/direction behavior, and
available fault or position-reporting interface must be verified before the
production controller depends on them.

**Owner software amendment — 2026-08-19:** The installed J1/J2 Servo42C boards
are to be operated in local `CR_OPEN` mode for the current release. Encoder
polling, display, and encoder-dependent faulting are disabled. The encoder data
model and dormant integration code remain in the software so a future,
hardware-qualified re-enable does not require a protocol redesign.

The upstream PAROL6 production firmware targets the custom STM32F446RE PAROL
control board with six TMC5160 drivers, so its pin map, SPI driver setup, enable
logic, current configuration, homing behavior, and packet implementation are
reference material rather than directly deployable firmware for this build. A
controller-specific port and electrical verification are required before
energizing the selected hardware.

The owner will run the high-level software on a personal computer rather than a
dedicated Intel Compute Stick. Gripper support is intentionally excluded from
the current software implementation. A future MG90S 360-degree-servo gripper
phase must begin by verifying the exact servo variant and whether its command is
continuous speed/direction or absolute position; no upstream CAN-gripper
behavior should be assumed for it.

### Printer and material

| Item | Selected configuration |
| --- | --- |
| Printer | Bambu Lab P2S |
| Nozzle | 0.4 mm standard-flow nozzle |
| Material | SUNLU PLA+ 2.0 |

The owner's selected material is PLA+, while upstream requires/recommends PETG
because enclosed motors and gearboxes can become hot. For this build, do not
silently reuse upstream PETG temperature assumptions. Motor/driver current,
holding current, ventilation, enclosure temperatures, duty cycle, and measured
part temperatures must be validated for SUNLU PLA+ 2.0 before sustained or
high-load operation. The upstream print table may be used as a geometry and
strength starting point, but PETG temperatures and behavior do not transfer
directly to PLA+.

## Safety and licensing

- The repository describes PAROL6 as experimental engineering hardware, not a
  consumer product.
- Hazards include unexpected motion, pinch/crush points, motor and driver heat,
  electrical shock, property damage, serious injury, and death.
- Use an emergency stop, physical guarding, software limits, PPE, and qualified
  robotics/electrical practices.
- The 2023 manual says the system is not certified for CE, FCC, UL, RoHS, WEEE,
  or EU EMC compliance.
- README, firmware, STL files, and repository license state GPLv3. The 2023
  manual says the PAROL6 STEP files and the PAROL6 control board design are not
  open source. The URDF package separately declares BSD, which is a packaging
  inconsistency that must not be generalized to the whole repository.
- Read `SAFETY_WARNING_AND_DISCLAIMER.md` before physical work.

## Repository map

- `BOM/`: current Markdown BOM, legacy PDF, and 60 reference images.
- `Building instructions/`: 101-page assembly manual and PETG guidance.
- `STL/`: 41 manufacturing/end-effector STL files plus two mounting-plate STEP
  files and one mounting-plate drawing PDF.
- `Print table/`: source XLSX and rendered PDF for printing recommendations.
- `PAROL6 control board main software/`: current STM32F446RE production firmware
  and bundled libraries.
- `PAROL6 control board test code/`: factory/diagnostic firmware variant.
- `PAROL6_URDF/`: ROS package, URDF, inertial data, and seven meshes.
- `Extras/LEAP motion control code/`: Leap Motion UDP teleoperation,
  visualization, inverse kinematics, and a stripped commander/serial bridge.
- `en.stsw-link009.zip`: signed Windows ST-Link USB driver package for Windows
  7/8/10, 32/64 bit.

## Mechanical specification

Repository-linked docs describe:

- 6 revolute degrees of freedom.
- Approximately 400 mm reach with the standard gripper.
- Nominal payload: 1 kg near the base, 0.5 kg across the full workspace.
- Robot weight: approximately 5.5 kg.
- PETG printed structure.
- Approximately 40 W power consumption.
- Stepper motors with planetary and belt reductions.
- Open-loop position sensing by three mechanical limit switches plus three
  inductive sensors.

Kinematic dimensions used by the Leap code, in metres after conversion:

| Parameter | Millimetres | Metres |
| --- | ---: | ---: |
| a1 | 110.50 | 0.11050 |
| a2 | 23.42 | 0.02342 |
| a3 | 180.00 | 0.18000 |
| a4 | 43.50 | 0.04350 |
| a5 | 176.35 | 0.17635 |
| a6 | 62.80 | 0.06280 |
| a7 | 45.25 | 0.04525 |

The Leap DH alpha sequence is `[-pi/2, pi, pi/2, -pi/2, pi/2, pi]`.

## Current purchasing BOM

The authoritative purchasing list is `BOM/BOM.md`. Key counts:

### Fasteners

| Fastener | Quantity | Note |
| --- | ---: | --- |
| M4 x 10 mm | 20 | ISO 4762 socket head |
| M3 x 8 mm low head | 10 | 2 mm head height |
| M3 x 8 mm | 50 | ISO 4762 socket head |
| M3 x 14 mm | 30 | ISO 4762 socket head |
| M3 nut | 10 | BOM incorrectly repeats a screw standard |
| M3 x 25 mm | 10 | ISO 4762 socket head |
| M2 x 10 mm | 10 | ISO 4762 socket head |
| M3 x 16 mm | 10 | ISO 4762 socket head |
| M3 x 6 mm | 10 | Phillips pan head |
| M3 x 12 mm | 30 | ISO 4762 socket head |
| M3 x 35 mm | 10 | ISO 4762 socket head |
| M4 x 16 mm | 30 | ISO 4762 socket head |
| M4 x 14 mm | 10 | ISO 4762 socket head |
| M4 x 50 mm | 10 | ISO 4762 socket head |
| M3 x 15 mm | 2 | tapered/countersunk head |

### Motion hardware

- Two NEMA 17 EG precision planetary gearboxes, 20:1.
- One NEMA 17 EG precision planetary gearbox, 10:1.
- One NEMA 17 motor, 16 Ncm, 42 x 42 x 20 mm.
- Three NEMA 17 motors, 45 Ncm, 42 x 42 x 40 mm.
- Two NEMA 17 motors, 65 Ncm, 42 x 42 x 60 mm.
- Belts: one each HTD3-396, HTD3-342, HTD3-201, and HTD3-246; all 6 mm
  wide.
- Pulleys: two HTD3M 12-tooth, 5 mm bore, 10 mm width; one HTD3M 15-tooth,
  5 mm bore, 10 mm width.
- Three shaft couplers with 8 mm shaft holes and M4 mounting holes.

### Bearings

- One AXK3552, 35 x 52 x 4 mm, with both thrust washers/plates.
- Five NSK HR32906J tapered bearings for J3/J4/J5.
- Four NSK HR32907J tapered bearings for J1/J2.
- Twenty 3 x 8 x 4 mm ball bearings and twenty 3 x 10 x 4 mm ball bearings
  for belt tensioning.

### Electronics and pneumatics

- One PAROL6 control board and six TMC5160 driver modules.
- Two units of thermal cement only if installing drivers on a board without
  drivers.
- One 12 mm illuminated 3-6 V on/off button and one 200 mm four-pin JST-PH
  2.0 cable.
- One 40 mm 5 V fan; BOM names Noctua NF-A4x20 5V.
- One GX16 two-pin male/female power connector pair.
- One M8 female four-pin electric-gripper connector.
- One 24 V, 5 A power supply.
- Three ZW12-3 mechanical limit switches.
- One 4 mm NPN-NO inductive sensor, one GX-F8A sensor, and one M5 NPN-NO
  inductive sensor.
- One normally-closed E-stop and one PG7 cable gland.
- Four M3 x 6 mm brass inserts.
- Four PM-style pneumatic fittings for 4 mm tube.
- One MHZ2-16D pneumatic gripper, two PC4-M5 fittings, one 24 V 5/2 valve,
  and nominally 5 m of 4 x 2.5 mm tube.
- One female XT30, one ST-Link/programming adapter with pin order SWCLK,
  SWDIO, GND, 3V3, 5V, and one USB-B cable.
- Optional vacuum end effector: one suction cup, one vacuum generator, and one
  4-to-6 mm coupler.

Current BOM anomalies to preserve:

- The M3 nut row incorrectly cites DIN 912 / ISO 4762, which is a screw
  standard.
- The pneumatic-tube row says quantity `5 meters` but its description says
  `1 meter, 4x2.5mm`; procure 5 m unless current assembly requirements prove
  otherwise.
- Several reference images show closed-loop or cable-system components that are
  not rows in the current open-loop BOM. A reference image is not by itself a
  purchasing requirement.
- MG-series gearboxes are offered as cheaper substitutions with more backlash.

## Printing and fabrication

- Print structural parts in PETG. PETG guidance: hot end 220-250 C, heated bed
  60-90 C, layer fan, enclosure optional, adhesive may help.
- Manual printer reference: Prusa MK2S.
- The print table describes suggestions, not mandatory settings.
- Common table settings: honeycomb infill, 15% cooling, `0.2 LA` layer setting,
  six top layers, four bottom layers, and three perimeters.
- Per-part infill ranges from 15% to 80%; support requirements range from none
  to minimal, with the elbow marked as requiring substantial support.
- The spreadsheet has known mass for only 26 rows, totaling 1,312.65 g, and
  known print time for 24 rows, totaling 108.05 h. These are incomplete totals,
  not whole-robot filament/time estimates.
- `STL/mounting plates/small_base.pdf` is an A3, 1:2 drawing in millimetres. It
  calls out four 5.20 mm holes, 15 mm corner radii, a 10 mm central hole, eight
  M5 tapped holes 10 mm deep, and six M4 tapped holes 10 mm deep.

Printable STL geometry bounds are in millimetres:

| File | Triangles | X x Y x Z bounds (mm) |
| --- | ---: | --- |
| BASE/Electronics_case_v1_1.STL | 127600 | 109 x 59.8 x 130.5 |
| BASE/fan_cover.STL | 91082 | 53 x 59.8 x 22.5 |
| BASE/J1_bottom_lid.STL | 27992 | 62.316 x 62.850 x 5 |
| BASE/lid_electronics.STL | 47184 | 105.4 x 107.1 x 10.3 |
| BASE/main_base_blocker.STL | 375652 | 118 x 107 x 70.5 |
| ELBOW/Elbow_lid_1.STL | 198094 | 64.5 x 66.731 x 8 |
| ELBOW/Elbow_lid_2.STL | 8408 | 48 x 28.555 x 2.7 |
| ELBOW/Elbow_part.STL | 145072 | 64.5 x 102 x 107.2 |
| ELBOW/J4_bearing_backplate.STL | 32400 | 38 x 38 x 5.5 |
| ELBOW/J4_limiter.STL | 3452 | 10.334 x 11.719 x 6 |
| ESTOP/ESTOP_BOTTOM.STL | 57952 | 48 x 48 x 24 |
| ESTOP/ESTOP_TOP.STL | 33034 | 48 x 21 x 48 |
| FOREARM/HTD3_48_J5_pulley.STL | 185322 | 45 x 45 x 26.3 |
| FOREARM/J4_output_pulley.STL | 213736 | 45 x 45 x 60.35 |
| FOREARM/J5_belt_lid.STL | 362010 | 121 x 57 x 15.3 |
| FOREARM/J5_electronics_lid.STL | 20688 | 50.3 x 71.418 x 37.728 |
| FOREARM/J5_part.STL | 131560 | 133.8 x 71.418 x 74 |
| GRIPPER_ATTACHMENTS/Gripper_ARMS.STL | 11560 | 21 x 10.5 x 14 |
| Horizontal/Pneumatic_gripper_holder_custom.stl | 22990 | 55 x 55 x 20 |
| Horizontal/Pneumatic_gripper_holder_horizontal.stl | 21334 | 55 x 55 x 20 |
| GRIPPER_ATTACHMENTS/Pneumatic_gripper_holder.STL | 51280 | 54.1 x 54.1 x 38.6 |
| GRIPPER_ATTACHMENTS/vacuum_gripper_holder.STL | 43066 | 54.1 x 54.1 x 37 |
| SHOULDER/J1_backplate.STL | 44110 | 46 x 7 x 46 |
| SHOULDER/J1_belt_cover_rework.STL | 50008 | 114.927 x 179.92 x 20 |
| SHOULDER/J1_rotation_shaft.STL | 23140 | 35 x 40.7 x 35 |
| SHOULDER/J1_turret_motor_holder.STL | 53222 | 55.476 x 20 x 55 |
| SHOULDER/J1_turret_rework_blocker.STL | 207164 | 113.527 x 174.52 x 75 |
| SHOULDER/J1_wires_cover_rework.STL | 11564 | 8.827 x 105.8 x 6.5 |
| SHOULDER/J2_limit_switch_cover.STL | 12814 | 40.264 x 21.093 x 5 |
| SHOULDER/J2_stopper_block.STL | 20792 | 39.519 x 46.132 x 11.155 |
| UPPER_ARM/38_pulley_J3.STL | 168790 | 40 x 40 x 37.6 |
| UPPER_ARM/42_pulley.STL | 102994 | 15.598 x 41 x 41 |
| UPPER_ARM/J2_bearing_backplate.STL | 81802 | 47 x 5.8 x 47 |
| UPPER_ARM/J2_cover.STL | 89162 | 60 x 12 x 60 |
| UPPER_ARM/J2_shaft.STL | 40376 | 35 x 47.8 x 35 |
| UPPER_ARM/J3_limit_switch_cover.STL | 25872 | 40.432 x 32.289 x 7 |
| UPPER_ARM/Upper_arm.STL | 677342 | 102.033 x 230.7 x 28 |
| UPPER_ARM/Upper_arm_cover.STL | 39058 | 101.38 x 157.744 x 16.7 |
| UPPER_ARM/Upper_arm_wires_cover.STL | 23080 | 39.266 x 32.970 x 7 |
| UTILITY/connectors_4_alu_profiles.STL | 5804 | 30 x 30 x 30 |
| WRIST/wrist.STL | 273930 | 56.999 x 54.1 x 81 |

Some STL origins are far from zero, notably `Upper_arm_wires_cover.STL`; use
the mesh geometry and slicer placement rather than assuming a zeroed CAD origin.

## Assembly sequence and routing

Required build direction is wrist to base because all conductors and pneumatic
tubes pass through the arm:

1. Wrist/J6.
2. Forearm/J5.
3. Elbow/J4.
4. Upper arm/J3.
5. Shoulder/J2/J1 turret.
6. Base and electronics.
7. Belts, covers, selected gripper, and E-stop.

The manual states that another build order will not work reliably because of
internal routing.

### Wire preparation

All listed starting lengths are 1 m:

| Circuit | Conductors |
| --- | ---: |
| J1 motor | 4 |
| J1 sensor | 3 |
| J2 motor | 4 |
| J2 limit switch | 2 |
| J3 motor | 4 |
| J3 limit switch | 2 |
| J4 motor | 4 |
| J4 sensor | 3 |
| J5 motor | 4 |
| J5 limit switch | 2 |
| J6 motor | 4 |
| J6 sensor | 3 |
| Pneumatics | two 1 m tubes |
| Gripper cable | four conductors, arranged as two twisted pairs |

- Label joints and bundle progressively with cloth/fabric or insulating tape.
- Leave approximately 8 cm of conductor beyond the base for termination.
- Leave approximately 15 cm of pneumatic tube beyond the base.
- Gripper M8 pinout: pin 1 = 24 V, pin 2 = CANH, pin 3 = GND, pin 4 = CANL.
- Inductive-sensor colors: black signal, blue negative, brown positive.
- Button JST harness length: 80 mm.
- GX16-to-XT30 power harness: at least 110 mm; polarity must match the manual.
- Board-route hole 1: J1/J3/J5 steppers and limits 2/1/5.
- Board-route hole 2: J2/J4/J6 steppers, limits 6/3/4, CAN, and pneumatic
  tubes.

### Assembly practices

- The printed M3 pilot holes are intentionally about 2.7-2.8 mm so the screw
  forms threads directly in PETG.
- Do not repeatedly disassemble printed threads; cured cyanoacrylate can be
  used to restore a slipping printed hole before retapping.
- Use blue threadlocker for metal-to-metal fasteners and especially shaft
  couplers. Align a coupler set screw with the gearbox keyway; allow several
  hours to cure.
- Use the gearbox-supplied screws to attach gearbox to motor.
- Belt tension bearings must be installed in pairs. The manual permits mixed
  3 x 8 x 4 and 3 x 10 x 4 bearings on M3 screws roughly 14-20 mm long.
- If a belt is loose, additional paired tension bearings may be added.
- Do not spin J5 indefinitely without a gripper; internal routing and physical
  limits still apply.
- The J1 rotation blocker is mandatory; omitting it risks wire damage from more
  than one base rotation.
- Tools listed: screwdriver set, drill, soldering iron, Allen-key set, torque
  screwdriver, rubber hammer, hammer, and pliers.
- Consumables listed: blue threadlocker, lithium grease, insulating tape,
  solder, heat-shrink, wire harness, and cloth/fabric tape.

## Production firmware platform

Source: `PAROL6 control board main software/`.

- MCU/board target: generic STM32F446RE.
- Framework: Arduino through PlatformIO `ststm32`.
- Upload/debug: ST-Link.
- USB CDC VID/PID: 0x0483/0x5740.
- Configured serial speed: 3,000,000 baud.
- Configured development port: COM8.
- Six TMC5160 drivers, SPI at approximately 45 MHz (`SPI_CLOCK_DIV4` from a
  180 MHz source comment).
- Six AccelStepper instances.
- Microstepping: 32.
- Sense resistance: 0.075 ohm.

Current constants in production firmware:

| Joint | Max-current constant (mA) | Driver request uses 85% (mA) | Hold multiplier | Reduction |
| --- | ---: | ---: | ---: | ---: |
| J1 | 2000 | 1700 | 0.70 | 6.4 |
| J2 | 2000 | 1700 | 0.80 | 20 |
| J3 | 1900 | 1615 | 0.80 | 18.0952381 |
| J4 | 1700 | 1445 | 0.80 | 4 |
| J5 | 1700 | 1445 | 0.80 | 4 |
| J6 | 965 | about 820 | 0.88 | 10 |

The board-test firmware uses the higher un-reduced constants
`[2100,2100,2000,2000,2000,1000]`, but its diagnostic driver routine actually
requests only `260 * 0.85` mA. Do not copy test-firmware currents into the
production firmware.

### Joint configuration

| J | Standby steps | Homing offset | Allowed step endpoints | Limit input | Motor pins | Trigger | Reversed |
| --- | ---: | ---: | --- | --- | --- | ---: | ---: |
| 1 | 10240 | 13500 | -14000 to 14000 | LIMIT6 | DIR1/PUL1/SELECT1 | 0 | 1 |
| 2 | -32000 | 19588 | -51587 to -1200 | LIMIT2 | DIR6/PUL6/SELECT6 | 1 | 0 |
| 3 | 57905 | 23020 | 34700 to 92605 | LIMIT3 | DIR5/PUL5/SELECT5 | 1 | 1 |
| 4 | 0 | -10200 | -7500 to 7500 | LIMIT4 | DIR4/PUL4/SELECT4 | 0 | 0 |
| 5 | 0 | 8900 | -6400 to 6400 | LIMIT5 | DIR2/PUL2/SELECT2 | 1 | 0 |
| 6 | 32000 | 15900 | 0 to 64000 | LIMIT1 | DIR3/PUL3/SELECT3 | 0 | 1 |

The unusual motor/limit permutation is intentional in the current source and
must be preserved unless the board wiring is deliberately changed.

### Board I/O pin map

| Function | Pins |
| --- | --- |
| Step | J1 PC6, J2 PA10, J3 PC0, J4 PC3, J5 PC9, J6 PC5 |
| Direction | J1 PB15, J2 PA1, J3 PC1, J4 PA0, J5 PA8, J6 PB1 |
| Limit | J1 PC12, J2 PB3, J3 PA15, J4 PD2, J5 PB4, J6 PC11 |
| Driver select | J1 PC7, J2 PA9, J3 PC15, J4 PC2, J5 PC8, J6 PC4 |
| Global driver enable | PA3 |
| SPI | MISO PA6, MOSI PA7, SCK PA5, flash select PA4 |
| LEDs | PB2, PB10 |
| Supply latch/button | PC10, PC14 |
| Inputs | PB6, PB5 |
| Outputs | PC13, PB7 |
| E-stop | PB14 |
| Bus-voltage ADC | PB0 / ADC1 channel 8 |
| USB | D+ PA12, D- PA11 |

`iodefs.h` defines `CAN1TX`/`CAN1RX` twice: first PB9/PB8, then PB13/PB12.
The second pair wins preprocessor redefinition, but the low-level CAN setup
explicitly uses remap 2 and configures CAN1 on PB8/PB9 and CAN2 on PB12/PB13.
Treat the duplicated macros as a source defect, not a reliable board map.

Bus voltage uses a 110 kohm / 16 kohm divider, 3.3 V reference, and 12-bit
4095-count conversion, returning millivolts.

### Motion commands

| Command | Firmware behavior |
| ---: | --- |
| 69 | built-in repeatability/demo movement |
| 100 | home all joints |
| 101 | enable and reset homing state |
| 102 | disable motion commands |
| 103 | clear/reset homing/error state |
| 123 | velocity jog using `runSpeed()` |
| 156 | go-to-position streaming mode |
| 255 | dummy/idle packet; also continues an active home sequence |

The home sequence first handles J1/J2/J3, then J4, then J6, then J5, and finally
assigns configured standby positions. J5 uses the global pneumatic-gripper
offset 8900; the code comment gives 8035 for the SSG48 gripper.

Observed source defects that must be verified before relying on homing:

- `joint123_stage2 == 0;` and `joint123_stage1 == 1;` are comparisons whose
  results are discarded, not assignments.
- Position-range fields are populated but the main motion branches do not
  visibly enforce them before stepping.
- CRC bytes are present in both directions but the current parser does not
  calculate or validate a CRC.
- Timeout fields exist but the current main loop does not visibly implement the
  documented timeout shutdown.
- The E-stop state is reported and the Leap bridge reacts to it, but production
  firmware motion gating is not visibly based directly on E-stop input.

These are audit observations, not permission to patch safety-critical firmware
without a separate verification task.

## USB serial packet protocol

Both directions begin `FF FF FF`, then one length byte, and end `01 02`.
Multi-byte signed numeric values use big-endian two's-complement packing.

### PC to robot: length 52

| Payload offsets | Content |
| --- | --- |
| 0-17 | six commanded positions, three bytes each |
| 18-35 | six commanded velocities, three bytes each |
| 36 | command |
| 37 | affected-joint byte/bitfield |
| 38 | I/O command bitfield |
| 39 | timeout setting |
| 40-41 | gripper position |
| 42-43 | gripper speed |
| 44-45 | gripper current |
| 46 | gripper command |
| 47 | gripper mode |
| 48 | gripper node ID |
| 49 | nominal CRC byte; Leap bridge sends 228 |
| 50-51 | end bytes 01 02 |

### Robot to PC: length 56

| Payload offsets | Content |
| --- | --- |
| 0-17 | six current positions, three bytes each |
| 18-35 | six current speeds, three bytes each |
| 36 | six homed flags plus two set bits |
| 37 | two inputs, two outputs, E-stop, plus three set bits |
| 38 | six temperature-error flags plus two set bits |
| 39 | six position-error flags plus two set bits |
| 40-41 | timer ticks between received commands |
| 42 | timeout error |
| 43 | echoed current command |
| 44 | gripper ID |
| 45-46 | gripper position |
| 47-48 | gripper speed |
| 49-50 | gripper current |
| 51 | gripper status byte |
| 52 | object-detection value |
| 53 | nominal CRC byte, currently 212 |
| 54-55 | end bytes 01 02 |

## CAN and gripper protocol

- CAN setup requests 1 Mbit/s and remap 2.
- The standard 11-bit ID layout is four node-ID bits, six command-ID bits, and
  one error bit: `(node & 0xF) << 7 | (command & 0x3F) << 1 | error`.
- Command 60: gripper-to-board four-byte status packet.
- Command 61: board-to-gripper position/speed/current/action packet.
- Command 62: calibrate gripper.
- Command 1: clear gripper error.
- Status packet: byte 0 position, bytes 1-2 signed current, byte 3 status bits.
- Command packet: byte 0 position, byte 1 speed, bytes 2-3 signed current,
  byte 4 command/status bits.

## URDF/ROS model

Source: `PAROL6_URDF/PAROL6/urdf/PAROL6.urdf`.

- Seven physical links: base_link plus L1-L6; world is fixed to base_link.
- URDF physical-link mass sum: 3.169729443953587 kg, not the complete 5.5 kg
  assembled robot specification.
- Each actuated transmission has mechanical reduction 1 and a position-joint
  interface.
- Gazebo self-collision is enabled for L1-L6.
- URDF effort limits are 300 and velocity limits are 3 for every joint.
- URDF limits: L1 -1.7 to 1.7 rad; L2 -0.98 to 1 rad; L3 -2 to 1.3 rad;
  L4 -2 to 2 rad; L5 -2.1 to 2.1 rad; L6 is continuous.

URDF mesh bounds are in metres:

| Mesh | Triangles | X x Y x Z bounds (m) |
| --- | ---: | --- |
| base_link.STL | 60436 | 0.2275 x 0.125 x 0.0705 |
| L1.STL | 52358 | 0.122227 x 0.23822 x 0.0863 |
| L2.STL | 32414 | 0.07715 x 0.239982 x 0.193 |
| L3.STL | 2858 | 0.102 x 0.1074 x 0.0645 |
| L4.STL | 7108 | 0.086418 x 0.0923 x 0.19415 |
| L5.STL | 23468 | 0.056856 x 0.0975 x 0.0541 |
| L6.STL | 5762 | 0.082294 x 0.0583 x 0.040074 |

ROS packaging inconsistencies:

- The package is ROS2/ament (`ament_cmake`, rviz2), but the launch files use
  ROS1 XML conventions and `gazebo_ros_control`.
- Launch files request lowercase `parol6.urdf`, while the repository file is
  uppercase `PAROL6.urdf`; this fails on case-sensitive filesystems.
- URDF mesh paths use lowercase package `parol6`, while the exported CSV uses
  uppercase `PAROL6`.
- URDF limits differ materially from current documentation/firmware limits.
  Do not use them as hardware safety limits without reconciliation.

## Leap Motion extras

- `PAROL6_LEAP_code.py` reads Leap frames and sends UDP to 127.0.0.1:5001.
- Hand position mapping is `x=-palm_z/2000`, `y=-palm_x/2000`,
  `z=palm_y/2000`, offset to initial robot position `[0.2,0,0.2]`.
- Virtual Cartesian walls are x 0.05-0.45 m, y -0.30 to 0.30 m, z 0-0.55 m.
- A grab strength above 0.8 is treated as closed hand; open/closed state is sent
  with the Cartesian command.
- `PAROL6_simulator_low_pass.py` visualizes UDP input, uses Levenberg-Marquardt
  IK and an exponential moving average with default alpha 0.8.
- `PAROL6_LEAP_comms_LEAP.py` binds UDP 127.0.0.1:5001, defaults to Windows
  COM58 at 3,000,000 baud, runs at a nominal 10 ms interval, and applies a
  five-sample Cartesian moving average.
- Keyboard controls: H home, J generate a trajectory to the starter position,
  E enable, T enable teleoperation. The setup note additionally says to release
  E-stop and press E after an E-stop.
- The bridge uses the same step conversions as firmware reductions:
  `[6.4, 20, 20*(38/42), 4, 4, 10]`.
- Its trajectory mode uses command 156; home command 100 is reset to 255 after
  sending; a reported E-stop drives command 102 and zeros commanded speeds.

## Board-test firmware

The board-test directory is a diagnostic image, not the production controller.
It adds a 64-byte `#command` serial parser and tests LEDs, I/O, limits, E-stop,
bus voltage, external flash, CAN, and individual step channels. Recognized
parser names include `start`, `STOP`, `CAN`, `IO`, `ONOFF`, `LED1TOGGLE`,
`LED2TOGGLE`, `FLASH`, `SUPPLYV`, `LIMIT`, `OUT1TOGGLE`, `OUT2TOGGLE`, `INPUT`,
`ESTOP`, `STATUS`, and `STEP1` through `STEP6`.

The test parser's handler methods mostly return true placeholders; actual test
actions are dispatched again in its `main.cpp`. Treat its current values and
behavior as factory diagnostics only.

## Known cross-source conflicts and traps

1. Current BOM versus legacy PDF: use `BOM.md`; the PDF is explicitly legacy.
2. Assembly manual date: version 1.2 rev1 from 2023-10-27 predates the 2026 BOM
   snapshot.
3. Manual page 4 says STEP/control-board designs are not open source even though
   the main project is GPLv3.
4. Current firmware currents are deliberately reduced relative to test code
   because enclosed motors can overheat PETG.
5. URDF mass and limits do not equal complete physical robot mass and firmware
   limits.
6. Print-table masses/times are incomplete; do not present their sums as total
   filament or total printing time.
7. Mechanical manual refers to some old part names while current STL names add
   `rework` or `blocker` suffixes.
8. Main-board pin header contains duplicate CAN macros; use low-level CAN remap
   behavior and schematic verification.
9. Current protocol contains nominal CRC and timeout fields without complete
   enforcement in the visible production main loop.
10. Firmware contains apparent homing-state comparison/assignment defects; do
    not assume homing is intrinsically safe because it compiles.

## Retrieval rule for future work

This reference intentionally records high-value specifics and conflicts. For an
exact purchase URL, screw location, page illustration, firmware statement, CAD
coordinate, or library implementation, read the corresponding checked-out file
at the pinned commit. State which source was used whenever two artifacts differ.
