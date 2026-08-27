"""Split binary STL assemblies into independently renderable components.

PAROL6's URDF stores each moving link as one STL even though the mesh contains
many disconnected physical pieces (covers, housings, shafts, and hardware).
Splitting on shared triangle vertices lets the UI color those pieces without
changing the kinematic or collision models.
"""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import struct


_STL_HEADER_SIZE = 84
_TRIANGLE_SIZE = 50
_VERTEX_TOLERANCE = 1e-7


@dataclass(frozen=True)
class MeshComponent:
    """One connected triangle component written to a cached STL file."""

    index: int
    path: Path
    triangle_count: int


def _binary_triangle_records(data: bytes) -> tuple[bytes, list[bytes]] | None:
    """Return ``(header, records)`` for a valid binary STL, otherwise ``None``."""
    if len(data) < _STL_HEADER_SIZE:
        return None
    triangle_count = struct.unpack_from("<I", data, 80)[0]
    expected = _STL_HEADER_SIZE + triangle_count * _TRIANGLE_SIZE
    if triangle_count <= 0 or len(data) < expected:
        return None
    return data[:80], [
        data[_STL_HEADER_SIZE + i * _TRIANGLE_SIZE : _STL_HEADER_SIZE + (i + 1) * _TRIANGLE_SIZE]
        for i in range(triangle_count)
    ]


def _vertex_key(x: float, y: float, z: float) -> tuple[int, int, int]:
    return (
        round(x / _VERTEX_TOLERANCE),
        round(y / _VERTEX_TOLERANCE),
        round(z / _VERTEX_TOLERANCE),
    )


def _connected_triangle_groups(records: list[bytes]) -> list[list[int]]:
    """Group triangle indices that share at least one quantized vertex."""
    count = len(records)
    parent = list(range(count))
    sizes = [1] * count

    def find(index: int) -> int:
        while parent[index] != index:
            parent[index] = parent[parent[index]]
            index = parent[index]
        return index

    def union(left: int, right: int) -> None:
        left_root = find(left)
        right_root = find(right)
        if left_root == right_root:
            return
        if sizes[left_root] < sizes[right_root]:
            left_root, right_root = right_root, left_root
        parent[right_root] = left_root
        sizes[left_root] += sizes[right_root]

    vertex_owner: dict[tuple[int, int, int], int] = {}
    for triangle_index, record in enumerate(records):
        values = struct.unpack_from("<12f", record, 0)
        for offset in (3, 6, 9):
            key = _vertex_key(values[offset], values[offset + 1], values[offset + 2])
            owner = vertex_owner.setdefault(key, triangle_index)
            union(triangle_index, owner)

    groups: dict[int, list[int]] = {}
    for triangle_index in range(count):
        groups.setdefault(find(triangle_index), []).append(triangle_index)

    # Largest first gives stable, useful component numbering for the UI.
    return sorted(groups.values(), key=lambda group: (-len(group), group[0]))


def _write_binary_stl(path: Path, header: bytes, records: list[bytes]) -> None:
    component_header = (header[:60] + b" PAROL6 component")[:80].ljust(80, b" ")
    with path.open("wb") as stream:
        stream.write(component_header)
        stream.write(struct.pack("<I", len(records)))
        for record in records:
            stream.write(record)


def split_stl_components(source: Path, cache_root: Path) -> list[MeshComponent]:
    """Return cached connected components for ``source``.

    ASCII or malformed STLs safely fall back to the original mesh as one
    component. Generated files live in the runtime cache, never beside the
    source or in Git.
    """
    source = Path(source)
    data = source.read_bytes()
    parsed = _binary_triangle_records(data)
    if parsed is None:
        return [MeshComponent(index=1, path=source, triangle_count=0)]

    header, records = parsed
    digest = hashlib.sha256(data).hexdigest()[:16]
    output_dir = Path(cache_root) / f"{source.stem}-{digest}"
    manifest_path = output_dir / "manifest.json"

    if manifest_path.exists():
        try:
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            cached = [
                MeshComponent(
                    index=int(item["index"]),
                    path=output_dir / str(item["file"]),
                    triangle_count=int(item["triangles"]),
                )
                for item in manifest["components"]
            ]
            if cached and all(component.path.exists() for component in cached):
                return cached
        except (KeyError, TypeError, ValueError, OSError, json.JSONDecodeError):
            pass

    groups = _connected_triangle_groups(records)
    output_dir.mkdir(parents=True, exist_ok=True)
    components: list[MeshComponent] = []
    for index, triangle_indices in enumerate(groups, start=1):
        filename = f"part-{index:02d}.stl"
        path = output_dir / filename
        _write_binary_stl(path, header, [records[i] for i in triangle_indices])
        components.append(
            MeshComponent(index=index, path=path, triangle_count=len(triangle_indices))
        )

    manifest_path.write_text(
        json.dumps(
            {
                "source": str(source),
                "sha256": hashlib.sha256(data).hexdigest(),
                "components": [
                    {
                        "index": component.index,
                        "file": component.path.name,
                        "triangles": component.triangle_count,
                    }
                    for component in components
                ],
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    return components
