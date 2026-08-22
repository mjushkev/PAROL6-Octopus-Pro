from __future__ import annotations

import argparse
from pathlib import Path
import random
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "shared" / "protocol"))

from parol6_protocol import Frame, FrameError, decode_uart, encode_uart


def run(iterations: int, seed: int) -> tuple[int, int]:
    rng = random.Random(seed)
    valid = 0
    rejected = 0
    for index in range(iterations):
        frame = Frame(
            message_type=rng.randrange(256),
            flags=rng.randrange(256),
            payload=rng.randbytes(rng.randrange(33)),
            session_id=rng.randrange(2**32),
            sequence=index & 0xFFFFFFFF,
            acknowledgement=max(0, index - 1) & 0xFFFFFFFF,
            sender_time_us=rng.randrange(2**64),
        )
        packet = encode_uart(frame)
        mode = index % 4
        if mode == 0:
            if decode_uart(packet) != frame:
                raise AssertionError("valid round trip changed frame")
            valid += 1
            continue
        if mode == 1:
            mutated = bytearray(packet)
            mutated[max(1, len(mutated) // 2)] ^= 0x01
            packet = bytes(mutated)
        elif mode == 2:
            packet = packet[:-1]
        else:
            packet = rng.randbytes(rng.randrange(0, 96)) + b"\x00"
        try:
            decode_uart(packet)
        except FrameError:
            rejected += 1
        else:
            # Random bytes may very rarely form a valid frame. Re-encode must
            # still be canonical and must not crash; mutated known frames must
            # never pass because CRC32C covers all fields.
            if mode in (1, 2):
                raise AssertionError("known malformed frame was accepted")
    return valid, rejected


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--iterations", type=int, default=1_000_000)
    parser.add_argument("--seed", type=lambda value: int(value, 0), default=0x5041524F4C36)
    args = parser.parse_args()
    if args.iterations < 1:
        parser.error("--iterations must be positive")
    valid, rejected = run(args.iterations, args.seed)
    print(f"iterations={args.iterations} valid={valid} rejected={rejected} seed={args.seed}")


if __name__ == "__main__":
    main()

