# Contributing

Thanks for taking an interest in this build.

Before opening a change, read [`PAROL6_PROJECT_KNOWLEDGE.md`](PAROL6_PROJECT_KNOWLEDGE.md) and the full [`SAFETY_WARNING_AND_DISCLAIMER.md`](SAFETY_WARNING_AND_DISCLAIMER.md). This fork intentionally differs from upstream hardware, so please identify whether a value comes from this build, current upstream source, or a legacy document.

For software changes:

1. Keep real outputs disabled unless the documented hardware gates explicitly allow them.
2. Add or update tests for protocol, safety, and motion behavior.
3. Run `SOFTWARE-FIRMWARE\scripts\test-all.ps1`.
4. Keep generated files, build output, dependencies, and local evidence out of commits.

Bug reports are most useful when they include the exact board revision, firmware version, command used, expected result, and sanitized logs. Never post credentials, serial numbers you consider private, or instructions that bypass a safety interlock.
