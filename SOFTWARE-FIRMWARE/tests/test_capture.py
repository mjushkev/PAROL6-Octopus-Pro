from pathlib import Path
import tempfile
import unittest

import _bootstrap

from parol6_protocol import CaptureRecord, Frame, MessageType, encode_body, read_pcap, write_pcap


class CaptureTests(unittest.TestCase):
    def test_pcap_round_trip(self) -> None:
        records = [
            CaptureRecord(1_234_567, encode_body(Frame(message_type=MessageType.HELLO))),
            CaptureRecord(1_235_000, b"malformed-for-replay-test"),
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.pcap"
            write_pcap(path, records)
            self.assertEqual(read_pcap(path), records)


if __name__ == "__main__":
    unittest.main()

