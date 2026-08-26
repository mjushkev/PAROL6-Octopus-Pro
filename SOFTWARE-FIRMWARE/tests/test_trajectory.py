import unittest

import _bootstrap

from parol6_backend import (
    TrajectoryBuffer,
    TrajectoryError,
    TrajectoryPlanner,
    TrajectoryPoint,
    load_default_calibration,
)


CALIBRATION = load_default_calibration()
START = CALIBRATION.pose_to_raw_steps((0, 0, 0, 0, 0, 0))
POINT1 = CALIBRATION.pose_to_raw_steps((0.1, 0.1, 0.1, 0.1, -0.1, 0.1))
POINT2 = CALIBRATION.pose_to_raw_steps((0.2, 0.2, 0.2, 0.2, -0.2, 0.2))


class TrajectoryTests(unittest.TestCase):
    def test_valid_initial_horizon(self) -> None:
        queue = TrajectoryBuffer()
        queue.begin(1, START)
        queue.append(TrajectoryPoint(0, 100, POINT1))
        queue.append(TrajectoryPoint(1, 100, POINT2))
        queue.validate_commit(1)
        self.assertEqual(queue.horizon_ms, 200)

    def test_non_monotonic_and_limits_are_rejected(self) -> None:
        queue = TrajectoryBuffer()
        queue.begin(2, START)
        with self.assertRaisesRegex(TrajectoryError, "non_monotonic"):
            queue.append(TrajectoryPoint(1, 100, START))
        with self.assertRaisesRegex(TrajectoryError, "j1_position"):
            queue.append(TrajectoryPoint(0, 100, (30_000, 0, 0, 0, 0, 0)))

    def test_rate_and_max_horizon_are_bounded(self) -> None:
        queue = TrajectoryBuffer()
        queue.begin(3, START)
        with self.assertRaisesRegex(TrajectoryError, "j4_rate"):
            queue.append(TrajectoryPoint(0, 1, (0, 0, 0, 100, 0, 0)))
        for index in range(240):
            queue.append(TrajectoryPoint(index, 250, START))
        with self.assertRaisesRegex(TrajectoryError, "horizon_too_large"):
            queue.append(TrajectoryPoint(240, 1, START))

    def test_planner_uses_calibrated_directions_limits_and_initial_speed_cap(self) -> None:
        planner = TrajectoryPlanner(CALIBRATION)
        target = (10.0, 5.0, 8.0, 12.0, -10.0, 15.0)
        plan = planner.plan_pose((0, 0, 0, 0, 0, 0), target, speed_percent=10)
        self.assertGreaterEqual(plan.duration_ms, 150)
        self.assertEqual(plan.points[-1].target_steps, CALIBRATION.pose_to_raw_steps(target))
        self.assertLess(plan.points[-1].target_steps[2], 0)  # J3 logical + is raw -.
        queue = TrajectoryBuffer(CALIBRATION)
        queue.begin(9, START)
        for point in plan.points:
            queue.append(point)
        queue.validate_commit(9)
        with self.assertRaisesRegex(TrajectoryError, "speed_percent_exceeds"):
            planner.plan_pose((0, 0, 0, 0, 0, 0), target, speed_percent=11)

    def test_long_plan_respects_fixed_queue_capacity(self) -> None:
        planner = TrajectoryPlanner(CALIBRATION)
        target = (-100.0, 20.0, 20.0, 20.0, -20.0, 20.0)
        plan = planner.plan_pose((0, 0, 0, 0, 0, 0), target, speed_percent=10)
        self.assertLessEqual(len(plan.points), 512)
        self.assertEqual(
            sum(point.duration_ms for point in plan.points), plan.duration_ms
        )


if __name__ == "__main__":
    unittest.main()
