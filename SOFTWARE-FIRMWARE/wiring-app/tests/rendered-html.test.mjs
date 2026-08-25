import assert from "node:assert/strict";
import test from "node:test";

async function render() {
  const workerUrl = new URL("../dist/server/index.js", import.meta.url);
  workerUrl.searchParams.set("test", `${process.pid}-${Date.now()}`);
  const { default: worker } = await import(workerUrl.href);
  return worker.fetch(new Request("http://localhost/", { headers: { accept: "text/html" } }), { ASSETS: { fetch: async () => new Response("Not found", { status: 404 }) } }, { waitUntil() {}, passThroughOnException() {} });
}

test("renders the PAROL6 joint homing and limit test workbench", async () => {
  const response = await render();
  assert.equal(response.status, 200);
  const html = await response.text();
  assert.match(html, /Home and test every joint/i);
  assert.match(html, /PAROL6 JOINT TEST · 0\.8\.11/i);
  assert.match(html, /Home → test 10° short of maximum → test maximum/i);
  assert.match(html, /Connect USB/i);
  assert.match(html, /Enable setup motion/i);
  assert.match(html, /Test raw −/i);
  assert.match(html, /Test raw \+/i);
  assert.match(html, /moves exactly 2° at half GENTLE speed/i);
  assert.match(html, /Which raw direction should mean joint \+/i);
  assert.match(html, /Which raw direction moves toward home/i);
  assert.match(html, /Sensor value when triggered/i);
  assert.match(html, /Save joint setup/i);
  assert.match(html, /Home J1/i);
  assert.match(html, /Set current J1 position as 0°/i);
  assert.match(html, /J1 TEMPORARY FALLBACK/i);
  assert.match(html, /Set min here/i);
  assert.match(html, /Set max here/i);
  assert.match(html, /Export JSON/i);
  assert.match(html, /Max −10°/i);
  assert.match(html, /Max limit/i);
  assert.match(html, /MOTOR STOP/i);
  assert.match(html, /Home J2–J6 sequence/i);
  assert.match(html, /Controller log/i);
  assert.doesNotMatch(html, /MOTION INTERLOCKS|Arm supported against gravity|Arm hold-to-jog/i);
  assert.doesNotMatch(html, /codex-preview|react-loading-skeleton|Your site is taking shape/i);
});
