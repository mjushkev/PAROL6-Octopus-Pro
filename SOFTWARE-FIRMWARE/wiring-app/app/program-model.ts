export const PROGRAM_SCHEMA = "parol6.commander-program.v1";

export type JointEnvelope = { id: string; min: number; max: number; vmax: number; accel: number };
export type ProgramWaypoint = {
  id: string;
  name: string;
  targets: number[];
  speedPercent: number;
  dwellMs: number;
};
export type RobotProgram = {
  schema: typeof PROGRAM_SCHEMA;
  name: string;
  repeatCount: number;
  waypoints: ProgramWaypoint[];
};
export type CompiledProgramMove = {
  cycle: number;
  waypointIndex: number;
  name: string;
  targets: number[];
  speedPercent: number;
  durationMs: number;
  dwellMs: number;
};

export function createEmptyProgram(): RobotProgram {
  return { schema: PROGRAM_SCHEMA, name: "My robot program", repeatCount: 1, waypoints: [] };
}

export function validateProgramDocument(value: unknown, jointCount = 6): RobotProgram {
  if (!value || typeof value !== "object") throw new Error("Program file is not valid.");
  const document = value as Partial<RobotProgram>;
  if (document.schema !== PROGRAM_SCHEMA) throw new Error("Unsupported program file version.");
  if (typeof document.name !== "string" || !document.name.trim() || document.name.length > 80) throw new Error("Program name is required.");
  if (!Number.isInteger(document.repeatCount) || Number(document.repeatCount) < 1 || Number(document.repeatCount) > 20) throw new Error("Repeat count must be 1–20.");
  if (!Array.isArray(document.waypoints) || document.waypoints.length < 1 || document.waypoints.length > 32) throw new Error("A program needs 1–32 waypoints.");
  document.waypoints.forEach((waypoint, index) => {
    if (!waypoint || typeof waypoint !== "object" || typeof waypoint.id !== "string" || typeof waypoint.name !== "string" || !waypoint.name.trim()) throw new Error(`Waypoint ${index + 1} needs a name.`);
    if (!Array.isArray(waypoint.targets) || waypoint.targets.length !== jointCount || waypoint.targets.some((target) => !Number.isFinite(target))) throw new Error(`Waypoint ${index + 1} needs ${jointCount} valid joint angles.`);
    if (!Number.isInteger(waypoint.speedPercent) || waypoint.speedPercent < 1 || waypoint.speedPercent > 10) throw new Error(`Waypoint ${index + 1} speed must be 1–10%.`);
    if (!Number.isInteger(waypoint.dwellMs) || waypoint.dwellMs < 0 || waypoint.dwellMs > 60_000) throw new Error(`Waypoint ${index + 1} wait must be 0–60 seconds.`);
  });
  return document as RobotProgram;
}

export function compileProgram(program: RobotProgram, current: number[], joints: readonly JointEnvelope[]): CompiledProgramMove[] {
  const checked = validateProgramDocument(program, joints.length);
  if (current.length !== joints.length || current.some((value) => !Number.isFinite(value))) throw new Error("Current robot position is unavailable.");
  const moves: CompiledProgramMove[] = [];
  let start = [...current];
  for (let cycle = 0; cycle < checked.repeatCount; cycle += 1) {
    checked.waypoints.forEach((waypoint, waypointIndex) => {
      waypoint.targets.forEach((target, axis) => {
        const joint = joints[axis];
        if (target < joint.min || target > joint.max) throw new Error(`${joint.id} in “${waypoint.name}” must stay between ${joint.min}° and ${joint.max}°.`);
      });
      let seconds = 0.5;
      joints.forEach((joint, axis) => {
        const distance = Math.abs(waypoint.targets[axis] - start[axis]);
        const scale = waypoint.speedPercent / 10;
        seconds = Math.max(seconds, distance / (0.75 * joint.vmax * scale), Math.sqrt(distance / (0.1875 * joint.accel * scale)));
      });
      const durationMs = Math.ceil((seconds * 1.08 * 1000) / 100) * 100;
      if (durationMs > 60_000) throw new Error(`Move to “${waypoint.name}” exceeds 60 seconds. Add a nearer waypoint or increase its speed.`);
      moves.push({ cycle, waypointIndex, name: waypoint.name, targets: [...waypoint.targets], speedPercent: waypoint.speedPercent, durationMs, dwellMs: waypoint.dwellMs });
      start = [...waypoint.targets];
    });
  }
  return moves;
}
