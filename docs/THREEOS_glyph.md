# `THREEOS_glyph` — glyph pack metadata (Phase 0.4)

Glyph packs are **valid GLB** files renamed to **`.3glyph`**. Association data lives in a vendor extension on the glTF root so mesh + filetype binding stay in **one file**.

## Authoring workflow

1. Model the low-poly glyph in Blender.
2. Export **glTF Binary (`.glb`)**.
3. Run the injector (or paste the extension by hand) to embed `THREEOS_glyph`.
4. Output file uses the **`.3glyph`** extension (still GLB bytes).

You do **not** need this metadata on content files such as demo `simple.glb`. Only on glyph packs (`gltf_mark.3glyph`, `box.3glyph`, `box_open.3glyph`, …).

## Schema location in the GLB

Inside the GLB JSON chunk, on the **root** glTF object:

```json
{
  "asset": { "version": "2.0", "generator": "…" },
  "extensionsUsed": ["THREEOS_glyph"],
  "extensions": {
    "THREEOS_glyph": {
      "schema_version": 1,
      "glyph_id": "gltf_mark",
      "display_name": "glTF / GLB",
      "category": "file",
      "priority": 100,
      "applies_to_extensions": [".gltf", ".glb"],
      "applies_to_mime": ["model/gltf+json", "model/gltf-binary"],
      "box_state": null,
      "bounds_m": [0.08, 0.08, 0.08],
      "tint_rgba": [1.0, 1.0, 1.0, 1.0]
    }
  },
  "scenes": [ … ],
  "nodes": [ … ],
  "meshes": [ … ]
}
```

`extensionsUsed` must list `THREEOS_glyph`. Do **not** put `THREEOS_glyph` in `extensionsRequired` (generic glTF viewers should still open the mesh and ignore the extension).

## Field reference (`schema_version` = 1)

| Field | Type | Required | Notes |
| --- | --- | --- | --- |
| `schema_version` | int | yes | Currently `1`. |
| `glyph_id` | string | yes | Stable id used by the kernel registry (`gltf_mark`, `box_closed`, `box_open`, …). |
| `display_name` | string | yes | Human label for debug / pack UI. |
| `category` | string | yes | `file` \| `media` \| `app` \| `box`. |
| `priority` | int | no | Higher wins if two packs claim the same extension (default `0`). |
| `applies_to_extensions` | string[] | no* | e.g. `[".glb", ".gltf"]`. Empty/omit for Box-only packs. |
| `applies_to_mime` | string[] | no | Optional MIME list. |
| `box_state` | string \| null | no* | `closed` \| `open` for Box glyphs; `null`/omit for Object/file glyphs. |
| `bounds_m` | [x,y,z] | no | Suggested axis-aligned size in meters (host may ignore). |
| `tint_rgba` | [r,g,b,a] | no | Optional host tint hint. **`a` should be below 1** — glyphs render semi-transparent (target ~0.5). |

**Rendering note (0.4):** All OS glyphs draw as see-through icons. Hosts must use a transparent material queue; do not treat glyph meshes as opaque props.

**Required Unity shader:** [ThreeOS/Glyph](../3OS_Unity/Shaders/ThreeOSGlyph.shader) — base + selection Fresnel \(same pass\) gated by `_Selected` (host `MaterialPropertyBlock`; no material swap). See README § Objects → Glyphs.

\*Object/file packs should set `applies_to_extensions` (and usually leave `box_state` null). Box packs should set `category: "box"` and `box_state`, and typically leave `applies_to_extensions` empty (Boxes are bound by role, not by file extension).

## Demo packs (0.4)

| Output file | `glyph_id` | Binding |
| --- | --- | --- |
| `gltf_mark.3glyph` | `gltf_mark` | `.gltf`, `.glb` **content** (e.g. demo `simple.glb`) |
| `box.3glyph` | `box_closed` | default closed Box glyph (`box_state: "closed"`) |
| `box_open.3glyph` | `box_open` | open Box glyph (`box_state: "open"`) |

Ready-to-paste JSON for each pack: [`docs/glyph_packs/`](glyph_packs/).

## Injector

From the 3OS repo root:

```powershell
python tools/inject_3glyph.py path\to\exported.glb docs\glyph_packs\gltf_mark.json
# writes path\to\exported.3glyph (or -o out.3glyph)
```

See `tools/inject_3glyph.py` for flags.
