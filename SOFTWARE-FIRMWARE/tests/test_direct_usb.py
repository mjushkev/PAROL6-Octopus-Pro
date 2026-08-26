import unittest

import _bootstrap

from parol6_backend import DirectUsbError, DirectUsbTransport, MotionRcLineParser


class FakeSerial:
    def __init__(self, *_args, **_kwargs) -> None:
        self.writes: list[bytes] = []
        self.responses: list[bytes] = []
        self.closed = False

    def write(self, value: bytes) -> None:
        self.writes.append(value)

    def flush(self) -> None:
        pass

    def readline(self) -> bytes:
        return self.responses.pop(0) if self.responses else b""

    def close(self) -> None:
        self.closed = True


class DirectUsbTests(unittest.TestCase):
    def test_parser_maps_ready_status_motion_hold_and_faults(self) -> None:
        parser = MotionRcLineParser()
        parser.connected()
        parser.feed("PAROL6_MOTION_RC_READY version=0.9.1-motion-rc token=1234ABCD")
        parser.feed(
            "PAROL6_STATUS moving=0 held=NONE "
            "J1_mdeg=1000 J1_homed=1 J2_mdeg=2000 J2_homed=1 "
            "J3_mdeg=3000 J3_homed=1 J4_mdeg=4000 J4_homed=1 "
            "J5_mdeg=-130000 J5_homed=1 J6_mdeg=6000 J6_homed=1"
        )
        self.assertTrue(parser.snapshot.firmware_ready)
        self.assertEqual(parser.snapshot.positions_deg["J5"], -130.0)
        self.assertTrue(all(parser.snapshot.homed.values()))
        parser.feed("PAROL6_COORDINATED_STARTED token=2345BCDE")
        self.assertTrue(parser.snapshot.moving)
        parser.feed(
            "PAROL6_COORDINATED_DONE result=complete hold=1 "
            "J1_mdeg=0 J2_mdeg=0 J3_mdeg=0 J4_mdeg=0 J5_mdeg=-100000 J6_mdeg=0 token=3456CDEF"
        )
        self.assertFalse(parser.snapshot.moving)
        self.assertEqual(parser.snapshot.held, "ALL")
        parser.feed("PAROL6_ERROR code=limit_abort token=4567DEFA")
        self.assertEqual(parser.snapshot.fault, "limit_abort")
        parser.disconnected()
        self.assertFalse(parser.snapshot.connected)
        self.assertFalse(any(parser.snapshot.homed.values()))

    def test_transport_frames_ascii_and_stops_before_close(self) -> None:
        fake = FakeSerial()
        transport = DirectUsbTransport("COM4", serial_factory=lambda *_a, **_k: fake)
        transport.connect()
        transport.send("IDENTIFY")
        self.assertEqual(fake.writes, [b"IDENTIFY\n"])
        with self.assertRaisesRegex(DirectUsbError, "invalid_usb_command"):
            transport.send("STOP\nMOVE")
        fake.responses.append(b"PAROL6_PONG moving=0\r\n")
        self.assertEqual(transport.read_line(), "PAROL6_PONG moving=0")
        transport.disconnect()
        self.assertEqual(fake.writes[-1], b"STOP\n")
        self.assertTrue(fake.closed)


if __name__ == "__main__":
    unittest.main()
