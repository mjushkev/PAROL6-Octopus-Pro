import unittest

import _bootstrap

from parol6_backend import (
    MotionProgram,
    ProgramError,
    ProgramPlanner,
    ProgramWaypoint,
)


class ProgramTests(unittest.TestCase):
    def test_multi_step_program_uses_calibrated_planner_and_repeats(self) -> None:
        program = MotionProgram(
            "Pick cycle",
            (
                ProgramWaypoint("Approach", (0, 5, 5, 10, -130, 0), 10, 500),
                ProgramWaypoint("Present", (-10, 10, 8, 20, -100, 15), 5, 250),
            ),
            repeat_count=2,
        )
        planned = ProgramPlanner().plan(program, (0, 0, 0, 0, -130, 0))
        self.assertEqual(len(planned.steps), 4)
        self.assertEqual(planned.steps[2].cycle, 1)
        self.assertEqual(planned.steps[-1].trajectory.target_deg, program.waypoints[-1].pose_deg)
        self.assertEqual(planned.total_dwell_ms, 1500)
        self.assertGreater(planned.total_motion_ms, 0)

    def test_program_rejects_limits_speed_dwell_and_empty_programs(self) -> None:
        planner = ProgramPlanner()
        with self.assertRaisesRegex(ProgramError, "1_to_32"):
            planner.plan(MotionProgram("Empty", ()), (0, 0, 0, 0, -130, 0))
        with self.assertRaisesRegex(ValueError, "J5_angle_out_of_range"):
            planner.plan(
                MotionProgram("Bad", (ProgramWaypoint("Unsafe", (0, 0, 0, 0, 1, 0)),)),
                (0, 0, 0, 0, -130, 0),
            )
        with self.assertRaisesRegex(ValueError, "speed_percent_exceeds"):
            planner.plan(
                MotionProgram("Fast", (ProgramWaypoint("Too fast", (0, 0, 0, 0, -130, 0), 11),)),
                (0, 0, 0, 0, -130, 0),
            )
        with self.assertRaisesRegex(ProgramError, "dwell_out_of_range"):
            planner.plan(
                MotionProgram("Wait", (ProgramWaypoint("Too long", (0, 0, 0, 0, -130, 0), 10, 60_001),)),
                (0, 0, 0, 0, -130, 0),
            )


if __name__ == "__main__":
    unittest.main()
