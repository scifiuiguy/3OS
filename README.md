# 3OS: A Volumetric Operating System Interface

Hollywod depicted a futuristic "Unix system" in Jurassic Park in 1994:
[![It's a Unix system](https://img.youtube.com/vi/dFUlAQZB9Ng/maxresdefault.jpg)](https://www.youtube.com/watch?v=dFUlAQZB9Ng)

Johnny Mnemonic shows a 3D VR internet in 1995:
[![Whoa.](https://img.youtube.com/vi/SoxGazX_kkw/maxresdefault.jpg)](https://www.youtube.com/watch?v=SoxGazX_kkw)

Both of these movies were shy of the mark to say the least, but they both hint at a 1990s sense of optimism toward 3D computer interfaces eventually becoming better, faster, and more efficient than 2D desktop equivalents. Obviously that didn't happen. YET. What happened to that optimism though?

Was it proven to be bunk? Are 3D interfaces destined to be slower than desktop PCs? Or did we simply stop investing in the VR industry for a decade after the dot-com bust, allowing mountains of incredible university research to collect dust in SIGCHI archives, never to be drawn upon again even after the industry was resurrected by Oculus?

---

## What is 3OS?

3OS (pronounced **"three-oh-ess"**) is a voxel-first operating system interface that attempts to take us back to our roots by drawing upon incredible 90s research to theorize a methodology that could make XR faster than desktop PCs for productivity tasks. Target hardware is HMDs, holographic displays, and any future hardware that enables stereoscopic depth visibility. 3OS rejects the "flatland" bias of legacy WIMP (Windows, Icons, Menus, and Pointers) and multi-touch paradigms that have been a crutch in modern XR for far too long. 

---

## WIMP is WIMPY

The WIMP paradigm is a backwards compatibility tool in XR. It should not be front-and-center in any spatial computing OS. A VR or MR HMD that uses WIMP as its default metaphor set is like Windows 95 launching into a command line with no Desktop, no Start button, and no File Explorer, except at least the command line still brought extra speed for power users. WIMP interfaces do the opposite in XR, yet they're treated as the ONLY OS interface in 2026, rather than a background legacy interface. We cannot expect enterprise software suite companies to consider porting their robust systems into XR using stone tools.

## 3OS Basics

Instead of forcing users to aim virtual laser pointers at floating 2D windows, 3OS treats the entire environment as a high-capacity voxel field where the parameterized resolution defaults to 1 cubic inch, the approximate size that satisfies high selection accuracy at rapid speed with 6DOF 10-finger hand motion. The goal is to provide users a voxel grid representing all the subdivisions of 3D space in which they can hover, select, or otherwise interact with any voxel in under 0.25 seconds regardless of their body's distance away from that voxel. This is an impossible feat in all current spatial computing operating systems because of the assumption that OS interfaces must be WIMP.

At 1 cubic inch voxel resolution and a default 50-foot radius from head pose to the grid's outer shell, the grid contains approximately 4 billion voxels if the user is standing in a wide open field. Theoretically, it could be larger as some viewers have accurate parallax stereo vision up to several hundred feet, but 1-inch objects side-by-side shrink into uselessness via perspective long before that distance, thus 50ft is a decent parameterized ballpark limit.

3OS is built in OpenXR and intended as a module to plug into any spatial computing OS. It is fully open-source under the MIT license, and it does not replace existing OS infrastructure, since WIMP-based interfaces still need to be installed for backwards compatibility, just as Windows 95 couldn't launch without command line dialogs. 

The 20-teens era of spatial OS design is the Xerox PARC era. The Windows 95 era is almost upon us. 3OS attempts to help clear the brambles of what an OS interface may look like in this new era. It can plug into a current OS and become the default, or it can become an optional alternative interface. If you like 3OS and you want to see it installed in the OS of your favorite HMD, email that company, share the repo link, and let them know the code is free!

---

## The Problem

Software companies (Adobe, Autodesk, Salesforce, Intuit, Shopify, etc.) assess whether they should port their legacy platforms into a new OS frequently. Their software is typically web-browser first for cross-platform efficiency, but even with cross-platform apps, the most robust productivity tools in these apps typically run on the desktop version only. 

**Why?**

These companies understand that touchscreens are more of an entertainment consumption platform than an enterprise productivity platform. There are exceptions, but a majority do not port all the desktop features of enterprise software onto mobile devices because...

**Multi-touch interfaces...**

1) ...are physically smaller. Shrinking the number of available interacative fragments reduces multitasking capability expnentially, hence why iOS requires developers to severely limit the number of user choices per screen controller.

2) ...cannot compete with desktop interfaces due to mouse/keyboard blowing multi-touch out of the water on raw data throughput.

3) ...mobile operating systems (especially iOS) actively discourage manual raw data management by obfuscating direct data access.

It follows that if you INCREASE the number of interactive fragments (or voxels), multitasking capability should explode exponentially beyond the capability of desktop interfaces. XR developers want every XR OS to be perceived as useful in enterprise for greater adoption, yet the boot interface of EVERY XR OS is obviously patterned after Apple and Android, an incredible oversight that conveys to every enterprise user that "this is a a platform that can't do the things your desktop PC can do." WindowsMR by Microsoft was a notable exception that clearly recognized this problem, yet failed to recognize that the crux of the problem is WIMP itself.

True to form, if you try to do nearly any run-of-the-mill office PC tasks on any XR OS, common interfaces make it so God-awfully slow that no enterprise company would EVER consider porting their desktop features into XR in the 2020s.

This must change. XR has greater enterprise potential than desktop computers. Raw throughput capability will one day be proven to exceed that of desktop computers as soon as the RIGHT OS interface enables it. AI voice-based interfaces are part of that story, but we can't always speak our work into a computer. Sometimes, work needs to be more private, even between colleagues in a shared office. We can't adopt XR in the office, let alone perform work on the go, if the interaction model slows us to a crawl in those moments. Thus, XR must present users an interface that is faster than desktop WITH OR WITHOUT an AI assistant.

The "right" XR interface must solve the two glaring challenges that keep XR interfaces slow:

**1) No Near-Field/Far-Field Parity:** The user has instant intuitive access to the near field, but extremely slow/cumbersome (if any) access to the far field, and since the far field is more than 90% of the whole field, the total productivity canvas in XR ends up smaller with lower throughput than its desktop equivalent. A 2D desktop screen has hundreds of pixel fragment zones in which to place colocated buttons, text labels, images, etc. with rapid access to every one of them at once. 

XR has millions of interaction zones in each frustum which would enable multi-tasking that blows desktop interfaces out of the water, but most of these zones are unreachable without silly raycast fishing reel actions, an impossible wrench in the gears for throughput. Imagine requiring 30 seconds to click an icon in Windows. Furthermore, we can't easily see how ridiculously slow our XR intefaces are because this throughput drag is hidden by #2.

**2) WIMP** The crutch of WIMP denies the use of the depth vector in concert with common 2D table layouts (i.e. WIMP makes 3D matrix and cluster layouts impossible), which occludes the vast potential utility of the XR field almost entirely, which in turn masks the throughput jank of the default raycasting interaction model. Raycasting successfully hides how terribly slow it is by forcing us to drive our Ferrari at 5mph as though that's how fast all racecars drive.

Solve these two problems, and enterprise adoption of XR can begin.

---

## 🏛️ Core Philosophy & The First Principles

1. **Voxels, Not Surfaces:** The fundamental building block of UI/UX in 3OS is a discrete coordinate volume (optimized at $\approx 1\text{ in}^3$ for reliable human motor capability), not a pixel fragment or a flat projection plane. 

2. **The Drummer's Rule:** A spatial OS should never subordinate micro-precision tasks to macro-stability muscles. First and foremost, this means the user should never be required to walk across a room to engage with an object simply because that object is outside the near field. The lost energy and time of closing the gap to bring the object into reach is a cardinal enemy of productivity. In 3OS, we compress far field spatial volumes into near field proxies called Voodoo Volumes instantaneously so that interaction is entirely driven by high-speed, low-fatigue motor groups (eyes, wrists, fingers), keeping the shoulder girdle (**coracobrachialis**) kinetically neutral for a vast majority of each session. The user can walk during use of the interface if the interface is anchored to their body rather than to the room, but large full-body "walk-across-the-room" actions are never required to manipulate a given element. At most, ten-finger or 6DOF-controlled actions require the elbow to move no more than 1.5 inches out of its neutral pose, and developers should encourage the elbow to return to neutral pose for a majority of any experience. This rule is derived from the Moeller Method that enables drummers to play for hours without fatigue. This methodology comes from the author's personal experience as a drum teacher relating drum student arm fatique to common "gorilla arm" issues in HCI research.

3. **Topology vs. Kinematics:** 
Spatial interaction in 3OS is comprised of...
    **Portals** (macro-topology; managing *where* data lives via instant coordinate remapping) and...
    **Telekinesis** (micro-kinematics; managing *how* data is arranged on smaller scales via velocity-controlled telekinesis). 
These two methods follow 80/20 Pareto distribution, where most objects occupying field space can and should be teleported into position via Voodoo Portals, though the user may occasionally desire to shift them telekinetically if the distance is fairly short.

---

## 🔄 The 3OS Unified Interaction State Machine

To eliminate physical exhaustion ("Gorilla Arm") and sluggishness of legacy XR selection systems, 3OS routes all user intent through a deterministic, cascading three-stage state machine.

```
[State 1: Macro-Targeting] ──> [State 2: Micro-Resolution] ──> [State 3: Possession / Transform]
(World Proxy Half-Dome)         (Double Proxy Voxel Grid)       (Velocity-Mapped CRUD Action)
```

### State 1: Macro-Targeting (The World-in-Miniature Proxy)
* **The Problem:** Humans cannot easily gaze at or raycast an empty voxel in which to instantiate or manage far field elements. A single stationary gaze vector penetrates thousands of voxels simultaneously. The ability to choose any one voxel in the entire field must be rapid and intuitive.
* **The Solution:** Via gesture or button press, the user instantiates a localized, downscaled translucent **World Proxy Half-Dome** within comfortable, nearly-at-rest reach. 


### State 2: Micro-Resolution (The Double-Proxy Voxel Lattice)
* **The Mechanism:** The moment a coarse region is selected in the World Proxy Half-Dome, that specific slice of space instantly blooms into a **Double-Proxy Voxel Grid** directly in front of the user's primary workspace. The chunk of empty space is proxied to the user. Simultaneously, the user's hand or cursor is proxied out into that chunk in the far field.
* **Lattice Visualization:** The hidden coordinate structure of that far field volume is rendered locally as a subtle, wireframe volumetric lattice.
* **Decoupled Selection:** The user drives a point cursor offset by a few inches away from the driving hand to prevent that hand from occluding the proxy. The user is free to gaze at the actual fragment in the far field, using the peripheral proxy as a remote control device, or if the fragment is too far away for good visual fidelity, the user can look primarily at the proxy instead. Cursor hover and selection can initiate CRUD actions, espeically CREATE functions if the current proxy contains empty voxel space. Menu options e.g. Create New Box (3D equivalent of a folder) or Create New Object (3D equivalent of a file) are typically presented in matrix layout as long as there are more than four options.

### State 3: Possession & Transformation (Jean Grey Engine)
* **The Mechanism:** An already created object can be hovered by sending a point cursor inside its volume rather than raycasting onto its surface. Once selected, the user has option to use the proxy (a voodoo doll) as a transformation remote control for **Velocity-Based translation and/or rotation**.
* **Kinetic Economy:** Tiny, low-fatigue wrist gestures generate acceleration and velocity vectors that scale proportionally, gliding objects effortlessly across the field.
* **Momentum & Friction:** The system incorporates virtual inertia. Releasing an asset with momentum allows it to slide naturally through the voxels and settle based on an OS-level spatial friction coefficient.

---

## 🌀 Volumetric Workspace Management: Portals & Teleportation

3OS bypasses the slow, physics-based fishing-line-reeling or lassoing of legacy XR by utilizing **Non-Euclidean Voodoo Volumes**.

* **Voodoo Volume Portal Dragging:** If a double-proxy voodoo volume is open, dragging an object from the near field into the voodoo volume teleports that object out into the far-field. Vice versa, dragging an object out of a voodoo volume teleports it from its far field volume into the near field. The need to grab onto a far field object with raycast and reel it in is entirely eliminated. The concept is heavily inspired by the Poros Framework (see Prior Art #4).
* **Cross-Proxy Portal Dragging:** If multiple double-proxy voodoo volumes are open, dragging an asset out of one local volume and dropping it into another triggers an instantaneous teleportation from volume A to volume B in the far field, essentially letting the object travel through two wormholes at once.
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

# 🏛️ 3OS_Core: Academic Lineage & Prior Art

Piles of incredible Human-Computer Interaction (HCI) literature have been left to languish by the modern spatial computing industry. In 2026, the following papers do not currently influence OS interfaces running on Meta Quest, Meta VR Glasses, Apple Vision Pro, Snap Specs, XReal Aura, Vive, Pico, Playstation VR, Lynx, Varjo, Bigscreen, or Steam Frame. They should.

By grounding **3OS** in this seminal research, we bridge the historic gap between the late-1990s proprioceptive research labs and modern non-Euclidean spatial interaction theories to break the productivity chains of WIMP.

---

### 1. Worlds in Miniature (WIM) — 1995
*   **Citation:** Stoakley, R., Conway, M. J., & Pausch, R. (1995). "Virtual reality on a WIM: interactive worlds in miniature." *Proceedings of the 22nd annual conference on Computer graphics and interactive techniques (SIGGRAPH '95)*, 265–272.
*   **Significance to 3OS:** This paper pioneered the paradigm of scaling down a massive, immersive 3D environment into a hand-held, near-field miniature copy to bypass the physical limitations of far-field targeting. It is the original world-to-user proxy, a voodoo doll of the entire virtual space. 3OS modernizes this concept into the **World Proxy Half-Dome**, updating it to track empty volumetric coordinate spaces and/or objects within those spaces.

### 2. Proprioceptive Interaction Models — 1997
*   **Citation:** Mine, M. R., Brooks Jr, F. P., & Sequin, C. H. (1997). "Moving objects in space: Exploiting proprioception in virtual-environment interaction." *Proceedings of the 24th annual conference on Computer graphics and interactive techniques (SIGGRAPH '97)*, 19–26.
*   **Significance to 3OS:** Proved that humans possess a highly accurate, innate biological awareness of their hands' positions relative to their body frames without requiring constant visual feedback (**proprioception**). This serves as the scientific bedrock for the **3OS Drummer's Law of Biomechanical Subordination**—keeping the shoulder girdle (coracobrachialis) completely neutral while the wrist and fingers drive intent inside near-field workspaces, i.e. peripheral remote controls in the near field manipulate objects in the far field.

### 3. The Foundational Voodoo Dolls Paper — 1999
*   **Citation:** Pierce, J. S., Stearns, B. C., & Pausch, R. (1999). "Voodoo Dolls: Seamless Interaction at Multiple Scales in Virtual Environments." *Proceedings of the 1999 symposium on Interactive 3D graphics (I3D '99)*, 141–145.
*   **Significance to 3OS:** Introduced the first dynamic object-proxy architecture, allowing users to instantiate scale-adjusted, hand-held replicas ("dolls") to manipulate distant assets bidirectionally. 3OS adopts this fundamental workflow but eliminates its rigid, skeuomorphic, object-bound limits—expanding proxies to target unconstrained voxel coordinate structures rather than just pre-existing 3D models.

### 4. The Poros Framework — 2021
*   **Citation:** Pohl, H., Lilija, K., McIntosh, J., & Hornbæk, K. (2021). "Poros: Configurable Proxies for Distant Interactions in VR." *Proceedings of the 2021 CHI Conference on Human Factors in Computing Systems (CHI '21)*, Article 357, 1–13.
*   **Significance to 3OS:** The direct conceptual and geometric peer to 3OS. This paper broke the legacy "Flatland" window paradigm by showing that users can select arbitrary volumetric bounding regions in the far field and mirror them as non-Euclidean near-field workspace portals. 3OS adapts this concept for bimanual selection frustums that are cubic instead of spherical, plus 3OS combines world-in-minature with these portals for  more rapid cross-proxy multi-portal manipulation primarily intended as a 3D corolary to a 2D desktop File Explorer.

### 5. Reality Proxy — 2025
*   **Citation:** Al-Sadoun, A., et al. (2025). "Reality Proxy: Fluid Interactions with Real-World Objects in MR." *Proceedings of the 2025 CHI Conference on Human Factors in Computing Systems (CHI '25)*.
*   **Significance to 3OS:** Evaluated and validated that using spatial computer vision and semantic machine intelligence are crucial for streamlining the associations between world mesh geo and virtual content that fully utilizes the context of that geo.

---

## 🧬 Architectural Synthesis

3OS acts as the ultimate engineering synthesis of these two historical schools of thought:

```text
[1990s: Proprioceptive School]  --> Mine, Pierce, Pausch
                                    Exploited near-field body space to drop motor fatigue.
                                              │
                                              ▼
[2020s: Volumetric Portal School] --> Pohl, Hornbæk, McIntosh
                                    Proved non-Euclidean workspace remapping matrices.
                                              │
                                              ▼
[3OS Operating System Engine]   --> 3OS Core Kernel Architecture
                                    Synthesizes both tracks AND applies the unified theory to the entire field's voxel grid whether any given voxel is occupied or not, which finally unlocks the same near-instantaneous control over every voxel as a PC user has over every pixel without requiring the hands to move more than a few inches total before returning to fully relaxed rest. 
```


## 🚀 Getting Started

```bash
# Clone the 3OS Volumetric Kernel Architecture
git clone https://github.com/your-repo/3OS.git

# Navigate to the core velocity mapping and proxy pipeline engine
cd 3OS/core/kinematics
```

*This project is dedicated to breaking the flatland illusion. Welcome to true volumetric computing.*