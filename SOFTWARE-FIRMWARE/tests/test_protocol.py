import json
from pathlib import Path
import random
import unittest

import _bootstrap

from parol6_protocol import (
    Frame,
    FrameError,
    MessageType,
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


class ProtocolTests(unittest.TestCase):
    def test_crc32c_standard_check_value(self) -> None:
        self.assertEqual(crc32c(b"123456789"), 0xE3069283)

    def test_cobs_round_trip_randomized(self) -> None:
        rng = random.Random(0x5041524F4C36)
        for _ in range(10_000):
            payload = rng.randbytes(rng.randrange(0, 512))
            self.assertEqual(cobs_decode(cobs_encode(payload)), payload)

    def test_frame_round_trip_randomized(self) -> None:
        rng = random.Random(6006)
        for sequence in range(5_000):
            frame = Frame(
                message_type=rng.randrange(0, 256),
                flags=rng.randrange(0, 256),
                payload=rng.randbytes(rng.randrange(0, 256)),
                session_id=rng.randrange(0, 2**32),
                sequence=sequence,
                acknowledgement=max(0, sequence - 1),
                sender_time_us=rng.randrange(0, 2**64),
            )
            self.assertEqual(decode_uart(encode_uart(frame)), frame)
            self.assertEqual(decode_tcp(encode_tcp(frame)), frame)

    def test_golden_vectors(self) -> None:
        root = Path(__file__).resolve().parents[1]
        document = json.loads(
            (root / "shared" / "test_vectors" / "protocol_v1.json").read_text(encoding="utf-8")
        )
        for vector in document["vectors"]:
            fields = vector["fields"]
            frame = Frame(
                version=fields["version"],
                message_type=fields["message_type"],
                flags=fields["flags"],
                payload=bytes.fromhex(fields["payload_hex"]),
                session_id=fields["session_id"],
                sequence=fields["sequence"],
                acknowledgement=fields["acknowledgement"],
                sender_time_us=fields["sender_time_us"],
            )
            self.assertEqual(encode_body(frame).hex(), vector["body_hex"])
            self.assertEqual(encode_uart(frame).hex(), vector["uart_hex"])
            self.assertEqual(encode_tcp(frame).hex(), vector["tcp_hex"])

    def test_bad_crc_and_length_are_rejected(self) -> None:
        body = bytearray(encode_body(Frame(message_type=MessageType.HELLO, payload=b"abc")))
        body[-1] ^= 0x80
        with self.assertRaisesRegex(FrameError, "bad_crc32c"):
            decode_body(bytes(body))

        body = encode_body(Frame(message_type=MessageType.HELLO, payload=b"abc"))[:-1]
        with self.assertRaisesRegex(FrameError, "payload_length_mismatch"):
            decode_body(body)

    def test_uart_requires_one_delimited_frame(self) -> None:
        packet = encode_uart(Frame(message_type=MessageType.HELLO))
        with self.assertRaisesRegex(FrameError, "missing_uart_delimiter"):
            decode_uart(packet[:-1])
        with self.assertRaisesRegex(FrameError, "multiple_uart_frames"):
            decode_uart(packet + packet)


if __name__ == "__main__":
    unittest.main()

