# Safety case notes — development baseline

This software is not a safety-rated controller. The physical E-stop and a
properly selected DC contactor must remove actuator power independently of the
PC, WiFi, ESP8266, and normal Octopus software.

Current safety claims are deliberately narrow:

- The simulation model defaults motor request, verified motor power, STEP,
  driver enable, and gripper PWM to off.
- Missing commissioning evidence forces `NOT_COMMISSIONED`.
- A reconnect starts status-only and cannot restore control, homing, a queued
  trajectory, or execution.
- A duplicate sequence is rejected by the replay window.
- A link timeout clears pending motion and enters `PROTECTIVE_STOP`.

These are executable software properties, not proof of physical wiring,
timing, or component performance. Physical claims require the HIL and completed
robot evidence defined in the implementation plan.

