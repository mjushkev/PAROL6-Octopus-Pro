"use client";

import { useEffect, useRef, useState } from "react";
import { createEmptyProgram, validateProgramDocument, type RobotProgram } from "./program-model";

const storageKey = "parol6-commander-program-v1";
const jointLabels = ["J1", "J2", "J3", "J4", "J5", "J6"];
const makeId = () => globalThis.crypto?.randomUUID?.() ?? `${Date.now()}-${Math.random()}`;

export default function ProgramPanel({ positions, canRun, running, activeStep, onRun, onStop, onMessage }: {
  positions: number[];
  canRun: boolean;
  running: boolean;
  activeStep: number;
  onRun: (program: RobotProgram) => void;
  onStop: () => void;
  onMessage: (message: string) => void;
}) {
  const [program, setProgram] = useState<RobotProgram>(createEmptyProgram);
  const [loaded, setLoaded] = useState(false);
  const importRef = useRef<HTMLInputElement | null>(null);

  useEffect(() => {
    const loadTimer = window.setTimeout(() => {
      try {
        const saved = window.localStorage.getItem(storageKey);
        if (saved) setProgram(JSON.parse(saved) as RobotProgram);
      } catch { /* keep a clean program */ }
      setLoaded(true);
    }, 0);
    return () => window.clearTimeout(loadTimer);
  }, []);
  useEffect(() => {
    if (!loaded) return;
    window.localStorage.setItem(storageKey, JSON.stringify(program));
  }, [loaded, program]);

  const addCurrent = () => setProgram((current) => ({ ...current, waypoints: [...current.waypoints, { id: makeId(), name: `Waypoint ${current.waypoints.length + 1}`, targets: [...positions], speedPercent: 10, dwellMs: 0 }] }));
  const updateWaypoint = (index: number, patch: Partial<RobotProgram["waypoints"][number]>) => setProgram((current) => ({ ...current, waypoints: current.waypoints.map((waypoint, itemIndex) => itemIndex === index ? { ...waypoint, ...patch } : waypoint) }));
  const moveWaypoint = (index: number, delta: number) => setProgram((current) => { const next = [...current.waypoints]; const target = index + delta; if (target < 0 || target >= next.length) return current; [next[index], next[target]] = [next[target], next[index]]; return { ...current, waypoints: next }; });
  const download = () => { const blob = new Blob([JSON.stringify(program, null, 2)], { type: "application/json" }); const url = URL.createObjectURL(blob); const anchor = document.createElement("a"); anchor.href = url; anchor.download = `${program.name.trim().replace(/[^a-z0-9_-]+/gi, "-") || "PAROL6-program"}.json`; anchor.click(); URL.revokeObjectURL(url); onMessage("Program exported as JSON."); };
  const importProgram = async (file?: File) => { if (!file) return; try { const document = validateProgramDocument(JSON.parse(await file.text())); setProgram(document); onMessage(`Loaded “${document.name}”. Review every waypoint before running.`); } catch (error) { onMessage(error instanceof Error ? error.message : "Could not import program."); } };

  return <section className="program-card">
    <div className="program-heading"><div><span>STAGE 3 · PROGRAMS</span><input aria-label="Program name" value={program.name} disabled={running} onChange={(event) => setProgram((current) => ({ ...current, name: event.target.value }))} /><p>Capture poses, set speed and wait time, then run the sequence through the same calibrated firmware limits.</p></div><div className="program-file-actions"><button disabled={running} onClick={() => importRef.current?.click()}>Import</button><button disabled={running || !program.waypoints.length} onClick={download}>Export</button><input ref={importRef} hidden type="file" accept="application/json,.json" onChange={(event) => { void importProgram(event.target.files?.[0]); event.target.value = ""; }} /></div></div>
    <div className="program-toolbar"><button disabled={running || program.waypoints.length >= 32} onClick={addCurrent}>+ Capture current pose</button><label>Repeat<input type="number" min="1" max="20" value={program.repeatCount} disabled={running} onChange={(event) => setProgram((current) => ({ ...current, repeatCount: Math.max(1, Math.min(20, Math.round(Number(event.target.value) || 1))) }))} /></label><b>{program.waypoints.length} waypoint{program.waypoints.length === 1 ? "" : "s"}</b>{running ? <button className="program-stop" onClick={onStop}>STOP PROGRAM</button> : <button className="program-run" disabled={!canRun || !program.waypoints.length} onClick={() => onRun(program)}>Dry run & start</button>}</div>
    {!program.waypoints.length && <div className="program-empty">Jog the robot or move to a pose, then press <b>Capture current pose</b>.</div>}
    <div className="program-waypoints">{program.waypoints.map((waypoint, index) => <article key={waypoint.id} className={running && activeStep === index ? "active" : ""}>
      <div className="program-step-number">{index + 1}</div><div className="program-step-body"><div className="program-step-top"><input aria-label={`Waypoint ${index + 1} name`} value={waypoint.name} disabled={running} onChange={(event) => updateWaypoint(index, { name: event.target.value })} /><label>Speed<input type="number" min="1" max="10" value={waypoint.speedPercent} disabled={running} onChange={(event) => updateWaypoint(index, { speedPercent: Math.max(1, Math.min(10, Math.round(Number(event.target.value) || 1))) })} />%</label><label>Wait<input type="number" min="0" max="60" step="0.1" value={waypoint.dwellMs / 1000} disabled={running} onChange={(event) => updateWaypoint(index, { dwellMs: Math.max(0, Math.min(60_000, Math.round((Number(event.target.value) || 0) * 1000))) })} />s</label></div><div className="program-targets">{jointLabels.map((joint, axis) => <label key={joint}>{joint}<input type="number" step="0.001" value={waypoint.targets[axis]} disabled={running} onChange={(event) => { const targets = [...waypoint.targets]; targets[axis] = Number(event.target.value); updateWaypoint(index, { targets }); }} /></label>)}</div></div>
      <div className="program-step-actions"><button aria-label="Move waypoint up" disabled={running || index === 0} onClick={() => moveWaypoint(index, -1)}>↑</button><button aria-label="Move waypoint down" disabled={running || index === program.waypoints.length - 1} onClick={() => moveWaypoint(index, 1)}>↓</button><button className="delete" aria-label="Delete waypoint" disabled={running} onClick={() => setProgram((current) => ({ ...current, waypoints: current.waypoints.filter((_, itemIndex) => itemIndex !== index) }))}>×</button></div>
    </article>)}</div>
    <small className="program-note">Programs are stored only in this browser. A stopped or interrupted program never resumes automatically.</small>
  </section>;
}
