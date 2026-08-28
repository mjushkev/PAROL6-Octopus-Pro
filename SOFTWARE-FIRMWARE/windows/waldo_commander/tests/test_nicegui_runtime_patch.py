from __future__ import annotations

import importlib.util
from pathlib import Path


PATCH_PATH = (
    Path(__file__).resolve().parents[2]
    / "runtime_patches"
    / "patch_nicegui_scene.py"
)
SPEC = importlib.util.spec_from_file_location("patch_nicegui_scene", PATCH_PATH)
assert SPEC is not None and SPEC.loader is not None
PATCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PATCH)


def _broken_scene_source() -> str:
    return f"""async enable_transform_controls(object_id) {{
{PATCH.BROKEN_ANCHOR}
      object.getWorldPosition(this._transformWP);
      const payload = {{object_name: object.name}};
    }}"""


def test_transform_emitter_patch_defines_the_attached_mesh() -> None:
    patched, changed = PATCH.patch_source(_broken_scene_source())

    assert changed is True
    assert "const object = record.mesh" in patched
    assert PATCH.PATCH_MARKER in patched


def test_transform_emitter_patch_is_idempotent() -> None:
    patched, _ = PATCH.patch_source(_broken_scene_source())
    second, changed = PATCH.patch_source(patched)

    assert changed is False
    assert second == patched
