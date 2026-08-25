# Hardware verification ledger

## As-built progress reported by owner

| Date | Report | Evidence status | Consequence |
| --- | --- | --- | --- |
| 2026-08-13 | Robot arm and base fully assembled; nothing wired | Owner statement only; photos and inspection not yet recorded | Begin pre-wiring identification and harness planning; all electrical gates remain closed |
| 2026-08-17 | Owner reports all wiring complete except the power button; requests direct USB operation with no ESP | Owner statement only; no as-built photos, measurements, connector table, or independent inspection recorded | Record progress but keep every HV gate closed; defer ESP/USART2; permit only a future USB-powered logic identity test after HV-01 evidence and the USB-only checklist |
| 2026-08-17 | Owner reports the Octopus appears after USB connection | Windows read-only enumeration captured in `evidence/2026-08-17_usb_enumeration_windows.txt`: COM4, VID 0483, PID 5740, descriptor `OCTOPUS_PRO_V1_1_H723 CDC in FS Mode` | USB-01 passes; do not infer installed firmware identity or authorize flash, actuator power, or motion |
| 2026-08-17 | Owner reports the E-stop now removes main 24 V power and the build uses only the Octopus `POWER` input | Owner statement plus observed COM4 disappearance when E-stop was pressed; no independent electrical inspection | Record this as the as-built architecture; source MOTOR2-MOTOR5 from `POWER`; retain electrical-review gate for ratings, fusing, and reset behavior |
| 2026-08-17 | J6 bounded diagnostic rotated perfectly | `evidence/2026-08-17_j6_bounded_motion.txt`; TMC2209 UART version `0x21`, 250 mA, 1,600 pulses, owner visual observation, driver disabled afterward | MOT-01 passes for this bounded J6 test only; no inference about home polarity, calibration, full travel, or another joint |
| 2026-08-17 | J4 and J6 inductive inputs changed in the commissioning GUI with 24 V applied | Owner observation using `0.5.0-commissioning`; corresponds to STOP3/PG11 and STOP5/PG13 | Record J4/J6 input-change PASS only; polarity, repeatability, physical home location, and J1/STOP0 remain unverified |
| 2026-08-17 | J2, J3, and J5 mechanical inputs also reported `CHANGE SEEN`; J1 was the only home input without an observed transition | Owner observation using the commissioning GUI; J2/STOP1, J3/STOP2, J4/STOP3, J5/STOP4, and J6/STOP5 changed individually | Five input channels pass transition detection; J1/STOP0 requires powered-down physical access and inspection before any homing work |
| 2026-08-25 | Owner identified the actual J2/J3/J5 cable routing by actuating each physical switch: J2 reported on the former J3 channel, J3 on the former J5 channel, and J5 on the former J2 channel | Owner observation with wiring confirmed functional | Firmware 0.8.5 preserves the wiring and maps logical J2 to STOP2/PG10, J3 to STOP4/PG12, and J5 to STOP1/PG9; the earlier sequential labels above are superseded for this robot |
| 2026-08-25 | Owner confirmed J2 and J3 normally rest with their home switches active | Owner observation on the assembled robot | Firmware 0.8.6 uses an active-start release and slow re-latch, saves the home-side boundary as 0 degrees automatically, and requires the operator to capture only the opposite limit |
| 2026-08-25 | Owner reported the original 5-degree active-start release was insufficient for J2 to clear its switch | Owner observation on the assembled robot | Firmware 0.8.7 moves opposite the configured home direction until the debounced switch clears, stopping immediately; 30 degrees is the bounded failure ceiling rather than a fixed move |
| 2026-08-25 | Owner reported J2 required substantially more lift torque | Owner observation on the assembled, gravity-loaded arm | Firmware 0.8.8 reduces J2 to 350 pulses/s and 900 pulses/s^2; Servo42C `CR_OPEN` current starts at local `Ma=1600 mA`, subject to loaded motion and thermal verification |
| 2026-08-25 | J3 (and possibly J2) would not begin homing while resting on its switch | Owner observation; firmware state-machine review | Firmware 0.8.9 hands an active stationary motor hold to the homing state machine before starting the switch-release phase |
| 2026-08-25 | Owner selected fixed J6 travel limits | Owner instruction | Firmware 0.8.10 enforces J6 at −180° through +180° relative to its sensor-established 0° home; calibration cannot overwrite the fixed endpoints |
| 2026-08-25 | Owner requested individual standard-style homing, maximum-limit tests, and software motor stop | Owner instruction plus calibration export sequence 60 | Firmware 0.8.11 preserves the learned directions/polarities, uses bounded two-pass homing with J2/J3 active-start release, adds GENTLE Max−10°/Max test targets, and retains STOP as an all-driver disable |
| 2026-08-17 | J3 visibly rotated successfully during repeated bounded `+10` degree tests | `0.5.0-commissioning` responses reported `result=complete`, `driver_disabled=1`, status `0x40070000`; owner confirmed visible success | J3 bounded bidirectional motion, calibration, homing, load capacity, and thermal behavior remain separate tests |
| 2026-08-17 | J4 visibly rotated successfully during a bounded `+10` degree test | `0.5.0-commissioning` reported `result=complete`, `driver_disabled=1`, status `0x40070000`; owner confirmed visible success | J4 bounded reverse motion, calibration, homing, load capacity, and thermal behavior remain separate tests |
| 2026-08-17 | J5 visibly rotated successfully during a bounded `+10` degree test | `0.5.0-commissioning` reported `result=complete`, `driver_disabled=1`, status `0x40070000`; owner confirmed visible success | J5 bounded reverse motion, calibration, homing, load capacity, and thermal behavior remain separate tests |
| 2026-08-17 | J6 visibly rotated successfully during a bounded `+10` degree test under the six-joint image, without reported cable strain | `0.5.0-commissioning` reported `result=complete`, `driver_disabled=1`, status `0x40070000`; owner confirmed success | J6 cable-limited reverse motion, calibration, homing, load capacity, and thermal behavior remain separate tests |
| 2026-08-18 | J1 and J2 Servo42C modules received and executed bounded `+1` degree commands through their supplied Pololu-format adapters | `evidence/2026-08-18_servo42c_bounded_motion.txt`; `0.5.1-servo-pushpull`; J1 displayed `114clk`; owner confirmed physical J1 motion and J2 success; both axes disabled after their commands | The installed adapters require 3.3 V push-pull signaling; preserve this in production configuration. Reverse motion, homing, load, thermal, and telemetry tests remain pending |
| 2026-08-17 | Owner explicitly authorizes the proposed USB-only, outputs-disabled identity firmware work | Conversation authorization plus USB-only isolation already confirmed by owner; image build evidence in `evidence/2026-08-17_identity_firmware_build.txt` | ADR-007 permits only the bootloader-preserving `safe_identity` microSD install; all output and motion firmware remains blocked |

The pre-wiring workflow and evidence request are in
[`WIRING_GUIDE.md`](WIRING_GUIDE.md). Mechanical assembly completion does not
close HV-08 and does not authorize power or motion.

`UNVERIFIED` means the dependent output remains disabled. Evidence must include
the instrument/method, date, operator, raw observation, and an attachment path
or immutable hash. A TODO or assumed datasheet family value is not evidence.

| Gate | Status | Evidence | Blocks |
| --- | --- | --- | --- |
| HV-01 Octopus identity/schematic continuity | PARTIAL | USB descriptor identifies `OCTOPUS_PRO_V1_1_H723`; physical board/revision photos, jumper record, and schematic continuity remain missing | Any output- or motion-capable Octopus flash; ADR-007 narrowly permits the verified identity-only image |
| HV-02 Servo42C firmware/electrical/trace | PARTIAL | Supplied adapters pass bounded STEP/DIR delivery with 3.3 V push-pull; J1 `114clk` and physical motion, J2 physical success; see `evidence/2026-08-18_servo42c_bounded_motion.txt` | Reverse direction, UART telemetry, reset behavior, current/thermal qualification, and production interface sign-off |
| HV-03 OLED controller/address/pins/colors | DEFERRED | ESP/OLED removed from initial commissioning scope by owner on 2026-08-17 | Optional future ESP/display phase |
| HV-04 Optocoupler identity/truth table | UNVERIFIED | None recorded | Proximity/contact feedback inputs |
| HV-05 Servo model/current/voltage/endpoints | UNVERIFIED | None recorded | Gripper power and PWM |
| HV-06 Contactor/E-stop DC ratings and behavior | UNVERIFIED | None recorded | Any motor-bus power |
| HV-07 PSU/fuses/wires/connectors/load budget | UNVERIFIED | None recorded | Multi-axis operation |
| HV-08 As-built direction/home/limits/standby | UNVERIFIED | None recorded | Normal-speed motion |

## Commissioning dashboard

| Area | Gate color | Meaning |
| --- | --- | --- |
| Software simulation | GREEN | Protocol and fake controller may run without hardware |
| Logic-only hardware | GREEN / USB ONLY | Verified `0.2.0-safe-core` firmware runs on COM4, reports all outputs/motion disabled, and rejects `MOVE`; the binary service core remains unflashed |
| Actuator power | RED | HV-02, HV-05, HV-06, and HV-07 evidence missing |
| Normal robot motion | RED | HV-08 and all physical acceptance evidence missing |

The green logic-only result applies only to the exact installed, verified safe
core with every actuator power domain absent. It does not permit another flash,
actuator power, or motion without its separate gate and authorization.

## Interface checks

| Check | Status | Evidence | Meaning |
| --- | --- | --- | --- |
| USB-01 Windows CDC enumeration | PASS | `evidence/2026-08-17_usb_enumeration_windows.txt`, SHA-256 `4749B28055729DD987A68BBD4E7B9069EF1AFAF131E95A7814BFE5B5CA088489` | USB cable, Windows CDC driver, and the board's USB device interface enumerate on COM4 |
| USB-02 Installed firmware identity | PASS | Installed response reports `safe_identity` version `0.1.0-identity`, board `BTT_OCTOPUS_PRO_V1_1_H723ZE`, and MCU `STM32H723ZE` | The formerly unknown application was replaced by the checksum-locked identity image |
| USB-03 PAROL6 safe identity image | PASS | Card renamed to `FIRMWARE.CUR` with artifact SHA-256 `25A16D997590C00BC158DF40061970802BEBB5B8A1845C57A2F8734748FC94BD`; post-flash identity/status/rejection responses captured in `evidence/2026-08-17_identity_firmware_hardware_verification.txt`, evidence SHA-256 `652C859AACB8F0D2E1561B755DF04701DF324D7FEA6CFD5DE1C0364BE4A2882E` | USB identity milestone complete; output-capable firmware remains blocked |
| USB-04 PAROL6 output-disabled safe core | PASS | Card renamed to `FIRMWARE.CUR` with artifact SHA-256 `25B176BA79B029E0D82FD4489A94DA84D734C8C08BA25A0DC1DFB9478F5BA2E4`; COM4 identity/status/diagnostics/heartbeat/rejection captured in `evidence/2026-08-17_safe_core_hardware_verification.txt`, evidence SHA-256 `D659E07EDC04FCEC4994204A49626C357087B51760EBECC8A46DC9F70242D700` | Safe core reports watchdog ready, safe config selected, `NOT_COMMISSIONED`, outputs/motion disabled; this does not authorize 24 V or output firmware |
| USB-05 PAROL6 binary service core | NOT RUN | Output-disabled artifact built twice with SHA-256 `3C176B9D75EE10711DB818E37B65F547089A30EC7E5BB4EE4096DB0ADB1B4FB1`; post-flash verifier prepared; no flash authorized or attempted | Installed `0.2.0-safe-core` remains the baseline; binary USB and internal-flash behavior require a later explicitly authorized logic-only HIL step |
| MOT-01 J6 bounded diagnostic | PASS / BOUNDED | `0.4.2-j6-diag`; TMC version `0x21`, 250 mA, 1,600 pulses; owner reported perfect visible rotation; post-command check reported driver disabled; see `evidence/2026-08-17_j6_bounded_motion.txt` | Pass applies only to the observed bounded J6 jog; calibration, homing polarity, full travel, and all other joints remain unverified |
| MOT-02 Six-joint commissioning image | PASS / BOUNDED | `0.5.0-commissioning` passed J3-J6 bounded motion; `0.5.1-servo-pushpull` SHA-256 `344A02FE5A5C2D210FAA70CFD0B8C80E5661BE6D1AA3314A3E5B05B9CCAEDA99` passed J1/J2 bounded positive motion through supplied adapters | All six joints have moved individually; reverse direction, calibrated scaling, homing, combined motion, load, thermal, and endurance remain unverified |
| MOT-03 Motion release candidate | INSTALLED / BASIC CONTROL PASS | `0.6.0-motion-rc` SHA-256 `2425A89C48B6A8F966F0AE4452A40DC798D70A0955CD2D1F33E1696AF560F698`; owner reported everything works after installation | Confirms the 0.6 manual-control path at the owner's tested settings; it does not validate higher speed, hold release, homing, calibration, load, thermal behavior, or endurance. |
| MOT-04 Fast motion release candidate | BUILT / NOT FLASHED | `0.7.0-motion-rc` passed the source safety suite and bootloader-bounded PlatformIO verification; `firmware.bin` SHA-256 `50B204AAB7AEC20606BE1E8D957F3C6484311E4C77AB5B1C45AB5FDA22BB8F76` | Adds 3-45 deg/s supervised hold-to-jog and serialized J2/J3/J4/J6/J5 homing. Hardware verification must start at 3 deg/s; J1 homing remains hard locked. |
| MOT-05 Planned TMC current correction | BUILT / NOT FLASHED | `0.7.1-motion-rc` passed 56 tests and bootloader-bounded PlatformIO verification; `firmware.bin` SHA-256 `8831A1B67839D4F143072AABA6F8BEC64951C5213220F8A54C4EBBC4029C1C67` | Replaces temporary 250 mA with approved starts of 700 mA RMS J3-J5 and 450 mA RMS J6. First hardware check is a supported 0.25-degree J3 lift at 3 deg/s; loaded thermal validation remains open. |
| MOT-06 Servo42C timing correction | BUILT / NOT FLASHED | `0.7.2-motion-rc` passed 56 tests and bootloader-bounded PlatformIO verification; `firmware.bin` SHA-256 `F079DA4148DC83C27D4AD49686654123C3FA64890033FCB227EEB020CDD5379F` | Restores the physically successful 1,000 µs active-low pulse and caps J1/J2 at 500 clock pulses/s. Validate 0.25-degree GENTLE Tap in each direction before Hold. |
