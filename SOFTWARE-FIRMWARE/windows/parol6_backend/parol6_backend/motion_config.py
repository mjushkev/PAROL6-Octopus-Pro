"""Owner-selected motion feedback policy.

The Servo42C encoder types and telemetry fields remain part of the backend
contract so they can be re-enabled later.  For the current robot, J1/J2 are
operated in the Servo42C's local open-loop mode and the host must not poll,
display, or fault on encoder data.
"""

from typing import Final


SERVO42C_MODE: Final[str] = "CR_OPEN"
ENCODER_INTEGRATION_ENABLED: Final[bool] = False

