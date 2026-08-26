# Stage 3 Robot Commander programs

The Stage 3 Commander runs directly in current Chrome or Edge over USB. It uses
the installed `0.9.1-motion-rc` firmware and the owner-validated calibration in
`config/robot.mattj.calibrated.json`. No ESP is involved.

## First small program

1. Clear the robot workspace, mechanically support any gravity-loaded joint as
   needed, and keep the physical E-stop within immediate reach.
2. Connect USB, enable motion, and home all six joints. J1 uses Manual zero by
   default; select Auto sensor only after the J1 sensor is repaired and tested.
3. Jog to a clear starting pose and choose **Capture current pose** in Programs.
4. Jog a small distance to a second clear pose and capture it.
5. Leave each waypoint at 1% speed for the first run and use one repeat.
6. Press **Dry run & start**, review the move count and estimated time, then
   confirm only if the entire swept path is clear.
7. Use **STOP PROGRAM**, **MOTOR STOP**, or the physical E-stop at any sign of
   unexpected movement.

The full sequence is validated before the first command. Every target must stay
inside the commissioned joint limits, each move uses the same synchronized
acceleration-limited duration calculation as single-pose motion, and the
firmware independently enforces its calibrated limits and 10% coordinated cap.

## Program behavior

- Up to 32 waypoints and 20 repeats.
- Six joint angles, 1–10% coordinated speed, and 0–60 seconds of wait time per
  waypoint.
- The controller keeps supervised holding torque between waypoints, so a
  gravity-loaded arm is not deliberately released during a normal sequence.
- The final waypoint hold is released after its wait time and all drivers are
  disabled.
- A controller error, USB disconnect, software Motor Stop, or physical E-stop
  cancels the in-memory queue. A stopped program never resumes automatically.
- The current program is retained in that browser only. Export JSON for a
  portable copy; imported files are schema-checked and are validated again
  against the live calibration before running.

## Current boundary

This stage is joint-space control. It does not yet calculate Cartesian tool
positions, plan collision-free paths, operate a gripper, or replace physical
guards and an E-stop. A valid set of joint angles can still produce a collision
with the robot, its base, wiring, or the surrounding workspace; the operator is
responsible for verifying the full swept path.
