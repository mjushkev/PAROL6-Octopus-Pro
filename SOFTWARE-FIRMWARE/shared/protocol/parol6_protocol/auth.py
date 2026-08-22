"""Transport authentication helpers; key storage belongs to the host/ESP."""

from __future__ import annotations

import hashlib
import hmac

AUTH_TAG_BYTES = 32


def challenge_response(key: bytes, challenge: bytes, session_id: int, role: bytes) -> bytes:
    if len(key) != 32:
        raise ValueError("per-robot key must be exactly 256 bits")
    if len(challenge) < 16:
        raise ValueError("challenge must contain at least 128 bits")
    if not 0 <= session_id <= 0xFFFFFFFF:
        raise ValueError("session_id out of range")
    message = b"PAROL6-AUTH-v1\x00" + role + b"\x00" + session_id.to_bytes(4, "little") + challenge
    return hmac.new(key, message, hashlib.sha256).digest()


def verify_challenge_response(
    key: bytes, challenge: bytes, session_id: int, role: bytes, received: bytes
) -> bool:
    expected = challenge_response(key, challenge, session_id, role)
    return len(received) == AUTH_TAG_BYTES and hmac.compare_digest(expected, received)


def message_auth_tag(session_key: bytes, canonical_body: bytes) -> bytes:
    """Authenticate a decoded canonical body on a control-capable transport."""

    if len(session_key) != 32:
        raise ValueError("session key must be exactly 256 bits")
    return hmac.new(session_key, b"PAROL6-MSG-v1\x00" + canonical_body, hashlib.sha256).digest()


def verify_message_auth_tag(session_key: bytes, canonical_body: bytes, received: bytes) -> bool:
    expected = message_auth_tag(session_key, canonical_body)
    return len(received) == AUTH_TAG_BYTES and hmac.compare_digest(expected, received)
