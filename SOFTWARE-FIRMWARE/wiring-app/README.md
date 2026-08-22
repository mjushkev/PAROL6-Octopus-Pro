# PAROL6 Octopus Wiring Map

An interactive click-to-wire board map for Matt J's custom PAROL6 robot.

The app places clickable connection boxes over BIGTREETECH's official Octopus
Pro V1.1 pin image. Selecting a box shows the assigned robot item, exact target
pins, wiring sequence, and any verification or safety hold point. It does not
authorize first power.

## Development

Requires Node.js 22.13 or newer.

```bash
npm install
npm run dev
```

Use `npm run build` and `npm test` before publishing. On Windows, set
`WRANGLER_LOG_PATH=.wrangler/wrangler.log` and invoke `vinext.cmd` directly if
the shell does not accept inline environment assignments.

## Accuracy boundary

The target allocation follows the owner-selected implementation plan and the
official Octopus Pro V1.1 H723 source. Actual component labels, meter readings,
reviewed schematics, and the project safety disclaimer remain authoritative.
