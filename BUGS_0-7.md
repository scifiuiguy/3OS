# Bug Tracker: v0.7

Generated from the files in [`bugs/`](bugs/) by `tools/gen_bugs_md.py`. Edit those files, then re-run the script. Never hand-edit this table.

| ID | Status | Severity | App ver | Build found | Build fixed | Description | Repro |
|---|---|---|---|---|---|---|---|
| [BUG-20261026-3f08](bugs/BUG-20261026-3f08.md) | Resolved | High | 0.7.9 | 328 | 330 | Sustained 17-19 ms frames (below 60 FPS) starting ~7 s after the Blender demon reveal. Suspected GPU-bound (15k-tri demon, VerticalWipe shader). GPU frame timing is now enabled for diagnosis. | Run the Blender drop and observe FPS for ~10 s after the reveal. |
| [BUG-20261026-7c2e](bugs/BUG-20261026-7c2e.md) | Fixed (untested) | High | 0.7.9 | 329 |  | Pasting Demo/Demo2 contents into a voodoo volume bunches items onto the near-BB faces (many items per layer at the same spot). | Cut Demo and Demo2 contents; miss DemonProject and paste into the voodoo volume enclosing Demo3. |
| [BUG-20261026-e5a3](bugs/BUG-20261026-e5a3.md) | Resolved | High | 0.7.9 | 328 | 330 | ~125 ms frame when opening DemonProject. Async prefetch and time-sliced child view creation are implemented but not yet verified on device. | Open DemonProject (~41 children) and watch `[3OS][HITCHDETAIL]` in logcat. |
| [BUG-20261026-04ea](bugs/BUG-20261026-04ea.md) | Open | Mid | 0.7.9 | 328 |  | Box-snap hold copies the target box ghost's near pose instead of resolving in far first (violates the far-field ground-truth convention). | Hold an item over a box in a near BB so matrix-snap engages. |
| [BUG-20261026-b91d](bugs/BUG-20261026-b91d.md) | Open | Mid | 0.7.9 | 329 |  | Dropping an object onto another object's ghost inside a near BB overlaps them; no bump or separation runs (`move_ghost_to_portal` and ghost-drag release do not resolve in far space). | Drag one item into a near BB and release it on top of another item's ghost. |
| [BUG-20261026-d716](bugs/BUG-20261026-d716.md) | Resolved | Mid | 0.7.9 | 328 | 330 | 183 ms then 43 ms frames at paste completion (post-paste heal). | Paste a large batch of items and watch the FPS log at completion. |
| [BUG-20261026-8ab2](bugs/BUG-20261026-8ab2.md) | Open | Low | 0.7.9 | 328 |  | mc_slot logs 'THREEOS_glyph extension missing' at every boot. | Boot the app and check logcat. |
