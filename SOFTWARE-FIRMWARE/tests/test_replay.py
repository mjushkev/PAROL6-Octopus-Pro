import unittest

import _bootstrap

from parol6_protocol import ReplayDecision, ReplayWindow


class ReplayTests(unittest.TestCase):
    def test_duplicate_and_old_sequences_are_rejected(self) -> None:
        window = ReplayWindow(width=8)
        self.assertEqual(window.check_and_mark(10), ReplayDecision.ACCEPT)
        self.assertEqual(window.check_and_mark(10), ReplayDecision.DUPLICATE)
        self.assertEqual(window.check_and_mark(12), ReplayDecision.ACCEPT)
        self.assertEqual(window.check_and_mark(11), ReplayDecision.ACCEPT)
        self.assertEqual(window.check_and_mark(11), ReplayDecision.DUPLICATE)
        self.assertEqual(window.check_and_mark(3), ReplayDecision.TOO_OLD)

    def test_large_forward_jump_drops_prior_window(self) -> None:
        window = ReplayWindow(width=4)
        window.check_and_mark(1)
        self.assertEqual(window.check_and_mark(100), ReplayDecision.ACCEPT)
        self.assertEqual(window.check_and_mark(1), ReplayDecision.TOO_OLD)


if __name__ == "__main__":
    unittest.main()

