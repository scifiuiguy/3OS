# Quest / Editor demo directory (Phase 0.4)

At **Start**, `ThreeOSStorageController` **scans** the demo directory and instantiates what is actually there: root files → Object glyphs, root folders → Box glyphs (child counts tallied from folder contents). Files already inside a box are not re-instanced at the top level. Glyphs are laid out in a cubic matrix **1 m in front of the HMD** (1–8 → 2×2×2, 9–27 → 3×3×3, …). If listing fails, the HUD shows why and the scene will still look like Phase 0.3.

## Windows Editor

`%USERPROFILE%\Documents\3OS_Demo`

```text
3OS_Demo/
  simple.glb      # content file — shown with gltf_mark glyph
  Test1/          # empty folder — shown as Box glyph
```

## Quest / Android (authoritative)

Quest **always** uses app-persistent storage for write-through (public `Documents` is not reliable for `File.Move` / inserts):

`Android/data/com.scifiuiguy.threeos.sample/files/3OS_Demo`

Via MTP / headset file browser that is typically:

`/sdcard/Android/data/com.scifiuiguy.threeos.sample/files/3OS_Demo/`

On insert, HUD shows `moved → 3OS_Demo/Test1/simple.glb` under that root.

### Documents / MTP (“hard drive”)

If you browse `Documents/3OS_Demo` over MTP, insert **renames** `simple.glb` → `Test1/simple.glb` on that volume (cut, not copy).

MTP-staged files sometimes cannot be deleted without **All files access**. On first failed cleanup the app opens that system toggle for `com.scifiuiguy.threeos.sample` — enable it, then retry the insert (or delete the leftover root `simple.glb` once).

## Fallback seed

If persistent `simple.glb` is missing, the app seeds it from `StreamingAssets/3OS/seed/simple.glb` (or a Documents copy).
