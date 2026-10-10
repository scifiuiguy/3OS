#!/usr/bin/env python3
"""Embed THREEOS_glyph metadata into a GLB and write a .3glyph file.

Usage:
  python tools/inject_3glyph.py exported.glb docs/glyph_packs/gltf_mark.json
  python tools/inject_3glyph.py exported.glb docs/glyph_packs/box_closed.json -o box.3glyph
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

GLB_MAGIC = 0x46546C67  # 'glTF'
CHUNK_JSON = 0x4E4F534A  # 'JSON'
CHUNK_BIN = 0x004E4942  # 'BIN\0'
EXT_NAME = "THREEOS_glyph"


def align4(n: int) -> int:
    return (n + 3) & ~3


def read_glb(path: Path) -> tuple[dict, bytes | None]:
    data = path.read_bytes()
    if len(data) < 12:
        raise ValueError(f"{path}: too small for GLB")
    magic, version, length = struct.unpack_from("<III", data, 0)
    if magic != GLB_MAGIC:
        raise ValueError(f"{path}: not a GLB (bad magic)")
    if version != 2:
        raise ValueError(f"{path}: unsupported GLB version {version}")
    if length != len(data):
        raise ValueError(f"{path}: length mismatch")

    offset = 12
    json_doc: dict | None = None
    bin_chunk: bytes | None = None
    while offset + 8 <= len(data):
        chunk_len, chunk_type = struct.unpack_from("<II", data, offset)
        offset += 8
        chunk = data[offset : offset + chunk_len]
        offset += chunk_len
        if chunk_type == CHUNK_JSON:
            # JSON chunk is space-padded to 4 bytes
            text = chunk.decode("utf-8").rstrip(" \x00")
            json_doc = json.loads(text)
        elif chunk_type == CHUNK_BIN:
            bin_chunk = chunk
    if json_doc is None:
        raise ValueError(f"{path}: missing JSON chunk")
    return json_doc, bin_chunk


def write_glb(path: Path, json_doc: dict, bin_chunk: bytes | None) -> None:
    json_bytes = json.dumps(json_doc, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    json_pad = align4(len(json_bytes)) - len(json_bytes)
    json_bytes = json_bytes + (b" " * json_pad)

    chunks = [(CHUNK_JSON, json_bytes)]
    if bin_chunk is not None:
        bin_pad = align4(len(bin_chunk)) - len(bin_chunk)
        chunks.append((CHUNK_BIN, bin_chunk + (b"\x00" * bin_pad)))

    total = 12 + sum(8 + len(payload) for _, payload in chunks)
    out = bytearray()
    out += struct.pack("<III", GLB_MAGIC, 2, total)
    for chunk_type, payload in chunks:
        out += struct.pack("<II", len(payload), chunk_type)
        out += payload
    path.write_bytes(out)


def inject(json_doc: dict, meta: dict) -> dict:
    used = list(json_doc.get("extensionsUsed") or [])
    if EXT_NAME not in used:
        used.append(EXT_NAME)
    json_doc["extensionsUsed"] = used

    # Never require the extension — viewers without 3OS should still open the mesh.
    required = list(json_doc.get("extensionsRequired") or [])
    if EXT_NAME in required:
        required = [x for x in required if x != EXT_NAME]
        if required:
            json_doc["extensionsRequired"] = required
        else:
            json_doc.pop("extensionsRequired", None)

    extensions = dict(json_doc.get("extensions") or {})
    extensions[EXT_NAME] = meta
    json_doc["extensions"] = extensions
    return json_doc


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("glb", type=Path, help="Source .glb from Blender")
    ap.add_argument("meta_json", type=Path, help="THREEOS_glyph JSON (see docs/glyph_packs/)")
    ap.add_argument("-o", "--output", type=Path, help="Output .3glyph path")
    args = ap.parse_args()

    if not args.glb.is_file():
        print(f"error: missing glb {args.glb}", file=sys.stderr)
        return 1
    if not args.meta_json.is_file():
        print(f"error: missing metadata {args.meta_json}", file=sys.stderr)
        return 1

    meta = json.loads(args.meta_json.read_text(encoding="utf-8"))
    if meta.get("schema_version") != 1:
        print("error: schema_version must be 1", file=sys.stderr)
        return 1
    if not meta.get("glyph_id"):
        print("error: glyph_id required", file=sys.stderr)
        return 1

    out = args.output
    if out is None:
        out = args.glb.with_suffix(".3glyph")

    doc, bin_chunk = read_glb(args.glb)
    inject(doc, meta)
    write_glb(out, doc, bin_chunk)
    print(f"wrote {out} (glyph_id={meta['glyph_id']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
