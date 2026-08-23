"use client";

import { useState } from "react";
import CommissioningPanel from "./commissioning-panel";

type PortStatus = "mapped" | "verify" | "blocked" | "unused" | "service";

type Port = {
  id: string;
  label: string;
  item: string;
  status: PortStatus;
  x: number;
  y: number;
  w: number;
  h: number;
  pins: string;
  wiring: string[];
  note: string;
};

const fanPins = ["PA8", "PE5", "PD12", "PD13", "PD14", "PD15", "Always on", "Always on"];

const stopAssignments = [
  { label: "STOP0", item: "J1 inductive home", pin: "PG6", wire: "Optocoupler Q1 output → PG6; Q1 logic ground → GND", status: "blocked" as const, note: "The M5 24 V NPN-NO sensor must feed Q1. Never put the sensor's 24 V signal on this header." },
  { label: "STOP1", item: "J2 mechanical home", pin: "PG9", wire: "ZW12-3 COM → GND; NC → PG9", status: "mapped" as const, note: "Meter COM/NC before crimping and leave the header's 5 V pin unused." },
  { label: "STOP2", item: "J3 mechanical home", pin: "PG10", wire: "ZW12-3 COM → GND; NC → PG10", status: "mapped" as const, note: "Meter COM/NC before crimping and leave the header's 5 V pin unused." },
  { label: "STOP3", item: "J4 inductive home", pin: "PG11", wire: "Optocoupler Q2 output → PG11; Q2 logic ground → GND", status: "blocked" as const, note: "The 4 mm 24 V NPN-NO sensor must feed Q2. Never put the sensor's 24 V signal on this header." },
  { label: "STOP4", item: "J5 mechanical home", pin: "PG12", wire: "ZW12-3 COM → GND; NC → PG12", status: "mapped" as const, note: "Meter COM/NC before crimping and leave the header's 5 V pin unused." },
  { label: "STOP5", item: "J6 inductive home", pin: "PG13", wire: "Optocoupler Q3 output → PG13; Q3 logic ground → GND", status: "blocked" as const, note: "The GX-F8A 24 V NPN-NO sensor must feed Q3. Never put the sensor's 24 V signal on this header." },
  { label: "STOP6", item: "Unused input", pin: "PG14", wire: "Leave the connector empty", status: "unused" as const, note: "Reserved for future reviewed expansion." },
  { label: "STOP7", item: "Contactor auxiliary feedback", pin: "PG15", wire: "Optocoupler Q4 output → PG15; Q4 logic ground → GND", status: "blocked" as const, note: "Use only a mechanically linked contactor auxiliary contact through the reviewed isolated Q4 circuit." },
];

const ports: Port[] = [
  {
    id: "motor-power", label: "MOTOR POWER", item: "Unused in this build", status: "unused",
    x: 8.4, y: 18.2, w: 7.8, h: 14.6, pins: "MOTOR-POWER + / −",
    wiring: ["Leave both terminals empty", "Do not bridge this input to POWER", "Driver source jumpers for MOTOR2–MOTOR5 select POWER"],
    note: "The owner-selected as-built configuration uses only the POWER input. MOTOR-POWER remains empty.",
  },
  {
    id: "main-power", label: "POWER", item: "Main switched 24 V input", status: "mapped",
    x: 8.4, y: 32.8, w: 7.8, h: 13.2, pins: "POWER VIN / GND",
    wiring: ["E-stop-switched, protected +24 V → VIN (+)", "PSU 0 V → GND (−)", "MOTOR2–MOTOR5 source jumpers → POWER"],
    note: "The owner reports the E-stop now removes main 24 V. With VUSB removed, USB remains data-only while external power is present.",
  },
  {
    id: "bed-power", label: "BED POWER", item: "Not used for PAROL6", status: "unused",
    x: 8.4, y: 46.2, w: 7.8, h: 11.2, pins: "BED-POWER VB / GND",
    wiring: ["Leave both terminals empty", "Do not bridge this input to POWER or MOTOR-POWER"],
    note: "This is a 3D-printer bed supply input and has no assigned robot function.",
  },
  {
    id: "bed-out", label: "BED OUT", item: "Not used for PAROL6", status: "unused",
    x: 8.4, y: 57.5, w: 7.8, h: 10.4, pins: "BED-OUT + / −",
    wiring: ["Leave both terminals empty"],
    note: "No robot load is assigned to the heated-bed output.",
  },
  {
    id: "m0", label: "M0", item: "Joint 1 Servo42C control", status: "mapped",
    x: 17.2, y: 12.0, w: 7.1, h: 22.5, pins: "PF13 STEP · PF12 DIR · PF14 EN",
    wiring: ["Install the supplied Pololu-format Servo42C adapter in MOTOR0", "Adapter harness → J1 Servo42C", "Set Servo42C locally to CR_OPEN", "Use 3.3 V push-pull STEP/DIR/EN with En=L and 32 microsteps"],
    note: "J1 runs open-loop by owner selection; encoder telemetry is disabled. The MOTOR0 A1/A2/B1/B2 output connector remains unused.",
  },
  {
    id: "m1", label: "M1", item: "Joint 2 Servo42C control", status: "mapped",
    x: 24.8, y: 12.0, w: 7.1, h: 22.5, pins: "PG0 STEP · PG1 DIR · PF15 EN",
    wiring: ["Install the supplied Pololu-format Servo42C adapter in MOTOR1", "Adapter harness → J2 Servo42C", "Set Servo42C locally to CR_OPEN", "Use 3.3 V push-pull STEP/DIR/EN with En=L and 32 microsteps"],
    note: "J2 runs open-loop by owner selection; encoder telemetry is disabled. The MOTOR1 A1/A2/B1/B2 output connector remains unused.",
  },
  {
    id: "m2", label: "M2", item: "Joint 3 motor", status: "mapped",
    x: 32.4, y: 12.0, w: 7.1, h: 22.5, pins: "PF11 STEP · PG3 DIR · PG5 EN · PC6 UART",
    wiring: ["Install one verified TMC2209 V1.3 in MOTOR2", "Measured motor coil pair A → A1/A2", "Measured motor coil pair B → B1/B2", "Select POWER; remove the DIAG jumper"],
    note: "Never infer a coil pair from wire colour. Reverse one measured pair only if direction must be corrected.",
  },
  {
    id: "m3", label: "M3", item: "Joint 4 motor", status: "mapped",
    x: 40.0, y: 12.0, w: 7.1, h: 22.5, pins: "PG4 STEP · PC1 DIR · PA2 EN · PC7 UART",
    wiring: ["Install one verified TMC2209 V1.3 in MOTOR3", "Measured motor coil pair A → A1/A2", "Measured motor coil pair B → B1/B2", "Select POWER; remove the DIAG jumper"],
    note: "PA2 is the V1.1 MOTOR3 enable pin. Do not reuse a V1.0 mapping.",
  },
  {
    id: "m4", label: "M4", item: "Joint 5 motor", status: "verify",
    x: 47.6, y: 12.0, w: 7.1, h: 22.5, pins: "PF9 STEP · PF10 DIR · PG2 EN · PF2 UART",
    wiring: ["Install one verified TMC2209 V1.3 in MOTOR4", "Measured motor coil pair A → A1/A2", "Measured motor coil pair B → B1/B2", "Select POWER; remove the DIAG jumper"],
    note: "The installed J5 motor label must resolve the model/current conflict before driver current is configured.",
  },
  {
    id: "m5", label: "M5", item: "Joint 6 motor", status: "verify",
    x: 55.2, y: 12.0, w: 7.1, h: 22.5, pins: "PC13 STEP · PF0 DIR · PF1 EN · PE4 UART",
    wiring: ["Install one verified TMC2209 V1.3 in MOTOR5", "Measured motor coil pair A → A1/A2", "Measured motor coil pair B → B1/B2", "Select POWER; remove the DIAG jumper"],
    note: "Confirm the installed J6 motor label. J6 is cable-limited and must never be treated as continuous rotation.",
  },
  {
    id: "m6", label: "M6", item: "Unused driver and motor port", status: "unused",
    x: 62.8, y: 12.0, w: 7.1, h: 22.5, pins: "MOTOR6",
    wiring: ["Leave driver slot empty", "Leave motor output empty"],
    note: "Reserved for future reviewed expansion.",
  },
  {
    id: "m7", label: "M7", item: "Unused driver and motor port", status: "unused",
    x: 70.4, y: 12.0, w: 7.1, h: 22.5, pins: "MOTOR7",
    wiring: ["Leave driver slot empty", "Leave motor output empty"],
    note: "Reserved for future reviewed expansion.",
  },
  {
    id: "he0", label: "HE0", item: "Unused high-current output", status: "unused",
    x: 17.2, y: 64.3, w: 5.2, h: 8.4, pins: "HE0 · PA0",
    wiring: ["Leave output empty"], note: "No robot function is assigned.",
  },
  {
    id: "he1", label: "HE1", item: "Unused high-current output", status: "unused",
    x: 22.5, y: 64.3, w: 5.2, h: 8.4, pins: "HE1 · PA3",
    wiring: ["Leave output empty"], note: "No robot function is assigned.",
  },
  {
    id: "he2", label: "HE2", item: "Unused high-current output", status: "unused",
    x: 27.8, y: 64.3, w: 5.2, h: 8.4, pins: "HE2 · PB0",
    wiring: ["Leave output empty"], note: "No robot function is assigned.",
  },
  {
    id: "he3", label: "HE3", item: "Software contactor inhibit", status: "blocked",
    x: 33.1, y: 64.3, w: 5.2, h: 8.4, pins: "HE3 · PB11",
    wiring: ["PB11 output → reviewed isolated/low-side interface", "Interface → contactor coil control circuit", "NC E-stop remains a direct hardware series path"],
    note: "PB11 may only inhibit an already safe circuit. Software must never be the sole E-stop path.",
  },
  ...fanPins.map((pin, index): Port => ({
    id: `fan${index}`, label: `FAN${index}`, item: "Cooling output—not assigned", status: "verify",
    x: 37.5 + index * 3.2, y: 69.4, w: 3.05, h: 10.4, pins: `FAN${index} · ${pin}`,
    wiring: ["Leave empty until a fan port is selected", "Read the installed fan nameplate", "Set this output's voltage jumper before attaching a fan"],
    note: "No fan port is locked yet. Confirm fan voltage, polarity and current before selecting one; FAN6 and FAN7 are always-on outputs.",
  })),
  {
    id: "tb", label: "TB", item: "Unused bed temperature input", status: "unused",
    x: 54.2, y: 0.2, w: 2.7, h: 9.7, pins: "TB · PF3", wiring: ["Leave empty"], note: "No robot sensor is assigned.",
  },
  {
    id: "t0", label: "T0", item: "Base temperature sensor 1", status: "verify",
    x: 57.0, y: 0.2, w: 2.7, h: 9.7, pins: "T0 · PF4 / GND", wiring: ["Qualified passive 100 kΩ NTC lead 1 → PF4 signal", "NTC lead 2 → GND"], note: "Record the probe's physical location and resistance before connection.",
  },
  {
    id: "t1", label: "T1", item: "Base temperature sensor 2", status: "verify",
    x: 59.8, y: 0.2, w: 2.7, h: 9.7, pins: "T1 · PF5 / GND", wiring: ["Qualified passive 100 kΩ NTC lead 1 → PF5 signal", "NTC lead 2 → GND"], note: "Record the probe's physical location and resistance before connection.",
  },
  {
    id: "t2", label: "T2", item: "Base temperature sensor 3", status: "verify",
    x: 62.6, y: 0.2, w: 2.7, h: 9.7, pins: "T2 · PF6 / GND", wiring: ["Qualified passive 100 kΩ NTC lead 1 → PF6 signal", "NTC lead 2 → GND"], note: "Record the probe's physical location and resistance before connection.",
  },
  {
    id: "t3", label: "T3", item: "Base temperature sensor 4", status: "verify",
    x: 65.4, y: 0.2, w: 2.7, h: 9.7, pins: "T3 · PF7 / GND", wiring: ["Qualified passive 100 kΩ NTC lead 1 → PF7 signal", "NTC lead 2 → GND"], note: "Record the probe's physical location and resistance before connection.",
  },
  {
    id: "uart2", label: "UART2", item: "ESP bridge deferred", status: "unused",
    x: 88.8, y: 9.6, w: 10.2, h: 19.0, pins: "PD5 TX · PD6 RX · GND",
    wiring: ["Leave PD5, PD6, 5 V and GND disconnected", "Do not power the ESP from its buck or from USB", "Use the Octopus USB-C port as the only PC data path"],
    note: "Wireless control is intentionally deferred. This UART remains empty until a later ESP phase is explicitly resumed.",
  },
  {
    id: "uart3", label: "USART3", item: "Servo42C telemetry disabled", status: "unused",
    x: 83.0, y: 30.0, w: 16.0, h: 10.5, pins: "PD8 TX · PD9 RX · GND",
    wiring: ["Leave PD8 TX disconnected", "Leave PD9 RX disconnected", "Leave the telemetry bus unpopulated"],
    note: "J1/J2 are configured for CR_OPEN. Encoder polling and encoder-dependent faulting are disabled, while the backend telemetry types remain dormant for a future qualified re-enable.",
  },
  {
    id: "power-det", label: "POWER DET", item: "Switched-bus voltage monitor", status: "blocked",
    x: 46.4, y: 81.2, w: 12.1, h: 7.3, pins: "PC0 / 3.3 V / GND",
    wiring: ["Switched 24 V bus → calculated protected divider/filter", "Conditioned divider output → PC0", "Divider return → logic GND"],
    note: "Never connect 24 V directly to PC0. The divider and fault clamp require calculation and bench proof.",
  },
  {
    id: "probe", label: "PROBE", item: "Future gripper PWM signal", status: "blocked",
    x: 45.5, y: 88.6, w: 20.4, h: 6.4, pins: "PB6 PWM / GND",
    wiring: ["PB6 → qualified gripper signal input", "Signal ground → gripper controller ground", "Servo power comes from a separate fused buck—not this header"],
    note: "The planned MG90S 360° variant is not a positional servo. The exact gripper behavior, voltage and stall current must be proven first.",
  },
  ...stopAssignments.map((assignment, index): Port => ({
    id: `stop${index}`, label: assignment.label, item: assignment.item, status: assignment.status,
    x: 64.5 + (index % 2) * 6.6, y: 75.0 + Math.floor(index / 2) * 5.6, w: 6.3, h: 5.3,
    pins: `${assignment.label} · ${assignment.pin} / 5 V / GND`,
    wiring: [assignment.wire], note: assignment.note,
  })),
  {
    id: "can", label: "CAN", item: "Unused CAN connector", status: "unused",
    x: 81.7, y: 71.8, w: 7.8, h: 14.0, pins: "CAN H / CAN L", wiring: ["Leave empty"], note: "The deferred gripper is not using the upstream PAROL6 CAN protocol.",
  },
  {
    id: "exp1", label: "EXP1", item: "Unused display expansion header", status: "unused",
    x: 86.1, y: 54.5, w: 6.5, h: 34.0, pins: "EXP1", wiring: ["Leave header empty"], note: "No robot function is assigned to this display header.",
  },
  {
    id: "exp2", label: "EXP2", item: "Unused display expansion header", status: "unused",
    x: 92.8, y: 54.5, w: 6.5, h: 34.0, pins: "EXP2", wiring: ["Leave header empty"], note: "No robot function is assigned to this display header.",
  },
  {
    id: "usb", label: "USB-C", item: "Primary PC control and service link", status: "mapped",
    x: 82.0, y: 24.7, w: 6.5, h: 9.5, pins: "USB-C · PA11 D− / PA12 D+", wiring: ["Leave the ESP completely disconnected", "Keep the VUSB jumper removed when external 24 V is connected", "Connect a known-good USB data cable directly from the PC → Octopus USB-C", "Use the joint setup console; every mutable command requires a fresh token"], note: "USB is the primary transport. Calibration RC 0.8.4 releases J1/J2 Servo42C torque after hold-to-jog and enforces fixed J1 limits of −230° to +35° from temporary manual zero. Support gravity-loaded J2. Remove USB before changing any jumper or connector.",
  },
  {
    id: "usb-a", label: "USB-A", item: "Unused USB host/OTG connector", status: "unused",
    x: 80.0, y: 42.0, w: 7.0, h: 11.8, pins: "USB-A · PB14 / PB15", wiring: ["Leave this connector empty"], note: "Connect the PC to the smaller USB-C device connector above, not this USB-A host/OTG port.",
  },
  {
    id: "i2c", label: "I²C", item: "Unused I²C header", status: "unused",
    x: 73.8, y: 0.2, w: 8.0, h: 11.7, pins: "PB8 SCL · PB9 SDA", wiring: ["Leave empty"], note: "No current robot function is assigned.",
  },
  {
    id: "spi", label: "SPI", item: "Unused SPI header", status: "unused",
    x: 37.5, y: 0.2, w: 16.1, h: 11.7, pins: "PA6 MISO · PA5 SCK · PA7 MOSI · CS", wiring: ["Leave header empty"], note: "No current robot function is assigned.",
  },
  {
    id: "max31865", label: "MAX31865", item: "Unused RTD interface", status: "unused",
    x: 25.4, y: 74.0, w: 12.0, h: 24.0, pins: "FORCE / RTDIN / SPI", wiring: ["Leave terminals and header empty"], note: "No PT100 or PT1000 sensor is assigned to the robot.",
  },
  {
    id: "rgb", label: "RGB", item: "Unused RGB header", status: "unused",
    x: 45.2, y: 94.7, w: 20.7, h: 5.0, pins: "PB10 / 5 V / GND", wiring: ["Leave header empty"], note: "No current robot function is assigned.",
  },
  {
    id: "boot-reset", label: "BOOT / RESET", item: "Board service controls", status: "service",
    x: 80.0, y: 1.1, w: 8.0, h: 20.2, pins: "BOOT0 / RESET", wiring: ["No permanent wiring", "Use only during documented firmware recovery"], note: "These are service controls, not robot I/O.",
  },
];

const statusLabel: Record<PortStatus, string> = {
  mapped: "Target mapped",
  verify: "Verify hardware",
  blocked: "Do not connect yet",
  unused: "Leave empty",
  service: "Service only",
};

export default function Home() {
  const [view, setView] = useState<"test" | "wiring">("test");
  const [selectedId, setSelectedId] = useState("usb");
  const selected = ports.find((port) => port.id === selectedId) ?? ports[0];

  if (view === "test") {
    return <CommissioningPanel onShowWiring={() => setView("wiring")} />;
  }

  return (
    <main className="app">
      <header className="topbar">
        <div className="brand"><span className="brand-dot" />PAROL6 <b>OCTOPUS WIRING MAP</b></div>
        <div className="board-id">OCTOPUS PRO V1.1 · STM32H723 · USB PRIMARY</div>
        <button className="map-test-button" onClick={() => setView("test")}>Test console</button>
      </header>

      <div className="notice">
        Direct USB commissioning selected. ESP deferred.
        <strong> E-stop removes main 24 V; MOTOR-POWER remains unused.</strong>
      </div>

      <div className="workspace">
        <section className="board-pane" aria-label="Interactive Octopus Pro V1.1 connector map">
          <div className="board-scroll">
            <div className="board-canvas">
              {/* Official BIGTREETECH V1.1 pin image; hotspot positions are percentages of this image. */}
              <img src="/octopus-pro-v1-1-pin.jpg" alt="Official BIGTREETECH Octopus Pro V1.1 top-down pin diagram" draggable={false} />
              {ports.map((port) => (
                <button
                  key={port.id}
                  className={`hotspot ${port.status} ${selected.id === port.id ? "active" : ""}`}
                  style={{ left: `${port.x}%`, top: `${port.y}%`, width: `${port.w}%`, height: `${port.h}%` }}
                  onClick={() => setSelectedId(port.id)}
                  aria-label={`${port.label}: ${port.item}`}
                  aria-pressed={selected.id === port.id}
                >
                  <span>{port.label}</span>
                </button>
              ))}
            </div>
          </div>
          <div className="legend" aria-label="Connection status legend">
            <span><i className="mapped" />Target mapped</span>
            <span><i className="verify" />Verify hardware</span>
            <span><i className="blocked" />Do not connect yet</span>
            <span><i className="unused" />Leave empty</span>
            <span><i className="service" />Service only</span>
          </div>
        </section>

        <aside className="detail" aria-live="polite">
          <div className="detail-kicker">SELECTED CONNECTION</div>
          <div className="detail-heading">
            <div><span>{selected.label}</span><h1>{selected.item}</h1></div>
            <span className={`status ${selected.status}`}>{statusLabel[selected.status]}</span>
          </div>

          <section className="pin-card">
            <span>BOARD PINS</span>
            <strong>{selected.pins}</strong>
          </section>

          <section className="wire-card">
            <h2>Wire it like this</h2>
            <ol>
              {selected.wiring.map((step) => <li key={step}>{step}</li>)}
            </ol>
          </section>

          <section className={`note-card ${selected.status}`}>
            <span>{selected.status === "blocked" ? "HOLD POINT" : selected.status === "verify" ? "CHECK FIRST" : "IMPORTANT"}</span>
            <p>{selected.note}</p>
          </section>

          <div className="always-check">
            <b>Before inserting a connector</b>
            <span>Meter polarity, continuity and shorts with every power source unplugged.</span>
          </div>

          <footer>
            <a href="https://github.com/bigtreetech/BIGTREETECH-OCTOPUS-Pro" target="_blank" rel="noreferrer">Official board source ↗</a>
            <span>Owner-selected PAROL6 allocation</span>
          </footer>
        </aside>
      </div>
    </main>
  );
}
