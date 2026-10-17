# Quest / Editor demo directory (Phase 0.4)

At **Start**, `ThreeOSStorageController` **scans** the demo directory and instantiates what is actually there: root files → Object glyphs, root folders → Box glyphs (child counts tallied from folder contents). Files already inside a box are not re-instanced at the top level. Glyphs are laid out in a cubic matrix **1 m in front of the HMD** (1–8 → 2×2×2, 9–27 → 3×3×3, …). If listing fails, the HUD shows why and the scene will still look like Phase 0.3.

## Windows Editor

`%USERPROFILE%\Documents\3OS_Demo`

```text
3OS_Demo/
  simple.glb      # content file — shown with gltf_mark glyph
  Test1/          # empty folder — shown as Box glyph
```

## Quest / Android

**List / Start priority:** when `Documents/3OS_Demo` (and `Documents/3OS_Demo2`) are populated, those are the primary demo roots. App-persistent storage is the writable mirror for inserts and a fallback when Documents is empty.

| Role | Path |
| --- | --- |
| Documents (MTP staging, list priority) | `/storage/emulated/0/Documents/3OS_Demo` (+ `3OS_Demo2`) |
| Persistent (writable mirror) | `Android/data/com.scifiuiguy.threeos.sample/files/3OS_Demo` (+ `3OS_Demo2`) |

Start **mirrors additively** both ways (never deletes). Dual MindPalace fixture loads `3OS_Demo` (−15°) and `3OS_Demo2` (+15°).

On insert, HUD shows `moved → 3OS_Demo/Test1/simple.glb` under the active write root.

### Documents / MTP (“hard drive”)

If you browse `Documents/3OS_Demo` over MTP, insert **renames** `simple.glb` → `Test1/simple.glb` on that volume (cut, not copy).

MTP-staged files sometimes cannot be deleted without **All files access**. On first failed cleanup the app opens that system toggle for `com.scifiuiguy.threeos.sample` — enable it, then retry the insert (or delete the leftover root `simple.glb` once).

## Fallback seed

If persistent `simple.glb` is missing, the app seeds it from `StreamingAssets/3OS/seed/simple.glb` (or a Documents copy).
