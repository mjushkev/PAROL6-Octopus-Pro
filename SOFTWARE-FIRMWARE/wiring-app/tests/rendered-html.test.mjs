import assert from "node:assert/strict";
import test from "node:test";

async function render() {
  const workerUrl = new URL("../dist/server/index.js", import.meta.url);
  workerUrl.searchParams.set("test", `${process.pid}-${Date.now()}`);
  const { default: worker } = await import(workerUrl.href);
  return worker.fetch(new Request("http://localhost/", { headers: { accept: "text/html" } }), { ASSETS: { fetch: async () => new Response("Not found", { status: 404 }) } }, { waitUntil() {}, passThroughOnException() {} });
}

test("renders the calibrated PAROL6 operator console", async () => {
  const response = await render();
  assert.equal(response.status, 200);
  const html = await response.text();
  assert.match(html, /Direct joint control/i);
  assert.match(html, /PAROL6 OPERATOR · MOTION RC 0\.9\.1/i);
  assert.match(html, /Calibrated limits · open-loop step tracking · direct USB/i);
  assert.match(html, /Connect USB/i);
  assert.match(html, /Enable motion/i);
  assert.match(html, /J1 HOME SOURCE/i);
  assert.match(html, /Manual zero/i);
  assert.match(html, /Auto sensor/i);
  assert.match(html, /Home all/i);
  assert.match(html, /Hold −/i);
  assert.match(html, /Hold \+/i);
  assert.match(html, /SYNCHRONIZED POSE/i);
  assert.match(html, /Dry run/i);
  assert.match(html, /10% coordinated ceiling/i);
  assert.match(html, /-230/i);
  assert.match(html, /232\.694/i);
  assert.match(html, /MOTOR STOP/i);
  assert.match(html, /Controller log/i);
  assert.doesNotMatch(html, /MOTION INTERLOCKS|Arm supported against gravity|Arm hold-to-jog/i);
  assert.doesNotMatch(html, /codex-preview|react-loading-skeleton|Your site is taking shape/i);
});
