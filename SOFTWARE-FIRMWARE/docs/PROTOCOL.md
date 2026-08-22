# Protocol revision 1.0

The canonical source is `config/protocol.yaml` (JSON-compatible YAML). UART and
USB frames are COBS encoded and terminated by `0x00`. The decoded body is:

```text
u16 version, u8 type, u8 flags, u16 payload length,
u32 session, u32 sequence, u32 acknowledgement, u64 sender time us,
payload, u32 CRC32C
```

All integer fields are little endian. The CRC covers the header and payload.
Payloads over 2048 bytes, unknown critical enum values, mismatched lengths,
invalid COBS, bad CRC, stale sequence numbers, and duplicate sequence numbers
are rejected. TCP uses a little-endian `u32` body length followed by the same
decoded body.

Golden vectors live in `shared/test_vectors/protocol_v1.json` and are generated
by `tools/protocol_analyzer/generate_vectors.py`.

The response flag is bit 0 (`RESPONSE`). The output-disabled H723 service core
implements these fixed response payloads, whose authoritative sizes/layouts are
also recorded in `config/protocol.yaml`:

- `HEARTBEAT_REPLY`: controller/fault/config/output state, uptime and protocol
  counters, selected config sequence, event count, storage status, and watchdog
  readiness.
- `GET_DEVICE_INFO | RESPONSE`: semantic firmware version, config schema,
  capability bits, physical/application/storage flash boundaries, and a
  fixed-width board identifier.
- `NACK | RESPONSE`: error, rejected message type, detail, and rejected
  sequence.

All response fields use the same little-endian rule as the frame header. The
service core does not accept any power, motion, homing, gripper, or I/O command;
those receive `NOT_COMMISSIONED` while physical gates remain incomplete.

Control-capable TCP messages are accompanied by an HMAC-SHA256 tag over the
canonical decoded body using the negotiated 256-bit session key. PCAP USER0
capture/replay helpers preserve canonical bodies for Wireshark hex inspection
without including keys.
