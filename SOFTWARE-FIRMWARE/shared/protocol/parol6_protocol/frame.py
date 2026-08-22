"""Strict framing shared by UART, USB and TCP transports."""

from __future__ import annotations

from dataclasses import dataclass
import struct

PROTOCOL_MAJOR = 1
PROTOCOL_MINOR = 0
PROTOCOL_VERSION = (PROTOCOL_MAJOR << 8) | PROTOCOL_MINOR
MAX_PAYLOAD = 2048
_HEADER = struct.Struct("<HBBHIIIQ")
_CRC = struct.Struct("<I")
_TCP_LENGTH = struct.Struct("<I")
MIN_BODY_BYTES = _HEADER.size + _CRC.size
MAX_DECODED_FRAME = MIN_BODY_BYTES + MAX_PAYLOAD


class FrameError(ValueError):
    """Rejected frame with a stable machine-oriented reason."""

    def __init__(self, reason: str) -> None:
        super().__init__(reason)
        self.reason = reason


@dataclass(frozen=True, slots=True)
class Frame:
    message_type: int
    payload: bytes = b""
    flags: int = 0
    session_id: int = 0
    sequence: int = 0
    acknowledgement: int = 0
    sender_time_us: int = 0
    version: int = PROTOCOL_VERSION

    def __post_init__(self) -> None:
        if not 0 <= self.message_type <= 0xFF:
            raise FrameError("message_type_out_of_range")
        if not 0 <= self.flags <= 0xFF:
            raise FrameError("flags_out_of_range")
        if len(self.payload) > MAX_PAYLOAD:
            raise FrameError("payload_too_large")
        for name in ("session_id", "sequence", "acknowledgement"):
            if not 0 <= getattr(self, name) <= 0xFFFFFFFF:
                raise FrameError(f"{name}_out_of_range")
        if not 0 <= self.sender_time_us <= 0xFFFFFFFFFFFFFFFF:
            raise FrameError("sender_time_us_out_of_range")
        if not 0 <= self.version <= 0xFFFF:
            raise FrameError("version_out_of_range")


def crc32c(data: bytes, initial: int = 0) -> int:
    """Castagnoli CRC-32 with the conventional initial/final xor."""

    crc = initial ^ 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0x82F63B78 if crc & 1 else 0)
    return crc ^ 0xFFFFFFFF


def cobs_encode(data: bytes) -> bytes:
    output = bytearray([0])
    code_index = 0
    code = 1
    for byte in data:
        if byte == 0:
            output[code_index] = code
            code_index = len(output)
            output.append(0)
            code = 1
        else:
            output.append(byte)
            code += 1
            if code == 0xFF:
                output[code_index] = code
                code_index = len(output)
                output.append(0)
                code = 1
    output[code_index] = code
    return bytes(output)


def cobs_decode(encoded: bytes) -> bytes:
    if not encoded:
        raise FrameError("empty_cobs_frame")
    if 0 in encoded:
        raise FrameError("embedded_cobs_delimiter")
    output = bytearray()
    index = 0
    while index < len(encoded):
        code = encoded[index]
        if code == 0:
            raise FrameError("invalid_cobs_code")
        index += 1
        end = index + code - 1
        if end > len(encoded):
            raise FrameError("truncated_cobs_block")
        output.extend(encoded[index:end])
        index = end
        if code != 0xFF and index < len(encoded):
            output.append(0)
        if len(output) > MAX_DECODED_FRAME:
            raise FrameError("decoded_frame_too_large")
    return bytes(output)


def encode_body(frame: Frame) -> bytes:
    header = _HEADER.pack(
        frame.version,
        frame.message_type,
        frame.flags,
        len(frame.payload),
        frame.session_id,
        frame.sequence,
        frame.acknowledgement,
        frame.sender_time_us,
    )
    protected = header + frame.payload
    return protected + _CRC.pack(crc32c(protected))


def decode_body(body: bytes, *, require_major: int = PROTOCOL_MAJOR) -> Frame:
    if len(body) < MIN_BODY_BYTES:
        raise FrameError("body_too_short")
    if len(body) > MAX_DECODED_FRAME:
        raise FrameError("body_too_large")
    header = body[: _HEADER.size]
    (
        version,
        message_type,
        flags,
        payload_length,
        session_id,
        sequence,
        acknowledgement,
        sender_time_us,
    ) = _HEADER.unpack(header)
    if version >> 8 != require_major:
        raise FrameError("unsupported_protocol_major")
    expected_length = MIN_BODY_BYTES + payload_length
    if expected_length != len(body):
        raise FrameError("payload_length_mismatch")
    protected = body[:-_CRC.size]
    received_crc = _CRC.unpack(body[-_CRC.size :])[0]
    if crc32c(protected) != received_crc:
        raise FrameError("bad_crc32c")
    return Frame(
        version=version,
        message_type=message_type,
        flags=flags,
        payload=body[_HEADER.size : -_CRC.size],
        session_id=session_id,
        sequence=sequence,
        acknowledgement=acknowledgement,
        sender_time_us=sender_time_us,
    )


def encode_uart(frame: Frame) -> bytes:
    return cobs_encode(encode_body(frame)) + b"\x00"


def decode_uart(packet: bytes) -> Frame:
    if not packet.endswith(b"\x00"):
        raise FrameError("missing_uart_delimiter")
    encoded = packet[:-1]
    if b"\x00" in encoded:
        raise FrameError("multiple_uart_frames")
    return decode_body(cobs_decode(encoded))


def encode_tcp(frame: Frame) -> bytes:
    body = encode_body(frame)
    return _TCP_LENGTH.pack(len(body)) + body


def decode_tcp(packet: bytes) -> Frame:
    if len(packet) < _TCP_LENGTH.size:
        raise FrameError("tcp_prefix_too_short")
    declared = _TCP_LENGTH.unpack(packet[: _TCP_LENGTH.size])[0]
    body = packet[_TCP_LENGTH.size :]
    if declared != len(body):
        raise FrameError("tcp_length_mismatch")
    return decode_body(body)

