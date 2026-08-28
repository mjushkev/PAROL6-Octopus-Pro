"""Apply the PAROL6 TransformControls event fix to the pinned NiceGUI fork.

The NiceGUI commit pinned by Waldo Commander attaches TransformControls to
``record.mesh`` but later emits transform events through an undefined
``object`` variable.  The gizmo renders and can highlight, yet dragging never
reaches Commander's start/move/end callbacks.  Keep this small, fail-closed
runtime patch until the pinned dependency includes the upstream correction.
"""

from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path

PATCH_MARKER = "PAROL6_FIX_TRANSFORM_RECORD_MESH"
BROKEN_ANCHOR = """      const record = await get_object(this.objects, object_id);
      if (!record) return false;
      const existing = this.transform_controls.get(object_id);"""
FIXED_ANCHOR = f"""      const record = await get_object(this.objects, object_id);
      if (!record) return false;
      const object = record.mesh; // {PATCH_MARKER}
      const existing = this.transform_controls.get(object_id);"""


def patch_source(source: str) -> tuple[str, bool]:
    """Return patched JavaScript and whether a modification was required."""
    if PATCH_MARKER in source:
        return source, False
    if BROKEN_ANCHOR not in source:
        raise RuntimeError(
            "Pinned NiceGUI scene.js no longer matches the reviewed source; "
            "refusing an unverified runtime patch."
        )
    if "object.getWorldPosition" not in source or "object_name: object.name" not in source:
        raise RuntimeError("NiceGUI transform emitter shape is not the reviewed version.")
    return source.replace(BROKEN_ANCHOR, FIXED_ANCHOR, 1), True


def locate_scene_js() -> Path:
    """Resolve the active NiceGUI scene component without importing NiceGUI."""
    spec = importlib.util.find_spec("nicegui")
    if spec is None or spec.origin is None:
        raise RuntimeError("NiceGUI is not installed in the Commander runtime.")
    return Path(spec.origin).resolve().parent / "elements" / "scene" / "scene.js"


def apply_runtime_patch(path: Path | None = None) -> tuple[Path, bool]:
    """Patch the selected runtime file in place and verify the result."""
    target = (path or locate_scene_js()).resolve()
    source = target.read_text(encoding="utf-8")
    patched, changed = patch_source(source)
    if changed:
        target.write_text(patched, encoding="utf-8", newline="\n")
    verified = target.read_text(encoding="utf-8")
    if PATCH_MARKER not in verified:
        raise RuntimeError(f"NiceGUI TransformControls patch did not verify: {target}")
    return target, changed


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="verify without modifying")
    args = parser.parse_args()
    target = locate_scene_js()
    if args.check:
        if PATCH_MARKER not in target.read_text(encoding="utf-8"):
            raise RuntimeError(f"NiceGUI TransformControls patch is missing: {target}")
        print(f"Verified NiceGUI TransformControls patch: {target}")
        return 0
    patched_path, changed = apply_runtime_patch(target)
    verb = "Applied" if changed else "Verified"
    print(f"{verb} NiceGUI TransformControls patch: {patched_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
