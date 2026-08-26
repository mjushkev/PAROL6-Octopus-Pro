"use client";

import { useCallback, useEffect, useRef, useState, useSyncExternalStore } from "react";
import ProgramPanel from "./program-panel";
import { compileProgram, type CompiledProgramMove, type RobotProgram } from "./program-model";

type SerialPortLike = { readable: ReadableStream<Uint8Array> | null; writable: WritableStream<Uint8Array> | null; open(options: { baudRate: number }): Promise<void>; close(): Promise<void> };
type SerialManager = { requestPort(options?: { filters?: { usbVendorId?: number; usbProductId?: number }[] }): Promise<SerialPortLike> };
type HomeMode = "manual" | "auto";

const joints = [
  { id: "J1", name: "Base", min: -230, max: 35, vmax: 4, accel: 8, servo: true },
  { id: "J2", name: "Shoulder", min: 0, max: 119.536, vmax: 1, accel: 2.5, servo: true },
  { id: "J3", name: "Upper arm", min: 0, max: 90.329, vmax: 4.5, accel: 12, servo: false },
  { id: "J4", name: "Elbow", min: 0, max: 232.694, vmax: 4.5, accel: 12, servo: false },
  { id: "J5", name: "Wrist pitch", min: -254.25, max: 0, vmax: 4.5, accel: 12, servo: false },
  { id: "J6", name: "Wrist rotate", min: -180, max: 180, vmax: 4.5, accel: 12, servo: false },
] as const;
const homeOrder = ["J1", "J2", "J3", "J4", "J6", "J5"];

export default function OperatorPanel({ onShowSetup, onShowWiring }: { onShowSetup: () => void; onShowWiring: () => void }) {
  const portRef = useRef<SerialPortLike | null>(null);
  const readerRef = useRef<ReadableStreamDefaultReader<Uint8Array> | null>(null);
  const writerRef = useRef<WritableStreamDefaultWriter<Uint8Array> | null>(null);
  const bufferRef = useRef(""); const tokenRef = useRef("");
  const sendRef = useRef<(command: string) => Promise<void>>(async () => undefined);
  const homeQueueRef = useRef<string[]>([]); const keepaliveRef = useRef<number | null>(null);
  const programQueueRef = useRef<CompiledProgramMove[]>([]); const programCurrentRef = useRef<CompiledProgramMove | null>(null);
  const programRunningRef = useRef(false); const programFinalReleaseRef = useRef(false); const programDwellTimerRef = useRef<number | null>(null);
  const [connected, setConnected] = useState(false); const [firmware, setFirmware] = useState("Not identified");
  const [message, setMessage] = useState("Connect USB. Nothing moves automatically.");
  const [motionEnabled, setMotionEnabled] = useState(false); const [busy, setBusy] = useState(false);
  const [held, setHeld] = useState("NONE"); const [homeMode, setHomeMode] = useState<HomeMode>("manual");
  const [positions, setPositions] = useState<Record<string, number>>(Object.fromEntries(joints.map(({ id }) => [id, 0])));
  const [homed, setHomed] = useState<Record<string, boolean>>(Object.fromEntries(joints.map(({ id }) => [id, false])));
  const [targets, setTargets] = useState<Record<string, string>>(Object.fromEntries(joints.map(({ id }) => [id, "0.000"])));
  const [jogSpeed, setJogSpeed] = useState(6); const [jogStep, setJogStep] = useState(1); const [speedPercent, setSpeedPercent] = useState(10);
  const [preview, setPreview] = useState<{ durationMs: number; targets: number[] } | null>(null); const [logs, setLogs] = useState<string[]>([]);
  const [programRunning, setProgramRunning] = useState(false); const [programStep, setProgramStep] = useState(-1);
  const serialSupported = useSyncExternalStore(() => () => undefined, () => "serial" in navigator, () => true);
  const firmwareReady = firmware === "0.9.1-motion-rc"; const allHomed = joints.every(({ id }) => homed[id]);

  useEffect(() => { const saved = window.localStorage.getItem("parol6-j1-home-mode"); if (saved !== "auto" && saved !== "manual") return; const timer = window.setTimeout(() => setHomeMode(saved), 0); return () => window.clearTimeout(timer); }, []);
  const appendLog = useCallback((line: string) => setLogs((current) => [...current.slice(-99), line]), []);
  const send = useCallback(async (command: string) => { if (!writerRef.current) throw new Error("USB is not connected"); appendLog(`> ${command}`); await writerRef.current.write(new TextEncoder().encode(`${command}\n`)); }, [appendLog]);
  useEffect(() => { sendRef.current = send; }, [send]);
  const clearKeepalive = useCallback(() => { if (keepaliveRef.current !== null) window.clearInterval(keepaliveRef.current); keepaliveRef.current = null; }, []);
  const clearProgramRun = useCallback(() => { if (programDwellTimerRef.current !== null) window.clearTimeout(programDwellTimerRef.current); programDwellTimerRef.current = null; programQueueRef.current = []; programCurrentRef.current = null; programRunningRef.current = false; programFinalReleaseRef.current = false; setProgramRunning(false); setProgramStep(-1); }, []);

  const startNextHome = useCallback((token: string) => {
    const id = homeQueueRef.current.shift();
    if (!id) { setBusy(false); setMessage("Homing sequence complete."); void sendRef.current("STATUS"); return; }
    const command = id === "J1" && homeMode === "manual" ? `MANUAL_HOME J1 ${token} SET_CURRENT_POSITION_ZERO_TEMPORARY` : `HOME ${id} ${token} START`;
    window.setTimeout(() => void sendRef.current(command), 140);
  }, [homeMode]);

  const startNextProgramMove = useCallback(() => {
    const move = programQueueRef.current.shift();
    if (!move) { clearProgramRun(); setBusy(false); setHeld("NONE"); setMessage("Program complete. Motor hold released."); void sendRef.current("STATUS"); return; }
    programCurrentRef.current = move; setProgramStep(move.waypointIndex); setBusy(true); setHeld("NONE");
    const targetsMdeg = move.targets.map((value) => Math.round(value * 1000)).join(" ");
    setMessage(`Program move: ${move.name} · ${(move.durationMs / 1000).toFixed(1)} s.`);
    void sendRef.current(`COORD_MOVE ${tokenRef.current} ${move.durationMs} ${targetsMdeg} COORDINATED_MOVE_VERIFIED`);
  }, [clearProgramRun]);

  const parseLine = useCallback((line: string) => {
    appendLog(line); const nextToken = line.match(/\btoken=([0-9A-F]{8})\b/)?.[1]; if (nextToken) tokenRef.current = nextToken;
    if (line.startsWith("PAROL6_MOTION_RC_READY")) { const version = line.match(/\bversion=([^ ]+)/)?.[1] ?? "Unknown"; setFirmware(version); setMessage(version === "0.9.1-motion-rc" ? "Controller ready. Enable motion when the robot area is clear." : `Motion firmware 0.9.1 is required; found ${version}.`); window.setTimeout(() => void sendRef.current("STATUS"), 100); return; }
    if (line.startsWith("PAROL6_STATUS")) { const nextPositions: Record<string, number> = {}; const nextHomed: Record<string, boolean> = {}; for (const match of line.matchAll(/\b(J[1-6])_mdeg=(-?[0-9]+)/g)) nextPositions[match[1]] = Number(match[2]) / 1000; for (const match of line.matchAll(/\b(J[1-6])_homed=([01])/g)) nextHomed[match[1]] = match[2] === "1"; setPositions(nextPositions); setHomed(nextHomed); setBusy(/\bmoving=1\b/.test(line)); setHeld(line.match(/\bheld=([^ ]+)/)?.[1] ?? "NONE"); return; }
    if (line.startsWith("PAROL6_SERVO_CONFIGURED")) { if (line.includes("joint=J1") && nextToken) window.setTimeout(() => void sendRef.current(`SERVO_CONFIG J2 ${nextToken} ACTIVE_LOW INTERFACE_VERIFIED`), 100); else { setMotionEnabled(true); setBusy(false); setMessage("Motion enabled at the calibrated commissioning limits."); } return; }
    if (line.startsWith("PAROL6_HOME_STARTED") || line.startsWith("PAROL6_MOTION_STARTED") || line.startsWith("PAROL6_HOLD_STARTED") || line.startsWith("PAROL6_COORDINATED_STARTED")) { setBusy(true); setHeld("NONE"); if (line.startsWith("PAROL6_HOLD_STARTED") && nextToken) keepaliveRef.current = window.setInterval(() => void sendRef.current(`HOLD_KEEPALIVE ${nextToken}`), 150); setMessage(line.startsWith("PAROL6_COORDINATED") ? "Synchronized move running at the selected bounded speed." : "Motion in progress…"); return; }
    if (line.startsWith("PAROL6_HOME joint=") || line.startsWith("PAROL6_MANUAL_HOME")) { const id = line.startsWith("PAROL6_MANUAL") ? "J1" : line.match(/\bjoint=(J[1-6])/)?.[1]; const complete = /\bresult=complete\b/.test(line); setBusy(false); if (id && complete) setHomed((current) => ({ ...current, [id]: true })); if (complete && homeQueueRef.current.length && nextToken) startNextHome(nextToken); else { if (!complete) homeQueueRef.current = []; setMessage(complete ? id === "J5" ? "J5 homed at 0° and moved to its −130° standby position." : `${id} homed at 0°.` : `Homing stopped: ${line.match(/\bresult=([^ ]+)/)?.[1]?.replaceAll("_", " ") ?? "unknown"}.`); void sendRef.current("STATUS"); } return; }
    if (line.startsWith("PAROL6_MOTION_DONE")) { clearKeepalive(); setBusy(false); setMessage(`Jog stopped: ${(line.match(/\bresult=([^ ]+)/)?.[1] ?? "complete").replaceAll("_", " ")}.`); void sendRef.current("STATUS"); return; }
    if (line.startsWith("PAROL6_MOTOR_HOLD")) { clearKeepalive(); setBusy(false); setHeld(line.match(/\bjoint=(J[1-6])/)?.[1] ?? "NONE"); void sendRef.current("STATUS"); return; }
    if (line.startsWith("PAROL6_COORDINATED_DONE")) {
      const complete = /\bresult=complete\b/.test(line); setBusy(false); setHeld(complete && /\bhold=1\b/.test(line) ? "ALL" : "NONE");
      const reported: Record<string, number> = {}; for (const match of line.matchAll(/\b(J[1-6])_mdeg=(-?[0-9]+)/g)) reported[match[1]] = Number(match[2]) / 1000; if (Object.keys(reported).length === 6) setPositions(reported);
      if (programRunningRef.current) {
        const move = programCurrentRef.current;
        if (!complete || !move) { const reason = line.match(/\bresult=([^ ]+)/)?.[1]?.replaceAll("_", " ") ?? "unknown"; clearProgramRun(); setMessage(`Program stopped: ${reason}.`); void sendRef.current("STATUS"); return; }
        const continueOrRelease = () => {
          programDwellTimerRef.current = null;
          programCurrentRef.current = null;
          if (programQueueRef.current.length) startNextProgramMove();
          else {
            programFinalReleaseRef.current = true;
            void sendRef.current(`COORD_HOLD_RELEASE ${tokenRef.current} RELEASE_COORDINATED_HOLD_VERIFIED`);
          }
        };
        setMessage(move.dwellMs ? `Reached ${move.name}. Waiting ${(move.dwellMs / 1000).toFixed(1)} s.` : `Reached ${move.name}. Continuing…`);
        programDwellTimerRef.current = window.setTimeout(continueOrRelease, Math.max(100, move.dwellMs));
        return;
      }
      setMessage(complete ? "Pose reached. Motors are holding while USB supervision remains active." : `Move stopped: ${line.match(/\bresult=([^ ]+)/)?.[1]?.replaceAll("_", " ") ?? "unknown"}.`); void sendRef.current("STATUS"); return;
    }
    if (line.startsWith("PAROL6_COORDINATED_HOLD_RELEASED")) { setHeld("NONE"); if (programFinalReleaseRef.current) { clearProgramRun(); setBusy(false); setMessage("Program complete. All drivers are disabled."); void sendRef.current("STATUS"); } else setMessage("Pose hold released; all drivers are disabled."); return; }
    if (line.startsWith("PAROL6_STOPPED")) { clearKeepalive(); clearProgramRun(); homeQueueRef.current = []; setBusy(false); setHeld("NONE"); setMotionEnabled(false); setMessage("Motor stop complete. All drivers are disabled."); return; }
    if (line.startsWith("PAROL6_ERROR")) { clearKeepalive(); clearProgramRun(); homeQueueRef.current = []; setBusy(false); setHeld("NONE"); setMessage(`Blocked: ${(line.match(/\bcode=([^ ]+)/)?.[1] ?? "unknown").replaceAll("_", " ")}.`); }
  }, [appendLog, clearKeepalive, clearProgramRun, startNextHome, startNextProgramMove]);

  const readLoop = useCallback(async (port: SerialPortLike) => { if (!port.readable) return; const reader = port.readable.getReader(); readerRef.current = reader; const decoder = new TextDecoder(); try { while (true) { const { value, done } = await reader.read(); if (done) break; bufferRef.current += decoder.decode(value, { stream: true }); const lines = bufferRef.current.split(/\r?\n/); bufferRef.current = lines.pop() ?? ""; for (const valueLine of lines) if (valueLine.trim()) parseLine(valueLine.trim()); } } catch (error) { if (portRef.current === port) setMessage(`USB connection stopped: ${error instanceof Error ? error.message : "unknown error"}`); } finally { reader.releaseLock(); if (portRef.current === port) { const writer = writerRef.current; portRef.current = null; writerRef.current = null; readerRef.current = null; try { writer?.releaseLock(); } catch { /* stream already closed */ } try { await port.close(); } catch { /* device already gone */ } tokenRef.current = ""; clearKeepalive(); clearProgramRun(); homeQueueRef.current = []; setConnected(false); setFirmware("Not identified"); setMotionEnabled(false); setBusy(false); setHeld("NONE"); setHomed(Object.fromEntries(joints.map(({ id }) => [id, false]))); setMessage("USB disconnected. Program and motion state were cleared."); } } }, [clearKeepalive, clearProgramRun, parseLine]);
  const connect = async () => { try { const manager = (navigator as Navigator & { serial?: SerialManager }).serial; if (!manager) throw new Error("Use current Chrome or Edge for USB control."); const port = await manager.requestPort({ filters: [{ usbVendorId: 0x0483, usbProductId: 0x5740 }] }); await port.open({ baudRate: 3000000 }); if (!port.writable) throw new Error("USB port is not writable"); portRef.current = port; writerRef.current = port.writable.getWriter(); setConnected(true); setMessage("Identifying controller…"); void readLoop(port); window.setTimeout(() => void send("IDENTIFY"), 350); } catch (error) { setMessage(error instanceof Error ? error.message : "Could not connect"); } };
  const disconnect = async () => { try { await send("STOP"); } catch { /* already gone */ } clearKeepalive(); clearProgramRun(); const reader = readerRef.current; const writer = writerRef.current; const port = portRef.current; portRef.current = null; try { await reader?.cancel(); } catch { /* closed */ } try { writer?.releaseLock(); } catch { /* closed */ } try { await port?.close(); } catch { /* closed */ } writerRef.current = null; readerRef.current = null; setConnected(false); setMotionEnabled(false); setBusy(false); setHeld("NONE"); setMessage("Disconnected."); };
  useEffect(() => { if (!connected) return; const timer = window.setInterval(() => void sendRef.current(busy ? "PING" : "STATUS"), 500); return () => window.clearInterval(timer); }, [busy, connected]);
  useEffect(() => () => { if (programDwellTimerRef.current !== null) window.clearTimeout(programDwellTimerRef.current); }, []);

  const enableMotion = async () => { if (motionEnabled) { await send("STOP"); return; } if (!window.confirm("Clear the robot workspace and keep one hand near the physical E-stop. Enable calibrated motion?")) return; setBusy(true); await send(`SERVO_CONFIG J1 ${tokenRef.current} ACTIVE_LOW INTERFACE_VERIFIED`); };
  const stopAll = async () => { clearKeepalive(); clearProgramRun(); await send("STOP"); };
  const changeHomeMode = (mode: HomeMode) => { if (mode === homeMode) return; if (mode === "auto" && !window.confirm("Use J1 automatic sensor homing only after the J1 sensor is repaired and its response is verified in Setup.")) return; setHomeMode(mode); window.localStorage.setItem("parol6-j1-home-mode", mode); setHomed((current) => ({ ...current, J1: false })); setMessage(`J1 home mode changed to ${mode}. Home J1 again before motion.`); };
  const homeJoint = async (id: string) => { if (held !== "NONE") { setMessage("Release pose/torque hold before homing."); return; } if (id === "J1" && homeMode === "manual" && !window.confirm("Power must be off while positioning J1 by hand. After placing J1 at your chosen 0°, restore power and press OK to set that position as temporary home.")) return; setBusy(true); const command = id === "J1" && homeMode === "manual" ? `MANUAL_HOME J1 ${tokenRef.current} SET_CURRENT_POSITION_ZERO_TEMPORARY` : `HOME ${id} ${tokenRef.current} START`; await send(command); };
  const homeAll = async () => { if (held !== "NONE") { setMessage("Release motor hold first."); return; } if (homeMode === "manual" && !window.confirm("Place J1 at its chosen 0° with power off, restore power, then press OK. J1 will be zeroed before J2–J6 home automatically.")) return; homeQueueRef.current = [...homeOrder]; setBusy(true); startNextHome(tokenRef.current); };
  const jog = async (id: string, direction: "+" | "-") => { await send(`JOG ${id} ${tokenRef.current} ${direction} ${Math.round(jogStep * 1000)} GENTLE`); };
  const startHold = async (id: string, direction: "+" | "-") => { if (busy) return; await send(`HOLD ${id} ${tokenRef.current} ${direction} ${Math.round(jogSpeed * 1000)} GENTLE`); };
  const endHold = async () => { if (keepaliveRef.current === null) return; clearKeepalive(); await send(`HOLD_RELEASE ${tokenRef.current} HOLD_POSITION_VERIFIED`); };
  const releaseHold = async () => { if (held === "ALL") await send(`COORD_HOLD_RELEASE ${tokenRef.current} RELEASE_COORDINATED_HOLD_VERIFIED`); else if (/^J[3-6]$/.test(held)) await send(`MOTOR_HOLD ${held} ${tokenRef.current} OFF HOLD_RELEASE_VERIFIED`); };
  const captureCurrent = () => { setTargets(Object.fromEntries(joints.map(({ id }) => [id, (positions[id] ?? 0).toFixed(3)]))); setPreview(null); };
  const dryRun = () => { try { const values = joints.map(({ id, min, max }) => { const value = Number(targets[id]); if (!Number.isFinite(value) || value < min || value > max) throw new Error(`${id} must stay between ${min}° and ${max}°`); return value; }); let seconds = 0.5; joints.forEach(({ id, vmax, accel }, index) => { const distance = Math.abs(values[index] - (positions[id] ?? 0)); const scale = speedPercent / 10; seconds = Math.max(seconds, distance / (0.75 * vmax * scale), Math.sqrt(distance / (0.1875 * accel * scale))); }); const durationMs = Math.ceil((seconds * 1.08 * 1000) / 100) * 100; if (durationMs > 60000) throw new Error("This move exceeds the 60 second firmware envelope; use a nearer intermediate pose."); setPreview({ durationMs, targets: values }); setMessage(`Dry run passed: ${(durationMs / 1000).toFixed(1)} s synchronized move at ${speedPercent}% commissioning speed.`); } catch (error) { setPreview(null); setMessage(error instanceof Error ? error.message : "Invalid pose"); } };
  const movePose = async () => { if (!preview || held !== "NONE") return; if (!window.confirm(`Move all joints to this pose in ${(preview.durationMs / 1000).toFixed(1)} seconds? Keep the physical E-stop ready.`)) return; const values = preview.targets.map((value) => Math.round(value * 1000)).join(" "); await send(`COORD_MOVE ${tokenRef.current} ${preview.durationMs} ${values} COORDINATED_MOVE_VERIFIED`); setPreview(null); };
  const runProgram = (program: RobotProgram) => { try { if (!motionEnabled || !allHomed || busy || held !== "NONE") throw new Error("Home all joints and release motor hold before running a program."); const current = joints.map(({ id }) => positions[id] ?? 0); const moves = compileProgram(program, current, joints); const totalMs = moves.reduce((sum, move) => sum + move.durationMs + move.dwellMs, 0); if (!window.confirm(`Run “${program.name}” with ${moves.length} move${moves.length === 1 ? "" : "s"}? Estimated time ${(totalMs / 1000).toFixed(1)} seconds. Keep the physical E-stop ready.`)) return; clearProgramRun(); programQueueRef.current = moves; programRunningRef.current = true; setProgramRunning(true); setMessage("Program accepted. Starting first bounded move…"); startNextProgramMove(); } catch (error) { setMessage(error instanceof Error ? error.message : "Program validation failed."); } };

  return <main className="operator-app">
    <header className="operator-header"><div><span>PAROL6 COMMANDER · MOTION RC 0.9.1</span><h1>Robot Commander</h1><p>Manual control · synchronized poses · offline programs · direct USB</p></div><div className="operator-header-actions"><button onClick={onShowSetup}>Setup</button><button onClick={onShowWiring}>Wiring</button><button className="operator-stop" disabled={!connected} onClick={() => void stopAll()}>MOTOR STOP</button><button className="operator-connect" onClick={connected ? () => void disconnect() : () => void connect()}>{connected ? "Disconnect" : "Connect USB"}</button></div></header>
    <section className={`operator-status ${motionEnabled ? "ready" : ""}`}><b>{!connected ? "DISCONNECTED" : firmwareReady ? motionEnabled ? "MOTION READY" : "CONNECTED · LOCKED" : "FIRMWARE REQUIRED"}</b><span>{message}</span><code>{firmware}</code></section>
    {!serialSupported && <div className="operator-alert">USB control requires current Chrome or Edge.</div>}
    <section className="operator-top-grid">
      <article className="home-mode-card"><div><span>J1 HOME SOURCE</span><h2>{homeMode === "manual" ? "Manual zero" : "Automatic sensor"}</h2><p>{homeMode === "manual" ? "Default until the J1 sensor is repaired. This zero must be set again after every controller restart." : "Uses the same two-pass sensor homing pattern as the other sensor-homed joints."}</p></div><div className="home-switch" role="group" aria-label="J1 home mode"><button className={homeMode === "manual" ? "selected" : ""} onClick={() => changeHomeMode("manual")}>Manual</button><button className={homeMode === "auto" ? "selected" : ""} onClick={() => changeHomeMode("auto")}>Auto sensor</button></div></article>
      <article className="motion-master"><div><span>MASTER MOTION</span><h2>{motionEnabled ? "Enabled" : "Locked"}</h2><p>Firmware still enforces every calibrated joint limit and the 10% coordinated ceiling.</p></div><button className={motionEnabled ? "lock" : "enable"} disabled={!firmwareReady || busy} onClick={() => void enableMotion()}>{motionEnabled ? "Lock motors" : "Enable motion"}</button></article>
    </section>
    <section className="operator-home-bar"><div><span>HOME STATUS</span><b>{joints.filter(({ id }) => homed[id]).length} / 6 referenced</b></div><button disabled={!motionEnabled || busy || held !== "NONE"} onClick={() => void homeAll()}>Home all · {homeMode === "manual" ? "J1 manual" : "J1 auto"}</button>{held !== "NONE" && <button className="release-all" onClick={() => void releaseHold()}>Release motor hold</button>}</section>
    <section className="operator-joints">{joints.map(({ id, name, min, max }) => <article key={id} className={homed[id] ? "homed" : ""}><div className="operator-joint-title"><strong>{id}</strong><div><b>{name}</b><small>{min}° to {max}°</small></div><output>{(positions[id] ?? 0).toFixed(3)}°</output></div><button className="joint-home" disabled={!motionEnabled || busy || held !== "NONE"} onClick={() => void homeJoint(id)}>{homed[id] ? "Home again" : "Home"}</button><div className="operator-jog"><button disabled={!motionEnabled || !homed[id] || busy || held !== "NONE"} onClick={() => void jog(id, "-")}>− {jogStep}°</button><button disabled={!motionEnabled || !homed[id] || busy || held !== "NONE"} onPointerDown={(event) => { if (event.button === 0) { event.currentTarget.setPointerCapture(event.pointerId); void startHold(id, "-"); } }} onPointerUp={() => void endHold()} onPointerCancel={() => void endHold()}>Hold −</button><button disabled={!motionEnabled || !homed[id] || busy || held !== "NONE"} onPointerDown={(event) => { if (event.button === 0) { event.currentTarget.setPointerCapture(event.pointerId); void startHold(id, "+"); } }} onPointerUp={() => void endHold()} onPointerCancel={() => void endHold()}>Hold +</button><button disabled={!motionEnabled || !homed[id] || busy || held !== "NONE"} onClick={() => void jog(id, "+")}>+ {jogStep}°</button></div></article>)}</section>
    <section className="operator-settings"><label>Tap distance<select value={jogStep} onChange={(event) => setJogStep(Number(event.target.value))}><option value="0.5">0.5°</option><option value="1">1°</option><option value="5">5°</option><option value="10">10°</option></select></label><label>Hold speed <b>{jogSpeed}°/s</b><input type="range" min="3" max="45" step="1" value={jogSpeed} onChange={(event) => setJogSpeed(Number(event.target.value))} /></label></section>
    <section className="pose-card"><div className="pose-heading"><div><span>SYNCHRONIZED POSE</span><h2>Move all six joints together</h2><p>Dry run checks every target and calculates one shared, acceleration-limited duration.</p></div><button onClick={captureCurrent}>Use current pose</button></div><div className="pose-inputs">{joints.map(({ id, min, max }) => <label key={id}>{id}<input type="number" min={min} max={max} step="0.001" value={targets[id]} onChange={(event) => { setTargets((current) => ({ ...current, [id]: event.target.value })); setPreview(null); }} /><small>{min}° … {max}°</small></label>)}</div><div className="pose-actions"><label>Speed <b>{speedPercent}%</b><input type="range" min="1" max="10" value={speedPercent} onChange={(event) => { setSpeedPercent(Number(event.target.value)); setPreview(null); }} /></label><button disabled={!motionEnabled || !allHomed || busy || held !== "NONE"} onClick={dryRun}>Dry run</button><button className="move-pose" disabled={!preview || busy || held !== "NONE"} onClick={() => void movePose()}>{preview ? `Move · ${(preview.durationMs / 1000).toFixed(1)} s` : "Move pose"}</button></div></section>
    <ProgramPanel positions={joints.map(({ id }) => positions[id] ?? 0)} canRun={motionEnabled && allHomed && !busy && held === "NONE"} running={programRunning} activeStep={programStep} onRun={runProgram} onStop={() => void stopAll()} onMessage={setMessage} />
    <details className="operator-log"><summary>Controller log</summary><pre>{logs.length ? logs.join("\n") : "Waiting for USB…"}</pre></details>
  </main>;
}
