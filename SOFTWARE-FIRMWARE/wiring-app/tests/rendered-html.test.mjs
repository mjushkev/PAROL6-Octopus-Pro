import assert from "node:assert/strict";
import test from "node:test";

async function render() {
  const workerUrl = new URL("../dist/server/index.js", import.meta.url);
  workerUrl.searchParams.set("test", `${process.pid}-${Date.now()}`);
  const { default: worker } = await import(workerUrl.href);
  return worker.fetch(new Request("http://localhost/", { headers: { accept: "text/html" } }), { ASSETS: { fetch: async () => new Response("Not found", { status: 404 }) } }, { waitUntil() {}, passThroughOnException() {} });
}

test("renders the simplified PAROL6 joint calibration workbench", async () => {
  const response = await render();
  assert.equal(response.status, 200);
  const html = await response.text();
  assert.match(html, /Set up one joint at a time/i);
  assert.match(html, /Test direction → save → home → set the far limit/i);
  assert.match(html, /Connect USB/i);
  assert.match(html, /Enable setup motion/i);
  assert.match(html, /Test raw −/i);
  assert.match(html, /Test raw \+/i);
  assert.match(html, /moves exactly 2° at half GENTLE speed/i);
  assert.match(html, /Which raw direction should mean joint \+/i);
  assert.match(html, /Which raw direction moves toward home/i);
  assert.match(html, /Sensor value when triggered/i);
  assert.match(html, /Save joint setup/i);
  assert.match(html, /Set temporary J1 zero/i);
  assert.match(html, /Set current J1 position as 0°/i);
  assert.match(html, /J1 SENSOR BYPASS/i);
  assert.match(html, /Set min here/i);
  assert.match(html, /Set max here/i);
  assert.match(html, /Export JSON/i);
  assert.match(html, /Home J2–J6/i);
  assert.match(html, /Controller log/i);
  assert.doesNotMatch(html, /MOTION INTERLOCKS|Arm supported against gravity|Arm hold-to-jog/i);
  assert.doesNotMatch(html, /codex-preview|react-loading-skeleton|Your site is taking shape/i);
});
