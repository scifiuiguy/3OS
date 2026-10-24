# 🗺️ 3OS_Core: Implementation Roadmap & Versioning Lifecycle

This roadmap moves **3OS_Core** from an empty repo to a production-grade, hardware-native volumetric OS module. Phases are dependency-ordered. Contiguous semver tracks real deliverables (no fake version gaps).

```text
Core  0.0 → … → 0.6 (voodoo) → 0.7 (final demo MR→Palace→MR) → 0.8 (multi-select) → 0.9 (layouts) → 1.0 (enclosure) → 1.1 (native OpenXR) → 2.0 (collab)
                                      │                                      │                    │                 │                    │
Unity 0.0 → … → 0.6 (voodoo) → 0.7 (final demo) → 0.8 (multi-select) → 0.9 (layouts) → 1.0 (enclosure) → 1.1 (UPM polish) → 2.0 (enterprise host)
```

### Companion repo (parallel track)

**[3OS_Unity](https://github.com/scifiuiguy/3OS_Unity)** is a **separate repository** walked **in lockstep** with these phases. Full paired milestones, Quest gates, and Unity tasks live in that repo’s [`IMPLEMENTATION_PLAN.md`](https://github.com/scifiuiguy/3OS_Unity/blob/main/IMPLEMENTATION_PLAN.md) (local checkout: `3OS_Unity/IMPLEMENTATION_PLAN.md`, gitignored here).

- **Core** proves correctness with headless C++ tests (and later its own native OpenXR path at **1.1**).
- **Unity** is the Quest 2 / Editor verification host from **0.1 onward** (thin harness on the frozen ABI) — not deferred until core 1.1.
- After each shared version, both the core tests **and** the Unity Quest gate should pass before calling the phase done (unless waived).

---

### ⚙️ Phase 0.0: Repo Scaffold
**Goal:** Make the tree buildable before any domain logic.

- [x] **Task 0.1: Skeleton & build system**
  - [x] Create the folder layout from the README (`include/`, `src/`, `shaders/`, `.github/workflows/`).
  - [x] Add `CMakeLists.txt` that builds a shared library (`3os_kernel`) and a console test target.
  - [x] Add MIT `LICENSE`, `.gitignore`, and a CI stub that compiles on Windows (and Linux if available).
- [x] **Task 0.2: Hello export**
  - [x] Export one `extern "C"` symbol (e.g. `threeos_version`) and call it from the console harness.

---

### 🧱 Phase 0.1: Core Types, Blittable ABI, Stub Facade (v0.1.0)
**Goal:** Define the domain model and the memory-safe plain-C ABI that Unity/Unreal/OpenXR hosts will call. Keep the facade as a stub until topology/kinematics exist.

- [x] **Task 1.1: Core types first (`include/core/`)**
  - [x] Define first-class types: `VoxelCoord`, `Pose`, `Box`, `Object`, `InteractionState`, and anchor/layout enums as needed by later phases.
  - [x] Prefer explicit padding + `static_assert` on `sizeof` / `offsetof` over blanket `#pragma pack(1)` unless a host ABI proves packing is required.
- [x] **Task 1.2: Design blittable interop layouts (`include/interop/`)**
  - [x] Implement binary-safe C structures (`InteropInputFrame`, `InteropTransformDelta`, etc.) with fixed-width primitives.
  - [x] Document ownership, lifetime, and units (meters vs inches, Left-handed vs OpenXR conventions).
- [x] **Task 1.3: Stub system orchestrator (`src/interaction_facade.cpp`)**
  - [x] Sketch `InteractionState` (`Idle`, `Hovering`, `Possessed`, `Transforming`) and callback slots (`OnObjectSelect`, `OnObjectHoverEnter`, `OnMultiSelectTriggered`) without full proxy logic yet.
- [x] **Task 1.4: Flatten native export layers (`src/threeos_marshaller.cpp`)**
  - [x] Wrap exports in `extern "C"` with platform macros (`__declspec(dllexport)` / visibility attributes).
- [x] **Task 1.5: Local test harness**
  - [x] Console pipeline passes mock frames across the C marshalling layer.
  - [x] Assert layout/size metrics; no silent padding surprises.

---

### 🕸 Phase 0.2: Topology, Proxies & Portals (v0.2.0)
**Goal:** Invisible voodoo-drop field proxy, Double-Proxy Voxel Lattice, and Voodoo portal teleportation. Prove with **headless tests** here; prove on Quest via the paired **Unity 0.2** gate. *(Product voodoo volumes — far/near bounding boxes, ghosts, snap centroids — are Phase 0.6; the drop proxy stays imaginary / unrendered — never a visible dome.)*

- [x] **Task 2.1: Multiscale coordinate workspace (`src/topology.cpp`)**
  - [x] Voxel indexing for discrete ~3-inch spatial blocks (`std::array<int, 3>` or equivalent).
  - [x] Scale-down remapping into a near-field **invisible voodoo-drop proxy**. Keep scale **configurable** (do not hardcode `1:100` as the only ratio).
  - [x] Matrix remapping that projects a far-field cluster into a near-field **Double-Proxy Voxel Lattice**.
- [x] **Task 2.2: Voodoo portal teleportation**
  - [x] Near ↔ far drag through an open double-proxy (Poros-style non-Euclidean remapping).
  - [x] Cross-proxy drag when multiple double-proxies are open (A → B teleport).
  - [x] Voodoo-drop proxy as global drop target for far-field placement.
- [x] **Task 2.3: Headless topology verification**
  - [x] Unit tests for voxel index math, proxy remaps, and portal teleport transforms (fake poses in, expected poses out).

---

### ⚡ Phase 0.3: Kinematics + Minimal Floor Clamp (v0.3.0)
**Goal:** Velocity telekinesis with inertia/friction, plus a **minimal enclosure** so demos do not sink objects through the floor.

- [x] **Task 3.1: Kinetic control system (`src/kinematics.cpp`)**
  - [x] Document the control law before coding: hand delta → acceleration/velocity, gain, deadzone, release → friction settle (the README’s rate-control story, not incomplete one-liners).
  - [x] Rate-based tracking (not 1:1 position). Sample hand/frame deltas to drive proportional velocity.
  - [x] Virtual inertia on release with an OS-level spatial friction coefficient.
- [x] **Task 3.2: Minimal floor clamp (subset of enclosure)**
  - [x] Clamp vertical motion when an object intersects the floor height profile (\(V_y = 0\) / snap) so Phase 0.3 demos stay usable.
- [x] **Task 3.3: Headless kinematics verification**
  - [x] Unit tests for gain/deadzone/friction settle and floor clamp with injected pose streams.

---

### 📂 Phase 0.4: Storage Glyphs, FS sync, selection & Box drill-down (v0.4.0)
**Goal:** Map legacy FS into Boxes/Objects with **typed 3D glyphs**, host-fed listing, **yaw-level** cubic demo matrix, sticky snap-insert with catch-up suppress + write-through **cut** (nested paths), single selection, and Box expand/collapse ABI. Glyphs are the OS “icons”; content payloads (the files themselves) stay separate. Host keyboard / rename-via-text is **not** in this phase (→ 1.0).

**Glyph package model (single file)**  
Windows associates types via the registry pointing at a separate `.ico`; the icon file does not carry the association. For 3OS we want the stronger case: **one file = mesh + type metadata**.

- **Container:** binary glTF (**GLB**). Keep the `glTF` magic / JSON+BIN chunks so any glTF tool can still open the mesh.
- **Custom type:** ship as **`.3glyph`** (rename/export of a `.glb`). Hosts register the extension; loaders may also sniff GLB magic. Relabeling is safe — it is still a valid GLB on disk.
- **Embedded association:** vendor extension `THREEOS_glyph` on the glTF root. **Canonical schema + demo pack JSON:** [`docs/THREEOS_glyph.md`](docs/THREEOS_glyph.md) and [`docs/glyph_packs/`](docs/glyph_packs/). Inject with [`tools/inject_3glyph.py`](tools/inject_3glyph.py).
- **Authoring:** Blender → glTF 2.0 (`.glb`) → `python tools/inject_3glyph.py mesh.glb docs/glyph_packs/<pack>.json` → `.3glyph`.
- **Resolution order:** registered `.3glyph` packs → built-in procedural fallbacks (unknown types still get a generic prism) → unlabeled Box/Object.

Core owns the **association table + descriptors**; hosts own **decoding GLB meshes and drawing**. Core does not need a full glTF renderer in 0.4 — it reads the small JSON metadata (and may treat mesh bytes as an opaque blob handed to the host).

**Quest 0.4 demo directory**  
Operator stages **`Documents/3OS_Demo`** (see [`docs/QUEST_DEMO_DIR.md`](docs/QUEST_DEMO_DIR.md)); host also mirrors under app persistent storage for writable cut. **Minimal smoke fixture** (also used by headless harness):

| On disk | Role in scene |
| --- | --- |
| `simple.glb` | **Content** file (not a glyph pack). Rendered with the registered **glTF/GLB Object glyph**; name label `simple.glb`. |
| `Test1/` | Empty folder → **Box**. Rendered with default **Box glyph** (`box.3glyph` / authored from `box.glb`); name label `Test1`; secondary count label `0 objects`. |

**Generalized host listing (shipped):** every file under the demo root → Object; every subdirectory → Box (child count from folder contents). Content `.glb` must not be confused with glyph pack assets.

**Demo field layout (0.4):** `threeos_storage_layout_demo` places all in-field roots in a **cubic matrix** 1 m in front of the head (side = ceil(cbrt(n)); 1–8 → 2³, 9–27 → 3³, …). Layout uses **yaw-only** heading with **world +Y up** so head roll/pitch at start does not tilt the field off the floor. Full Matrix/Cluster/Real-Anchor *mode toggles* remain Phase 0.9.

Authored glyph sources (Blender → GLB → `.3glyph` with `THREEOS_glyph`): glTF-logo-like pack for `.glb`/`.gltf`; **`box.glb`** as the default closed Box glyph for all Boxes; **`box_open.glb`** as the open-state Box glyph (toggleable).

**Rendering:** Object and Box **glyphs** (including demo `gltf_mark`, `box`, `box_open`) must draw **semi-transparent** (see-through prismatic icons), not opaque solids. Hosts use a transparent/alpha material path (URP Transparent or equivalent). Pack `tint_rgba.a` and mesh alpha should stay below 1 (target ~0.45–0.65 unless tuned). Content payloads like `simple.glb` are unaffected unless they are also shown as glyphs.

**Drop-into-Box interaction (kernel logic + host viz)**  
While dragging an Object **or Box** near another in-field Box within snap distance (**0.2 m**): snap to a hold pose just above the target Box; host shows an insert-arrow affordance. Leaving the threshold unsnaps. Releasing while snapped commits insert: emit storage events, run host anim (closed → open glyph, +10% scale bump and return, open → closed), hide the dragged glyph from the field, refresh Box count (`1 object` / `N objects`). Persist by **cutting (moving)** the file/folder into the target folder on disk (Quest Documents may require all-files access). Nested relative paths (`Parent/Child`) are preserved on cut; failed FS ack restores the prior `parent_box` (not always root).

**Snap ↔ stick telekinesis:** On snap enter, clear Object velocity to zero and **re-anchor `start_hand` to the player’s current hand pose** without requiring a grip release. While snapped, the kinematic escape pose + velocity keep integrating under the stick; display stays on the hold. On unsnap, lerp display from hold → escape pose and **keep the stick-built velocity** (do not re-anchor / zero). **Sticky snap:** once pending, ignore other Boxes until the escape pose leaves a **≤3″** buffer around the hold seat (enter still uses 0.2 m from Box center — prevents hold-pose cascade up a dense vertical stack). **Catch-up suppress:** while the unsnap lerp is active, snap does not re-engage (neighbor above the hold cannot suck the object back mid-escape).

**Box expand / collapse (0.4):** Host double short-trigger opens a Box (global drill-down); kernel `expand_box` lays children out in a cubic matrix above the parent (shared `field_spacing_m`, cell-hash lift). X dismiss closes; nested open trees close **deepest-first** then the selected parent. Host reconstructs demo-relative folder paths from the `parent_box` chain so nested folders instantiate deeper levels correctly.

- [x] **Task 4.1: Host-fed workspace listing (no in-kernel FS crawler)**
  - [x] Host lists sandbox paths and feeds Objects/Boxes via `threeos_storage_add_object` / `threeos_storage_add_box` (Quest Documents ∪ persistent; core stays path-agnostic).
  - [x] Workspace records carry relative path, display name, kind, child count, glyph resolve; no mesh IO in core.
  - [x] Document demo contract ([`docs/QUEST_DEMO_DIR.md`](docs/QUEST_DEMO_DIR.md)); harness fixture `simple.glb` + empty `Test1`; host supports arbitrary files/folders under `3OS_Demo`.
- [x] **Task 4.2: Glyph registry & `.3glyph` metadata (`volumetric_glyph`)**
  - [x] Define `THREEOS_glyph` schema (documented JSON fields + size/version constants), including Box closed/open glyph ids ([`docs/THREEOS_glyph.md`](docs/THREEOS_glyph.md)).
  - [x] Parse GLB JSON chunk enough to extract `THREEOS_glyph` (or reject / fall back); do not require full scene graph eval in core.
  - [x] Registry API: install pack bytes; map extension → `glyph_id`; Box closed/open pair; resolve on workspace entries.
  - [x] Built-in procedural fallbacks when packs are missing (generic Object / Box glyph ids + host prism visuals).
  - [x] Sample packs: `gltf_mark.3glyph` → `.gltf`/`.glb`; `box.3glyph` + `box_open.3glyph` → Boxes (toggle states); injector [`tools/inject_3glyph.py`](tools/inject_3glyph.py).
- [x] **Task 4.3: Host glyph / workspace ABI + demo matrix layout**
  - [x] Blittable `ThreeOS_WorkspaceItem`: entity id, kind, pose, `glyph_id`, tint, name + secondary label, child count, in-field / expanded flags; Box open via `threeos_storage_set_box_open`.
  - [x] Label fields: primary name and secondary (`0 objects` / `1 object` / `N objects`).
  - [x] Query path: `threeos_storage_item_count` / `get_item` for host instantiation.
  - [x] `threeos_storage_layout_demo(head)` — cubic matrix; yaw-only / world-up (head roll does not tilt the field); headless asserts layout poses + yaw-only under rolled head.
- [x] **Task 4.4: Box insert proximity & commit**
  - [x] While Object **or Box** is dragged: test distance to candidate Boxes; enter/exit **snap** (hold pose above Box); expose snap via `threeos_storage_get_snap` (arrow affordance). Refuse nesting a Box into its own descendant.
  - [x] **Sticky snap:** once pending, ignore other Boxes until escape pose leaves the snapped Box’s radius (no cascade up a dense vertical stack).
  - [x] **Catch-up suppress:** no snap engage while unsnap lerp is active (neighbor cannot re-suck mid-escape). Sticky exit does not same-frame retarget.
  - [x] On snap enter: `velocity = 0` and reset grab-anchored `start_hand`. On snap escape: lerp catch-up to escape pose; **keep** stick-built velocity (no re-anchor / zero).
  - [x] On release while snapped: `try_commit_insert` parents under Box, updates child counts (incl. prior parent decrement), clears snap, queues FS event; clears selection if the inserted entity was selected.
  - [x] Headless tests: snap pending; sticky Bot in dense stack; snap suppressed during catch-up; escape preserves velocity; release → child count 1, secondary label, Object leaves field.
- [x] **Task 4.5: Storage mutation event system (write-through cut)**
  - [x] Emit typed `ObjectMovedIntoBox` (source rel path, dest rel path, entity ids) on commit; host polls + acks.
  - [x] Host FS sink performs real **cut/move** into the Box folder (Quest: Documents + persistent; all-files access for Documents delete); supports nested `Parent/Child` relative paths for files and folders.
  - [x] On FS success, workspace stays consistent; on failure, roll back model (entity returns to field, **prior `parent_box` restored**, counts adjusted) and surface error.
  - [x] Harness: poll event asserts path rewrite `simple.glb` → `Test1/simple.glb`; ack success.
- [x] **Task 4.6: Single selection (0.4 tack-on)**
  - [x] Global selection state in ABI: `EntityId` or null (`0`) — `threeos_selection_get/set/clear`.
  - [x] Host index-trigger select / empty-space deselect; selection highlight is a host shader rim (see Unity plan).
  - [x] Selection-gated far telekinesis (possess while selected ignores near-field distance).
- [x] **Task 4.7: Storage/glyph verification**
  - [x] Harness fixture `simple.glb` + empty `Test1`; assert glyph ids, labels, and `0 objects` → `1 object` after insert.
  - [x] Assert `.3glyph` metadata maps `.glb` content to the glTF pack (distinct from Box packs).
  - [x] Insert flow + event/ack as in 4.4/4.5; selection set/get/clear + clear-on-insert.
  - [ ] *(Rename via host text is **not** a 0.4 gate — deferred to Phase 1.0 with the text router.)*
- [x] **Task 4.8: Box expand / collapse layout**
  - [x] Shared `field_spacing_m` (default 0.22 m) for demo + expand matrices.
  - [x] ABI: `set_parent_box` / `set_in_field` / `get_parent_box` / `expand_box` / `collapse_box`; flag bit2 `box_expanded` on workspace items.
  - [x] `expand_box` / `collapse_box`: cubic matrix above parent; uniform cell-hash encroachment (no octree); children stay independent world poses (no lasting hierarchy).
  - [x] Harness: expand places children above box without overlapping a blocker; collapse clears `in_field` / expanded flag.

**Headless / Quest gate (0.4):** Harness green for glyphs, yaw-level cubic layout, snap-insert (sticky + catch-up suppress + escape velocity), selection ABI, box expand/collapse. Quest APK demonstrates host listing + matrix field + cut write-through (incl. nested paths) + selection rim / far possess + Box double-tap expand / nested X close / nested open drill-down (device smoke on latest APK, currently Phase04 XR **51+**).

---

### ✋ Phase 0.5: Hand & Controller Integration (v0.5.0)
**Goal:** Make interaction input first-class: tracked hands and Quest controllers drive the same semantic actions through a stable ABI — including **primary / secondary / ternary** pinches and **palm-orientation** modes — so later phases are not stuck on Quest Trigger-only hacks.

**Scope note:** Phase 0.1–0.4 already marshal a thin `InteropInputFrame` and Quest trigger/grip paths enough for storage demos. 0.5 hardens **parity** (hands ↔ controllers), richer pose/button semantics, and headless coverage — not a greenfield rewrite of kinematics. Host hand/controller **meshes** are a Unity/Meta concern (see Unity U5.0); core only needs joints/poses and mapped actions.

**Pinch channels (mouse analogue)** — per hand, three discrete pinch channels like LMB / RMB / MMB:

| Channel | Hands | Controllers (Quest Touch) |
| --- | --- | --- |
| **Primary** | Index–thumb pinch | **Trigger** |
| **Secondary** | Middle–thumb pinch | **Grip** |
| **Ternary** | Ring–thumb pinch | **A / X** (thumb face button) |

Ternary on controllers has no ring-finger hardware; **A (right) / X (left)** is the closest equivalent because every hand pinch is thumb-opposed — the face button is the remaining thumb digital. Prefer A/X over stick-click (too easy to hit while steering) or B/Y (reserve for menu / cancel / mode unless remapped). Document B/Y as an optional host remap, not the default ternary.

**Palm orientation (ulnar / forearm roll), quantized** — three modes from palm normal vs world up (hysteresis bands so it does not chatter):

| Mode | Sense |
| --- | --- |
| **Palm up** | Palm faces roughly sky / ceiling |
| **Palm sideways** | Palm faces roughly world horizontal (knife-hand) |
| **Palm down** | Palm faces roughly floor |

Orientation is a **modifier** on pinch channels: the facade may bind distinct actions to any pair `(pinch_channel × palm_mode)` per hand (up to 3×3×2 combinatorial slots; unused slots stay unbound). Controllers synthesize the same three palm modes from controller pose / grip orientation so hand and controller share one action table.

**Locked 0.5 defaults**

| Intent | Binding |
| --- | --- |
| **Select** | Primary press, **any** palm |
| **Drag / telekinesis** | Primary **hold** + palm **sideways** (0.5 shipped down∥sideways; **0.6** frees palm-down for voodoo) |
| **Voodoo volume** | Stub in 0.5; **0.6** locks **snap** = primary-hold + palm-down, **free-range** = secondary-hold + palm-down |

**Cursor:** one white-sphere probe **per hand**. Scale lerp may follow primary, secondary, or ternary approach, but **only one finger pair at a time** — if multiple pairs are inside `d_thresh`, drive scale (and that frame’s pinch midpoint) from the pair with the **shortest** tip distance. Default `d_thresh` = **2.5 in (0.0635 m)**.

**Devices:** Quest **2** is the smoke gate (hardware on hand); keep OpenXR paths Quest **3**-ready (no Quest-2-only APIs where a shared path exists).

- [x] **Task 5.1: Input ABI — hands + controllers**
  - [x] Extend blittable input (append-only after 0.1 freeze): per-hand / per-controller pose, tracking confidence / lost flags, **primary / secondary / ternary** press+release+hold, per-channel tip distances, and **palm_mode** enum (`Up` / `Sideways` / `Down`) with confidence.
  - [x] Document units, handedness, and OpenXR ↔ Unity convention parity for palm normal, pinch tips, and controller tip; ternary = Unity/OpenXR **`primaryButton`** (Touch `a`/`x`).
  - [x] Keep device transport out of core: hosts fill frames; core consumes semantics.
- [x] **Task 5.2: Facade action mapping**
  - [x] Action table: `(handedness, pinch_channel, palm_mode[, edge|hold])` → intents. Ship locked defaults above (select / drag / voodoo-volume stub).
  - [x] Hands: primary = index–thumb, secondary = middle–thumb, ternary = ring–thumb.
  - [x] Controllers: primary = trigger, secondary = grip, ternary = primaryButton (A/X); same palm_mode from controller orientation.
  - [x] Dual-source policy: prefer hands when confidently tracked; fall back to controllers without double-firing the same action.
- [x] **Task 5.3: Interaction-point / point-cursor policy**
  - [x] **One** unparented world-space point cursor **per hand** (1:1 Euclidean — not a child of the hand transform).
  - [x] **Driving pair:** among finger pairs with `d ≤ d_thresh`, pick shortest `d`; else default to index–thumb. `cursor0 = (fingerTip + thumbTip) * 0.5` for that pair. Controllers: tip/aim as `cursor0`.
  - [x] **User-aligned offset (not hand-local):** from `cursor0`, add **4 in (0.1016 m)** along the user’s forward projected onto the floor plane (yaw-only / horizontal), plus **2 in (0.0508 m)** along world up → hover/select/snap probe.
  - [x] Soft-fail when tracking drops mid-possess (no crash; clear or coast per kinematics rules).
- [x] **Task 5.3b: Point-cursor sphere scale interpolation (Quest-OS-inspired)**
  - [x] Default `d_thresh = 2.5 in`. Above threshold for all pairs → scale **100%**.
  - [x] While approaching (pinch not yet triggered): use **only** the shortest in-threshold pair’s `d`; map `d_thresh → 0` linearly onto scale **100% → 50%**. Never blend multiple pairs’ lerps in one frame.
  - [x] On **pinch trigger** for that channel: **fast-lerp** sphere scale back to **100%** (held pinch does not stay shrunken).
  - [x] Controllers: approximate approach with trigger/grip/primaryButton value for the active channel, then same fast-lerp to 100% on press.
- [x] **Task 5.4: Headless verification**
  - [x] Inject mock hand and controller streams; assert primary/secondary/ternary edges and palm_mode quantization (incl. hysteresis).
  - [x] Assert cursor probe = driving-pair midpoint + 4″ floor-forward + 2″ up; hover/select use that probe.
  - [x] Assert scale: `d_thresh=2.5″`, shortest-pair wins, 100%→50% then fast-lerp to 100% on trigger; multi-pair fixture picks min `d`.
  - [x] Assert select = primary+any; drag = primary-hold+down/sideways; voodoo-volume stub distinguishable (bindings remapped in **0.6**). Tracking-lost mid-drag does not corrupt workspace/snap.

**Headless / Quest gate (0.5):** Harness green for dual-source mapping + pinch/palm/cursor ABI. **Quest 2** smoke (Quest 3–ready codepaths): storage path with hands-only and controllers-only; white-sphere cursors; select vs drag vs voodoo stub distinguishable on device.

---

### 🧿 Phase 0.6: Voodoo Volumes (v0.6.0)
**Goal:** Ship **snap** and **free-range** voodoo volumes as the product interaction for far↔near editing — floor-aligned far-field bounding boxes, yellow drag arrows, sibling **centroids**, near-field ghost portals, and bump-lerp field hygiene. Core owns centroids, snap gravity, scale/aspect math, and state; Unity draws boxes/arrows/ghosts (paired Unity 0.6).

**Scope note:** Phase 0.2 topology/portal remaps remain available as math. The **voodoo-drop proxy is imaginary** — do **not** render a dome or mini-map. Users see the far-field bounding box, yellow drag affordance, snap nodes (snap mode), and near-field ghost boxes. Final demo VR↔MR → **0.7**; multi-select across volumes → **0.8**; layouts/anchors → **0.9**; enclosure/shaders/AI → **1.0**; native OpenXR → **1.1**.

**Input remaps (supersede 0.5 voodoo stub)**

| Mode | Binding | On activate |
| --- | --- | --- |
| **Snap voodoo** | Primary hold + palm **down** | Far-field BB + yellow drag arrow; snap-node cluster from open-box centroids |
| **Free-range voodoo** | Secondary hold + palm **down** | Same BB + yellow drag arrow; no snap-node cluster / gravity |
| **MindPalace / drive picker** | Secondary hold + palm **up** | Multi-Choice Node widget (Task 6.7); no conflict with free-range (palm-down) |
| **Drag / telekinesis** | Primary hold + palm **sideways** | Unchanged path (palm-down freed for snap voodoo) |

**Parameterized field metrics (ABI get/set; host may override; clamps required)**

| Parameter | Default | Notes |
| --- | --- | --- |
| `voxel_size_m` | **3″** (`0.0762 m`) | World voxel edge; BB counts are in these units |
| `near_bb_voxels` | **4×6×4** (x,y,z = W,H,D) | Product size **W×D×H = 12″×12″×18″** → extents (W,H,D)=(12″,18″,12″) |
| `field_radius_m` | **50 ft** (`15.24 m`) | Horizontal field limit; also full-scale radial gain target |
| `field_height_m` | **30 ft** (`9.144 m`) | Vertical field limit; also full-scale height gain target |
| `arm_reach_m` | **12″** (`0.3048 m`) | Hand travel that maps to full `field_radius_m` / `field_height_m` |

Far and near BBs share the same start size (`near_bb_voxels × voxel_size_m`). Far BB keeps that **aspect** while later scaling nominal size to fit snapped contents — never stretch axes independently. Changing `voxel_size_m` or `near_bb_voxels` updates derived extents and the far aspect lock together.

**Dual hands:** Left and right may each hold an independent voodoo session (two far BBs, two arrows; two near BBs after release). Host draws boxes as **wireframes**, not solid prisms.

**Far-field bounding box:** Instantiates at gesture start at the standardized **navel-forward rest pose**, floor-yaw facing the user. While the snap/free hold is active, position tracks the hand vector with field-scale gain: `far = rest + hand_delta * (field_radius/arm_reach)` on horizontal axes and `(field_height/arm_reach)` on vertical, clamped to field limits. On intentional release the far BB **parks** where dropped (does not dismiss). Tracking loss mid-hold cancels without opening near. Nominal size later scales to fit snapped centroid children while **preserving** aspect (Task 6.3).

**Near-field bounding box:** On **any** intentional release of snap or free-range (even with no centroids, or dropped away from snap nodes), opens with 1× start extents. **Seed** at today’s short navel-forward rest (~0.41 m / ≈16″, Quest-tuned; box center so **bottom-center** sits on that anchor, **4″ below** navel height), then **orbit** to the creating hand’s cursor (L/R bias) before bump-lerp (Task 6.4). Far spawn stays farther out (~1 m) for field placement. Ghost contents (Task 6.4+) use a shrink multiplier when the far BB has grown larger than near; free-range ghosts = objects encapsulated by the parked far BB.

**Yellow drag arrow:** Same geometry as the selection drag arrow, but **yellow**, with a translucent yellow deadzone sphere. **Deadzone arming (also fix selection arrow):** inactive on instantiate; arms only after the hand first **leaves** the threshold and later **re-enters**. Snap-node spheres (Task 6.3) use the **same** hand→field scale so tip contact places the far BB on the represented objects.

- [x] **Task 6.1: Voodoo input + session ABI + far/near shell**
  - [x] Map snap vs free-range intents; **per-hand** sessions (L and R concurrent); expose open/hold/release/park without a parallel host state machine.
  - [x] Expose get/set for `voxel_size_m`, `near_bb_voxels` (ivec3 W,H,D = 4×6×4 default), `field_radius_m`, `field_height_m`, `arm_reach_m` with clamps; harness asserts defaults and extents = counts × voxel size → (12″,18″,12″).
  - [x] Blittable far/near BB descriptors: pose (floor-yaw), extents, aspect lock, mode, linked centroid id(s), open/closed. Hand-indexed `get_session` / `get_drag_arrow`.
  - [x] Far BB: navel-forward rest at activate at **2×** near start size; yaw-only billboard toward user while dragging (retained on park); subtle position damp; scaled hand-Δ motion; intentional release parks far and opens near at navel (1×, no ghosts yet); tracking-loss cancels drag only.
  - [x] Yellow drag-arrow + deadzone state (origin, tip, armed flag) shared contract with selection drag (fix unarmed-on-spawn bug there too).
  - [x] Unity: wireframe far + near BBs (not solid cubes); dual-hand viz.
- [x] **Task 6.2: Centroids (sibling groups + MindPalace parent)**
  - [x] Persistent centroid per open Box’s child group (average of in-field children); update live each tick; **omit** empty sets.
  - [x] Nested rule: expanded Box in-scene → sibling children share a box centroid.
  - [x] Snap activation **samples current** centroid poses (registry rebuilt every tick — no freeze).
  - [x] **MindPalace parent** centroid: imaginary root (no glyph); average of that MindPalace’s top-level in-field children.
  - [x] Data model supports **multiple MindPalace nodes** (`mind_palace_id` on entries + `threeos_mind_palace_register`).
  - [x] **Quest snap-test Start layout:** dual MindPalaces from `3OS_Demo` + `3OS_Demo2` at **10 ft**, **±15°** azimuth, **2.5 ft** up; field centroid markers + 6.3 hand-space snap dots.
- [x] **Task 6.3: Snap-mode nodes + gravity** *(not in 6.1 — snap dots / gravity land here)*
  - [x] Around the yellow drag tip, render a cluster of spherical snap dots at **field-scaled** positions of every open-box centroid (plus MindPalace-root node) so the same gain that moves the far BB maps tip contact onto real objects.
  - [x] Tip gravity: when near multiple nodes, pull toward the **closest**; snap tip to that node and snap the far-field BB to the real centroid.
  - [x] On snap, interpolate BB size on all three axes to encapsulate that centroid’s children **without** breaking the parameterized aspect (`near_bb_voxels`).
- [x] **Task 6.4: Near-field ghosts + portal contents**
  - [x] **Near-BB place + bump-lerp first (before ghosts):** keep today’s navel-forward rest as the **seed** pose, then **orbit** the new near BB around the user until it aligns with the **creating hand’s** cursor (L→left bias, R→right bias) — that orbit-aligned pose is the instantiation target (does not warp the current navel offset math, only reassigns the rest target after seed). Existing near BBs bump-lerp clear so volumes never overlap (no cursor-in-two-portals Venn); if flanked on both sides, bump **both** directions and cascade through all open near BBs. Paths orbit the body; duration **under 0.25 s**. Capacity ≈ **12** near BBs; if a new open would not fit, **refuse** new voodoo volume creation until the user dismisses a near BB. Non-voodoo near objects can wait for 6.6.
  - [x] Near BB shell already opens on any intentional release in 6.1; here: ghost copies inside the near BB, scale-shrunk so far-field nominal size maps into the near box (when far has grown).
  - [x] **Ghost membership:** **Snap** → sibling group of the linked centroid (as today). **Free-range** → every **literal in-field object whose bounds are encapsulated by the parked far BB** gets a corresponding near ghost (not “empty until snapped”).
  - [x] Ghost of the **parent Box** at the near BB’s bottom-center node when the linked centroid is a **Box**; **skip** for MindPalace / free-range with no parent Box (no marker substitute).
  - [x] **Select on ghosts** works (highlight / selection state mirrors the far counterpart). **No ghost drag in 6.4** — motion waits for 6.5; summon can be validated later once select is in.
  - [x] Maintain two centroids while open: **persistent** (always tracks true sibling poses) and **temporary** (created at near-BB open; drives far↔near BB relationship; does **not** expand the far BB when objects are summoned into the near field).
  - [x] **Cursor tint:** when the point cursor enters a near BB, match the near wireframe blue; restore white on exit.
- [x] **Task 6.5: Ghost drag, summon, dismiss, dual-mind-palace fixture** *(consolidates shipped UX through APK 120)*
  - [x] **Ghost drag / sticks:** inside any near BB → ghost-scale drag + **blue** arrow; outside all near BBs → far-real drag + white/cyan stick; 1:1 real-time far edits; planar face clamp keeps ghost in portal while velocity drives far; Windows-style grab-to-select; deselect on empty pinch inside near BB.
  - [x] **Lock park-time ratio + expand clutter:** far BB refit grows/shrinks near by park-time `lock_scale` (ghosts do not shrink mid-drag); resizing near BB is the bumper; siblings bump away; near-field clutter pushed out of grown footprint (seed for 6.6 open/orbit bump).
  - [x] **Summon / cross-proxy / insert:** pinch outside source near BB in open near space → summon real into near field; pinch inside a different near BB → map to that portal’s far; drop onto a Box glyph → insert/cut like possess; ghost audition snap-insert during drag.
  - [x] **Near↔portal role swap:** near-field real dragged into near BB spawns far ghost at scale; pull back out restores; release inside swaps roles (far ghost → real, near real → ghost).
  - [x] **X-close dismiss:** mid-Y front edge sphere (R-created → mid-front-left; L-created → mid-front-right); dual-hand / dual-portal; dismisses far+near, destroys ghosts, clears temp centroid. *(Clutter return bump waits on 6.6; parent-Box collapse → parking lot.)*
  - [x] **Dual MindPalace snap fixture + ABI:** Quest demo registers `3OS_Demo` + `3OS_Demo2` at 10 ft / ±15° / 2.5 ft up; path-agnostic `threeos_mind_palace_register` / `mind_palace_id` kept open for future network shares. *(Generic N-folder + picker UI → Task 6.7.)*
- [x] **Task 6.6: Scissors cut/paste + near-field clutter bump**
  - [x] **Scissors `.3glyph`:** author a low-poly open-scissors glyph; save as `assets/glyphs/scissors.3glyph` (valid GLB + `THREEOS_glyph` metadata, `glyph_id` e.g. `scissors`); install via the existing glyph registry path (alongside `box` / `box_open` / `gltf_mark`).
  - [x] **Cut button on near BB:** second edge sphere **just below** the X-close sphere, **same radius** as X; float the scissors glyph inside sized similarly to the X mark; L/R mirrored with the dismiss control.
  - [x] **Cut (pinch scissors):** **voodoo-level cut** — mark **content ghosts only** (exclude parent Box ghost; parent is considered just outside the portal). Duck cut ghosts **by 50% opacity** relative to their normal ghost alpha (e.g. 50% → 25%; host; geometric scale unchanged). **Move** on paste (not duplicate); FS write-through matches Box-insert cut semantics when pasting into a Box. One active cut set at a time (scissors on another portal replaces prior cut). **Do not** dismiss or refit/resize the source volume on cut — leave it open; ghosts leave the portal on successful paste (empty shell + parent ghost remain).
  - [x] **Paste (next primary pinch outside the source near BB):**
    - **Empty near-field space** or **empty space inside a different near BB** → paste the **cluster centroid** at the pinch (billboarded toward the user); objects keep relative offsets around that centroid (not stacked on the pinch).
    - **Any Box not belonging to the cut near BB** (in-field or ghost in another portal) → move/insert into that Box.
    - **Paste bump:** orbit overlapping near BBs clear of the paste footprint, then **cascade** sibling overlaps on the body orbit (A into B → B yields; no settled overlap); if the ring cannot absorb them, nudge the paste origin; bump non-BB near clutter via pose-lerp. Cursor / `point_in` uses live portal poses only (parked hand-session BB snapshots must not leave phantom hit volumes after bump).
  - [x] **Cancel:** **any** primary pinch **inside the source near BB** (empty space, cut ghosts, or parent) cancels cut — restore opacity and clear cut state (cannot paste into the cut set / itself).
  - [x] **Near-field object priority + bump-lerp:** near BB instantiation bumps **non-voodoo** near clutter out of the way (must not suck into the new BB); body-orbit L/R paths **under 0.25 s**; if an orbital slot is blocked, push just beyond reach into the far field. Expand-time clutter push already seeded in 6.5. Paste reuses the same orbit / clutter policy.
- [x] **Task 6.7: Unlimited MindPalace nodes + Multi-Choice drive/favorites picker + mutual bump**
  - [x] **N MindPalace spawn / layout:** Instantiate contents from any number of assigned root directories (Documents/`…` today), each as a MindPalace node with collision avoidance. One node → forward-of-user layout; multiple → lateral body orbit; if full, spill to farther concentric ground orbits out to `field_radius_m`, then fill upward to `field_height_m`; if still no room, HUD error naming the directory that could not instantiate.
  - [x] **Dynamic drive / directory ingest (pre-alpha):** Any top-level Quest-internal `Documents/…` directory (and children as navigable content) can be assigned as a MindPalace node. Picker UI must accept new roots as they appear in the available set without a scene rebuild.
  - [x] **Open picker:** secondary hold + palm **up** summons the **Multi-Choice Node** widget. Fresh menu each call (no sticky Add/Jump strip from a prior summon). If a prior Multi-Choice group is still open, close it and instantiate a new one.
  - [x] **Dismiss:** primary pinch **outside** the active Multi-Choice hover thresholds dismisses **all** open Multi-Choice groups for that interaction (main grid and Add/Jump strip if present).
  - [x] **Geometry / hover math:** Multi-Choice geo + octant selection ported to core C++ / OpenXR. Hover = simultaneous 3-axis dot products of cursor-to-center vs widget axes → octant; magnitude within **min–max** = octant hover; magnitude **&lt; min** = center hover. Widget SFX under `assets/audio/` (`hover_click_*`, `select_click_02`, `selection_click_02`) for later use.
  - [x] **Main grid (2×2×2):** Eight option slots around center. Center holds **two** side-by-side pagination nodes (forward / back). Page size = 8. Pagination enable rules: 8-or-fewer options → both disabled; page 1 of N&gt;1 → forward on / back off; pages 2…N−1 → both on; page N → forward off / back on. Fewer than 8 options → unused slots empty/disabled.
  - [x] **Sections + labels:** Title starts **"Root Drive:"**; hover name label above the widget tracks the hovered option. After paging past the root-drive section, a second section **"Favorites:"** with its own 1…N pagination. **Favorites** = Boxes (folders) whose favorite heart is **filled** via the folder context menu (below).
  - [x] **Folder context menu (Box “right-click”):** palm-up secondary on **any** in-scene Box opens an **8-slot Multi-Choice** (same 2×2×2 + center pagination geometry as the drive picker) with **folder-specific actions** (not the Root Drive list). Remaining option slots are TBD; ship the first slot now:
    - [x] **Favorite heart (slot 1):** author unfilled + filled heart glyphs in Blender → `.3glyph` (e.g. `heart` / `heart_filled`). Primary-pinch toggle: unfilled → filled **adds** that Box to the main picker’s **Favorites** section; filled → unfilled **removes** it. Does not by itself Jump/Add the folder into the field.
  - [x] **Confirm strip (2nd step, drive/favorites picker only):** primary pinch on a hovered **Root Drive / Favorites** option reveals a second Multi-Choice strip above the main widget with **Add** and **Jump to Root** (two nodes only → small min threshold, no center choice; own max threshold). Pinch outside either group's threshold cancels both without action.
  - [x] **Add:** merge contents of the chosen drive/Box into the **current** field (compete for orbit/space; mutual bump required — below).
  - [x] **Jump to Root:** clear current field hierarchy (no Unity scene reload) and instantiate a fresh empty field filled only with the selected drive/Box — 3D analogue of opening that directory in a new File Explorer window.
  - [x] **Mutual object bump-lerp (fix):** when cut-paste or Add pushes near BBs **and** nearby objects, bumped objects must resolve **against each other** (not all to one shared destination) so post-bump poses do not overlap. Especially required when adding an entire drive into an occupied field.
- [ ] **Task 6.8: OpenXR-first transform audit (host → core)** — *refactor check only; do not implement until 6.7 Multi-Choice debug is closed.* Goal: inventory and migrate any **semantic** pose/hit/billboard math still originating in Unity so Unreal / native C++ hosts only marshall coordinates at the boundary (same rule as the 6.7 `WidgetPoseFromCursor` fix). Keep engine adapters for draw/apply only (`InteropStructs` X-flip, `Transform` write, mesh chrome).

  **Migrate (logic in core / ABI; host consumes blittable poses):**
  - [ ] **Multi-Choice open pose:** `ThreeOSMultiChoiceHost.WidgetPoseFromCursor` (cursor + head → yaw so local −Z faces head) — already OpenXR arithmetic but still host-owned; fold into core open path / shared `voodoo_geom` yaw helper.
  - [ ] **MindPalace fixture / demo anchors:** `ThreeOSStorageController.MindPalaceFixturePose` (azimuth × range × height from head, yaw-face user) — core already has `mind_palace_layout` + `yaw_facing`; fixture should call core or share one yaw API.
  - [ ] **Portal edge chrome poses:** `ThreeOSDismissAffordances.PortalEdgeButtonUnity` (yaw-quat rotate of mid-front L/R local offset + scissors Y offset) + hover radii in Unity space — expose dismiss/scissors world poses (and hover) from voodoo ABI.
  - [ ] **Yaw-aligned selection pick:** `ThreeOSStorageInteraction.FloorYawRotation` / `RayHitsYawAlignedAabb` / `TryHitTestYawAligned` (`LookRotation` + slab AABB) — core OpenXR ray vs yaw-aligned (or per-item) AABBs; host stops inventing floor yaw from `Camera.main`.
  - [ ] **Box-under-cursor (folder context):** `FindBoxNearCursor` distance in Unity world — compare OpenXR cursor to workspace item poses in core/host-agnostic space.
  - [ ] **Multi-Choice slot / strip local offsets:** SyncViz hardcodes octant / pagination / confirm local positions — prefer ABI-exported slot locals (or document that viz mirrors core constants once).

  **Shared yaw billboard (dedupe; host may still *apply*):**
  - [ ] Glyph name labels (`ThreeOSGlyphViewRegistry.BillboardLabels` → `LookRotation`) and portal X/scissors face-user (`LookRotation` to camera) — same floor-yaw-toward-head as `voodoo_geom::yaw_billboard_toward`; either core writes orientations on items/chrome descriptors or host calls one shared OpenXR helper (no Unity `LookRotation` as source of truth).

  **Stay host-only (adapters / viz):**
  - [ ] `InteropStructs` Unity↔OpenXR X-flip pose conversion at the marshalling boundary.
  - [ ] Applying core poses to engine transforms; arrow mesh `FromToRotation` / shaft scale from core stick endpoints; DebugHud head-local placement; authored glyph mesh local pivots (e.g. scissors pitch).

  **Exit for 6.8:** checklist reviewed post-6.7; migrations scheduled or parked with owners; no new Unity-originated semantic transform math for interaction/layout without a core twin.

**0.6 exit note:** Formal harness/Quest gate checklist and parent-Box collapse are **shelved** to the parking lot (unscheduled). Task **6.8** waits on 6.7 debug. Final demo is **0.7**; multi-select across volumes is **0.8**.

---

### 🎬 Phase 0.7: Final Demo — MR Intro → Mind Palace ↔ MR Office (v0.7.0)
**Goal:** Ship the **final product demo reel** in three acts. **Act I (~⅓):** open in the **MR office**, introduce 3OS via a togglable GitHub plane, then engage a **3D agent (NPC)** with head+hands performance clips. **Acts II–III (~⅔):** summon the **Mind Palace** for rapid FS management, then transition back to MR for desk Box / Blender floor spawn beside Unreal. *(Monitor spectator drop + STREAM was **7.10** — **86’d** for 0.7.)* The Mind Palace remains a **VR overlay** over an MR-enabled office; summon/dismiss with a gesture; the same Box can have different spatial anchors in VR vs world-mesh MR.

**Story beat (filmed order):**
1. **MR cold open** — XR Origin + NPC already in the office, NPC held on **frame 0 of clip 01** before engagement. Show GitHub plane for intro; dismiss. Voodoo the NPC head → agent Multi-Choice widget → Speak (mic) → “talk” → NPC clip 01. Speak again → NPC clip 02, then NPC shrinks into the floor.
2. **Mind Palace** — Open-book VR toggle; FS cut-paste / third Palace node (as already scoped).
3. **Back to MR** — Desk-anchored Box edited in VR; Blender floor spawn FX beside Unreal (loading glyph → laser perimeter → badge → logos → DemonA wipe). Optionally re-summon GitHub plane for the close. **No STREAM.**

**Depends on:** 0.6 voodoo / MindPalace / Multi-Choice (debug closed). Multi-select (**0.8**) is not required for the scripted path. *(Inserted ahead of multi-select / layouts / enclosure.)* Hand-performance JSON **01/02** + matched audio already captured in the Android hands test scene (`Assets/3OS_FinalDemo/Performances/`, `Assets/Audio/jarvis_voice_clip_0{1,2}`).

#### Act I — MR open + GitHub + NPC agent (~⅓, new)

- [x] **Task 7.1: Final-demo staging scene opens in MR** *(origin + NPC slice done; FS/voodoo/Multi-Choice migrate = follow-up)*
  - [x] Final-demo home scene (`3OS_Final_DemoScene`); **default mode = MR** — office geo is **fake passthrough** (no real PT in any act). Palace skybox overlays later (7.7).
  - [x] Prefab DemoScene **XR Origin** (+ Interaction Manager) → instance at **`Player_Spawn_Point`**. *(FS / voodoo / Multi-Choice harness migrate deferred to follow-up.)*
  - [x] Prefab hands-test **PerfNPC** (head + SegHands); place under **`NPC_Floor_Parent`** for shrink-to-floor; remove Jarvis **v07** placeholder.
  - [x] WispySky (`Assets/Wispy Sky/Materials/WispySkyboxMat.mat`) stored on `ThreeOSFinalDemoStage.palaceSkybox` — **not** applied at cold open; Act II wipe applies it over office geo.
- [x] **Task 7.2: Togglable GitHub plane**
  - [x] **Comp plate:** Stitched `Blender/screenshots/github-comp-1…5.png` → `Assets/3OS_FinalDemo/Textures/github_3os_page.png` (master width; row1+row2 full width; body ¾ left / ¼ sidebar with 4 over 5; **white** below 5).
  - [x] World-fixed plane **24" wide**, **1.5 ft** ahead of XR Origin, **1 ft** below eye height; **one-shot** billboard toward eyes on spawn (not continuous). Default **ON**.
  - [x] **Toggle gesture:** raw XR **right palm up + right secondary pinch**; edge toggles on/off (`ThreeOSFinalDemoGitHubGesture`).
  - [x] Demo use: visible for intro → dismiss → re-show at end via same gesture.

- [x] **Task 7.3: NPC head focus-anchor → agent Multi-Choice widget (not a normal voodoo)**
  - [x] Mark the NPC **head** as the **only** MR focus anchor for 0.7 (Task 7.8 office-anchor list **cancelled**). `CentroidKind::FocusAnchor` + `ThreeOSFinalDemoNpcFocusAnchor`.
  - [x] **Open:** snap **or** free-range voodoo (**primary or secondary** + palm down) **snap-to** that head focus anchor; on **release** do **not** open a normal far/near voodoo — spawn the agent Multi-Choice instead (`PendingAgentOpen` → `open_agent`).
  - [x] **Widget pose:** park at the usual **near-BB navel rest** (`threeos_voodoo_near_rest_pose` / `near_rest_pose`).
  - [x] Geometry = Mind Palace drive-selection Multi-Choice (2×2×2 octants + center hover math). **No pagination** (hide page buttons); single page of **8** options. Mode `MultiChoiceMode::Agent`.
  - [x] Fill order (core `multi_choice.hpp`): **0 TLF, 1 TRF, 2 BLF, 3 BRF, 4 TLB, 5 TRB, 6 BLB, 7 BRB** (front toward user). Labels follow this index order:
    | Idx | Octant | Label / affordance |
    | --- | --- | --- |
    | 0 | TLF | **Speak** — extruded **microphone** `mic.3glyph` (no text) |
    | 1 | TRF | Review (text) |
    | 2 | BLF | Dismiss (text) |
    | 3 | BRF | Focus (text) |
    | 4 | TLB | Revert (text) |
    | 5 | TRB | Restore (text) |
    | 6 | BLB | Shelve (text) |
    | 7 | BRB | Submit (text) |
  - [x] Only **Speak** is interactive for the reel (`MultiChoiceAction::Speak` / `OnAgentSpeak`); other nodes may highlight/select but perform no action.
  - [x] Harness migrate: `ThreeOSFinalDemo73Setup` prefabs DemoScene `3OS_Harness` into Final Demo + wires NPC focus anchor.
- [x] **Task 7.4: Speak / mic → EQ visualizer → clip 01 → clip 02 → shrink-out**
  - [x] **Cold pose (frame 0):** on Final Demo load, before the player engages Speak / mic, bind the NPC to the **first frame of performance slot 01** (`android_hands_perf_01.json`) — head + hands (+ finger hinges) held on that static pose with **no audio / no playback**. Slot 01 play still starts only after the first end-of-utterance cue. NPC stays under **`NPC_Floor_Parent`** (shrink pivot).
  - [x] **Widget appear:** agent Multi-Choice instantiates at **0% scale** and **expand-lerps from its center** to full size with a **tiny overshoot** curve (pop-in), then settles at 100%. Re-run pop-in whenever the agent widget is **reopened** (demo restart without app reboot).
  - [x] **Mic toggle on:** selecting Speak arms “listening” (**real Quest mic**). Spawn a **3D EQ visualizer** above the widget: **8–12** small cube primitives, **10% opacity black** material. Each cube has a **parent anchor at its bottom face**; scaling that anchor on **Y** stretches the cube into a tall rectangle (per-band amplitude).
  - [x] **End-of-utterance cue:** after real mic energy + begin-speak, **~0.3 s relative silence** cues the NPC. **If mic levels stay flat, do not timer-interrupt** — **second Speak** while listening is the reliable cue. **Mouth returns to rest scale** when each cue finishes.
  - [x] **Second turn:** widget **stays open**. User selects Speak again (allowed even while clip 01 is still playing — **do not stop** clip 01 yet) → EQ returns → same begin-speak + silence / second-Speak cue → mic off / EQ hide → play **slot 02**. If clip 01 is **still playing when clip 02 is cued**, stop clip 01 then.
  - [x] **Exit beat:** during the **last 0.5 s** of clip 02 JSON playback, **lerp `NPC_Floor_Parent` scale → 0**. In parallel, agent widget **shrink-scale-lerps to 0%** over **0.5 s**. **Do not deallocate** the widget (keep Act I restartable).
  - [x] **Act I reset gesture:** **both palms down + dual ternary (ring) pinch** → restore NPC floor scale, agent widget pop-in scale, cold pose, and Speak turn index so the reel can restart without rebooting.
  - [x] **Host-only OK for 0.7:** extract Final Demo playlist player from hands-test capture controller; no new core ABI. Persist clips under **`persistentDataPath/3OS_FinalDemoPerf/`** (never `AndroidHandsPerf/`) so test-scene re-records cannot clobber Final Demo files.
- [x] **Task 7.5: Performance asset bind**
  - [x] Ship both JSON/audio pairs under stable Final Demo paths (Performances + Audio / StreamingAssets); final scene references by slot. On first run, copy JSON into `3OS_FinalDemoPerf/` **only if missing**.
  - [x] NPC bind: head + hands (+ finger hinges) from JSON drive matching transforms; mouth amp from playing `AudioSource`; **force mouth rest** after each slot ends.

#### Acts II–III — Mind Palace then back to MR (~⅔, prior 0.7 scope unchanged)

- [ ] **Task 7.6: Third MindPalace node (`3OS_Demo3`)**
  - [ ] Seed / register a **3rd** MindPalace root (Quest `Documents/3OS_Demo3` or project stand-in) so the Palace demo has three nodes without new layout math.
  - [ ] Quest fixture: dual cut-paste of two directories into a destination Box remains the primary Palace beat (existing scissors + bump).
- [x] **Task 7.7: VR ↔ MR toggle gesture + skybox wipe**
  - [x] **Delivery target / done when:** on device (or Editor MR simulation), the open-book + thumb-close flow can **round-trip** — **MR → VR (Palace)** and **VR → MR (office)** and **back again** — with matching glyph swap + skybox wipe each way. That reversible toggle is the confirmation test that 7.7 is complete (not a one-shot enter/exit).
  - [x] **0.7 scope for 7.7:** wipe + **host mode flag** + toggle glyph only. Do **not** require undeferring Mind Palace FS roots / glyphs (7.6) for this task to pass. *(Palace FS contents re-enabled under **7.8**.)*
  - [x] Filmed path still uses the same gesture after Act I to **enter** Mind Palace (**MR → VR**), then later **return** to the office (**VR → MR**); film may only show one enter/return, but implementation must support repeated toggling.
  - [x] **Open-book gesture:** both palms toward the user’s eyes, fingers unsplayed, palms aligned like an open book, thumbs out L/R. On register (palms + thumbs in starting position), spawn the beveled toggle glyph between the hands showing the **current** mode: **"MR"** while in MR, **"VR"** while in the Palace.
  - [x] **Toggle glyphs:** author beveled **extruded text** `MR` / `VR` in Blender → `.3glyph` (rare text-glyph exception; most glyphs stay symbol meshes). Packs: `mr.3glyph` / `vr.3glyph`.
  - [x] **Cancel before confirm:** if open-book breaks **before** the thumbs trigger fires, **dismiss the glyph** and do **not** change mode / start wipe. *(Also cancel if open-book base breaks mid-arm; thumb-closing motion gate removed.)*
  - [x] **Thumb close (confirm):** thumbs against palm blade (threshold distance to base index knuckle). **Wipe begins on this edge.** Glyph swaps to the destination mode label **at wipe start**. Dual-thumb close uses **enter/exit hysteresis latch** so a held close cannot re-fire every frame (must open past exit before the next pulse). After a pulse, open-book may stay armed for an immediate toggle-back once thumbs re-open and close again.
  - [x] **MR → Palace:** glyph **"MR" → "VR"**; skybox wipe **in reverse** of the Palace→MR blast (inward / reverse cylindrical wipe with feathered edge) so the Mind Palace skybox **closes in** over the office.
  - [x] **Palace → MR:** glyph **"VR" → "MR"**; skybox wipe plays **forward** (user-outward cylindrical blast → horizon → wink out) revealing the MR office again.
  - [x] **Wipe duration:** default **0.5 s** (tunable; may slow later).
  - [x] Bidirectional wipe + reverse glyph order required for UX; both directions appear in the filmed path.
  - [x] **Open-book geometry (host XR joints + HUD):** both palms face user (dot thresh); index/middle/ring dirs parallel to each other and orthogonal to palm normal; tip-cluster unsplayed (enter **and** continuous cancel); thumbs ortho to palm + finger axis (**enter only**, dropped once armed so thumbs can close). World HUD + logcat `[3OS][OpenBook]` gate flips.
  - [x] **Palace layering (baseline first):** Sky Overlay + `ZTest Always` draws **over everything** (confirmed). Keep-list (hands → cursors → HUD → glyphs) in front via Overlay queue. Settled Palace still voids MR `Office` root. WispySky Mat2 for cleaner seams.
  - [x] **Core ABI intents** (still present): `ModeToggleArmed` / `ModeTogglePulse`. Host open-book is authoritative for Quest arm/pulse; core OR'd when available. Pulse = latched dual-thumb-close edge. Host drives glyph + wipe + mode flag.
- [x] **Task 7.8: Palace contents + dual-anchor Box (Palace vs MR desk)** *(was 7.9; old 7.8 office focus-anchors cancelled — NPC head only)*
  - [x] **Delivery target / done when:** after sky wipe into VR, Mind Palace **boxes + object glyphs** scale in (no pop); MR stand-in world-mesh **colliders do not block** Palace motion; on wipe back to MR, non-selected Palace content scales out and deallocates while a **current selection persists into MR** (demo: select a 3D model file in Palace → return to MR with that file still present for later open / Blender spawn in 7.9). **Device-verified** (scale-in, colliders, MR persist).
  - [x] **Context:** Palace FS roots / box+object instantiation were **deferred / cancelled in the Final Demo scene** while 7.7 focused on the skybox. Re-enabled under this task (not during the wipe).

  ##### 7.8.a MR world-mesh colliders — MR only
  - [x] Stand-in office **wall / floor / ceiling colliders** (world-mesh substitutes) must be **active in MR only**.
  - [x] On enter VR / Palace (wipe **start**): **disable** WorldMesh colliders + stop enclosure publish so they cannot stop a voodoo volume’s motion in the Mind Palace (`ThreeOSVoodooController.SetWorldMeshCollisionActive`).
  - [x] On return to MR (wipe **start**): **re-enable** them.
  - [x] Skybox overlay already occludes office **geo**; this task is collision-only (do not rely on visual void for physics).

  ##### 7.8.b Introduce Palace boxes + objects (after wipe)
  - [x] Do **not** instantiate Palace field contents during the sky wipe. Wait until the **skybox transition is complete** (`WipeSettled` → Palace).
  - [x] Then instantiate (or re-enable) all Mind Palace **boxes and object glyphs** via undefer (`TryRevealPalaceAtSpawn` / soft-restore).
  - [x] **World origin:** lay out using **`Player_Spawn_Point` position + yaw** (`PoseFromSpawnTransform`) — **not** HMD / `_layoutHeadPose` for Final Demo reveal.
  - [x] **Cluster = one MindPalace node** (entire field matrix for that palace, e.g. `3OS_Demo` / `3OS_Demo2`) — not per-glyph or per-open-box.
  - [x] **No pop-in:** each MindPalace **cluster** starts at **~0% scale** and lerp-scales to **100%** over **0.5 s** (hide pending → centroid pivots).
  - [x] Scale pivot = that cluster’s **centroid** (transient pivot; unparent on complete).
  - [x] **All clusters animate simultaneously** (same 0.5 s window right after wipe end).
  - [x] Clean SmoothStep 0→100% lerp (ExpandedBox-style pivot).

  ##### 7.8.c Dismiss Palace contents (wipe back to MR) + selection persist
  - [x] On gesture **VR → MR**, run dismiss **in parallel with the sky wipe** (`WipeStarted`): **scale each non-selected MindPalace cluster down to 0%**, then soft-dismiss (`StorageSetInField(false)` + hide views).
  - [x] **Exception — current selection:** must **not** scale down or dealloc. It **stays at its current world pose** and **re-anchors to MR**.
  - [x] **MR re-anchor (demo minimum):** on VR→MR, selected object **detaches** (`SetMindPalaceRoot(0)`), records original mp id, anim-locks world pose; logical MR anchor = **`Player_Spawn_Point`**. Full multi-anchor → parking lot.
  - [x] **Return to Palace (MR→VR) with persisted selection:** re-attach original MindPalace id; **keep world pose** at MR location (anim lock; do not snap into field matrix).
  - [x] Demo beat path wired for select → MR persist → later **7.9**.
  - [x] Soft-dismiss avoids `StorageClear` so selection entity survives.

  ##### 7.8.d Dual-anchor Box (Palace vs MR desk) — demo slice
  - [x] **0.7 demo:** MR anchor for the **persisted selection** via `Player_Spawn_Point` (7.8.c) bridges toward 7.9; full multi-anchor storage not required.
  - [x] Full dual-anchor Box desk path + “open persisted file in MR → Blender spawn” are **out of 7.8 scope** (parking lot multi-anchor + **Task 7.9**).
- [ ] **Task 7.9: App surface — Blender beside Unreal** *(was 7.10)*
  - [ ] From the desk Box voodoo: pull the Blender source for an asset animating in the Unreal volume; dismiss that voodoo.
  - [ ] **Double-pinch the Blender model file** → immediately spawn a **small spinning loading glyph** **above and slightly forward** of the **double-pinch hand** (that hand is the one that must later palm-drop the instance). No floor spawn yet.
  - [ ] **Palm-drop arming (line-renderer start):** while the loading glyph is up, begin the floor perimeter line-renderer animation only when **both** hold:
    - **Fingers sufficiently splayed** (spread threshold vs closed fist / pinch), and
    - **Palm aiming toward the floor** (palm-normal · world-down / floor-up **dot-product threshold**).
  - [ ] **Cancel / reset:** if palm leaves the toward-floor dot threshold **or** fingers close up past the splay threshold mid-trace, **cancel and reset** the line-renderer animation (no partial commit). Loading glyph may remain until a clean arm succeeds or the user cancels the hand flow.
  - [ ] **Localization:** place the imaginary base plane where the palm points; center = **average of eye-gaze floor hit + palm-normal floor hit** (rare surface-control case) when both are valid.
  - [ ] **Line renderer (loading perimeter):**
    - Traces the **edge of an imaginary base plane** matching the **in-scene `Blender_Badge` base-plane size** (same footprint as the staged badge mesh).
    - Material/texture with a **basic feathered opacity edge** (glowing laser look).
    - Color = **agent Multi-Choice widget blue** for now (`~0.45, 0.78, 0.95`).
    - On full perimeter complete → proceed to mesh reveal sequence (below).
  - [ ] **Staged hierarchy (`Blender Group` parent):** all objects that become “the Blender instance” live under this parent. Keep them inactive / zero-scale / shader-hidden until their step fires.
  - [ ] **Reveal sequence (strict order):**
    1. **`Blender_Badge`** is the **first** actual mesh to become visible. After the line renderer finishes tracing, instantiate / activate it at **0% scale** in the **center** of the traced square. **Lerp yaw** while **lerp-scaling** until it lands at **exactly the dimensions drawn by the line renderer** (footprint match).
    2. Immediately after badge settle: all four **`Blender_Logo`** children under `Blender Group` **simultaneously** lerp-scale **0% → 100%** with a **~10% overshoot** then settle.
    3. Then meshes under **`DemonA`**: **shader-wipe reveal toe→head / bottom→top**, **plasma-esque feathered wipe edge**, default duration **0.5 s** (quick).
  - [ ] After the sequence: Blender instance is anchored on the floor beside the Unreal stand-in (same t-posed character as in the Unreal volume; side-by-side apps in one frustum). Stand-in apps OK for the reel.
  - [ ] Floor Unreal stand-in: invisible rectangular volume (normal far-BB aspect rules) + Unreal logo on floor — **no** focus-anchor requirement; **no real Unreal runtime**.
- [x] **Task 7.10: Monitor voodoo — spectator drop + STREAM** *(was 7.11)* — **86’d / cancelled for 0.7**
  - [x] Dropped from the filmed Final Demo path (no monitor voodoo chrome, spectator drop, or STREAM beat in 0.7). Revisit later only if a post-0.7 reel needs it; do not block Acts II–III gate.

**Quest / film gate (0.7):** Scripted walkthrough on device (or Editor MR simulation): **MR open** → GitHub plane toggle → NPC head widget → Speak/EQ → clip 01 → Speak/EQ → clip 02 + floor shrink → **Palace wipe** → Palace clusters **scale in** (MR colliders off) → FS cut-paste / **select 3D model** → **MR wipe** + non-selected clusters **scale out** (selection **persists**) → desk Box / open persisted file → **Blender floor spawn** (double-pinch loading glyph → splay+palm-down laser perimeter → `Blender_Badge` → logos → `DemonA` wipe) → (optional) GitHub plane again. **No 7.10 STREAM.** Unreal/Blender may be stand-ins; **NPC-head focus anchor**, wipe, Palace content intro/dismiss, selection carry into MR, NPC playlist, mic silence cue, and Blender spawn FX must be real.

---

### ⧉ Phase 0.8: Multi-Select (v0.8.0)
**Goal:** 3D equivalents of PC **Ctrl** / **Shift** multi-selection, including selection that crosses **voodoo volume** boundaries, plus moving objects through **multiple** open voodoo volumes.

**Scope note:** Task list intentionally thin — flesh out after 0.7 final demo. Builds on 0.5 pinch parity and 0.6 portal/ghost rules. Not required for the demo reel.

- [ ] **Task 8.1: Multi-select gestures & ABI**
  - [ ] Define additive / range-style multi-select intents (Ctrl/Shift analogues) for hands and controllers.
  - [ ] Selection sets that span near-field ghosts and far-field reals without double-counting.
- [ ] **Task 8.2: Cross-volume multi-select**
  - [ ] Multi-select across boundaries of one or more open voodoo volumes; clear rules for ghost vs real identity.
- [ ] **Task 8.3: Multi-volume object moves**
  - [ ] Move selected objects through multiple voodoo volumes in one interaction sequence (ordering, scale, centroid updates).

**Headless / Quest gate (0.8):** Multi-select and multi-volume moves demonstrable on Quest with harness coverage for set membership across portal boundaries.

---

### 🗂️ Phase 0.9: Layouts, Anchors & Selection Frustum (v0.9.0)
**Goal:** Cover README workspace features deferred from the original 0.5 slot — toggleable layouts, anchor subordination, and dual-hand selection frustum. *(Global Box expand without dismissing parents remains seeded in 0.4; this phase owns the explicit global/local mode toggle. Was 0.8 — bumped after final demo 0.7 / multi-select 0.8.)*

- [ ] **Task 9.1: Procedural volumetric layouts**
  - [ ] **Matrix View** — packed 3D cubic grids as a **toggleable layout mode** (demo cubic matrix already seeded in 0.4 `layout_demo_field`; 0.9 owns mode switching + persistence).
  - [ ] **Cluster View** — metadata/semantic groupings.
  - [ ] **Real Anchor View** — snap back to world surfaces / custom origins.
- [ ] **Task 9.2: Layered coordinate subordination**
  - [ ] Toggle **Egocentric** (user-locked), **Allocentric** (world-locked), **Object-locked** (parent asset) origins.
- [ ] **Task 9.3: Selection frustum**
  - [ ] Dual-pinch / dual-hand scaled bounding frustum inside a double-proxy; rigid group transforms preserve relative offsets. *(Builds on 0.5 hand/controller pinch parity; multi-select sets from 0.8.)*
- [ ] **Task 9.4: Global vs local box drill-down**
  - [ ] Global mode (default): open Box without dismissing parent contents. *(Seeded in 0.4 Task 4.8 / Unity U4.8 — 0.9 owns an explicit mode toggle.)*
  - [ ] Local mode: dismiss parent level when opening a Box.

---

### 🧱 Phase 1.0: Full Enclosure, Shader Adapters & AI Hooks (v1.0.0)
**Goal:** Complete boundary policy; keep plane math in C++; treat shaders as **per-engine adapters**. Optional AI as typed commands, not speech hashes. *(Was 0.9 — bumped after final demo / layouts.)*

- [ ] **Task 10.1: Enclosure engine (`src/enclosure.cpp`)**
  - [ ] Strict floor policy (already seeded in 0.3) plus wall/ceiling plane constraints.
  - [ ] Rigidbody matrix default `Disabled`; optional per-object gravity opt-in.
  - [ ] Semantic wall masking: core supplies plane equations; hosts clip meshes.
- [ ] **Task 10.2: Shader adapters (not core-only HLSL)**
  - [ ] Reference plane → clip interface in core.
  - [ ] Ship example `SemanticWallMask.hlsl` for D3D/Unity-style hosts; document that Vulkan/GL/Unreal use equivalent adapters.
- [ ] **Task 10.3: Agentic service layer (`src/ai/`)**
  - [ ] Accept **typed intent/commands** (string or enum) from the host OEM assistant via ABI.
  - [ ] Map commands onto facade actions (e.g. matrix layout, portal open) — no in-kernel “speech hashing.”

---

### 🚀 Phase 1.1: Native OpenXR Realization (v1.1.0)
**Goal:** Run the kernel against real OpenXR handles, with a concrete acceptance checklist (not a single vague “certification” checkbox). *(Was 1.0 — bumped after enclosure.)*

- [ ] **Task 11.1: OpenXR lifecycle (`src/main.cpp` or dedicated runtime entry)**
  - [ ] Replace mock telemetry with OpenXR session/instance/system handles.
  - [ ] Request `XR_EXT_hand_tracking` (and `XR_EXT_eye_gaze_interaction` when available).
  - [ ] Establish `XR_REFERENCE_SPACE_TYPE_STAGE` (allocentric) and `XR_REFERENCE_SPACE_TYPE_VIEW` (egocentric).
- [ ] **Task 11.2: Multi-modal text router (`src/text/`) — host-injected**
  - [ ] Priority router in `text_io_router.cpp` that accepts strings from a **host-supplied callback / ABI entry** (keyboard, IME, engine UI).
  - [ ] Do **not** embed Bluetooth/WiFi keyboard sockets in core; leave transport to the host OS or Unity/Unreal layer.
  - [ ] Hooks for volumetric **Lexting** (3D hand-in-voxel → text field) as a later input source on the same router.
  - [ ] `text_label.cpp` CRUD for Box/Object annotations (primary + secondary count strings).
- [ ] **Task 11.3: Acceptance checklist**
  - [ ] Snap / free-range voodoo volumes with near-field ghosts (from 0.6).
  - [ ] Final demo VR↔MR path (from 0.7) or waived stand-in.
  - [ ] Multi-select across voodoo boundaries (from 0.8).
  - [ ] Hands **and** controllers complete the demo path (from 0.5).
  - [ ] Velocity glide with friction settle; floor clamp holds.
  - [ ] Wall masking does not clip through partition planes incorrectly (from 1.0).
  - [ ] Storage glyphs + rename via host text injection.
  - [ ] Layout mode toggle and at least one anchor-mode switch work in-session (from 0.9).

---

### 🏢 Phase 2.0: Collaborative Room Workspaces (v2.0.0)
**Goal:** Multi-user 3OS on **on-prem VDI** (office GPU farms) and **home/edge compute** (IoT-class servers), interfacing every 2D screen in a room — wall-mounted or handheld — as a first-class, possessable display in shared volumetric workspaces. Phase **1.1** remains the single-user / native-OpenXR product bar; 2.0 adds session, presence, display, and media-sync systems.

**Design write-up:** [`docs/COLLABORATIVE_ROOMS.md`](docs/COLLABORATIVE_ROOMS.md) (room fixture, flyweight/copy, VCS, co-editor VOIP/avatars, site-to-site opt-in).

**Reference office room (design target)**  
Optimal layout: **20 ft × 20 ft**. Three walls are workstation walls; the fourth has the entry door and a **guest couch**. Room center is empty with a large carpet — everyone’s primary volumetric workspace.

| Element | Spec |
| --- | --- |
| Workstation walls | 3 walls × **4 stations** each (**12 workers**); each station **50" wide** |
| Monitors / wall | **8× 24"** displays on a single wall rail; ball joint for quick portrait ↔ landscape; small padding for rotation; **IMU on each monitor back** (pose → server) |
| Console | **48" wide**; two drawers **22" wide × 10" deep**; feet raise console **4"** off floor |
| Desk beam dock | Square groove **3.0625"** wide at console center for a **3" square-tube** standing-desk beam; pogo-pin power/data + spring latch; press-release within reach on entry |
| Standing desk | Off-the-shelf unit with power/data routing; surface **30" × 16"** (keyboard, mouse, flush USB/power only). Worker unlatches, pulls out, spins to face room center for volumetric work; can latch back to wall for legacy dual-monitor work |
| Spectator / guest screens | Couch wall **and** opposite wall: **two large 4K screens each**, in addition to workstation monitors |
| Room cameras | **Four** corner cameras aimed at center; each captures virtual shared-space content and, by default, drives monitors on the **opposite** side of the room as spectator views |
| Compute | On-prem **VDI** with GPU headroom for **12 simultaneous** workers (enterprise); home deployments use edge/IoT-class servers with the same session/display contracts at smaller scale |

**Arrival flow (office):** Latch-release desk → face carpet → launch eyewear app → log into **3OS enterprise** on the VDI → land in **personal workspace** first; join or create **shared** workspaces (pairs, 3–4+, or **full-room**) as invited.

- [ ] **Task 12.1: Multi-user session & workspace model**
  - [ ] Session identity: authenticated user, device credentials, room/tenant binding (enterprise VDI vs home edge).
  - [ ] Workspace kinds: **personal** (default on login), **shared** (any invite size — pairs, trios, larger), **full-room** (everyone present; owns room-wide spectator routing).
  - [ ] Invite / join / leave ABI; presence list per workspace; host is not the source of truth for ACLs (server/session service is).
  - [ ] Solo-in-shared: other invitees’ volumetric content **masked** in eyewear until explicitly unmasked or co-possessed — personal focus without leaving the shared session.
- [ ] **Task 12.2: Display registry (every 2D screen in the room)**
  - [ ] First-class **Display** entities: wall monitors, large 4K guest screens, handheld tablets/phones once logged in.
  - [ ] Pose pipeline: per-display **IMU** (and/or camera-assisted localization for handhelds) fused into room coordinates; portrait/landscape from mount/IMU.
  - [ ] Static vs handheld lifecycle; hot-plug / battery / tracking-lost soft-fail without dropping the session.
  - [ ] Reference-room catalog helpers (optional): 3×8 workstation monitors + 4× 4K guest screens + N handhelds — parameterization, not a hardcode-only layout.
- [ ] **Task 12.3: Spectator cameras & default monitor routing**
  - [ ] Four corner camera frustums as room-aligned virtual observers of the shared volumetric field.
  - [ ] Default binding: each camera feeds spectator views to monitors on the **opposite** wall; full-room workspace can reassign routing.
  - [ ] Large 4K pair on couch wall / opposite wall prioritized for guests without eyewear.
  - [ ] Compositor contract: server (VDI/edge) renders spectator frames; eyewear clients do not each re-render all wall feeds.
- [ ] **Task 12.4: Snap-to-monitor & shared monitor possession**
  - [ ] Extend standard 3OS transforms: snap any volumetric content onto any registered display → that display leaves default spectator mode and shows an **object-focused** view (3D asset render, video playback, image, text surface, etc. by content type).
  - [ ] Every monitor is **possessable** by any workspace member; possession is shared — any other invitee may interact or **release** possession, restoring default spectator view.
  - [ ] Legacy workstation mode: user faces wall, latches desk into console, possesses the **two monitors above their station**; those displays behave as a conventional dual-monitor desktop until released.
- [ ] **Task 12.5: Logged-in device localization & pull-in**
  - [ ] Cameras + session creds **semantically track** issued devices (e.g. tablet) in room space while logged in.
  - [ ] Gesture/ABI to **pull** that device’s live surface into the shared volumetric workspace (real-time mirror), using localized pose for placement.
  - [ ] Same localization authority used for wall displays (IMU → server) so handheld and bolted screens share one coordinate frame.
- [ ] **Task 12.6: Asset references (flyweights) vs asset copies**
  - [ ] First-class open binding: each workspace open of an asset is either a **copy** (independent instance) or a **reference** (flyweight to one server-RAM object). Same filename / same path must not imply a mode by accident.
  - [ ] Flyweight: attribute edits and **object** transforms propagate immediately to every workspace holding that reference, even across parallel workspaces with different view angles. **No merge** — one singleton in RAM; durable save/commit of that singleton is ordinary persistence, not a collaboration merge path. Teams may choose flyweight specifically to avoid diff/merge (especially same-room).
  - [ ] Copy: edits and object transforms stay local to that instance; no cross-workspace mutation; reconciliation is VCS (12.6b).
  - [ ] **Locality gate (default):** allow flyweight inside a low-latency co-residency domain (**room**; **building/campus** on the same on-prem fabric). By default refuse or force **copy** for cross-region / high-RTT peers.
  - [ ] **Optional site-to-site flyweight:** tenant opt-in when they provision an **MMO-class** (expensive) sync fabric between sites; then cross-site opens may bind as flyweight under the same no-merge + co-editor presence rules. Document cost/SLA; never imply WAN flyweight is free or default.
  - [ ] **Co-editor presence (required on flyweight open/join):** expose who else is live on that singleton (user, room/station if known, workspace). Invisible co-editors are not allowed; this is social coordination, not a VCS merge.
  - [ ] **Roster actions:** in-3OS **VOIP conference** with selected co-editors (esp. other rooms on campus); **bidirectional avatar fly-in** — place remote co-editor avatars in the local room field and local avatars in theirs so deixis/gaze align with live flyweight edits.
  - [ ] Default staging: place document origin / primary content at **room center** when opening into a workspace (overridable); placement is not the same as copy-vs-reference choice.
  - [ ] Headless: two workspaces open one flyweight — object spin in A visible in B; camera flip in A leaves B’s object upright; presence list includes both. Two copies — object spin in A leaves B upright. Cross-domain open request cannot bind as flyweight.
- [ ] **Task 12.6b: VCS integration for simultaneous copies**
  - [ ] Treat concurrent copies of the same logical asset as **divergent lineages** (branch-like): source revision identity, per-copy dirty state, commit/check-in, merge/rebase to trunk, conflict surfacing (including binary/DCC payloads). Required whenever flyweight is disallowed (cross-site) or teams choose isolation.
  - [ ] Session/ABI hooks for VCS actions as volumetric intents (checkout/open-as-copy, promote, get-latest, resolve conflict) — plumb Git/Perforce/Plastic/etc.; do not embed a full VCS engine in core.
  - [ ] Optional lock/lease policy when merge would be unsafe. True flyweight sessions have **no** merge UX for live edits.
  - [ ] Headless/fixture: two copy opens diverge → merge path reports conflict or clean promote; same-domain flyweight path has no merge step for live object spin.
- [ ] **Task 12.7: View transform vs object transform**
  - [ ] Separate ABI/state: **view / camera** (per user or per workspace view rig) vs **object anchor** (coordinates / orientation relative to document or room origin).
  - [ ] Camera/view moves never write object attributes; object transforms never silently rewrite another user’s camera.
  - [ ] Spectator and snap-to-monitor focused views may follow a workspace camera into a flyweight without baking that orbit into the asset transform.
  - [ ] Host DCC ports (e.g. Maya-in-3OS thought experiment): scene edits = object/document; orbit/tumble/“flip upside down for looking” = view unless the user explicitly transforms the asset.
- [ ] **Task 12.8: Real-time media sync**
  - [ ] Sync text, images, video, and 3D assets across eyewear clients and possessed/spectator displays with session-scoped consistency (who sees what under mask/invite/copy-vs-reference rules).
  - [ ] Bandwidth/GPU policy hooks for 12-user VDI vs smaller edge nodes (quality ladders, spectator vs personal LOD; flyweight sharing vs duplicated instances) — policy in server/host; kernel exposes intent, not vendor codecs.
- [ ] **Task 12.9: Deployment contracts (VDI + edge)**
  - [ ] Document split: **session/display/compositor** on on-prem VDI or home edge server; **eyewear/host** as thin interactive clients.
  - [ ] Headless tests: workspace invite graph; display pose update; snap → focused view → release → spectator restore; mask rules for solo-in-shared; flyweight vs copy + view vs object cases from 12.6/11.7; copy-divergence → VCS promote/merge hook from 12.6b.
  - [ ] No requirement that core embed a full VDI stack — define ABI + reference server responsibilities; implementations may be separate services.

**Gate (2.0):** Reference-room (or scaled fixture) demo — 2+ authenticated users in a shared workspace; wall monitors show opposite-camera spectator views; snap content to a monitor → focused view; second user releases possession → spectator restored; logged-in handheld localized and pull-in works; masked solo presence hides peer content until unmasked; parallel workspaces prove **flyweight** object edits cross sessions while **camera** flips do not, and **copies** isolate object edits with a **VCS promote/merge** path (not silent overwrite). Home-edge path exercises the same contracts at smaller display counts.

---

### 📝 Parking lot (ideas without tasks yet)

Capture design notes and UX instincts that are not ready for checklist tasks. No sequencing — promote into a real phase (including 2.x) when fleshed out. Mirror host-facing notes in Unity’s parking-lot section when relevant.

**Notes**

- *(Selection-gated telekinesis promoted into Phase 0.4 Task 4.6.)*
- [ ] **Glyph front-face convention:** authored `.3glyph` meshes with a clear front should orient that face along **local +Z** (layout forward / toward the user after host pose apply). Hosts should not need per-pack roll/yaw corrections (e.g. `blender_mark` currently needs roll 90° then yaw 90°). Document in `THREEOS_glyph` schema + authoring notes (`inject_3glyph` / Blender export).
- [ ] **Glyph pack GUI (artist tool):** Simple desktop/Editor UI to generate `.3glyph` files (pick GLB, edit `THREEOS_glyph` fields, inject metadata) for artists who do not want to run `tools/inject_3glyph.py` from the CLI. Wraps the existing injector; does not embed host shaders in the pack.
- [ ] **Global field spacing + glyph scale:** Runtime `threeos_storage_get/set_field_spacing(float meters)` (clamped); Unity inspector/settings drive both kernel spacing and host `GlyphSizeM` (keep ~2.2× ratio or one master “field cell size”); optionally re-layout in-field matrices and rescale glyph roots so the whole field can be resized without hunting magic numbers. Kernel already shares `Workspace::kDefaultFieldSpacingM` / `field_spacing_m_` for demo + box-expand layout.
- [ ] **Matrix padding / margins:** Tunable cell padding and snap-margin (independent of spacing) so dense stacks are comfortable without relying only on sticky-snap; expose alongside field-spacing controls in a later milestone.
- [ ] **Parent-Box collapse (shelved from 0.6):** X-close on the **parent Box ghost** collapses voodoo contents into that Box (except objects dragged in from the near field); destroy content ghosts; leave parent ghost at the base ready to reopen.
- [ ] **0.6 headless + Quest gate (shelved):** Formal harness/Quest smoke for snap/free-range, centroids, gravity, aspect lock, summon/temp centroid, deadzone arming, scissors, bump-lerp, MindPalace picker (6.7) — promote when closing the phase.
- *(Final demo / MR NPC+GitHub intro / 3rd MindPalace / Palace↔MR office reel **promoted to Phase 0.7**.)*
- *(0.7 Task **7.10** monitor voodoo / STREAM **86’d** from the filmed path — see Phase 0.7 checklist.)*
- [ ] **Remote / cloud / hypervisor drives (unscheduled):** Beyond Quest `Documents/…`, ingest wireless/LAN volumes and cloud providers (Google Drive, OneDrive, iCloud) as Multi-Choice MindPalace options when they come online. Includes understanding how Proxmox / ESXi (and similar) expose drives to the client so the same picker UI can list them — no phase number yet (likely folds into VDI / enterprise work later).
- [ ] **Multi-anchor objects/boxes (unscheduled; seeded by 7.8.c demo):** Any object or Box in Mind Palace should carry **multiple spatial anchors** — **one per MindPalace field** it has occupied, **plus one MR / world-mesh desk anchor**. Mode toggles switch which anchor relationship is active without destroying the others; world pose may stay put while logical parent/centroid relinks (see 7.8.c persist). **0.7 only needs the MR anchor** on the carried selection (`Player_Spawn_Point`). Flesh out ABI + host storage (pose slots, field ids, promote/demote, Box dual-anchor desk path) in a later milestone — not required to close 7.8.
- [ ] **Parking lot — near-BB box → far-BB parent transition (post-0.7):** Double-pinch a box inside a near BB to **transition the current far BB** onto that box as the new parent (bottom of far BB), resizing the far BB to encapsulate its child contents. 0.7 ships a simpler interim: expand grows/refits the active portal far BB (+ lock-scaled near BB) in place without re-parenting the portal.

---

### 🎮 Parallel track: 3OS_Unity (see that repo’s plan)

Do not duplicate the full Unity checklist here. Use **[3OS_Unity/IMPLEMENTATION_PLAN.md](https://github.com/scifiuiguy/3OS_Unity/blob/main/IMPLEMENTATION_PLAN.md)** for UPM tasks and Quest gates. Summary:

| After core… | Unity must… |
| --- | --- |
| **0.1** ABI freeze | Thin harness: bridge, input router, plugin load/ping on Quest |
| **0.2–0.4** | Proxies (functional), telekinesis, storage glyphs / FS cut |
| **0.5** | Hands + controllers parity on device |
| **0.6** | Snap / free-range voodoo volumes (BB, centroids, near ghosts) |
| **0.7** | Final demo opens in MR: GitHub plane, NPC head focus-anchor → agent widget + Speak/EQ → clips 01/02 + floor shrink; Palace wipe (7.7 done) → Palace content scale-in / collider gate / selection persist (7.8) ↔ MR desk; Blender floor spawn FX (no monitor STREAM — 7.10 cancelled) |
| **0.8** | Multi-select + multi-volume moves |
| **0.9** | Layouts / frustum / anchors |
| **1.0** | Enclosure mask + shader adapters + AI command hooks |
| **1.1** | UPM polish + native OpenXR parity / acceptance |
| **2.0** | Enterprise/edge collaborative host: room displays, spectator cams, shared workspaces |

Unreal (if pursued) can mirror the same paired-version pattern against the same ABI.
