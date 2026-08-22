# Architecture

The first milestone models the future end-to-end path without commanding
hardware:

```text
simulation backend -> canonical protocol frame -> fake bounded ESP link
                   -> fake MCU session/safety supervisor -> status only
```

The fake MCU owns state, replay protection, control lease, and all output
interlocks. The fake ESP can inject deterministic loss, duplication,
corruption, and latency. The PC side treats commanded joint angles as primary;
J1/J2 motor-side telemetry is optional and separately marked for validity and
freshness.

Later H723/ESP implementations must consume the same schema and golden vectors.
They may not weaken the simulator's fail-closed state transitions.

