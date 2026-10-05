# 3OS: Volumetric Operating System Architecture

3OS (pronounced **"three-oh-ess"**) is a first-principles, voxel-first operating system for spatial computing. Its target hardware is HMDs and holographic displays. It rejects the "flatland" bias of legacy WIMP (Windows, Icons, Menus, and Pointers) and multi-touch paradigms that have been a crutch in modern XR for far too long. 

The WIMP paradigm is a backwards compatibility tool in XR. It should not be front-and-center in any spatial computing OS. A VR or AR HMD that uses WIMP as its default metaphor set is like Windows 95 launching into a command line with no desktop, no Start button, and no File Explorer. We are asking developers to build the next-big-thing killer app using stone tools.

Instead of forcing users to aim virtual laser pointers at floating 2D windows, 3OS treats the entire environment as a high-capacity voxel field where the parameterized resolution defaults to 1 cubic inch, the approximate size that satisfies high selection accuracy at rapid speed in 3D. The goal is to provide users a voxel grid representing all the subdivisions of 3D space in which they can hover, select, or otherwise interact with any voxel in under 0.25 seconds regardless of their body's distance to that voxel. This is an impossible feat in all current spatial computing operating systems because of WIMP. 

At 1 inch resolution and a default 50-foot radius from head pose to the grid's outer shell, the grid will contain approximately 4 billion voxels if the user is standing in a wide open field. Theoretically, it could be larger as some viewers have accurate parallax stereo vision up to several hundred feet, but 1-inch objects side-by-side shrink into uselessness long before that distance, thus 50ft is a decent ballpark limit that is also parameterized and editable by integrators.

3OS is built in OpenXR and intended as a module to plug into any spatial computing OS. It is fully open-source under the MIT license, and it does not replace existing OS infrastructure, since WIMP-based interfaces still need to be in play for backwards compatibility, just as Windows 95 needed command line capability. 

3OS can plug into a current OS and become the default, or it can become an optional alternative interface. If you like 3OS and you want to see it installed in the OS of your favorite HMD, email that company, share the repo link, and let them know the code is free!

---

## 🏛️ Core Philosophy & The First Principles

1. **Voxels, Not Surfaces:** The fundamental building block of UI/UX in 3OS is a discrete coordinate volume (optimized at $\approx 1\text{ in}^3$ for reliable human motor capability), not a pixel fragment or a flat projection plane. 
2. **The Drummer's Rule:** A spatial OS should never subordinate micro-precision tasks to macro-stability muscles. In 3OS, far-field spatial volumes are compressed into near-field proxies so that interaction is entirely driven by high-speed, low-fatigue motor groups (eyes, wrists, fingers), keeping the shoulder girdle (**coracobrachialis**) kinetically neutral. At most, ten-finger or 6DOF-controlled actions should require the elbow to move no more than 1.5 inches out of its neutral pose, and developers should encourage the elbow to return to neutral pose for a majority of any experience. This rule is derived from the Moeller Method that enables drummers to play for hours without fatigue, and from the author's knowledge as a drum teacher relating drum student arm fatique to common "gorilla arm" issues in HCI research.
3. **Topology vs. Kinematics:** 
Spatial interaction is comprised of...
    **The Portal** (macro-topology; managing *where* data lives via instant coordinate remapping) and...
    **The Vector** (micro-kinematics; managing *how* data is arranged via velocity-controlled telekinesis). 
These two methods follow 80/20 Pareto distribution, where most objects occupying field space can and should be teleported into position via the Portal, though the user may occasionally desire to shift them telekinetically if the distance is fairly short.

---

## 🔄 The 3OS Unified Interaction State Machine

To eliminate physical exhaustion ("Gorilla Arm") and sluggishness of legacy XR selection systems, 3OS routes all user intent through a deterministic, cascading three-stage state machine.

```
[State 1: Macro-Targeting] ──> [State 2: Micro-Resolution] ──> [State 3: Possession / Transform]
(World Proxy Half-Dome)         (Double Proxy Voxel Grid)       (Velocity-Mapped CRUD Action)
```

### State 1: Macro-Targeting (The World-in-Miniature Proxy)
* **The Problem:** Humans cannot easily gaze at or raycast an empty voxel in which to instantiate or manage deep-field elements. A single stationary gaze vector penetrates thousands of voxels simultaneously. The means to choose any one voxel in the entire field must be rapid and intuitive.
* **The Solution:** Via gesture or button press, the user instantiates a localized, downscaled translucent **World Proxy Half-Dome** within comfortable, armrest-supported reach. 


### State 2: Micro-Resolution (The Double-Proxy Voxel Lattice)
* **The Mechanism:** The moment a coarse region is selected in the mini-map, that specific slice of space instantly blooms into a **Double-Proxy Voxel Grid** directly in front of the user's primary workspace. The chunk of empty space is proxied to the user. Simuultaneously, the user's hand or cursor is proxied out into the actual chunk in the deep field.
* **Lattice Visualization:** The hidden coordinate structure of that deep-field fragment is rendered locally as a subtle, wireframe volumetric lattice.
* **Decoupled Selection:** The user drives a point cursor offset by a few inches away from the driving hand to prevent that hand from occluding the proxy. The user is free to gaze at the actual fragment in the deep field, using the peripheral proxy as a remote control device, or if the fragment is too far away for good visual fidelity, the user can look primarily at the proxy instead. Cursor hover and selection can initiate CRUD actions, espeically CREATE functions if the current proxy contains empty voxel space.

### State 3: Possession & Transformation (Jean Grey Engine)
* **The Mechanism:** An already created object can be hovered by sending the point cursor inside its volume rather the raycasting onto its surface. Once selected, the user has option to use the proxy (a voodoo doll) as a transformation remote control for **Velocity-Based translation and/or rotation**.
* **Kinetic Economy:** Tiny, low-fatigue wrist gestures generate acceleration and velocity vectors that scale proportionally, gliding objects effortlessly across the field.
* **Momentum & Friction:** The system incorporates virtual inertia. Releasing an asset with momentum allows it to slide naturally through the voxels and settle based on an OS-level spatial friction coefficient.

---

## 🌀 Volumetric Workspace Management: Portals & Teleportation

3OS bypasses the slow, physics-based fishing-line-reeling or lassoing of legacy XR by utilizing the **Non-Euclidean Spatial Clipboard**.

* **Double-Proxy Portal Dragging:** If a double-proxy voodoo doll frame is open, dragging an from the near field into it teleports that object out into its far-field fragment. Vice versa, dragging an object that is currently proxied out of its proxy frame teleports it from its far-field fragment into the near field. The need to grab onto a far-field object with raycast and reel it in is eliminated.
* **Cross-Proxy Portal Dragging:** If multiple double-proxies are open, dragging an asset out of one local volume box and dropping it into another triggers an instantaneous teleportation. from fragment A to fragment B.
* **The Global Desktop Drop:** The World Proxy Half-Dome acts as an omnipresent directory. To move a massive dashboard to the back corner of a warehouse, the user grabs its near-field proxy and drops it onto a destination within the world proxy half-dome. A double proxy opens representing that spot and the object teleports into it.

---

## 🗂️ Volumetric Topology & System Architecture

### 1. The Selection Frustum
Multi-selection is resolved by pulling open both hands or twisting wrists in a dual-pinch to scale a dynamic bounding **Selection Frustum** inside the double-proxy volume. The possessed group behaves rigidly, preserving relative spatial offsets during velocity-mapped transformations.

### 2. Layered Coordinate Subordination
The system treats the origin $(0,0,0)$ as a fluid variable, allowing users to toggle fields across three structural anchor types:
* **Egocentric (User-Locked):** Subordinated to the user's body frame (travels with them).
* **Allocentric (World-Locked):** Grounded permanently into the physical architecture mapped by semantic room-mesh tracking.
* **Virtual / Canonical (Object-Locked):** Anchored to an arbitrary parent asset (e.g., a virtual desk), moving cohesively whenever the parent shifts.

### 3. Procedural Volumetric Layouts (Lattice Toggling)
Emulating the 2D File Explorer's view shifts, 3OS can instantly re-compute the spatial distribution of all active elements in a field:
* **Matrix View:** Collapses loose assets into tightly packed, perfectly aligned 3D cubic grids.
* **Cluster View:** Formulates structural groupings automatically based on shared metadata and semantic relationships.
* **Real Anchor View:** Disperses layouts instantly, snapping elements back to their assigned real-world surfaces or custom coordinate origins.

---

## 🧱 Architectural Enclosure & World Mesh Penetration

3OS strictly decouples Newtonian physics from spatial containment boundaries to preserve environmental stability and presence. These rules apply to all data structures the OS recognizes. These structures are analagous to files and folders of other operating systems, but they use different metaphors in 3D. For example, folders are boxes in 3OS. Files are objects whose low-poly prismatic geometry is defined by the application that generates the data, much like applications define file types and icons in other operating systems.

* **Global vs. Local Mode:** 3OS is in global mode by default, meaning if the user opens a box (i.e. drills into a folder) the contents of that box will emerge out of the box without dismissing the contents of the parent file structure. If the user opts to toggle global mode to local mode, then opening a box dismisses the objects at the parent level before revealing the contents of the box.
* **Rigidbody Matrix:** Set to `Disabled` by default for each object. The user can opt in a given object's properties to toggle physics on for that object, allowing it to receive gravity, collide with world geo, etc. Newly created objects float statically in zero-gravity space to eliminate continuous structural management chores. For OS objects, the user must manually turn gravity on if desired, but they should understand that full use of 3OS capability means allowing most objects to float.
* **Enclosure Engine:** Active by default with a `Strict` policy. Leveraging semantic AI mesh pipelines, the system enforces a hard spatial floor constraint—preventing floating voxels from clipping through physical floorboards or sinking into unreachable space even if its rigidbody is off.
* **Semantic Masking:** For walls and ceilings, users can unlock constraints to place items "over-the-horizon" (e.g., a weather widget on a balcony). The OS cleanly cuts the rendering mesh at the wall boundary plane, creating a crisp, non-clipping partition illusion.3OS has parameterized input enabling other service-layer systems of the HMD to provide planar definitions of such boundaries if desired.

---
## Structural Outline
```
3os-core/
├── include/
│   ├── openxr/                     # OpenXR header dependencies
│   ├── core/
│   │   ├── interaction_facade.hpp  # Input Action Facade (Hover, Select, Group)
│   │   ├── topology.hpp            # World Proxy Half-Dome & Double-Proxy grids
│   │   ├── kinematics.hpp          # Telekinetic Velocity Engine (Zero-fatigue curves)
│   │   └── enclosure.hpp           # Semantic Mesh Occlusion & Floor Constraints
│   ├── storage/
│   │   ├── file_bridge.hpp         # Legacy FS Sync (Directories -> Volumetric Elements)
│   │   └── volumetric_glyph.hpp    # Translates extensions into low-poly prismatic poly-poly meshes
│   ├── text/
│   │   ├── text_io_router.hpp      # Multi-modal input hub (External, Virtual, Volumetric)
│   │   └── text_label.hpp          # Lightweight 3D text primitives for Box/Object annotations
│   ├── ai/
│   │   └── agent_service_layer.hpp # Verbal Trigger Registry for host OEM AI integration
│   └── interop/
│       └── threeos_marshaller.hpp  # Flattened extern "C" bindings for P/Invoke & Dynamic Loading
├── src/
│   ├── interaction_facade.cpp
│   ├── topology.cpp
│   ├── kinematics.cpp
│   ├── enclosure.cpp
│   ├── storage/
│   │   ├── file_bridge.cpp         # Target directory crawler & delta-tracking engine
│   │   └── volumetric_glyph.cpp    # Procedural mesh compiler for text/data/box primitives
│   ├── text/
│   │   ├── text_io_router.cpp      # Prioritization router for physical vs. fallback input
│   │   └── text_label.cpp          # Transient floating text box CRUD lifecycle logic
│   ├── ai/
│   │   └── agent_service_layer.cpp # Binds semantic string hashes directly to 3OS system states
│   └── threeos_marshaller.cpp       # Core DLL/SO export implementations
├── shaders/
│   └── SemanticWallMask.hlsl       # Vertex/Pixel shader logic for wall clipping
├── .github/
│   └── workflows/
│       └── build-binaries.yml      # CI/CD cross-compilation pipeline (.dll and .so)
├── CMakeLists.txt
└── README.md

```

## 🚀 Getting Started

```bash
# Clone the 3OS Volumetric Kernel Architecture
git clone https://github.com/your-repo/3OS.git

# Navigate to the core velocity mapping and proxy pipeline engine
cd 3OS/core/kinematics
```

*This project is dedicated to breaking the flatland illusion. Welcome to true volumetric computing.*