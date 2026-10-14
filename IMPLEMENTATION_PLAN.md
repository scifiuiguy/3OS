# 🗺️ 3OS_Core: Implementation Roadmap & Versioning Lifecycle

This roadmap moves **3OS_Core** from an empty repo to a production-grade, hardware-native volumetric OS module. Phases are dependency-ordered. Contiguous semver tracks real deliverables (no fake version gaps).

```text
Core  0.0 → 0.1 → … → 0.5 (hands) → 0.6 (voodoo volumes) → 0.7 (multi-select) → 0.8 → 0.9 → 1.0 → 2.0 (collab room / VDI + edge)
                │                 │                           │                      │     │     │     │
Unity 0.0 → 0.1 → … → 0.5 (hands) → 0.6 (voodoo volumes) → 0.7 (multi-select) → 0.8 → 0.9 → 1.0 → 2.0 (enterprise host + room displays)
```

### Companion repo (parallel track)

**[3OS_Unity](https://github.com/scifiuiguy/3OS_Unity)** is a **separate repository** walked **in lockstep** with these phases. Full paired milestones, Quest gates, and Unity tasks live in that repo’s [`IMPLEMENTATION_PLAN.md`](https://github.com/scifiuiguy/3OS_Unity/blob/main/IMPLEMENTATION_PLAN.md) (local checkout: `3OS_Unity/IMPLEMENTATION_PLAN.md`, gitignored here).

- **Core** proves correctness with headless C++ tests (and later its own native OpenXR path at 1.0).
- **Unity** is the Quest 2 / Editor verification host from **0.1 onward** (thin harness on the frozen ABI) — not deferred until core 1.0.
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
**Goal:** World Proxy Half-Dome, Double-Proxy Voxel Lattice, and Voodoo portal teleportation. Prove with **headless tests** here; prove on Quest via the paired **Unity 0.2** gate. *(Product voodoo volumes — far/near bounding boxes, ghosts, snap centroids — are Phase 0.6; Half-Dome stays imaginary / unrendered.)*

- [x] **Task 2.1: Multiscale coordinate workspace (`src/topology.cpp`)**
  - [x] Voxel indexing for discrete ~3-inch spatial blocks (`std::array<int, 3>` or equivalent).
  - [x] Scale-down remapping into a near-field **World Proxy Half-Dome**. Keep scale **configurable** (do not hardcode `1:100` as the only ratio).
  - [x] Matrix remapping that projects a far-field cluster into a near-field **Double-Proxy Voxel Lattice**.
- [x] **Task 2.2: Voodoo portal teleportation**
  - [x] Near ↔ far drag through an open double-proxy (Poros-style non-Euclidean remapping).
  - [x] Cross-proxy drag when multiple double-proxies are open (A → B teleport).
  - [x] World Proxy Half-Dome as global drop target for far-field placement.
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

**Demo field layout (0.4):** `threeos_storage_layout_demo` places all in-field roots in a **cubic matrix** 1 m in front of the head (side = ceil(cbrt(n)); 1–8 → 2³, 9–27 → 3³, …). Layout uses **yaw-only** heading with **world +Y up** so head roll/pitch at start does not tilt the field off the floor. Full Matrix/Cluster/Real-Anchor *mode toggles* remain Phase 0.8.

Authored glyph sources (Blender → GLB → `.3glyph` with `THREEOS_glyph`): glTF-logo-like pack for `.glb`/`.gltf`; **`box.glb`** as the default closed Box glyph for all Boxes; **`box_open.glb`** as the open-state Box glyph (toggleable).

**Rendering:** Object and Box **glyphs** (including demo `gltf_mark`, `box`, `box_open`) must draw **semi-transparent** (see-through prismatic icons), not opaque solids. Hosts use a transparent/alpha material path (URP Transparent or equivalent). Pack `tint_rgba.a` and mesh alpha should stay below 1 (target ~0.45–0.65 unless tuned). Content payloads like `simple.glb` are unaffected unless they are also shown as glyphs.

**Drop-into-Box interaction (kernel logic + host viz)**  
While dragging an Object **or Box** near another in-field Box within snap distance (**0.2 m**): snap to a hold pose just above the target Box; host shows an insert-arrow affordance. Leaving the threshold unsnaps. Releasing while snapped commits insert: emit storage events, run host anim (closed → open glyph, +10% scale bump and return, open → closed), hide the dragged glyph from the field, refresh Box count (`1 object` / `N objects`). Persist by **cutting (moving)** the file/folder into the target folder on disk (Quest Documents may require all-files access). Nested relative paths (`Parent/Child`) are preserved on cut; failed FS ack restores the prior `parent_box` (not always root).

**Snap ↔ stick telekinesis:** On snap enter, clear Object velocity to zero and **re-anchor `start_hand` to the player’s current hand pose** without requiring a grip release. While snapped, the kinematic escape pose + velocity keep integrating under the stick; display stays on the hold. On unsnap, lerp display from hold → escape pose and **keep the stick-built velocity** (do not re-anchor / zero). **Sticky snap:** once pending, ignore other Boxes until the escape pose leaves *that* Box’s radius (prevents hold-pose cascade up a dense vertical stack). **Catch-up suppress:** while the unsnap lerp is active, snap does not re-engage (neighbor above the hold cannot suck the object back mid-escape).

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

**Scope note:** Phase 0.2 topology/portal remaps remain available as math. The **Half-Dome is imaginary** — do **not** render it. Users see the far-field bounding box, yellow drag affordance, snap nodes (snap mode), and near-field ghost boxes. Multi-select across volumes and multi-volume object moves wait for **0.7**. Layouts/anchors → **0.8**; enclosure/shaders/AI → **0.9**.

**Input remaps (supersede 0.5 voodoo stub)**

| Mode | Binding | On activate |
| --- | --- | --- |
| **Snap voodoo** | Primary hold + palm **down** | Far-field BB + yellow drag arrow; snap-node cluster from open-box centroids |
| **Free-range voodoo** | Secondary hold + palm **down** | Same BB + yellow drag arrow; no snap-node cluster / gravity |
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
- [x] **Task 6.2: Centroids (sibling groups + god parent)**
  - [x] Persistent centroid per open Box’s child group (average of in-field children); update live each tick; **omit** empty sets.
  - [x] Nested rule: expanded Box in-scene → sibling children share a box centroid.
  - [x] Snap activation **samples current** centroid poses (registry rebuilt every tick — no freeze).
  - [x] **God parent** centroid: imaginary root (no glyph); average of that god’s top-level in-field children.
  - [x] Data model supports **multiple god nodes** (`god_root_id` on entries + `threeos_god_register`).
  - [x] **Quest snap-test Start layout:** dual gods from `3OS_Demo` + `3OS_Demo2` at **10 ft**, **±15°** azimuth, **2.5 ft** up; field centroid markers + 6.3 hand-space snap dots.
- [x] **Task 6.3: Snap-mode nodes + gravity** *(not in 6.1 — snap dots / gravity land here)*
  - [x] Around the yellow drag tip, render a cluster of spherical snap dots at **field-scaled** positions of every open-box centroid (plus god-root node) so the same gain that moves the far BB maps tip contact onto real objects.
  - [x] Tip gravity: when near multiple nodes, pull toward the **closest**; snap tip to that node and snap the far-field BB to the real centroid.
  - [x] On snap, interpolate BB size on all three axes to encapsulate that centroid’s children **without** breaking the parameterized aspect (`near_bb_voxels`).
- [x] **Task 6.4: Near-field ghosts + portal contents**
  - [x] **Near-BB place + bump-lerp first (before ghosts):** keep today’s navel-forward rest as the **seed** pose, then **orbit** the new near BB around the user until it aligns with the **creating hand’s** cursor (L→left bias, R→right bias) — that orbit-aligned pose is the instantiation target (does not warp the current navel offset math, only reassigns the rest target after seed). Existing near BBs bump-lerp clear so volumes never overlap (no cursor-in-two-portals Venn); if flanked on both sides, bump **both** directions and cascade through all open near BBs. Paths orbit the body; duration **under 0.25 s**. Capacity ≈ **12** near BBs; if a new open would not fit, **refuse** new voodoo volume creation until the user dismisses a near BB. Non-voodoo near objects can wait for 6.6.
  - [x] Near BB shell already opens on any intentional release in 6.1; here: ghost copies inside the near BB, scale-shrunk so far-field nominal size maps into the near box (when far has grown).
  - [x] **Ghost membership:** **Snap** → sibling group of the linked centroid (as today). **Free-range** → every **literal in-field object whose bounds are encapsulated by the parked far BB** gets a corresponding near ghost (not “empty until snapped”).
  - [x] Ghost of the **parent Box** at the near BB’s bottom-center node when the linked centroid is a **Box**; **skip** for god / free-range with no parent Box (no marker substitute).
  - [x] **Select on ghosts** works (highlight / selection state mirrors the far counterpart). **No ghost drag in 6.4** — motion waits for 6.5; summon can be validated later once select is in.
  - [x] Maintain two centroids while open: **persistent** (always tracks true sibling poses) and **temporary** (created at near-BB open; drives far↔near BB relationship; does **not** expand the far BB when objects are summoned into the near field).
  - [x] **Cursor tint:** when the point cursor enters a near BB, match the near wireframe blue; restore white on exit.
- [x] **Task 6.5: Ghost drag, boundary, summon, insert**
  - [x] **Drag while selected (palm ignored):** if the point cursor is **inside any near BB**, drag at **ghost scale** (hand Δ on the near proxy maps to far via `far/near` scale — smaller proxy → faster far travel) and show a **blue** drag arrow; if the cursor is **outside all near BBs**, drag the **far real** at normal scale (white/cyan stick as today). Motion edits apply **1:1 in real time** to the far counterpart.
  - [x] **Windows-style grab:** hold on an unselected glyph/ghost selects it and starts drag (palm-down empty still creates voodoo).
  - [x] **Lock park-time ratio:** when far BB refits, near BB extents grow/shrink by the same park-time `lock_scale` so ghosts do **not** shrink mid-drag; the resizing near BB is always the **bumper** (pose/yaw pinned) — siblings bump away + near-field clutter is pushed out of the grown footprint.
  - [x] Dragging a ghost does **not** remove it from the near BB by default; crossing a planar face constrains the ghost at the plane hit while velocity from vector length still drives the far real object; **both** centroids update as needed (persistent always; temp per portal rules).
  - [x] **Ghost/audition drag-drop:** snap-insert into in-field Boxes works during ghost drag (near pose for proximity); release commits cut like possess.
  - [x] Selected ghost + primary pinch on **empty space inside** near BB → deselect ghost and real.
  - [x] **Summon / cross-proxy:** selected ghost + primary pinch **outside its near BB in open near space** → fast-interpolate ghost + real to that world pinch (real enters near field). Selected + pinch **inside a different near BB** → real summons to the **far** pose mapped from that pinch in the target portal; ghost jumps to the pinch in that portal’s near space. Persistent centroid updates for the new far pose; **temp centroid does not** grow the far BB to swallow the near field.
  - [x] **Cross-proxy select-drop onto a Box:** when the destination of a cross-proxy summon would be a Box glyph, insert into that Box instead of lerping to empty space.
  - [x] Near-field real dragged **into** near BB: spawn far ghost at correct scale/offset during drag; real scales down to ghost differential; pull back out in same drag destroys far ghost and restores near scale (may cross boundary repeatedly). On release **inside** near BB: far ghost becomes real, near real becomes ghost at ghost scale (roles swap) — **ship in first pass**.
- [ ] **Task 6.6: Near-field object priority + bump-lerp**
  - [ ] Near BB instantiation takes priority over near-field clutter: existing **non-voodoo** near objects bump-lerp out of the way and must **not** be sucked into the new BB (near-BB↔near-BB clearance already in 6.4). *(Expand-time clutter push seeded in 6.5 lock-ratio refit.)*
  - [ ] Inserting a portal among cluttered near objects pushes neighbors left/right on body-orbit paths; duration **under 0.25 s**.
  - [ ] If an object’s orbital slot is blocked, push it just beyond reach into the far field to finish the shift without collision.
- [ ] **Task 6.7: Dismiss + collapse** *(X-close bumped ahead of 6.6 — ship dismiss now)*
  - [x] Each near BB has an **X-close** sphere: R-created → bottom-front-left; L-created → bottom-front-right; dismiss both far+near BBs, destroy ghosts (temp centroid cleared). *(Bump-lerp return of near-field clutter displaced for that portal waits on 6.6.)*
  - [ ] Activating X-close on the **parent Box ghost** collapses voodoo contents into that Box (except objects dragged in from the near field); destroy content ghosts; leave parent ghost at the base ready to reopen.
- [ ] **Task 6.8: Multi god-node spawn**
  - [ ] Instantiate contents from any number of local Documents folders (e.g. `3OS_Demo`, `3OS_Demo2`, …), each as a god node with collision avoidance at Start.
  - [ ] One node → current forward-of-user layout; multiple → lateral orbit around the user; if orbit is full, spill to farther concentric ground orbits out to `field_radius_m` (default 50 ft), then fill upward to `field_height_m` (default 30 ft); if still no room, HUD error naming the directory that could not instantiate.
  - [ ] **Snap-test fixture:** when ≥2 gods are configured for Quest demos, prefer the Task 6.2 dual layout (10 ft range, ±15° azimuth, 2.5 ft up) before falling back to generic orbit spill.
  - [ ] Keep ABI open for future god nodes on network shares (Google Drive / PC `F:` over Wi‑Fi) — not required to ship in 0.6.
- [ ] **Task 6.9: Headless + Quest gate**
  - [ ] Harness: snap vs free-range intents; centroid updates; snap gravity picks nearest; BB aspect lock; parameterized voxel/near-BB/field limits; near open creates temp vs persistent centroids; summon does not expand far BB via temp; deadzone arms only after leave/re-enter.
  - [ ] Quest: yellow arrow + far BB (no dome mesh); snap dots; near ghosts 1:1 with far; near-BB bump-lerp (no overlap) + cursor blue-in-portal; object bump-lerp under 0.25 s; X-close dismiss/collapse; multi-folder god spawn when configured.

**Headless / Quest gate (0.6):** Snap and free-range voodoo complete on device with centroids, yellow drag, non-overlapping near BBs + ghosts, cursor portal tint, and object bump-lerp hygiene. Multi-select across volumes is **out of scope** (0.7).

---

### ⧉ Phase 0.7: Multi-Select (v0.7.0)
**Goal:** 3D equivalents of PC **Ctrl** / **Shift** multi-selection, including selection that crosses **voodoo volume** boundaries, plus moving objects through **multiple** open voodoo volumes.

**Scope note:** Task list intentionally thin — flesh out after 0.6 ships. Builds on 0.5 pinch parity and 0.6 portal/ghost rules.

- [ ] **Task 7.1: Multi-select gestures & ABI**
  - [ ] Define additive / range-style multi-select intents (Ctrl/Shift analogues) for hands and controllers.
  - [ ] Selection sets that span near-field ghosts and far-field reals without double-counting.
- [ ] **Task 7.2: Cross-volume multi-select**
  - [ ] Multi-select across boundaries of one or more open voodoo volumes; clear rules for ghost vs real identity.
- [ ] **Task 7.3: Multi-volume object moves**
  - [ ] Move selected objects through multiple voodoo volumes in one interaction sequence (ordering, scale, centroid updates).

**Headless / Quest gate (0.7):** Multi-select and multi-volume moves demonstrable on Quest with harness coverage for set membership across portal boundaries.

---

### 🗂️ Phase 0.8: Layouts, Anchors & Selection Frustum (v0.8.0)
**Goal:** Cover README workspace features deferred from the original 0.5 slot — toggleable layouts, anchor subordination, and dual-hand selection frustum. *(Global Box expand without dismissing parents remains seeded in 0.4; this phase owns the explicit global/local mode toggle. Bumped from 0.7 to make room for multi-select.)*

- [ ] **Task 8.1: Procedural volumetric layouts**
  - [ ] **Matrix View** — packed 3D cubic grids as a **toggleable layout mode** (demo cubic matrix already seeded in 0.4 `layout_demo_field`; 0.8 owns mode switching + persistence).
  - [ ] **Cluster View** — metadata/semantic groupings.
  - [ ] **Real Anchor View** — snap back to world surfaces / custom origins.
- [ ] **Task 8.2: Layered coordinate subordination**
  - [ ] Toggle **Egocentric** (user-locked), **Allocentric** (world-locked), **Object-locked** (parent asset) origins.
- [ ] **Task 8.3: Selection frustum**
  - [ ] Dual-pinch / dual-hand scaled bounding frustum inside a double-proxy; rigid group transforms preserve relative offsets. *(Builds on 0.5 hand/controller pinch parity; multi-select sets from 0.7.)*
- [ ] **Task 8.4: Global vs local box drill-down**
  - [ ] Global mode (default): open Box without dismissing parent contents. *(Seeded in 0.4 Task 4.8 / Unity U4.8 — 0.8 owns an explicit mode toggle.)*
  - [ ] Local mode: dismiss parent level when opening a Box.

---

### 🧱 Phase 0.9: Full Enclosure, Shader Adapters & AI Hooks (v0.9.0)
**Goal:** Complete boundary policy; keep plane math in C++; treat shaders as **per-engine adapters**. Optional AI as typed commands, not speech hashes. *(Bumped from 0.8.)*

- [ ] **Task 9.1: Enclosure engine (`src/enclosure.cpp`)**
  - [ ] Strict floor policy (already seeded in 0.3) plus wall/ceiling plane constraints.
  - [ ] Rigidbody matrix default `Disabled`; optional per-object gravity opt-in.
  - [ ] Semantic wall masking: core supplies plane equations; hosts clip meshes.
- [ ] **Task 9.2: Shader adapters (not core-only HLSL)**
  - [ ] Reference plane → clip interface in core.
  - [ ] Ship example `SemanticWallMask.hlsl` for D3D/Unity-style hosts; document that Vulkan/GL/Unreal use equivalent adapters.
- [ ] **Task 9.3: Agentic service layer (`src/ai/`)**
  - [ ] Accept **typed intent/commands** (string or enum) from the host OEM assistant via ABI.
  - [ ] Map commands onto facade actions (e.g. matrix layout, portal open) — no in-kernel “speech hashing.”

---

### 🚀 Phase 1.0: Native OpenXR Realization (v1.0.0)
**Goal:** Run the kernel against real OpenXR handles, with a concrete acceptance checklist (not a single vague “certification” checkbox).

- [ ] **Task 10.1: OpenXR lifecycle (`src/main.cpp` or dedicated runtime entry)**
  - [ ] Replace mock telemetry with OpenXR session/instance/system handles.
  - [ ] Request `XR_EXT_hand_tracking` (and `XR_EXT_eye_gaze_interaction` when available).
  - [ ] Establish `XR_REFERENCE_SPACE_TYPE_STAGE` (allocentric) and `XR_REFERENCE_SPACE_TYPE_VIEW` (egocentric).
- [ ] **Task 10.2: Multi-modal text router (`src/text/`) — host-injected**
  - [ ] Priority router in `text_io_router.cpp` that accepts strings from a **host-supplied callback / ABI entry** (keyboard, IME, engine UI).
  - [ ] Do **not** embed Bluetooth/WiFi keyboard sockets in core; leave transport to the host OS or Unity/Unreal layer.
  - [ ] Hooks for volumetric **Lexting** (3D hand-in-voxel → text field) as a later input source on the same router.
  - [ ] `text_label.cpp` CRUD for Box/Object annotations (primary + secondary count strings).
- [ ] **Task 10.3: Acceptance checklist**
  - [ ] Snap / free-range voodoo volumes with near-field ghosts (from 0.6).
  - [ ] Multi-select across voodoo boundaries (from 0.7).
  - [ ] Hands **and** controllers complete the demo path (from 0.5).
  - [ ] Velocity glide with friction settle; floor clamp holds.
  - [ ] Wall masking does not clip through partition planes incorrectly (from 0.9).
  - [ ] Storage glyphs + rename via host text injection.
  - [ ] Layout mode toggle and at least one anchor-mode switch work in-session (from 0.8).

---

### 🏢 Phase 2.0: Collaborative Room Workspaces (v2.0.0)
**Goal:** Multi-user 3OS on **on-prem VDI** (office GPU farms) and **home/edge compute** (IoT-class servers), interfacing every 2D screen in a room — wall-mounted or handheld — as a first-class, possessable display in shared volumetric workspaces. Phase 1.0 remains the single-user / native-OpenXR product bar; 2.0 adds session, presence, display, and media-sync systems.

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

- [ ] **Task 11.1: Multi-user session & workspace model**
  - [ ] Session identity: authenticated user, device credentials, room/tenant binding (enterprise VDI vs home edge).
  - [ ] Workspace kinds: **personal** (default on login), **shared** (any invite size — pairs, trios, larger), **full-room** (everyone present; owns room-wide spectator routing).
  - [ ] Invite / join / leave ABI; presence list per workspace; host is not the source of truth for ACLs (server/session service is).
  - [ ] Solo-in-shared: other invitees’ volumetric content **masked** in eyewear until explicitly unmasked or co-possessed — personal focus without leaving the shared session.
- [ ] **Task 11.2: Display registry (every 2D screen in the room)**
  - [ ] First-class **Display** entities: wall monitors, large 4K guest screens, handheld tablets/phones once logged in.
  - [ ] Pose pipeline: per-display **IMU** (and/or camera-assisted localization for handhelds) fused into room coordinates; portrait/landscape from mount/IMU.
  - [ ] Static vs handheld lifecycle; hot-plug / battery / tracking-lost soft-fail without dropping the session.
  - [ ] Reference-room catalog helpers (optional): 3×8 workstation monitors + 4× 4K guest screens + N handhelds — parameterization, not a hardcode-only layout.
- [ ] **Task 11.3: Spectator cameras & default monitor routing**
  - [ ] Four corner camera frustums as room-aligned virtual observers of the shared volumetric field.
  - [ ] Default binding: each camera feeds spectator views to monitors on the **opposite** wall; full-room workspace can reassign routing.
  - [ ] Large 4K pair on couch wall / opposite wall prioritized for guests without eyewear.
  - [ ] Compositor contract: server (VDI/edge) renders spectator frames; eyewear clients do not each re-render all wall feeds.
- [ ] **Task 11.4: Snap-to-monitor & shared monitor possession**
  - [ ] Extend standard 3OS transforms: snap any volumetric content onto any registered display → that display leaves default spectator mode and shows an **object-focused** view (3D asset render, video playback, image, text surface, etc. by content type).
  - [ ] Every monitor is **possessable** by any workspace member; possession is shared — any other invitee may interact or **release** possession, restoring default spectator view.
  - [ ] Legacy workstation mode: user faces wall, latches desk into console, possesses the **two monitors above their station**; those displays behave as a conventional dual-monitor desktop until released.
- [ ] **Task 11.5: Logged-in device localization & pull-in**
  - [ ] Cameras + session creds **semantically track** issued devices (e.g. tablet) in room space while logged in.
  - [ ] Gesture/ABI to **pull** that device’s live surface into the shared volumetric workspace (real-time mirror), using localized pose for placement.
  - [ ] Same localization authority used for wall displays (IMU → server) so handheld and bolted screens share one coordinate frame.
- [ ] **Task 11.6: Asset references (flyweights) vs asset copies**
  - [ ] First-class open binding: each workspace open of an asset is either a **copy** (independent instance) or a **reference** (flyweight to one server-RAM object). Same filename / same path must not imply a mode by accident.
  - [ ] Flyweight: attribute edits and **object** transforms propagate immediately to every workspace holding that reference, even across parallel workspaces with different view angles. **No merge** — one singleton in RAM; durable save/commit of that singleton is ordinary persistence, not a collaboration merge path. Teams may choose flyweight specifically to avoid diff/merge (especially same-room).
  - [ ] Copy: edits and object transforms stay local to that instance; no cross-workspace mutation; reconciliation is VCS (10.6b).
  - [ ] **Locality gate (default):** allow flyweight inside a low-latency co-residency domain (**room**; **building/campus** on the same on-prem fabric). By default refuse or force **copy** for cross-region / high-RTT peers.
  - [ ] **Optional site-to-site flyweight:** tenant opt-in when they provision an **MMO-class** (expensive) sync fabric between sites; then cross-site opens may bind as flyweight under the same no-merge + co-editor presence rules. Document cost/SLA; never imply WAN flyweight is free or default.
  - [ ] **Co-editor presence (required on flyweight open/join):** expose who else is live on that singleton (user, room/station if known, workspace). Invisible co-editors are not allowed; this is social coordination, not a VCS merge.
  - [ ] **Roster actions:** in-3OS **VOIP conference** with selected co-editors (esp. other rooms on campus); **bidirectional avatar fly-in** — place remote co-editor avatars in the local room field and local avatars in theirs so deixis/gaze align with live flyweight edits.
  - [ ] Default staging: place document origin / primary content at **room center** when opening into a workspace (overridable); placement is not the same as copy-vs-reference choice.
  - [ ] Headless: two workspaces open one flyweight — object spin in A visible in B; camera flip in A leaves B’s object upright; presence list includes both. Two copies — object spin in A leaves B upright. Cross-domain open request cannot bind as flyweight.
- [ ] **Task 11.6b: VCS integration for simultaneous copies**
  - [ ] Treat concurrent copies of the same logical asset as **divergent lineages** (branch-like): source revision identity, per-copy dirty state, commit/check-in, merge/rebase to trunk, conflict surfacing (including binary/DCC payloads). Required whenever flyweight is disallowed (cross-site) or teams choose isolation.
  - [ ] Session/ABI hooks for VCS actions as volumetric intents (checkout/open-as-copy, promote, get-latest, resolve conflict) — plumb Git/Perforce/Plastic/etc.; do not embed a full VCS engine in core.
  - [ ] Optional lock/lease policy when merge would be unsafe. True flyweight sessions have **no** merge UX for live edits.
  - [ ] Headless/fixture: two copy opens diverge → merge path reports conflict or clean promote; same-domain flyweight path has no merge step for live object spin.
- [ ] **Task 11.7: View transform vs object transform**
  - [ ] Separate ABI/state: **view / camera** (per user or per workspace view rig) vs **object anchor** (coordinates / orientation relative to document or room origin).
  - [ ] Camera/view moves never write object attributes; object transforms never silently rewrite another user’s camera.
  - [ ] Spectator and snap-to-monitor focused views may follow a workspace camera into a flyweight without baking that orbit into the asset transform.
  - [ ] Host DCC ports (e.g. Maya-in-3OS thought experiment): scene edits = object/document; orbit/tumble/“flip upside down for looking” = view unless the user explicitly transforms the asset.
- [ ] **Task 11.8: Real-time media sync**
  - [ ] Sync text, images, video, and 3D assets across eyewear clients and possessed/spectator displays with session-scoped consistency (who sees what under mask/invite/copy-vs-reference rules).
  - [ ] Bandwidth/GPU policy hooks for 12-user VDI vs smaller edge nodes (quality ladders, spectator vs personal LOD; flyweight sharing vs duplicated instances) — policy in server/host; kernel exposes intent, not vendor codecs.
- [ ] **Task 11.9: Deployment contracts (VDI + edge)**
  - [ ] Document split: **session/display/compositor** on on-prem VDI or home edge server; **eyewear/host** as thin interactive clients.
  - [ ] Headless tests: workspace invite graph; display pose update; snap → focused view → release → spectator restore; mask rules for solo-in-shared; flyweight vs copy + view vs object cases from 11.6/11.7; copy-divergence → VCS promote/merge hook from 11.6b.
  - [ ] No requirement that core embed a full VDI stack — define ABI + reference server responsibilities; implementations may be separate services.

**Gate (2.0):** Reference-room (or scaled fixture) demo — 2+ authenticated users in a shared workspace; wall monitors show opposite-camera spectator views; snap content to a monitor → focused view; second user releases possession → spectator restored; logged-in handheld localized and pull-in works; masked solo presence hides peer content until unmasked; parallel workspaces prove **flyweight** object edits cross sessions while **camera** flips do not, and **copies** isolate object edits with a **VCS promote/merge** path (not silent overwrite). Home-edge path exercises the same contracts at smaller display counts.

---

### 📝 Parking lot (ideas without tasks yet)

Capture design notes and UX instincts that are not ready for checklist tasks. No sequencing — promote into a real phase (including 2.x) when fleshed out. Mirror host-facing notes in Unity’s parking-lot section when relevant.

**Notes**

- *(Selection-gated telekinesis promoted into Phase 0.4 Task 4.6.)*
- [ ] **Glyph pack GUI (artist tool):** Simple desktop/Editor UI to generate `.3glyph` files (pick GLB, edit `THREEOS_glyph` fields, inject metadata) for artists who do not want to run `tools/inject_3glyph.py` from the CLI. Wraps the existing injector; does not embed host shaders in the pack.
- [ ] **Global field spacing + glyph scale:** Runtime `threeos_storage_get/set_field_spacing(float meters)` (clamped); Unity inspector/settings drive both kernel spacing and host `GlyphSizeM` (keep ~2.2× ratio or one master “field cell size”); optionally re-layout in-field matrices and rescale glyph roots so the whole field can be resized without hunting magic numbers. Kernel already shares `Workspace::kDefaultFieldSpacingM` / `field_spacing_m_` for demo + box-expand layout.
- [ ] **Matrix padding / margins:** Tunable cell padding and snap-margin (independent of spacing) so dense stacks are comfortable without relying only on sticky-snap; expose alongside field-spacing controls in a later milestone.

---

### 🎮 Parallel track: 3OS_Unity (see that repo’s plan)

Do not duplicate the full Unity checklist here. Use **[3OS_Unity/IMPLEMENTATION_PLAN.md](https://github.com/scifiuiguy/3OS_Unity/blob/main/IMPLEMENTATION_PLAN.md)** for UPM tasks and Quest gates. Summary:

| After core… | Unity must… |
| --- | --- |
| **0.1** ABI freeze | Thin harness: bridge, input router, plugin load/ping on Quest |
| **0.2–0.4** | Proxies (functional), telekinesis, storage glyphs / FS cut |
| **0.5** | Hands + controllers parity on device |
| **0.6** | Snap / free-range voodoo volumes (BB, centroids, near ghosts) |
| **0.7** | Multi-select + multi-volume moves |
| **0.8–0.9** | Layouts / frustum / anchors; enclosure mask + command hook |
| **1.0** | UPM polish + full demo acceptance; core native OpenXR is parity, not a blocker |
| **2.0** | Enterprise/edge collaborative host: room displays, spectator cams, shared workspaces |

Unreal (if pursued) can mirror the same paired-version pattern against the same ABI.
