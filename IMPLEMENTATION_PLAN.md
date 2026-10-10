# 🗺️ 3OS_Core: Implementation Roadmap & Versioning Lifecycle

This roadmap moves **3OS_Core** from an empty repo to a production-grade, hardware-native volumetric OS module. Phases are dependency-ordered. Contiguous semver tracks real deliverables (no fake version gaps).

```text
Core  0.0 → 0.1 (small ABI freeze) → 0.2 → 0.3 → 0.4 → 0.5 → 0.6 → 1.0 (native OpenXR parity)
                │                     │     │     │     │     │     │
Unity 0.0 → 0.1 (thin Quest harness) → 0.2 → 0.3 → 0.4 → 0.5 → 0.6 → 1.0 (UPM polish)
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
**Goal:** World Proxy Half-Dome, Double-Proxy Voxel Lattice, and Voodoo portal teleportation. Prove with **headless tests** here; prove on Quest via the paired **Unity 0.2** gate.

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

### 📂 Phase 0.4: Storage Glyphs & Text Labels (v0.4.0)
**Goal:** Map legacy FS into Boxes/Objects with **typed 3D glyphs**, plus host-injected text labels. Glyphs are the OS “icons”; content payloads (the files themselves) stay separate.

**Glyph package model (single file)**  
Windows associates types via the registry pointing at a separate `.ico`; the icon file does not carry the association. For 3OS we want the stronger case: **one file = mesh + type metadata**.

- **Container:** binary glTF (**GLB**). Keep the `glTF` magic / JSON+BIN chunks so any glTF tool can still open the mesh.
- **Custom type:** ship as **`.3glyph`** (rename/export of a `.glb`). Hosts register the extension; loaders may also sniff GLB magic. Relabeling is safe — it is still a valid GLB on disk.
- **Embedded association:** vendor extension `THREEOS_glyph` on the glTF root. **Canonical schema + demo pack JSON:** [`docs/THREEOS_glyph.md`](docs/THREEOS_glyph.md) and [`docs/glyph_packs/`](docs/glyph_packs/). Inject with [`tools/inject_3glyph.py`](tools/inject_3glyph.py).
- **Authoring:** Blender → glTF 2.0 (`.glb`) → `python tools/inject_3glyph.py mesh.glb docs/glyph_packs/<pack>.json` → `.3glyph`.
- **Resolution order:** registered `.3glyph` packs → built-in procedural fallbacks (unknown types still get a generic prism) → unlabeled Box/Object.

Core owns the **association table + descriptors**; hosts own **decoding GLB meshes and drawing**. Core does not need a full glTF renderer in 0.4 — it reads the small JSON metadata (and may treat mesh bytes as an opaque blob handed to the host).

**Quest 0.4 demo directory (minimal, two entries)**  
Operator pre-stages **`Documents/3OS_Demo`** (see [`docs/QUEST_DEMO_DIR.md`](docs/QUEST_DEMO_DIR.md)). Contents are intentionally tiny:

| On disk | Role in scene |
| --- | --- |
| `simple.glb` | **Content** file (not a glyph pack). Rendered with the registered **glTF/GLB Object glyph**; name label `simple.glb`. |
| `Test1/` | Empty folder → **Box**. Rendered with default **Box glyph** (`box.3glyph` / authored from `box.glb`); name label `Test1`; secondary count label `0 objects`. |

Authored glyph sources (Blender → GLB → `.3glyph` with `THREEOS_glyph`): glTF-logo-like pack for `.glb`/`.gltf`; **`box.glb`** as the default closed Box glyph for all Boxes; **`box_open.glb`** as the open-state Box glyph (toggleable). Content `simple.glb` must not be confused with these glyph assets.

**Rendering:** Object and Box **glyphs** (including demo `gltf_mark`, `box`, `box_open`) must draw **semi-transparent** (see-through prismatic icons), not opaque solids. Hosts use a transparent/alpha material path (URP Transparent or equivalent). Pack `tint_rgba.a` and mesh alpha should stay below 1 (target ~0.45–0.65 unless tuned). Content payloads like `simple.glb` are unaffected unless they are also shown as glyphs.

**Drop-into-Box interaction (kernel logic + host viz)**  
While dragging an Object glyph near a Box within a distance threshold: snap the Object to a hold pose just above the Box; host shows an insert-arrow affordance under the Object’s name label. Leaving the threshold unsnaps. Releasing while snapped commits insert: emit storage events, run host anim (closed → open glyph, +10% scale bump and return, open → closed), hide/remove the Object glyph from the parent field, refresh Box count (`1 object`). Persist by moving/copying the file into the folder on disk via the host FS event sink.

**Snap ↔ stick telekinesis:** On snap enter, clear Object velocity to zero and **re-anchor `start_hand` to the player’s current hand pose** without requiring a grip release. That way stick deflection is measured from the snap moment (no flyaway from the pre-snap stick). On unsnap, keep the same rule (re-anchor `start_hand`, zero velocity) so leaving the threshold is also a clean re-grab.

- [ ] **Task 4.1: Legacy file system bridge (`src/storage/`)**
  - [ ] Background crawler / host-provided listing in `file_bridge.cpp` (Quest sandbox paths via host).
  - [ ] Emit Object/Box records with path, extension, display name, child count; no mesh IO in the crawler.
  - [ ] Document the 0.4 demo contract: listing must surface exactly `simple.glb` + empty `Test1` when that is what is on disk.
- [ ] **Task 4.2: Glyph registry & `.3glyph` metadata (`volumetric_glyph`)**
  - [ ] Define `THREEOS_glyph` schema (documented JSON fields + size/version constants), including Box closed/open glyph ids.
  - [ ] Parse GLB JSON chunk enough to extract `THREEOS_glyph` (or reject / fall back); do not require full scene graph eval in core.
  - [ ] Registry API: install/uninstall glyph pack by id; map extension/MIME → `glyph_id`; map Box → closed/open glyph pair; query descriptor for a path.
  - [ ] Built-in procedural fallbacks when packs are missing (generic Object prism, generic Box).
  - [ ] Sample packs: glTF-logo `.3glyph` → `.gltf`/`.glb`; `box.3glyph` + `box_open.3glyph` → all Boxes (toggle states).
- [ ] **Task 4.3: Host glyph ABI**
  - [ ] Blittable glyph descriptor: `glyph_id`, category, applies-to, bounds, procedural kind **or** opaque `.3glyph` blob/URI; Box state (`closed` / `open`).
  - [ ] Label fields: primary name (`simple.glb`, `Test1`) and optional secondary (`0 objects` / `N objects`).
  - [ ] Tick/query path: entity/path → descriptor + labels for host instantiation.
- [ ] **Task 4.4: Box insert proximity & commit**
  - [ ] While Object is dragged: test distance to candidate Boxes; enter/exit **snap** (hold pose above Box); expose snap target id + insert-pending flag to host (arrow affordance).
  - [ ] On snap enter **and** snap exit: `velocity = 0` and reset grab-anchored `start_hand` to the current hand pose (grip may still be held).
  - [ ] On release while snapped: validate, parent Object under Box in the workspace model, update Box child count, clear snap.
  - [ ] Headless tests: enter threshold → snapped + vel zero + start_hand re-anchored; leave → unsnapped with same re-anchor; release snapped → child count 1 and Object no longer a root sibling.
- [ ] **Task 4.5: Storage mutation event system (write-through)**
  - [ ] Emit typed events (e.g. `ObjectMovedIntoBox`, with source path, destination folder path, entity ids) on successful insert (and later rename/delete).
  - [ ] Host implements an FS sink: on Quest/Editor, perform the real filesystem move of `simple.glb` into `Test1/`; report success/failure back to core.
  - [ ] On FS success, workspace stays consistent; on failure, roll back model (Object returns to field, count restored) and surface error.
  - [ ] Harness: mock sink records the event; assert path rewrite `…/simple.glb` → `…/Test1/simple.glb`.
- [ ] **Task 4.6: Multi-modal text router (`src/text/`) — host-injected**
  - [ ] Priority router in `text_io_router.cpp` that accepts strings from a **host-supplied callback / ABI entry** (keyboard, IME, engine UI).
  - [ ] Do **not** embed Bluetooth/WiFi keyboard sockets in core; leave transport to the host OS or Unity/Unreal layer.
  - [ ] Hooks for volumetric **Lexting** (3D hand-in-voxel → text field) as a later input source on the same router.
  - [ ] `text_label.cpp` CRUD for Box/Object annotations (primary + secondary count strings).
- [ ] **Task 4.7: Storage/text/glyph verification**
  - [ ] Fixture listing mirrors demo dir (`simple.glb` + empty `Test1`); assert glyph ids, labels, and `0 objects`.
  - [ ] Assert `.3glyph` metadata maps `.glb` content to the glTF pack (distinct from Box packs).
  - [ ] Insert flow + mock FS sink as in 4.4/4.5; rename via text does not break association.

---

### 🗂️ Phase 0.5: Layouts, Anchors & Selection Frustum (v0.5.0)
**Goal:** Cover README workspace features omitted from the original Gemini plan.

- [ ] **Task 5.1: Procedural volumetric layouts**
  - [ ] **Matrix View** — packed 3D cubic grids.
  - [ ] **Cluster View** — metadata/semantic groupings.
  - [ ] **Real Anchor View** — snap back to world surfaces / custom origins.
- [ ] **Task 5.2: Layered coordinate subordination**
  - [ ] Toggle **Egocentric** (user-locked), **Allocentric** (world-locked), **Object-locked** (parent asset) origins.
- [ ] **Task 5.3: Selection frustum**
  - [ ] Dual-pinch / dual-hand scaled bounding frustum inside a double-proxy; rigid group transforms preserve relative offsets.
- [ ] **Task 5.4: Global vs local box drill-down**
  - [ ] Global mode (default): open Box without dismissing parent contents.
  - [ ] Local mode: dismiss parent level when opening a Box.

---

### 🧱 Phase 0.6: Full Enclosure, Shader Adapters & AI Hooks (v0.6.0)
**Goal:** Complete boundary policy; keep plane math in C++; treat shaders as **per-engine adapters**. Optional AI as typed commands, not speech hashes.

- [ ] **Task 6.1: Enclosure engine (`src/enclosure.cpp`)**
  - [ ] Strict floor policy (already seeded in 0.3) plus wall/ceiling plane constraints.
  - [ ] Rigidbody matrix default `Disabled`; optional per-object gravity opt-in.
  - [ ] Semantic wall masking: core supplies plane equations; hosts clip meshes.
- [ ] **Task 6.2: Shader adapters (not core-only HLSL)**
  - [ ] Reference plane → clip interface in core.
  - [ ] Ship example `SemanticWallMask.hlsl` for D3D/Unity-style hosts; document that Vulkan/GL/Unreal use equivalent adapters.
- [ ] **Task 6.3: Agentic service layer (`src/ai/`)**
  - [ ] Accept **typed intent/commands** (string or enum) from the host OEM assistant via ABI.
  - [ ] Map commands onto facade actions (e.g. matrix layout, portal open) — no in-kernel “speech hashing.”

---

### 🚀 Phase 1.0: Native OpenXR Realization (v1.0.0)
**Goal:** Run the kernel against real OpenXR handles, with a concrete acceptance checklist (not a single vague “certification” checkbox).

- [ ] **Task 7.1: OpenXR lifecycle (`src/main.cpp` or dedicated runtime entry)**
  - [ ] Replace mock telemetry with OpenXR session/instance/system handles.
  - [ ] Request `XR_EXT_hand_tracking` (and `XR_EXT_eye_gaze_interaction` when available).
  - [ ] Establish `XR_REFERENCE_SPACE_TYPE_STAGE` (allocentric) and `XR_REFERENCE_SPACE_TYPE_VIEW` (egocentric).
- [ ] **Task 7.2: Acceptance checklist**
  - [ ] Macro-target via World Proxy Half-Dome on far-field empty space.
  - [ ] Double-proxy micro-resolve + CRUD in empty voxels.
  - [ ] Portal teleport near ↔ far and cross-proxy.
  - [ ] Velocity glide with friction settle; floor clamp holds.
  - [ ] Wall masking does not clip through partition planes incorrectly.
  - [ ] Storage glyphs + rename via host text injection.
  - [ ] Layout mode toggle and at least one anchor-mode switch work in-session.

---

### 📝 Post-1.0: Parking lot (ideas without tasks yet)

Capture design notes and UX instincts that are not ready for checklist tasks. No sequencing — promote into a real phase when fleshed out. Mirror host-facing notes in Unity’s Post-1.0 section when relevant.

**Notes**

- **Selection-gated telekinesis.** Possession/drive should follow selection state, not grip lifetime alone: deselection disables telekinesis; while selected, grip can re-drive immediately even after far-field throws (rest grip without losing the object). Complements later voodoo volumes without requiring voodoo for every long move. *(See Unity Post-1.0; felt on Quest 0.3.)*

---

### 🎮 Parallel track: 3OS_Unity (see that repo’s plan)

Do not duplicate the full Unity checklist here. Use **[3OS_Unity/IMPLEMENTATION_PLAN.md](https://github.com/scifiuiguy/3OS_Unity/blob/main/IMPLEMENTATION_PLAN.md)** for UPM tasks and Quest gates. Summary:

| After core… | Unity must… |
| --- | --- |
| **0.1** ABI freeze | Thin harness: bridge, input router, plugin load/ping on Quest |
| **0.2–0.6** | Matching visuals/UX for proxies, telekinesis, glyphs, layouts, mask |
| **1.0** | UPM polish + full demo acceptance; core native OpenXR is parity, not a blocker |

Unreal (if pursued) can mirror the same paired-version pattern against the same ABI.
