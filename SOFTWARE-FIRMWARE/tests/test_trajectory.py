import unittest

import _bootstrap

from parol6_backend import TrajectoryBuffer, TrajectoryError, TrajectoryPoint


START = (10_240, -32_000, 57_905, 0, 0, 32_000)


class TrajectoryTests(unittest.TestCase):
    def test_valid_initial_horizon(self) -> None:
        queue = TrajectoryBuffer()
        queue.begin(1, START)
        queue.append(TrajectoryPoint(0, 100, (10_250, -31_990, 57_915, 10, 10, 32_010)))
        queue.append(TrajectoryPoint(1, 100, (10_260, -31_980, 57_925, 20, 20, 32_020)))
        queue.validate_commit(1)
        self.assertEqual(queue.horizon_ms, 200)

    def test_non_monotonic_and_limits_are_rejected(self) -> None:
        queue = TrajectoryBuffer()
        queue.begin(2, START)
        with self.assertRaisesRegex(TrajectoryError, "non_monotonic"):
            queue.append(TrajectoryPoint(1, 100, START))
        with self.assertRaisesRegex(TrajectoryError, "j1_position"):
            queue.append(TrajectoryPoint(0, 100, (20_000, -32_000, 57_905, 0, 0, 32_000)))

    def test_rate_and_max_horizon_are_bounded(self) -> None:
        queue = TrajectoryBuffer()
        queue.begin(3, START)
        with self.assertRaisesRegex(TrajectoryError, "j4_rate"):
            queue.append(TrajectoryPoint(0, 1, (10_240, -32_000, 57_905, 100, 0, 32_000)))
        for index in range(4):
            queue.append(TrajectoryPoint(index, 250, START))
        with self.assertRaisesRegex(TrajectoryError, "horizon_too_large"):
            queue.append(TrajectoryPoint(4, 1, START))


if __name__ == "__main__":
    unittest.main()

