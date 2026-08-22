# PAROL6 project context

Before answering or changing anything related to PAROL6, read
`PAROL6_PROJECT_KNOWLEDGE.md`. It records the exact upstream snapshot, source
hierarchy, mechanical/electrical/firmware specifics, and known conflicts found
during the repository-wide review.

Treat the **Owner-selected build configuration** in that file as authoritative
for this particular robot. Upstream hardware values remain reference data only
where they conflict with the owner's selected controller, motors, drivers,
computer, printer, or material.

The complete official upstream repository is checked out in this directory at
commit `77597de127a844990965189f0e6062e2551a2842`. Use the checked-in source file
itself when an exact value, line, drawing, or mesh is needed. Do not silently
resolve disagreements among the current BOM, the 2023 assembly manual, legacy
files, firmware, and URDF; identify the source and version being used.

This is experimental robotic hardware with electrical, motion, pinch, heat,
and stored-energy hazards. Preserve the safety constraints in
`SAFETY_WARNING_AND_DISCLAIMER.md` and never infer that an open-source file is a
certified or production-safe design.
