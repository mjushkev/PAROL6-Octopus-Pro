# PAROL6 joint calibration release candidate

Version `0.8.1-calibration-rc` provides direct-USB manual control, fixed raw
direction-discovery moves, guarded sensor homing for J1-J6, retained direction
configuration, captured firmware soft limits, and JSON export. The Site is a
single-joint setup workbench rather than an unrestricted production controller.

## Angle model

Each successful slow home latch sets that joint to exactly `0.000°`. The home
sensor is a repeatable reference; it is not a continuous angle sensor. After
homing, firmware derives the displayed joint angle from commanded step pulses
and the owner-selected hardware conversion values:

| Joint | Pulses per degree |
| --- | ---: |
| J1 | 114 |
| J2 | 356 |
| J3 | 161 |
| J4 | 36 |
| J5 | 36 |
| J6 | 89 |

J1/J2 remain in Servo42C `CR_OPEN`. Encoder polling and encoder-dependent
faulting remain disabled. Rehome after power-up, physical back-driving, a
suspected missed step, belt slip, or collision.

## Simple setup workflow

1. Connect USB and press **Enable setup motion** once for the session. The Site
   applies the already-verified J1/J2 Servo42C interface setting automatically.
2. Select one joint. Before saving a direction mapping, press **Test raw -** or
   **Test raw +**. Each press commands exactly 2° at the gentle profile and then
   disables that driver. Use these buttons only to identify physical direction.
3. Choose which raw direction means logical positive, which raw direction seeks
   home, and whether the home sensor triggers high or low. Logical negative is
   always the inverse. Press **Save setup**.
4. Press **Home joint**. Firmware clears an active sensor, seeks at gentle speed
   for at most 30°, backs off, adds a 0.5° margin, and re-latches slowly at 0°.
5. Tap-jog by 1°, 5°, or 10°, or hold a direction button at 3-45°/s. Release of
   a hold-jog stops and keeps stationary motor torque, so another jog can begin
   without releasing torque first.
6. Jog to a safe point before each hard stop and press **Set minimum** or **Set
   maximum**. Repeat for all joints, then export `PAROL6_joint_limits_*.json`.

The interface has one setup-motion gate, but the firmware still independently
requires a fresh command token, permits only one axis, supervises the browser
heartbeat, aborts on guarded sensor transitions, bounds raw discovery at 2°,
and disables drivers on STOP, timeout, disconnect, or focus loss.

Do not push the arm by hand while relying on the displayed angle. The saved
minimum must be at or below 0°, the maximum must be at or above 0°, and the
minimum must be lower than the maximum.

## Persistence and reset

The controller stores the six records in reserved H723 flash sectors at
`0x08040000` and `0x08060000`. Each update erases and writes the inactive slot,
uses a monotonically increasing sequence, validates a CRC32C, and retains the
previous valid slot until the new record verifies. A compatibility-safe header
keeps the storage fail-closed for the earlier service core.

Each joint has a **Reset joint calibration** button. Reset removes its home
direction, logical direction, sensor polarity, minimum, and maximum without
moving the robot. It does not affect the other five joints.

## Firmware enforcement

- Only one joint can be enabled at a time.
- Before homing, a configured joint accepts only `GENTLE` tap jogs of 1° or
  less so the operator can approach a sensor if necessary. Hold-to-jog and
  stationary motor hold require a successful home.
- Tap motion that would cross a captured minimum or maximum is rejected before
  driver enable.
- Hold motion is shortened to the captured boundary and disables the driver at
  the limit.
- A direction or sensor-configuration change clears that joint's limits and
  requires a new home.
- The host must contact the controller at least every two seconds while moving
  or holding torque. Loss of browser focus, Web Serial, heartbeat, hold
  keepalive, a guarded sensor transition, or STOP disables motion.
- J3-J5 remain at 700 mA RMS and J6 at 450 mA RMS. J1/J2 retain the proven
  1,000 µs active-low clock pulse and temporary 500 pulse/s ceiling.
- Absolute tracked-angle containment remains ±360° even before a side-specific
  limit is captured. This is not permission to approach cable or mechanical
  stops.

## Home All and test programs

Home All is available only after every joint has a saved minimum and maximum
and J1/J2 are armed for the current boot. It runs one axis at a time in the
order J1, J2, J3, J4, J6, J5 and stops on the first fault.

The direction check, wrist articulation, and Servo42C repeatability programs
remain serialized and become available after all six joints are homed.

## Calibration protocol

```text
IDENTIFY
CALIBRATION
SERVO_CONFIG J1 <token> ACTIVE_LOW INTERFACE_VERIFIED
RAW_JOG J1 <token> + DIRECTION_DISCOVERY_VERIFIED
CAL_CONFIG J1 <token> HOME_RAW_NEG POSITIVE_RAW_POS ACTIVE_LOW SAVE_CALIBRATION_VERIFIED
HOME J1 <token> START
JOG J1 <token> - 1000 GENTLE
CAL_LIMIT J1 <token> MIN CAPTURE_LIMIT_VERIFIED
JOG J1 <token> + 1000 GENTLE
CAL_LIMIT J1 <token> MAX CAPTURE_LIMIT_VERIFIED
CAL_RESET J1 <token> RESET_JOINT_CALIBRATION_VERIFIED
```

USB baud is 3,000,000. Flash with 24 V off and USB disconnected using the
normal bootloader-preserving FAT32 microSD process. Never test motion without
physical support, clear travel, and immediate E-stop access.
