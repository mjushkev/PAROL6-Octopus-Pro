# Commissioning tools — gated

No physical commissioning command is implemented. Future tools must default to
read-only, expose the hardware gate ledger, and require explicit local safety
actions before any output-capable command.

The current scripts are read-only USB verifiers:

- `identify-octopus.ps1` verifies the installed identity image.
- `verify-safe-core.ps1` is prepared for an explicitly authorized safe-core
  flash and checks watchdog/config diagnostics plus fail-closed command
  rejection.
- `verify-service-core.py` is prepared for an explicitly authorized
  service-core flash. It checks canonical binary USB framing, fixed diagnostic
  payloads, flash boundaries, persistent-storage/watchdog readiness, replay
  rejection, and rejection of `MOTOR_ENABLE`. Install the pinned, hardware-only
  dependency from `requirements.txt` before running it.

Neither script can enable power or motion.
