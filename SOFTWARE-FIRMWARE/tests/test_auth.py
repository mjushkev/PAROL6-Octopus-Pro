import unittest

import _bootstrap

from parol6_protocol import (
    challenge_response,
    message_auth_tag,
    verify_challenge_response,
    verify_message_auth_tag,
)


class AuthTests(unittest.TestCase):
    def test_challenge_response_is_bound_to_role_and_session(self) -> None:
        key = bytes(range(32))
        challenge = bytes(range(32, 64))
        tag = challenge_response(key, challenge, 7, b"pc")
        self.assertTrue(verify_challenge_response(key, challenge, 7, b"pc", tag))
        self.assertFalse(verify_challenge_response(key, challenge, 8, b"pc", tag))
        self.assertFalse(verify_challenge_response(key, challenge, 7, b"esp", tag))

    def test_short_key_and_challenge_fail(self) -> None:
        with self.assertRaises(ValueError):
            challenge_response(b"short", b"x" * 16, 1, b"pc")
        with self.assertRaises(ValueError):
            challenge_response(b"x" * 32, b"short", 1, b"pc")

    def test_message_tag_detects_body_change(self) -> None:
        key = b"k" * 32
        tag = message_auth_tag(key, b"canonical-body")
        self.assertTrue(verify_message_auth_tag(key, b"canonical-body", tag))
        self.assertFalse(verify_message_auth_tag(key, b"canonical-bodx", tag))


if __name__ == "__main__":
    unittest.main()
