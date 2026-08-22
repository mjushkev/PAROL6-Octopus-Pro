#!/usr/bin/env python3
"""Read-only USB verification for PAROL6 H723 service-core 0.3.0."""

from __future__ import annotations

import argparse
from pathlib import Path
import secrets
import struct
import sys
import time


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "shared" / "protocol"))

from parol6_protocol import ErrorCode, Frame, MessageType, decode_uart, encode_uart  # noqa: E402


RESPONSE_FLAG = 1
HEARTBEAT_REPLY = struct.Struct("<BBBBIIIIIHBB")
DEVICE_INFO = struct.Struct("<HHHHIIIIII32s")
NACK = struct.Struct("<BBHI")


def read_packet(port, timeout_seconds: float) -> bytes:
    deadline = time.monotonic() + timeout_seconds
    packet = bytearray()
    while time.monotonic() < deadline:
        byte = port.read(1)
        if not byte:
            continue
        packet += byte
        if byte == b"\x00":
            return bytes(packet)
    raise TimeoutError("timed out waiting for a delimited service-core response")


def transact(port, request: Frame, timeout_seconds: float) -> Frame:
    port.write(encode_uart(request))
    port.flush()
    response = decode_uart(read_packet(port, timeout_seconds))
    if response.flags & RESPONSE_FLAG == 0:
        raise RuntimeError("device response flag is absent")
    if response.session_id != request.session_id:
        raise RuntimeError("device response session does not match the request")
    if response.acknowledgement != request.sequence:
        raise RuntimeError("device acknowledgement does not match the request sequence")
    return response


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--timeout", type=float, default=3.0)
    args = parser.parse_args()

    try:
        import serial
    except ImportError as error:
        raise SystemExit(
            "pyserial is required for hardware verification; install "
            "tools/commissioning/requirements.txt"
        ) from error

    session = secrets.randbits(32)
    if session == 0:
        session = 1

    with serial.Serial(
        args.port,
        baudrate=3_000_000,
        timeout=0.05,
        write_timeout=args.timeout,
        dsrdtr=False,
        rtscts=False,
    ) as port:
        port.dtr = True
        time.sleep(0.25)
        port.reset_input_buffer()
        heartbeat_request = Frame(
            message_type=MessageType.HEARTBEAT,
            session_id=session,
            sequence=1,
        )
        heartbeat = transact(port, heartbeat_request, args.timeout)
        if heartbeat.message_type != MessageType.HEARTBEAT_REPLY:
            raise RuntimeError("HEARTBEAT did not receive HEARTBEAT_REPLY")
        fields = HEARTBEAT_REPLY.unpack(heartbeat.payload)
        (
            state,
            fault,
            config_valid,
            outputs_enabled,
            uptime_ms,
            accepted,
            rejected,
            replay_rejected,
            config_sequence,
            event_count,
            storage_status,
            watchdog_ready,
        ) = fields
        if state != 1 or not config_valid or outputs_enabled != 0:
            raise RuntimeError("service core is not in its required output-safe state")
        if storage_status != 1 or watchdog_ready != 1:
            raise RuntimeError("persistent storage or hardware watchdog is not ready")

        info = transact(
            port,
            Frame(
                message_type=MessageType.GET_DEVICE_INFO,
                session_id=session,
                sequence=2,
            ),
            args.timeout,
        )
        if info.message_type != MessageType.GET_DEVICE_INFO:
            raise RuntimeError("GET_DEVICE_INFO response used the wrong message type")
        info_fields = DEVICE_INFO.unpack(info.payload)
        version = info_fields[0:3]
        capabilities = info_fields[4]
        boundaries = info_fields[5:10]
        board_id = info_fields[10].split(b"\0", 1)[0].decode("ascii")
        if version != (0, 3, 0):
            raise RuntimeError(f"unexpected service-core version {version}")
        if capabilities & (1 << 31) == 0:
            raise RuntimeError("output-disabled capability bit is absent")
        if boundaries != (
            0x00080000,
            0x08020000,
            0x08040000,
            0x08040000,
            0x08080000,
        ):
            raise RuntimeError(f"unexpected flash boundaries {boundaries!r}")

        blocked_request = Frame(
            message_type=MessageType.MOTOR_ENABLE,
            session_id=session,
            sequence=3,
        )
        blocked = transact(port, blocked_request, args.timeout)
        blocked_fields = NACK.unpack(blocked.payload)
        if blocked.message_type != MessageType.NACK or blocked_fields[0] != ErrorCode.NOT_COMMISSIONED:
            raise RuntimeError("MOTOR_ENABLE was not rejected as NOT_COMMISSIONED")

        duplicate = transact(port, blocked_request, args.timeout)
        duplicate_fields = NACK.unpack(duplicate.payload)
        if duplicate.message_type != MessageType.NACK or duplicate_fields[0] != ErrorCode.REPLAY:
            raise RuntimeError("duplicate sequence was not rejected as REPLAY")

    print(
        "PASS: service-core USB protocol verified; "
        f"board={board_id} uptime_ms={uptime_ms} config_sequence={config_sequence} "
        f"events={event_count} accepted={accepted} rejected={rejected} "
        f"replay_rejected={replay_rejected} initial_fault={fault}"
    )
    print("PASS: MOTOR_ENABLE remains blocked; outputs_enabled=0.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
