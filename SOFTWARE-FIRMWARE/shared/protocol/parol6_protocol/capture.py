"""Minimal PCAP USER0 capture for Wireshark inspection and deterministic replay."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import struct

_GLOBAL = struct.Struct("<IHHIIII")
_PACKET = struct.Struct("<IIII")
_MAGIC = 0xA1B2C3D4
_LINKTYPE_USER0 = 147
_MAX_CAPTURE_PACKET = 1 << 20


@dataclass(frozen=True, slots=True)
class CaptureRecord:
    timestamp_us: int
    data: bytes


def write_pcap(path: str | Path, records: list[CaptureRecord]) -> None:
    with Path(path).open("wb") as stream:
        stream.write(_GLOBAL.pack(_MAGIC, 2, 4, 0, 0, _MAX_CAPTURE_PACKET, _LINKTYPE_USER0))
        for record in records:
            if record.timestamp_us < 0 or len(record.data) > _MAX_CAPTURE_PACKET:
                raise ValueError("capture record out of range")
            seconds, micros = divmod(record.timestamp_us, 1_000_000)
            stream.write(_PACKET.pack(seconds, micros, len(record.data), len(record.data)))
            stream.write(record.data)


def read_pcap(path: str | Path) -> list[CaptureRecord]:
    with Path(path).open("rb") as stream:
        global_header = stream.read(_GLOBAL.size)
        if len(global_header) != _GLOBAL.size:
            raise ValueError("truncated PCAP header")
        magic, major, minor, _, _, snaplen, linktype = _GLOBAL.unpack(global_header)
        if (magic, major, minor, linktype) != (_MAGIC, 2, 4, _LINKTYPE_USER0):
            raise ValueError("unsupported PCAP format")
        if snaplen > _MAX_CAPTURE_PACKET:
            raise ValueError("unsafe PCAP snap length")
        records: list[CaptureRecord] = []
        while True:
            packet_header = stream.read(_PACKET.size)
            if not packet_header:
                return records
            if len(packet_header) != _PACKET.size:
                raise ValueError("truncated PCAP packet header")
            seconds, micros, captured, original = _PACKET.unpack(packet_header)
            if captured != original or captured > snaplen or micros >= 1_000_000:
                raise ValueError("invalid PCAP packet metadata")
            data = stream.read(captured)
            if len(data) != captured:
                raise ValueError("truncated PCAP packet")
            records.append(CaptureRecord(seconds * 1_000_000 + micros, data))

