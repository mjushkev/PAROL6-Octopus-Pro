# Legacy quick motor tool exclusion

The parent checkout's `quick_motor_step_gui` is a historical bench-only tool
for an earlier TMC2209 test in Octopus `MOTOR0`. The selected robot assigns
`MOTOR0` logic pins to the J1 Servo42C interface.

The tool is therefore excluded from this project's launcher, backend, release,
and completed-robot commissioning workflow. It must never be pointed at this
project's controller firmware or connected to the completed robot. Any future
bench use requires a physically isolated test setup and an explicit bench-only
configuration.

