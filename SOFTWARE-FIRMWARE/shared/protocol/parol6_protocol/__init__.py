"""Canonical PAROL6 protocol primitives."""

from .auth import (
    AUTH_TAG_BYTES,
    challenge_response,
    message_auth_tag,
    verify_challenge_response,
    verify_message_auth_tag,
)
from .capture import CaptureRecord, read_pcap, write_pcap
from .enums import ControllerState, ErrorCode, MessageType
from .frame import (
    MAX_DECODED_FRAME,
    MAX_PAYLOAD,
    Frame,
    FrameError,
    cobs_decode,
    cobs_encode,
    crc32c,
    decode_body,
    decode_tcp,
    decode_uart,
    encode_body,
    encode_tcp,
    encode_uart,
)
from .replay import ReplayDecision, ReplayWindow

__all__ = [
    "AUTH_TAG_BYTES",
    "ControllerState",
    "CaptureRecord",
    "ErrorCode",
    "Frame",
    "FrameError",
    "MAX_DECODED_FRAME",
    "MAX_PAYLOAD",
    "MessageType",
    "ReplayDecision",
    "ReplayWindow",
    "challenge_response",
    "cobs_decode",
    "cobs_encode",
    "crc32c",
    "decode_body",
    "decode_tcp",
    "decode_uart",
    "encode_body",
    "encode_tcp",
    "encode_uart",
    "message_auth_tag",
    "read_pcap",
    "verify_challenge_response",
    "verify_message_auth_tag",
    "write_pcap",
]

