from __future__ import annotations

from dataclasses import dataclass
import unittest


@dataclass(frozen=True)
class Slot:
    sequence: int | None
    valid_crc: bool = True
    safe: bool = True

    @property
    def valid(self) -> bool:
        return self.sequence not in (None, 0) and self.valid_crc and self.safe


def newer(candidate: int, reference: int) -> bool:
    return candidate != reference and ((candidate - reference) & 0xFFFFFFFF) < 0x80000000


def select(a: Slot, b: Slot) -> int | None:
    if not a.valid and not b.valid:
        return None
    if a.valid and (not b.valid or not newer(b.sequence, a.sequence)):
        return 0
    return 1


class PersistentStoreRecoveryModelTests(unittest.TestCase):
    """Power-cut checkpoints for the dual-sector commit order used in C++."""

    def test_interrupted_erase_keeps_the_active_slot(self) -> None:
        active = Slot(17)
        interrupted_inactive = Slot(None, valid_crc=False)
        self.assertEqual(select(active, interrupted_inactive), 0)

    def test_partial_new_config_keeps_the_previous_slot(self) -> None:
        previous = Slot(17)
        partial = Slot(18, valid_crc=False)
        self.assertEqual(select(previous, partial), 0)

    def test_complete_new_config_commits_before_old_slot_is_reused(self) -> None:
        previous = Slot(17)
        committed = Slot(18)
        self.assertEqual(select(previous, committed), 1)

    def test_unsafe_or_crc_corrupt_newer_config_is_never_selected(self) -> None:
        previous = Slot(17)
        self.assertEqual(select(previous, Slot(18, safe=False)), 0)
        self.assertEqual(select(previous, Slot(18, valid_crc=False)), 0)

    def test_sequence_wrap_selects_one_as_newer_than_maximum(self) -> None:
        # Sequence zero itself is reserved/invalid in the firmware, so the
        # writer must fail before this boundary. The comparison still models
        # wrap-safe ordering for all other values.
        self.assertTrue(newer(1, 0xFFFFFFFF))
        self.assertEqual(select(Slot(0xFFFFFFFF), Slot(1)), 1)


if __name__ == "__main__":
    unittest.main()
