from pathlib import Path
import struct

from waldo_commander.services.urdf_scene.component_meshes import (
    split_stl_components,
)


def _write_stl(path: Path, triangles: list[tuple[tuple[float, float, float], ...]]) -> None:
    with path.open("wb") as stream:
        stream.write(b"test".ljust(80, b" "))
        stream.write(struct.pack("<I", len(triangles)))
        for triangle in triangles:
            values = (0.0, 0.0, 1.0, *(value for vertex in triangle for value in vertex))
            stream.write(struct.pack("<12fH", *values, 0))


def test_split_stl_components_groups_shared_vertices(tmp_path: Path) -> None:
    source = tmp_path / "assembly.stl"
    _write_stl(
        source,
        [
            ((0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)),
            ((1.0, 0.0, 0.0), (1.0, 1.0, 0.0), (0.0, 1.0, 0.0)),
            ((10.0, 0.0, 0.0), (11.0, 0.0, 0.0), (10.0, 1.0, 0.0)),
        ],
    )

    components = split_stl_components(source, tmp_path / "cache")

    assert [component.triangle_count for component in components] == [2, 1]
    assert all(component.path.exists() for component in components)
    assert split_stl_components(source, tmp_path / "cache") == components
