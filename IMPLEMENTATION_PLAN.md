# 🗺️ 3OS_Core: Implementation Roadmap & Versioning Lifecycle

This roadmap moves **3OS_Core** from an empty repo to a production-grade, hardware-native volumetric OS module. Phases are dependency-ordered. Contiguous semver tracks real deliverables (no fake version gaps).

```text
[0.0 Scaffold] → [0.1 ABI + stubs] → [0.2 Topology + portals] → [0.3 Kinematics + floor]
        → [0.4 Storage + text] → [0.5 Layouts + anchors + frustum] → [0.6 Enclosure + AI hooks]
        → [1.0 Native OpenXR]
                 └─► [Separate track] 3OS_Unity (after OpenXR core ships)
```

### Companion repo (separate track)

**[3OS_Unity](https://github.com/scifiuiguy/3OS_Unity)** is a **separate repository** with its own development track. It is a UPM package that P/Invokes the native `3os_kernel` binaries produced by this repo (`Native~/` submodule + `Plugins/` DLLs/SOs). It is **not implemented yet**. Implement it **after** the OpenXR/native core path is in place (post–Phase 1.0), not as a mid-pipeline dependency for verifying topology/kinematics.

Until then, verify core behavior with **headless C++ tests** and a thin pose-injection harness inside this repo.

---

### ⚙️ Phase 0.0: Repo Scaffold
**Goal:** Make the tree buildable before any domain logic.

- [ ] **Task 0.1: Skeleton & build system**
  - [ ] Create the folder layout from the README (`include/`, `src/`, `shaders/`, `.github/workflows/`).
  - [ ] Add `CMakeLists.txt` that builds a shared library (`3os_kernel`) and a console test target.
  - [ ] Add MIT `LICENSE`, `.gitignore`, and a CI stub that compiles on Windows (and Linux if available).
- [ ] **Task 0.2: Hello export**
  - [ ] Export one `extern "C"` symbol (e.g. `threeos_version`) and call it from the console harness.

---

### 🧱 Phase 0.1: Core Types, Blittable ABI, Stub Facade (v0.1.0)
**Goal:** Define the domain model and the memory-safe plain-C ABI that Unity/Unreal/OpenXR hosts will call. Keep the facade as a stub until topology/kinematics exist.

- [ ] **Task 1.1: Core types first (`include/core/`)**
  - [ ] Define first-class types: `VoxelCoord`, `Pose`, `Box`, `Object`, `InteractionState`, and anchor/layout enums as needed by later phases.
  - [ ] Prefer explicit padding + `static_assert` on `sizeof` / `offsetof` over blanket `#pragma pack(1)` unless a host ABI proves packing is required.
- [ ] **Task 1.2: Design blittable interop layouts (`include/interop/`)**
  - [ ] Implement binary-safe C structures (`InteropInputFrame`, `InteropTransformDelta`, etc.) with fixed-width primitives.
  - [ ] Document ownership, lifetime, and units (meters vs inches, Left-handed vs OpenXR conventions).
- [ ] **Task 1.3: Stub system orchestrator (`src/interaction_facade.cpp`)**
  - [ ] Sketch `InteractionState` (`Idle`, `Hovering`, `Possessed`, `Transforming`) and callback slots (`OnObjectSelect`, `OnObjectHoverEnter`, `OnMultiSelectTriggered`) without full proxy logic yet.
- [ ] **Task 1.4: Flatten native export layers (`src/threeos_marshaller.cpp`)**
  - [ ] Wrap exports in `extern "C"` with platform macros (`__declspec(dllexport)` / visibility attributes).
- [ ] **Task 1.5: Local test harness**
  - [ ] Console pipeline passes mock frames across the C marshalling layer.
  - [ ] Assert layout/size metrics; no silent padding surprises.

---

### 🕸 Phase 0.2: Topology, Proxies & Portals (v0.2.0)
**Goal:** World Proxy Half-Dome, Double-Proxy Voxel Lattice, and Voodoo portal teleportation — with **headless tests**, not engine wrappers.

- [ ] **Task 2.1: Multiscale coordinate workspace (`src/topology.cpp`)**
  - [ ] Voxel indexing for discrete ~1-inch spatial blocks (`std::array<int, 3>` or equivalent).
  - [ ] Scale-down remapping into a near-field **World Proxy Half-Dome**. Keep scale **configurable** (do not hardcode `1:100` as the only ratio).
  - [ ] Matrix remapping that projects a far-field cluster into a near-field **Double-Proxy Voxel Lattice**.
- [ ] **Task 2.2: Voodoo portal teleportation**
  - [ ] Near ↔ far drag through an open double-proxy (Poros-style non-Euclidean remapping).
  - [ ] Cross-proxy drag when multiple double-proxies are open (A → B teleport).
  - [ ] World Proxy Half-Dome as global drop target for far-field placement.
- [ ] **Task 2.3: Headless topology verification**
  - [ ] Unit tests for voxel index math, proxy remaps, and portal teleport transforms (fake poses in, expected poses out).

---

### ⚡ Phase 0.3: Kinematics + Minimal Floor Clamp (v0.3.0)
**Goal:** Velocity telekinesis with inertia/friction, plus a **minimal enclosure** so demos do not sink objects through the floor.

- [ ] **Task 3.1: Kinetic control system (`src/kinematics.cpp`)**
  - [ ] Document the control law before coding: hand delta → acceleration/velocity, gain, deadzone, release → friction settle (the README’s rate-control story, not incomplete one-liners).
  - [ ] Rate-based tracking (not 1:1 position). Sample hand/frame deltas to drive proportional velocity.
  - [ ] Virtual inertia on release with an OS-level spatial friction coefficient.
- [ ] **Task 3.2: Minimal floor clamp (subset of enclosure)**
  - [ ] Clamp vertical motion when an object intersects the floor height profile (\(V_y = 0\) / snap) so Phase 0.3 demos stay usable.
- [ ] **Task 3.3: Headless kinematics verification**
  - [ ] Unit tests for gain/deadzone/friction settle and floor clamp with injected pose streams.

---

### 📂 Phase 0.4: Storage Glyphs & Text Labels (v0.4.0)
**Goal:** Map legacy FS into Boxes/Objects and route text via **host callbacks**, not in-kernel Bluetooth/WiFi sockets.

- [ ] **Task 4.1: Legacy file system bridge (`src/storage/`)**
  - [ ] Background crawler in `file_bridge.cpp` using `std::filesystem` (or host-provided file lists where sandboxed).
  - [ ] `volumetric_glyph.cpp`: map extensions (`.txt`, `.pdf`, `.usd`, …) to low-poly Box/Object mesh definitions.
- [ ] **Task 4.2: Multi-modal text router (`src/text/`) — host-injected**
  - [ ] Priority router in `text_io_router.cpp` that accepts strings from a **host-supplied callback / ABI entry** (keyboard, IME, engine UI).
  - [ ] Do **not** embed Bluetooth/WiFi keyboard sockets in core; leave transport to the host OS or Unity/Unreal layer.
  - [ ] Hooks for volumetric **Lexting** (3D hand-in-voxel → text field) as a later input source on the same router.
  - [ ] `text_label.cpp` CRUD for Box/Object annotations.
- [ ] **Task 4.3: Storage/text verification**
  - [ ] Point the harness at a real directory; assert Boxes/Objects populate with correct glyph types; rename via injected text callback.

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

### 🎮 Separate track: 3OS_Unity (after Phase 1.0)

Tracked in **[scifiuiguy/3OS_Unity](https://github.com/scifiuiguy/3OS_Unity)**, not in this repo’s critical path for Phases 0.0–1.0.

From its README (outline only today): UPM package with `ThreeOSBridge` / `ThreeOSInputRouter` / `ThreeOSMovable`, `InteropStructs.cs` mirroring native layouts, prebuilt `Plugins/` binaries, `Native~/` submodule to this core, URP semantic mask shader, and `Samples~/` demo scene.

- [ ] **Task U.1:** Fill in real Git URL / `package.json` (README placeholders are incomplete).
- [ ] **Task U.2:** Submodule or CI copy of `3os-core`; build/copy `3os_kernel.dll` / `lib3os_kernel.so`.
- [ ] **Task U.3:** Implement P/Invoke bridge + input router + movable component against the frozen ABI from Phase 0.1+.
- [ ] **Task U.4:** Port/adapt semantic wall mask as `UnitySemanticMask.shader`.
- [ ] **Task U.5:** Ship `WorldProxyDome` / `DoubleProxyLattice` prefabs and Quest-oriented demo scene.

Unreal (if pursued) follows the same pattern: separate repo, same native ABI, after OpenXR core is stable.
