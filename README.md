# 3OS: A Volumetric Operating System Interface

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![OpenXR](https://img.shields.io/badge/API-OpenXR-orange.svg)](https://www.khronos.org/openxr/)

Johnny Mnemonic showed a 3D VR replacement for 2D desktop computers in 1995:
<a href="https://www.youtube.com/watch?v=SoxGazX_kkw" target="_blank" rel="noopener noreferrer">
  <img src="mnemonic.jpeg" alt="Johnny Mnemonic — 3D VR internet (1995)" />
</a>

This movie was shy of the mark of dreaming up an actual usable interface, but it hinted at a 1990s sense of optimism toward 3D computer interfaces eventually becoming better, faster, and more efficient than 2D desktop equivalents. Obviously that didn't happen. YET. What happened to that optimism though?

Was it proven to be bunk? Are 3D interfaces destined to be slower than desktop PCs? Or did we stop investing in the VR industry for a decade after the dot-com bust, allowing mountains of incredible university research to collect dust in SIGCHI archives, never to be drawn upon again by modern industry titans? Until...

---

<a id="what-is-3os"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">🧊 What is 3OS?</summary>

3OS (pronounced **"three-oh-ess"**) is inspired by cult classic sci-fi flicks like Johnny Mnemonic, The Matrix, Minority Report, Iron Man, and The Expanse, but it also grounds its efficacy in our rich history of HCI research.

3OS is a voxel-first operating system interface that attempts to take us back to our roots by drawing largely upon nearly-forgotten 90s HCI studies (see the Prior Art section). It aims to be the first 3D interface that's faster than desktop PCs for enterprise teams using the HCI road less traveled (A.K.A. NOT raycast cursors). Target hardware is HMDs, holographic displays, on-prem edge VDI servers running surround-spectator monitors, and any other future hardware that enables stereoscopic depth visibility plus integration with legacy 2D screens.

The 3OS endgame is to allow file management, version control, collaboration, communication, presentation, and all forms of complex multimedia development to coexist within a single 3D field (or collection of superimposed fields) supporting multiple simultaneous workspaces in the same room, controlled by one interface, powered by an on-prem server supporting up to 12 simultaneous users in any number of workspace groups as they all face the center of a room rather than facing a wall all day with their backs to each other. 

The 3OS Enterprise Server prototype is very specifically <b>NOT</b> a conference room. It's not for meetings. It's for simultaneous collaborative and solo development. It can be used to present work progress, but a majority of the time it is best dedicated to the 12 workers working in small teams together to complete a large range of tasks involving 3D assets. One user can work in a solo workspace using the same space as 11 other users who are in a separate shared workspace. Early adopters are teams of developers and artists in product dev, engineering, VFX, game dev, architecture, auto, aerospace, etc. 


</details>

---

<a id="wimp-is-wimpy"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">🪟 WIMP is wimpy</summary>

3OS rejects the default bias of legacy WIMP (Windows, Icons, Menus, and Pointers) and multi-touch paradigms that have been a crutch in modern XR for far too long. Legacy paradigms are supported for full real-time integration, but they are not the primary interface. 3OS aims to knock the dust off of the works of HCI giants whose volumetric contributions fly circles around WIMP, yet their research got buried by the fervor of the mobile era's obsession with minimalism. 

<b>Minimalism has its place in consumer software.</b> For enterprise, it's time to unbolt the training wheels of minimalism to prove that 90s 3D UI optimism was not misplaced.

The WIMP paradigm is a backwards compatibility tool in XR. It should not be front-and-center in any spatial computing OS. A VR or MR HMD that uses WIMP as its default metaphor set is like Windows 95 launching into a command line with no Desktop, no Start button, and no File Explorer, except at least the command line still brought extra speed for power users. WIMP interfaces do the opposite in XR. Their primary value is familiarity comfort, but their incredible UX latency baggage comes with it. Even so, they're currently the ONLY XR OS interface available in 2026.

We cannot expect enterprise software suite companies to consider porting their robust systems into XR using stone tools. Their customers want what's "cool" in sci-fi. Their customers want to be Iron Man, tossing holograms around the room. 3OS aims to prove they can have that dream in a way that makes them faster at their jobs, not slower, without uprooting their workflows from any legacy 2D software that must remain 2D.

</details>

---

<a id="3os-basics"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">📐 3OS Basics</summary>

Instead of forcing users to aim virtual laser pointers at floating 2D windows, 3OS treats the entire environment as a high-capacity voxel field where the parameterized resolution defaults to a 3-inch voxel, the approximate size that satisfies high selection accuracy at rapid speed with 6DOF 10-finger hand motion (see the Trivariate validation below). The goal is to provide users a voxel grid representing all the subdivisions of 3D space in which they can hover, select, or otherwise interact with any voxel in under 0.25 seconds regardless of their body's distance away from that voxel. This is an impossible feat in all current spatial computing operating systems because of the false assumption that OS interfaces must use WIMP.

At 3 inch voxel resolution and a default 50-foot radius from head pose to the grid's outer shell, the grid contains on the order of ~15 million voxels if the user is standing in a wide open field. Theoretically, it could be larger as some viewers have accurate parallax stereo vision up to several hundred feet, but 3-inch objects side-by-side shrink into uselessness via perspective long before that distance, thus 50ft is a decent parameterized ballpark limit.

3OS is built in OpenXR and is intended as a module to plug into any spatial computing OS, or in its early days it can simply run as a lightweight streaming client connected to an authoritative VDI server in an enterprise setting. It is fully open-source under the MIT license, and it does not replace existing OS infrastructure, since WIMP-based interfaces still need to be installed for backwards compatibility, just as Windows 95 couldn't launch without command line dialogs.

The 20-teens era of spatial OS design is the Xerox PARC era. The Windows 95 era is almost upon us. 3OS attempts to help clear the brambles of what an OS interface may look like in this new era. It can plug into a current OS and become the default, or it can become an optional alternative interface. If you like 3OS and you want to see it installed in the OS of your favorite HMD, email that company, share the repo link, and let them know the code is free!


</details>

---

<a id="the-problem"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">⚠️ The Problem</summary>

Software companies (Adobe, Autodesk, Epic, NVidia, etc.) assess whether they should port their legacy platforms into a new OS frequently. Their software is typically web-browser first for cross-platform efficiency, but even with cross-platform apps, the most robust productivity tools in these apps typically run on the desktop version only.

**Why?**

These companies understand that touchscreens are more of an entertainment consumption platform than an enterprise productivity platform. There are exceptions, but a majority do not port all the desktop features of enterprise software onto mobile devices because...

**Multi-touch interfaces...**

1) ...are physically smaller. Shrinking the number of available interactive fragments (especially when hands are constantly occluding some of them) reduces multitasking capability exponentially, hence why iOS requires developers to severely limit the number of user choices per screen controller.

2) ...cannot compete with desktop interfaces due to mouse/keyboard blowing multi-touch out of the water on throughput.

3) ...actively discourage manual raw data management by obfuscating direct data access, implying that some OS makers do not want the developer community thinking of touchscreens (or other new platforms) as capable of cannibalizing desktop/laptop sales through competitive levels of productivity. 

It follows, though, that if you INCREASE the number of interactive fragments (or voxels) instead of decreasing it, shortened total adjacency and multitasking capability should explode exponentially beyond the capability of the best desktop interfaces. Enterprise workers understand the simple virtue of multi-screen setups, but their value barely scratches the throughput surface of multiple screens times multiple layers of depth, IF we use those layers.

### Why then is the boot interface of EVERY XR OS patterned after Apple and Android?

The author of 3OS suspects it's primarily because companies like Meta and HTC hired the smartest people in the world on the subject of UI, and they were too smart for their own good. People who have their doctorates in HCI, bless their hearts, probably told everyone at these companies that raycast has 2X the throughput of any other option in XR, which is true, therefore everything BUT raycast is DOA. Case closed.

That case needs to be reopened. They were right about the data, but wrong about the conclusion. Raycast discourages bimanual interaction, so its 2X throughput is cancelled by nearly always being used one-hand-at-a-time, whereas volumetric isometric models feel effortless with two hands. Add to that the fact that raycast squashes the field to a plane, destroying the entire value proposition of XR as a productivity booster when it can only use 10% of its own adjacency and multitasking potential, and here we are 10 years after the launch of modern VR, and we still can't copy/paste two files into a folder at once.

The author of 3OS doesn't blame anyone on the design teams at Meta or Apple or Steam or Microsoft. If the path to becoming Iron Man were easy to see, thousands of us would have built it and no one would care, because it was just too easy. But our best UI/UX scientists so far have followed in the footsteps of Steve Jobs and Jony Ive with their best efforts, which implies "this XR platform can't do the things your desktop PC can do."

If you try to complete nearly any run-of-the-mill office PC tasks on any XR OS, common features make progress so God-awfully slow that no enterprise company would ever consider porting their desktop features into XR in the 2020s. They don't believe XR is viable for work because the boot interface told them it's not.

XR has greater enterprise potential than desktop computers. This will be obvious as soon as the right OS interface enables the platform to shine with all of its advantages. AI voice-based interfaces are part of that story, but we can't always speak our work into a computer. Sometimes, work needs to be more private, even among colleagues in a shared office. We can't adopt XR in the office if the interaction model slows us to a crawl in these private moments. Thus, XR must present an interface that is faster than desktop interfaces WITH OR WITHOUT an AI assistant.


</details>

---

<a id="the-solution"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">💡 The Solution</summary>

The "right" XR interface must solve the three glaring challenges that keep XR interfaces slow:

**1) No Near-Field/Far-Field Parity:** The user has instant intuitive access to the near field, but extremely slow/cumbersome (if any) access to the far field. Since the far field is more than 90% of the whole field, the total productivity canvas in XR ends up smaller with lower throughput than its desktop equivalent despite having exponentially more usable voxels than 2D has usable fragments. A 2D desktop screen has hundreds of pixel fragment zones in which to place colocated buttons, text labels, images, etc. with rapid access to every one of them at once.

XR has millions of interaction zones in each frustum which would enable adjacency efficiency and multi-tasking that blows desktop interfaces out of the water, but most of these zones are unreachable without silly raycast fishing reel actions, an impossible wrench in the gears for throughput. Imagine requiring 30 seconds to click an icon in Windows. Furthermore, we can't easily see how ridiculously slow our XR interfaces are because this throughput drag is hidden by #2.

**2) WIMP Insufficiency:** The crutch of WIMP denies the use of the depth vector in concert with common 2D table layouts (i.e. WIMP makes 3D matrix and cluster layouts impossible), which occludes the vast potential utility of the XR field almost entirely, which in turn masks the throughput jank of the default raycasting interaction model. Raycasting successfully hides how terribly slow it is by forcing us to drive our Ferrari at 5mph as though that's how fast all racecars drive. A model that can enable hands or cursors to hover in any subdivision of the field with equivalent effort must replace the legacy model. 

Raycast has won the XR OS implmementation battle so far by putting up HCI speed test numbers that double volumetric isometric competitors only because the tests in research studies are never done apples-to-apples according to real-world applications. In the real world, users only use raycast with one hand, not two, and apples-to-apples interfaces built to take full advantage of adjacency have rarely been tested in HCI studies at all. The author of 3OS can build apples-to-apples tests that prove raycast actually has slower throughput in the real world than volumetric isometric techniques under fair criteria, and that's before accounting for the value of volumetric isometric models having access to every depth layer of the far field simultaneously.


**3) Perspective vs. Isometric Spatial awareness:** We see in perspective, but perspective can warp our mind's eye of Euclidean space. Our short arms define the near field where perspective causes almost unnoticeable size warping, thus our mind's eye for hand coordination is formed isometrically. We think of 3D space the way it actually is... true width, depth, and height have an equal relationship, not a scalar one. We know the object moving away is not actually shrinking, but if we need to use a magical interface to reach out and touch that object, we should not expect our hands to easily convert to angular space where farther objects require scaled hand motions. To access the far field with equivalent efficacy to the near field, we need to use an volumetric isometric interaction model, one that treats hand motion in the near field identically to the far field.

Long story short - an XR OS interface that works at rapid speed for enterprise MUST use a voluemtric isometric model, NOT a raycast model. It must be volume-based, not surface-based. Raycast has no chance of achieving speeds competitive to 2D desktops. Volemtric isometric models can beat desktop speeds if they're built to careful specs. Solve this problem, and enterprise adoption of XR can begin as soon as we have sunglasses-form-factor HMDs capable of MR and ergo-safe hand tracking (e.g. hand tracking FOV of Quest).

</details>

---

<a id="3os-core-implementation-philosophy"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">🏛️ 3OS Core Implementation Philosophy</summary>

1. **Voxels, Not Surfaces:** The fundamental building block of UI/UX in 3OS is a discrete coordinate volume (optimized at $\approx 3\text{ in}^3$ for reliable human motor capability under rapid hand motion), not a pixel fragment or a flat projection plane. Everything that was surface-based in WIMP is assumed volume-based in 3OS. Where WIMP gave us Windows, Icons, Menus, and Pointers, 3OS gives Volumes, Objects, Matrices, and Points. 3OS is a VOMP interface.

2. **The Drummer's Rule:** 3OS volumes adhere to strict ergonomic criteria. A spatial OS should never subordinate micro-precision tasks to macro-stability muscles. First and foremost, this means the user should never be required to walk across a room to engage with an object simply because that object is outside the near field. The lost energy and time of closing the gap to bring the object into reach is a cardinal enemy of productivity. In 3OS, we compress far field spatial volumes into near field proxies called Voodoo Volumes instantaneously so that interaction is entirely driven by high-speed, low-fatigue motor groups (eyes, wrists, fingers), keeping the shoulder girdle (**coracobrachialis**) kinetically neutral for a vast majority of each session. The user can walk during use of the interface if the interface is anchored to their body rather than to the room, but large full-body "walk-across-the-room" actions are never required to manipulate a given element. At most, ten-finger or 6DOF-controlled actions require the elbow to move no more than 1.5 inches out of its neutral pose, and developers should encourage the elbow to return to neutral pose for a majority of any experience. This rule is derived from the Moeller Method that enables drummers to play for hours without fatigue. This methodology comes from the author's personal experience as a drum teacher relating drum student arm fatigue to common "gorilla arm" issues in HCI research.

3. **Topology vs. Kinematics:** 
Spatial interaction in 3OS is comprised of...
    **Voodoo Volumes** (macro-topology; managing *where* data lives via instant coordinate remapping) and...
    **Telekinesis** (micro-kinematics; managing *how* data is moved on smaller scales via velocity-controlled telekinesis). Telekinesis and direct grabbing transition seamlessly as soon as an object enters or exits the near field. 
These two methods follow 80/20 Pareto distribution, where most objects occupying field space can and should be teleported into position via Voodoo Portals, though the user may occasionally desire to shift them telekinetically if the distance is fairly short.


</details>

---

<a id="ergonomics-determine-interactivity-resolution"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">✋ Ergonomics Determine Interactivity Resolution</summary>

**The limits of the body determine ideal interaction volume bounds:** 

The Trivariate Target paper by Grossman and Balakrishnan was instrumental in determining how large the voxels should be for fast-moving hands in a voxel-based OS. The default assumption in 3OS is that hands will be moving rapidly at various moments within the bounding box of a given voodoo volume. The constraint of elbow position for "gorilla arm" prevention requires a specific bounding box size for that volume. That size is determined by beginning with the hands as close to complete rest as possible while still being visible to the HMD for tracking, which places them at the height of the navel, 12 inches forward from the body with elbows completely at rest in their neutral hanging position beside the body.

**Derived Interaction Volume Parameters (Body Anchor/Height/Offset):**

With palms up or down, the hands define the bottom plane of the ideal voodoo volume bounding box in this navel-level position. The center of the bounding box is approximately 12 inches forward of the navel, allowing the user to place fingertips in the center of the box's bottom plane simply by touching their fingertips together with both hands. This is not a gesture to instantiate the box. It's merely an imaginary reference to show approximately where the box anchor lives if the user instantiates it directly forward of the stomach. The user can instantiate multiple voodoo volumes side-by-side. 3OS may use IPD and/or user height to estimate the ideal offset of the voodoo volume's center anchor. A height of approximately 12-18 inches above that center point constitutes the total height of the voodoo volume bounding box because lifting fingers above that height immediately overtaxes the coracobrachialis muscle, which connects the collar bone to the bicep bone. This muscle can handle hours of flex below the height of eye-level fingertips, which gives us approximately ~18 inches max for the voodoo volume height. The box may have a small offset away from the body defined by 3OS near-field point cursor parameters. For instance, if the near-field cursor offset in front of the hand is 3", then the voodoo volume should also be offset forward of the navel by three more inches.

**Research-backed Voxel Size:**

See [spatial_os_trivariate_validation.md](spatial_os_trivariate_validation.md) for proof that 3" voxel size is ideal for any spatial computing OS in which users want to move hands voxel-to-voxel without overshoot, yielding an ideal 3×3×5 matrix as our default voodoo volume. Objects can certainly be bigger and smaller than this resolution, but the underlying default size of objects and spaces between objects if grid-snapping is ever in use will follow this format, and developers of 3OS apps must note that if they want to unlock the fastest possible user hand motion, they should build elements no smaller than this approximate target. 3OS parameterizes all of these default assumptions.


</details>

---

<a id="the-3os-unified-interaction-state-machine"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">🔄 The 3OS Unified Interaction State Machine</summary>

To eliminate physical exhaustion ("Gorilla Arm") and sluggishness of legacy XR selection systems, 3OS routes all user intent through a deterministic, cascading three-stage state machine.

```
[State 1: Macro-Targeting] ──> [State 2: Micro-Resolution] ──> [State 3: Possession / Transform]
(World Proxy Half-Dome)         (Double Proxy Voxel Grid)       (Velocity-Mapped CRUD Action)
```

### State 1: Macro-Targeting (The World-in-Miniature Proxy)
* **The Problem:** 

  Humans cannot easily gaze at or raycast an empty voxel in which to instantiate or manage far field elements. A single stationary gaze vector penetrates thousands of voxels simultaneously. The ability to choose any one voxel in the entire field must be rapid and intuitive.
* **The Solution:** 
  
  Via gesture or button press, the user instantiates a localized, downscaled translucent **World Proxy Half-Dome** within comfortable, nearly-at-rest reach. 

### State 2: Micro-Resolution (The Double-Proxy Voxel Lattice)
* **The Mechanism:** 

  The moment a coarse region is selected in the World Proxy Half-Dome, that specific slice of space instantly blooms into a **Double-Proxy Voxel Grid** directly in front of the user's primary workspace. The chunk of empty space is proxied to the user. Simultaneously, the user's hand or cursor is proxied out into that chunk in the far field.
* **Lattice Visualization:** 

  The hidden coordinate structure of that far field volume is rendered locally as a subtle, wireframe volumetric lattice.
* **Decoupled Selection:** 

  The user drives a point cursor that is a 1:1 Euclidean replica of the pinch midpoint (not parented to the hand), then offset **4″** along floor-parallel user-forward and **2″** up, so the hand does not occlude the proxy. The user is free to gaze at the actual fragment in the far field, using the peripheral proxy as a remote control device, or if the fragment is too far away for good visual fidelity, the user can look primarily at the proxy instead. Cursor hover and selection can initiate CRUD actions, especially CREATE functions if the current proxy contains empty voxel space. Menu options e.g. Create New Box (3D equivalent of a folder) or Create New Object (3D equivalent of a file) are typically presented in matrix layout as long as there are more than four options.

### State 3: Possession & Transformation (Telekinesis Engine)
* **The Mechanism:** 

  An already created object can be hovered by sending a point cursor inside its volume rather than raycasting onto its surface. Once hovered, the user can select it with a gesture, usually index pinch. Once selected, the user has the option to use the proxy (a voodoo doll) as a transformation remote control for **Velocity-Based translation and/or rotation**.
* **Kinetic Economy:** 

  Tiny, low-fatigue wrist gestures generate acceleration and velocity vectors that scale proportionally, gliding objects effortlessly across the field.
* **Momentum & Friction:** 

  The system incorporates virtual inertia. Releasing an asset with momentum allows it to slide naturally through the voxels and settle based on an OS-level spatial friction coefficient.


</details>

---

<a id="volumetric-workspace-management-portals-teleportation"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">🌀 Volumetric Workspace Management: Voodoo Volumes</summary>

3OS bypasses the slow, physics-based fishing-line-reeling or lassoing of legacy XR by utilizing **Non-Euclidean Voodoo Volumes**.

* **Voodoo Volume Dragging:** If a double-proxy voodoo volume is open, dragging an object from the near field into the voodoo volume teleports that object out into the far-field. Vice versa, dragging an object out of a voodoo volume teleports it from its far field volume into the near field. The need to grab onto a far field object with raycast and reel it in is entirely eliminated. The concept is heavily inspired by the Poros Framework (see Prior Art #6).
* **Cross-Proxy Voodoo Volume Dragging:** If multiple double-proxy voodoo volumes are open, dragging an asset out of one local volume and dropping it into another triggers an instantaneous teleportation from volume A to volume B in the far field, essentially letting the object travel through two wormholes at once.
* **The World-In-Miniature Drag/Drop:** To move a near field object out into the far field rapidly, the user need not open a voodoo volume first. The user can activate the object as though it itself is a voodoo volume, which allows a simple drag/drop gesture to send the object anywhere into the far field via the same technique as creating a voodoo volume.


</details>

---

<a id="volumetric-topology-system-architecture"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">🗂️ Volumetric Topology & System Architecture</summary>

### 1. The Selection Frustum
Multi-selection is resolved via drag to scale a dynamic bounding **Selection Frustum** inside a near-field voodoo volume. The possessed group behaves rigidly, preserving relative spatial offsets during velocity-mapped transformations.

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

### 4. Field Subdivision and Layout
The near field is defined as the capsule surrounding the user whose radius is approximately matched to the user's fingertip reach in any direction while standing on the same spot.

The far field is non-uniform, but it can often be logically and conceptually subdivided especially during instantiation of large numbers of objects that must align in matrices and also avoid collision with each other. The rule of thumb for incoming objects is to instantiate in the near field if the total number of objects is fewer than 6 and the near field is clear of potential collisions. Otherwise, priority instantiation happens at ground level, and matrices lay out isometrically relative to themselves but concentrically relative to each other. For example, if two dozen matrices each containing 125 objects must instantiate simultaneously, they would do so in a panorama of 5x5x5 matrices side by side in the far field as close as possible with adequate padding and margins.

Until extensive UX testing can be done on densely populated fields, it is assumed that the maximum number of objects that can occupy an entire single field is 1.5 million. We back into this napkin math by assuming a field of cylindrical shape could fill the ground level with matrices starting with concentric rings from the user out to the radial limit of the field, then stack multiple more layers of matrices on top until the field is full. At the ideal minimum size of 3" for each voxel, we assume the objects should not be smaller, and we assume that ~90% of the voxel space is always dedicated to padding and margins between the objects. A field of 50' radius and 30' height can accommodate ~15 million voxels, thus ~1.5 million objects completely fill that field. It is less than ideal to fill an entire field and leave no breathing room to utilize extra empty space just as it is less than ideal to fill a 2D desktop with hundreds of icons until there is no room to move an icon anywhere. Thus, we assume, the functional working range of total number of objects in a field can be measured in six figures at maximum capacity, though most users will probably opt for sparse fields in the three-to-four figure range since they can jump from one field to another as easily as a change-directory command in a CLI.


</details>

---

<a id="architectural-enclosure-world-mesh-penetration"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">🧱 Architectural Enclosure & World Mesh Penetration</summary>

3OS strictly decouples Newtonian physics from spatial containment boundaries to preserve environmental stability and presence. These rules apply to all data structures the OS recognizes. These structures are analogous to files and folders of other operating systems, but they use different metaphors in 3D. For example, folders are boxes in 3OS. Files are objects whose low-poly prismatic geometry is defined by the application that generates the data, much like applications define file types and icons in other operating systems.

* **Global vs. Local Mode:** 3OS is in global mode by default, meaning if the user opens a box (i.e. drills into a folder) the contents of that box will emerge out of the box without dismissing the contents of the parent file structure. If the user opts to toggle global mode to local mode, then opening a box dismisses the objects at the parent level before revealing the contents of the box.
* **Rigidbody Matrix:** Set to `Disabled` by default for each object. The user can opt in a given object's properties to toggle physics on for that object, allowing it to receive gravity, collide with world geo, etc. Newly created objects float statically in zero-gravity space to eliminate continuous structural management chores. For OS objects, the user must manually turn gravity on if desired, but they should understand that full use of 3OS capability means allowing most objects to float.
* **Enclosure Engine:** Active by default with a `Strict` policy. Leveraging semantic AI mesh pipelines, the system enforces a hard spatial floor constraint—preventing floating voxels from clipping through physical floorboards or sinking into unreachable space even if their rigidbodies are disabled or they have none.
* **Semantic Masking:** For walls and ceilings, users can unlock constraints to place items "over-the-horizon" (e.g., a weather widget on a balcony). The OS cleanly cuts the rendering mesh at the wall boundary plane, creating a crisp, non-clipping partition illusion. 3OS has parameterized input enabling other service-layer systems of the HMD to provide planar definitions of such boundaries if desired.


</details>

---

<a id="common-3d-element-types"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">🎲 Common 3D Element Types</summary>

### 1) Objects
  3OS is intended for full cross-compatibility with desktop OS interfaces because the file metaphor remains intact in an extruded 3D-ified form. A file is an OBJECT in 3OS. A folder is a BOX. Their icons are no longer icons. They're glyphs, low-poly prismatic pieces of geometry that can represent the brand of the app that generated the object. In addition, 3OS will support treating media as objects as well, including audio clips, video clips, images, 3D assets, and point-cloud-based assets (particle effects and/or Gaussian splat assets).

### 2) Glyphs (`.3glyph`)
  A **glyph** is a specific type of object, a volumetric file type icon: a small, semi-transparent mesh plus associative metadata in one file. Packs are valid **glTF Binary (GLB)** renamed to **`.3glyph`**, with a root vendor extension `THREEOS_glyph` (schema, injector, and demo packs: [`docs/THREEOS_glyph.md`](docs/THREEOS_glyph.md)).

  **What lives in the pack (author-controlled):** mesh, UVs, textures, and `THREEOS_glyph` metadata (id, associations, tint hints, bounds). End users and companies are free to author branded looks with full UV/texture control inside the GLB.

  **What does *not* live in the pack:** host shaders. Packs must not embed Unity ShaderLab, Vulkan SPIR-V, or any other renderer-specific shader bytecode. The OS/host binds the current glyph material at load time.

  **Host shader (adapter, not pack content):**

  - **Unity alpha / demo:** instances use **`ThreeOS/Glyph`** ([`3OS_Unity/Shaders/ThreeOSGlyph.shader`](3OS_Unity/Shaders/ThreeOSGlyph.shader)).
  - **Later native hosts:** the same shading *contract* (translucent base + selection Fresnel) can be reimplemented in Vulkan (or Metal/D3D) without changing `.3glyph` assets—mesh, UVs, and textures keep working.

  The Unity shader is a single `SRPDefaultUnlit` pass (URP only runs one matching LightMode): translucent `_BaseColor` plus white Fresnel edge glow scaled by `_Selected` (0 = off, 1 = on).

  Selection is an OS affordance: the host sets `_Selected` (usually via `MaterialPropertyBlock`) and does **not** swap materials. Authors do not ship a separate “selected” material. Custom packs keep their mesh, UVs, and textures; the OS still owns the highlight so every third-party glyph gets a consistent stereo-safe selection look.

  **Authoring a `.3glyph` pack**

  1. Model a low-poly prismatic glyph in Blender (keep scale near the OS glyph size, ~0.1 m); author UVs and textures as desired.
  2. Export **glTF 2.0 Binary (`.glb`)** (textures embedded or referenced per normal glTF practice).
  3. Write or copy a pack JSON under [`docs/glyph_packs/`](docs/glyph_packs/) (`glyph_id`, `applies_to_extensions` or Box `box_state`, `tint_rgba` with alpha &lt; 1, etc.).
  4. Inject metadata:  
   `python tools/inject_3glyph.py path\to\mesh.glb docs\glyph_packs\<pack>.json`  
   → writes `path\to\mesh.3glyph` (still GLB bytes).
  5. Register the pack with the kernel at host startup (`threeos_glyph_install_pack` / Unity `InstallGlyphPack`).
  6. In the Unity host, assign materials that use **`ThreeOS/Glyph`** (or let `ThreeOSStorageController` create them). Drive selection with `_Selected` = `0` / `1`. Look-dev Fresnel via **Create → 3OS → Glyph Glow Settings** (assign on `ThreeOSStorageController`, or save as `Resources/3OS/GlyphGlowSettings.asset`).

  Content files (e.g. a user’s `report.pdf` or demo `simple.glb`) are **not** glyph packs — they keep their normal formats. Only the OS icon that *represents* that type is a `.3glyph`.

### 3) Boxes
  Boxes in 3OS are the 3D equivalent of folders in legacy platforms. They can expand/collapse all of their contents into the current field, and they are the control source for a variety of spatial arrangements. 3OS has a global box mode and a local box mode, meaning expansion of a box can automatically trigger collapse of any currently-expanded other boxes in local mode, or expansion can be cumulative with all other expanded boxes in global mode. 3OS deals with spatial collision resolution of nearest neighbors by bumping newcomers into the closest available adjacent space. Global and Local box modes are only relevant in Manual Arrangement Mode. In Matrix and Cluster Modes, nested child boxes expand to fill space in separate matrices or clusters above the parent matrices or clusters.

  **Options** (equivalent of View options in File Explorer):

  * **Manual:** All objects within the box store their own list of world anchors if they each recognize the current environment; otherwise they default to a matrix layout near and above the box on expand. This is the only mode in which the object's spatial position is stored within the object and recalled in future sessions.
  * **Matrix:** All objects default to a matrix layout near and above the box.
  * **Multi-Matrix:** All objects default to a matrix of matrices categorized by a chosen attribute (file name, application, age in years/months/weeks/days, etc.).
  * **Cluster:** All objects default to a cluster layout near and above the box.
  * **Multi-Cluster:** All objects default to a cluster of clusters categorized by a chosen attribute.
  * **Agent-Generated:** All objects default to a custom 3D layout generated by an AI assistant per user instructions, including advanced filtering, combinations of clusters and matrices of multi-condition criteria, etc.


### 4) Voodoo Volumes
  These are the workhorse of 3OS, merging the near and far field via non-Euclidean portal behaviors. To engage one, the user can manifest a World Proxy Half Dome which represents a miniature model of the entire field. While holding the gesture, the user drags a point cursor through the interior of the Half Dome, and a far-field voodoo volume instantiates and moves in concert around the actual field. By releasing this same gesture that opened the Half Dome, the user chooses where to drop the far field voodoo volume. Upon placing it, a near-field voodoo volume immediately replaces the half dome, opening the other end of the portal. Now any contents or empty space in that part of the far field are suddenly in hand's reach in the near field. Any objects or boxes encapsulated by the voodoo volume now show a ghostly copy of themselves easily within the user's reach. Likewise, the user can place a point cursor or hand into the voodoo volume and a ghostly copy of their cursor or hand will appear out on the other end of the portal in the far field. The user is welcome to gaze at either the near-field voodoo volume or the far-field voodoo volume. The voodoo volume's bounding box can be pinched/stretched to scale the far-field bounding box while the near field bounding box remains the same ergonomically-perfect size. If an object in the volume is dragged in real far-field space, the far field bounding box will scale up to keep it in scope. Meanwhile, the bounding box of the near field volume will remain the same size, causing its inner contents to appear to scale down. The bounding box can also be single-hand dragged for velocity-based translation of the far-field bounding box. And finally, the voodoo volume will have a button at its top back right corner to collapse the portal.

### 5) Voodoo Widgets
  These are peripheral remote control devices. They appear temporarily just forward of the hand that engages the associated action, and they disappear as soon as the action is complete. The most common use is for remote transformations (dragging/rotating far-field objects). These widgets visualize the hand's current action state e.g. a vector arrow to confirm drag is currently active at a specific velocity. For rotation, the widget is a miniature voodoo copy the object so that any tumbling action can happen accurately even if the actual object is too distant to make out.

### 6) Focus Anchors
  These are the invisible backbone enabling voodoo snapping to work so rapidly. At the top level of 3OS, each open directory has contents potentially strewn about in a matrix or cluster, but a voodoo volume can latch onto that entire cluster instantly because it contains an invisible centroid, a Focus Anchor. Every directory has a focus anchor allowing voodoo volumes to snap to them and summon a voodoo doll of their contents into the near field for easy control. Focus Anchors are a key feature that will be available in the 3OS 3rd party SDK as well. When a 3rd party app launches, it is in multitasking mode by default, meaning it shares the same global field as everything else, including the file system and other open apps. But if the user sends a voodoo volume into the 3rd party app's bounding box, the OS won't have any default anchors to snap to, leaving the user to fish around the app's volume manually. Thus, it is important for 3rd party apps to define focus anchors early on because it enables the user to instantly summon access to any collection of elements within the app. This is a key advantage of 3OS because it enables the app content to BE the interface. If, for example, it's a CAD program, it can be designed such that every part automatically has a focus anchor, thus the developer can allow the user to pull/inspect/edit any part instantly without needing to code a custom UI. The OS already enables it.

### 8) Application Volumes
  In 3OS, applications exist within 3D volumes rather than within 2D windows. Unmaximized volumes default to the 2x2x3 (WxDxH) aspect ratio of the default 3OS voodoo volume. 3rd party developers can use the 3OS SDK to configure their app manifest to launch minimized or maximized. Unlike 2D windows that have three states (minimized, default, and maximized), 3OS app volumes have four states: minimized, default, max_MR, and max_VR. This is because an app maximized in MR is assumed to take priority over all available space in the room, yet still must respect the boundaries of the room as its bounding box, whereas in VR the app's volume can expand to encapsulate infinite space. 3OS can be toggled from MR to VR with a gesture, and applications can be launched in either mode, so the maximize button dynamically fills the available space in either MR or VR. Maximizing one app minimizes all other open apps, sending them each a message that their render pipelines and tick loops can sleep, and they can await a re-focus message in standby. Minimized apps retain memory allocation, but minimize and re-focus callbacks can be used to preform appropriate memory management for perf op.

### 7) 3D Buttons
  In 3OS, buttons are typically depicted as translucent cubes or other simple volumetric prisms, often with smoothed edges. Text labels may hover near, above, below, or inside the geometry, typically shaded over the top of their geo to enable clear legibility even if geo occludes any part of the text. Buttons in 3OS are volumetric, meaning cursors don't hover ONTO them; they hover INTO them. If touched directly by virtual hands, the button surface has no physics properties. The hand travels through it as though it is a ghost. We call it a button to avoid confusion around functionality. It is an event trigger handler just like any other button, but it does not reside on a surface, nor is it to be pressed. It floats in mid air, needing no flat panel to rest upon. A cursor enters into it from any vector and triggers a hover-state glow effect or other visual cue. During hover, the user performs a gesture, usually index-to-thumb pinch, to activate the button.

### 8) Matrices
  Elements in legacy UI lay out on X and Y dimensions in table form. In a voxel-based OS, utilizing even one more interactive plane ahead of or behind the first plane unlocks more grid positions with less travel distance. For instance, a 4x4 or 2x8 table of elements in a 2D UI is much better laid out as a 2x2x2 matrix in 3OS. A minimum adjacency analysis on any collection of items will show matrices are ergonomically better and faster than tables.

### 9) Clusters
  Similar to matrices, but spatial positioning is more loose in that the objects need not align on any kind of local Euclidean grid. Instead, they operate on a set of min-max adjacency parameters from their nearest neighbors, usually grouped in close proximity due to some shared attribute.

### 10) Spherical Multi-Choice Nodes
  A 3D-ified radio button element. 3OS supports easy creation of these spherical interfaces which are similar to desktop radio button lists in that alternate choices (A or B or C etc.) are arranged near each other, but in 3OS they can occupy uniform angular space around a center point. They may render as segments of a sphere, or each as 3D buttons floating in a formation. The cursor's vector relative to the center point changes hover state enabling highlight of one of the elements in that collection. This interface style is the highest-performance voxel-based multiple-choice option because it eliminates any possibility of overshoot errors. The multi-choice node may have an extra core node at its center which could be an extra multi-choice option or a neutral "none" option, and it may also have an out-of-bounds limit radius encapsulating the group.

### 11) Sync Planes
  Generally, planar UI in 3OS is discouraged. Default UI elements are volumetric and isometric, whereas planar elements typically require perspective-based raycasting for far field interaction, which is not impossible, but it must be custom-built case-by-case at the service and application layers. In general, if a planar UI exists in 3OS, it typically represents the virtual twin of a physical screen in an MR context. For instance, in a room with 12 2-monitor workstations mounted to its walls, any pair of monitors might be in use with WIMP-style UI elements on-screen. 3OS will be able to render that screen's output at any size via a sync plane, but more likely the sync plane will be invisible and superimposed over the physical screen it represents, enabling remote control interaction. Sync planes will feature API integration and transition between 2D and 3D elements. The WIMP instance running on the 2D screens would be rendered by the same server as the OS driving the eyewear, thus it holds an inventory of all shared interactive elements, which means an HMD user could use a voodoo volume to target the screen itself, or target one of its open windows, e.g. a File Explorer. Once the 2D File Explorer window is targeted, it would automatically be converted to its 3D representation in the near field in the same fashion as using a 3D-to-3D voodoo volume. This 2D-to-3D transition can work identically for any 3rd party software. For example, a control widget in a 2D Autodesk Maya window might be pulled into 3D MR space via a voodoo volume if Autodesk developers used the 3OS SDK to enable that functionality.


</details>

---
<a id="the-application-layer"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">🧩 The Application Layer</summary>

3OS is designed first-and-foremost for professionals who are already working in 3D editing software (CAD, VFX, and Gaming), but it is also designed to be able to grow into mass market adoption after establishing 3D professionals as early adopters. To approach early adopters, 3OS must natively support XR ports of a few dozen professional software suites.

At the advent of modern VR with the Oculus Kickstarter, millions of us dreamed of being able to use Maya or Fusion or Unreal from inside a VR headset, but so far to date, these systems don't feel compatible with HMDs for productive eight-hour work yet because there is always some part of the task flow that requires constant removal of the HMD for fallback to a desktop interface, not to mention the ergonomic issues of bulky headsets worn all day.

Hardware engineers are doing their part to shrink the technology to near-sunglasses form factor. As we approach elimination of this ergo bottleneck, it's well-past time for the OS layer to have provided a uniform 3D UI paradigm capable of enabling all 3D software compatibility, including CAD, VFX tools, and game engines to run in a cohesive single environment with multitasking (e.g. Maya and Unreal both running in a shared field). 

3OS encourages 3rd parties not to think of their ported app as a floating window. In 3OS, floating windows are like command-line tools. Power users may need them occasionally, but most users should prefer engaging directly with the 3D content. For example, it may be challenging to picture all of Autodesk Maya's many features working without the panoply of sidebars and menus and dropdowns, but it can be done with a little imagination. It is likely to include conversion of 2D sidebars into 3D widgets that are summon-able per-context relative to the content itself. Widgets are also achnorable to any rolling standing desk as a feature of the 3OS Server spec, but for the most part, <b>the content itself IS the interface.</b>

3OS aims to support XR app multitasking despite its immense perf op challenges. The first professional deployments of 3OS are intended to run on on-prem VDI servers with plenty of horsepower to handle the job, and 3OS aims to provide an SDK for enterprise 3rd parties to finally port their apps natively into XR.

### Deep Agentic Integration

3OS assumes that every app in the future will not only have an MCP server or similar protocol installed for agentic integration, but eventually, basic agent servers will evolve into full-blown concierge agents themselves. Every app could theoretically engage with every other app where appropriate based on data that didn't exist until the app instance arrived in a 3OS context, e.g. the internal agent of a Maya instance may realize it is sitting in an MR room in close proximity to an Unreal instance agent, and they can automatically establish a series of pipeline handshakes without any work from the user. The user placed them side-by-side, implying they will soon want pipeline capabilities between them. This evolution of every app becoming conversational with every other app implies that app multitasking may evolve beyond simple pipeline toolchaining into real-time improvisation, desire, personality, etc. 

The most basic example is a T-rex app that understands it is a predator, and it actively hunts through a room at the OS level, seeking another app that broadcasts itself as prey. Such an app has no enterprise utility, but it is not architecturally much different from a coding agent. It simply wears a t-rex skin. Thus, the lines between agents, apps, and objects may blur significantly. They may all become variations on the same theme eventually.

</details>

---

<a id="basic-transformation"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">🔀 Basic Transformation</summary>

The greatest challenge in resolving the chicken-egg problem of enterprise users having no natively supported XR enterprise apps and enterprise app companies (Adobe, Autodesk, etc.) having no enterprise XR-only users is...

* ### Basic 3D UI for transform gizmos

In desktop interfaces, transform gizmos have become somewhat uniform and standardized across most 3D software, but they're not as standardized as "mouse left-click always causes selection." The gizmos are designed for the quirks of translating 2D mouse motion to 3D virtual space, and each app has its flavor. 

With 6DOF control, the complexity of that 2D-to-3D mapping becomes unnecessary in most cases. Instead, control designers can focus on deciding whether to map motion 1-to-1, snap to specific angles/positions, or constrain certain axes in certain situations as is already common in their 2D software. They simply have more DOFs to work with in 3D, yet more options may lead to more chaos.

3OS aims to offer a basic initial transformation standard below the app layer to encourage control uniformity. The nuances of DOF constraints and scalar quantizations must be available to professionals, but by default, 3OS aims also to make basic transformation palatable to all laypeople as well. Desktop gizmos are not palatable to laypeople. They look like a math equation to newbies. It's an instant turnoff. Laypeople need to be able to transform 3D objects via telekinesis just as laypeople in Windows 95 needed to know how to drag an icon across a desktop and drop it into the Recycle Bin.

As part of the 3rd party SDK, 3OS will offer a Transformation UI framework that includes...

* ### Telekinesis (remote translation)
* ### Tumbling (remote quaternion-based rotation)
* ### Scale
* ### Basic Bimanual Controls for axis constraint and motion quantization

For example, the user could spin the object with one hand and optionally constrain the spin to a specific axis with the other.

In most cases, rendering complex gizmos on the object will be unnecessary, whereas some controls will instantiate simple OS-level widgets floating in front of the hand to indicate which action is currently active, and some widgets will be anchorable to a desktop or other world anchor so that users can configure virtual consoles anchored in front of their bodies or to a desk. These widget mappings will be override-able for 3rd party apps, but 3rd party app developers should remember that, in 3OS, their app is running in tandem in a shared environment with the OS interface and with other 3rd party apps unless they configure it for viewport exclusivity (i.e. disable multitasking). If they override an OS transform widget to create their own in a multitasked app, they may confuse the user when every element in the same field operates one way and theirs operates another.

</details>

---

<a id="the-mind-palace"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">🧠 The Mind Palace</summary>

Roman statesmen, philosophers, and lawyers used a technique called <b>"the Mind Palace"</b> to deliver long, unscripted speeches in the Roman Senate or courts. It was an imaginary place where they could stash chunks of information in specific spatial locations, which, when they imagined retrieval, would assist in rapid recall.

The Mind Palace in 3OS is a transitional multi-field construct that bridges MR and VR. The default field in 3OS is an MR office room where professionals are doing collaborative creative 3D work (CAD, VFX, game engine dev, etc.) with each other and with 3D agents. If each user can drop a folder on the floor or a desk or pin to the wall in that singular MR space, the space may become cluttered quickly.

Alternatively, in 3OS, each user can gesture to toggle their Mind Palace, which begins as an empty VR void that retains any voodoo volume(s) already open from the MR scene, allowing users to quickly bring data from the real world into their Mind Palace and back out again. The Mind Palace begins as an empty void, but the user can associate any directory of any hard drive in the system as the root node of a new field, and navigate between any number of those fields similarly to shortcuts on a desktop.

Any given field can be decorated with static 3D objects (walls, furniture, etc.) if those structures help the user mentally organize the data, but most users will simply place floating files (objects) and folders (boxes) in a certain azimuth, depth, and height relative to their body. Like a desktop PC, hundreds of files can easily cause clutter in one field, but each field allows an order of magnitude more breathing room than a desktop before needing reorganization.

</details>

---

<a id="collaborative-rooms-vdi-enterprise-edge-compute-phase-20"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">🏢 Collaborative Rooms, VDI & Enterprise Edge Compute (Phase 2.0)</summary>

Phase **2.0** takes 3OS into multi-user enterprise rooms: every wall and handheld screen joins a shared volumetric field, sessions run on on-prem **VDI** or home **edge** compute, and collaboration semantics cover personal/shared/full-room workspaces, flyweight vs copy assets, view vs object transforms, spectator cameras, and optional site-to-site sync. The full design — reference 20×20 office fixture, desk docks, IMU monitors, co-editor VOIP/avatars, and VCS for simultaneous copies — is in [docs/COLLABORATIVE_ROOMS.md](docs/COLLABORATIVE_ROOMS.md); task checklists are Phase 2.0 in [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md).


</details>

---

<a id="AI-integration"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">🤖 Agentic Integration (Phase 3.0)</summary>

It is assumed 3OS will host agentic integration, though that is not the focus for the early alpha prototype. The endgame is to enable agents to exist as avatars who can converse with team members in real time before diving lower in the stack to complete their work, then return to the visual field for reporting and for more instructions.

Though agents will be heavily integrated, 3OS must stand as an interface on its own without agentic text-to-speech and speech-to-text as the primary interaction models due to a funny quirk of the human psyche:

### Impostor Syndrome!

One of the biggest value propositions agents bring at work is the ability to let junior team members ask infinite stupid questions with no fear of judgement. Juniors may be surprised to learn that this syndrome never goes away. Grizzled 20-year veterans in every field are also quietly asking agents stupid questions they'd be embarrassed to ask out loud.

Even in an office where every coworker is very familiar with every other coworker in the room, verbal agent conversations cannot be the only, nor even the primary, interface during work hours. In dealing with agents, every team member needs to be able to ask judgement-free stupid questions frequently and quietly.

Therefore, if teams are ever able to transition to an agentic XR interface that replaces desktop PCs, it must be one that enables silent communication as a core option. A 3D gesture-based form of communication that operates equally well on any data in the entire field is necessary to limit the amount of prompt typing required, whether the user is communicating with agents or team members. Thus, 3OS early prototypes focus on the raw manual interface to ensure it can stand on its own without agents, and then its processes can be accelerated once agents arrive as team members.

</details>

---

<a id="structural-outline"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">📁 Source Structural Outline</summary>

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
│   │   └── volumetric_glyph.hpp    # Translates extensions into low-poly prismatic meshes
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

</details>

---

<a id="prior-art"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">🏛️ Academic Lineage & Prior Art</summary>

Piles of incredible Human-Computer Interaction (HCI) literature have been left to languish by the modern spatial computing industry. In 2026, the following papers do not currently influence OS interfaces running on Meta Quest, Meta VR Glasses, Apple Vision Pro, Magic Leap, Hololens, Snap Specs, XReal Aura, Vive, Pico, Playstation VR, Lynx, Varjo, Bigscreen, or Steam Frame. They should.

By grounding **3OS** in this seminal research, we bridge the historic gap between the late-1990s proprioceptive research labs and modern non-Euclidean spatial interaction theories to break the productivity chains of WIMP. Despite raycast having double the throughput of isometric virtual hands (point cursors), it is unsuitable for rapid serial and/or parallel two-handed interactions. Point cursors, on the other hand, are stellar at utilizing two-handed speed boosts, which more than covers the throughput gap even before completely unlocking the far field, allowing point cursors to leave raycast in the dust. The following research shows how and why.

---

### 1. The "Silk Cursor" — 1994

**Citation:** Zhai, S., Buxton, W., & Milgram, P. (1994). *The 'silk cursor': investigating visual cues on target acquisition in 3D user interfaces. Proceedings of the SIGCHI Conference on Human Factors in Computing Systems (CHI '94)*, 185–192.
*   **Significance to 3OS:** Introduced a future where raycast cursors must be abandoned in order to access the entire XR field, implying that interaction via perspective is inferior to interaction via isometry. The "silk" metaphor heavily emphasized the need for most interactive elements to utilize semitransparency in such a system to prevent occlusion from causing confusion. The same is true in 3OS.


### 2. Worlds in Miniature (WIM) — 1995
*   **Citation:** Stoakley, R., Conway, M. J., & Pausch, R. (1995). "Virtual reality on a WIM: interactive worlds in miniature." *Proceedings of the 22nd annual conference on Computer graphics and interactive techniques (SIGGRAPH '95)*, 265–272.
*   **Significance to 3OS:** This paper pioneered the paradigm of scaling down a massive, immersive 3D environment into a hand-held, near-field miniature copy to bypass the physical limitations of far-field targeting. It is the original world-to-user proxy, a voodoo doll of the entire virtual space. 3OS modernizes this concept into the **World Proxy Half-Dome**, updating it to track empty volumetric coordinate spaces and/or objects within those spaces.

### 3. Proprioceptive Interaction Models — 1997
*   **Citation:** Mine, M. R., Brooks Jr, F. P., & Sequin, C. H. (1997). "Moving objects in space: Exploiting proprioception in virtual-environment interaction." *Proceedings of the 24th annual conference on Computer graphics and interactive techniques (SIGGRAPH '97)*, 19–26.
*   **Significance to 3OS:** Proved that humans possess a highly accurate, innate biological awareness of their hands' positions relative to their body frames without requiring constant visual feedback (**proprioception**). This serves as the scientific bedrock for the **3OS Drummer's Law of Biomechanical Subordination**—keeping the shoulder girdle (coracobrachialis) completely neutral while the wrist and fingers drive intent inside near-field workspaces, i.e. peripheral remote controls in the near field manipulate objects in the far field.

### 4. The Foundational Voodoo Dolls Paper — 1999
*   **Citation:** Pierce, J. S., Stearns, B. C., & Pausch, R. (1999). "Voodoo Dolls: Seamless Interaction at Multiple Scales in Virtual Environments." *Proceedings of the 1999 symposium on Interactive 3D graphics (I3D '99)*, 141–145.
*   **Significance to 3OS:** Introduced the first dynamic object-proxy architecture, allowing users to instantiate scale-adjusted, hand-held replicas ("dolls") to manipulate distant assets bidirectionally. 3OS adopts this fundamental workflow but eliminates its rigid, skeuomorphic, object-bound limits—expanding proxies to target unconstrained voxel coordinate structures rather than just pre-existing 3D models.

### 5. Pointing at Trivariate Targets — 2004

*   **Citation:** Grossman, T., & Balakrishnan, R. (2004). "Pointing at trivariate targets in 3D environments." *Proceedings of the SIGCHI Conference on Human Factors in Computing Systems (CHI '04)*, 447–454.
*   **Significance to 3OS:** Introduced an important equation derived from Fitt's Law that models a 6DOF hand's ability to hit voxel-based targets in 3D space at maximum speed without overshoot. 3OS relies on a slight modification of this equation to determine the default 3" voxel size (see spatial_os_trivariate_validation.md).


### 6. The Poros Framework — 2021
*   **Citation:** Pohl, H., Lilija, K., McIntosh, J., & Hornbæk, K. (2021). "Poros: Configurable Proxies for Distant Interactions in VR." *Proceedings of the 2021 CHI Conference on Human Factors in Computing Systems (CHI '21)*, Article 357, 1–13.
*   **Significance to 3OS:** The direct conceptual and geometric peer to 3OS. This paper broke the legacy "Flatland" window paradigm by showing that users can select arbitrary volumetric bounding regions in the far field and mirror them as non-Euclidean near-field workspace portals. 3OS adapts this concept for bimanual selection frustums that are cubic instead of spherical. The most salient difference between Poros and 3OS voodoo volumes is that Poros still used raycasting to generate portals, whereas 3OS engages a modified world-in-miniature technique as its creation model, yielding rapid cross-proxy multi-portal manipulation intended as a competitive volumetric corollary to a 2D desktop File Explorer.

### 7. Reality Proxy — 2025
*   **Citation:** Al-Sadoun, A., et al. (2025). "Reality Proxy: Fluid Interactions with Real-World Objects in MR." *Proceedings of the 2025 CHI Conference on Human Factors in Computing Systems (CHI '25)*.
*   **Significance to 3OS:** Evaluated and validated that using spatial computer vision and semantic machine intelligence are crucial for streamlining the associations between world mesh geo and virtual content that fully utilizes the context of that geo.

### 8. Fitts' Law + XR Review of 122 studies  — 2025
*   **Citation:** Amini, M., Stuertzlinger, W., Teather, R.J., & Batmaz, A.U. "A Systematic Review of Fitt's Law in 3D Extended Reality" *Proceedings of the 2025 CHI Conference on Human Factors in Computing Systems (CHI '25)*.
*   **Significance to 3OS:** Identified that only 36% of XR HCI studies of the past several decades (44 total) that focused on throughput performance had targeted virtual hand (point cursor) interaction models, whereas more than 50% targeted raycast techniques. The other 14% were eye tracking studies.


</details>

---

<a id="control-basics"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">🎮 Control Basics</summary>


3OS supports cross-compatibility of all input actions for 10-finger hand tracking first and foremost, but 6DOF controllers as well. The 3OS interface defines the following universal event triggers:

* ### Primary
  * Index-to-thumb pinch for hands
  * Trigger button for controllers
  * Equivalent to 2D mouse left-click
* ### Secondary 
  * Middle-to-thumb pinch for hands
  * Grab button for controllers
  * Equivalent to 2D mouse right-click
* ### Ternary
  * Ring-to-thumb pinch for hands
  * X button for right controller or A button for left controller
  * Equivalent to 2D mouse middle-click

3OS also defines three ulnar wrist rotation modes:
* ### Palm up
* ### Palm down
* ### Palm sideways

Each ulnar wrist mode has the option to modify the primary, secondary, or ternary event trigger, yielding a total of nine event combinations for each hand:
* ### Primary-palm-up
* ### Primary-palm-down
* ### Primary-palm-sideways
* ### Secondary-palm-up
etc.

The 3OS interface layer reserves all primary event triggers and secondary-palm-down for selection, voodoo control, and other OS actions everywhere in the field outside the bounding box of a 3rd party app and a voodoo volume accessing a 3rd party app, and 3rd party devs are encouraged to implement these features similarly within their app bounds via the 3OS SDK.

### Hover and Selection

Buttons in 3OS are 3D and volumetric by default, meaning point cursors hover INTO them rather than raycast cursors hovering ONTO them. When a point cursor enters or exits the volume of a 3D button, its hover state toggles. While hovered, primary pinch activates the buttton. Similarly, hover and pinch in an object flags that object as currently selected. Multi-choice nodes work in the same way, except hover is defined by an angular relationship between the cursor and a center point between a group of buttons.

### Multi-Selection

Drag-selection is also volumetric. The objects encapsulated by the cubic volume drawn from corner-to-corner with a primary-pinch-drag action will all be flagged as selected simultaneously.

### Voodoo Volume Activation

Palm-down + Primary-pinch-hold begins voodoo volume selection. Drag position of the pinched fingertips are a voodoo doll of the far-field voodoo volume during drag, meaning the hand can move in 3D to place the volume in 3D with a multiplier of 50-to-1. This means a fully outstretched arm can place the far-field voodoo ~50 feet away in any direction with default sensitivity, and sensitivity is adjustable. Once the volume is dropped in the far field, its near-field companion volume (often referenced as the near BB ~ short for bounding box) appears in the ergonomically safe intimate space directly in front of the user.

### Toggling the Mind Palace

To toggle the Mind Palace (MR-office-to-VR-hard-drive or vice versa), the user turns both flat palms toward their face like an open book, thumbs out in opposite directions. This calls up the current toggle status. It will read either "MR" or "VR". Closing the thumbs simultaneously will toggle MR to VR or vice versa.

</details>

---

<a id="field-rings"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">⭕ Field Rings</summary>

### Understanding the Field in 3OS

For decades, HCI research has subdivided the interactable spatial field into two main categories:

#### Near Field: 
  * everything your fingertips can reach
#### Far Field: 
  * everything your fingertips can't reach

These are the root categories of an interactive field because they require two different interaction techniques. Users may grab objects in the near field and may expect them to have physics-based and/or skeumorphic properties, whereas some form of raycasting or telekinesis is needed to reach far-field objects.

### Deeper Subcategories:

#### The Pseudo-Near Field

In 3OS, the goal is always to merge chunks of the far field into the near field instantly to take advantage of the proprioceptive power of near field interaction, yet 3OS also avoids physics-based and skeumorphic interactions by default since neither lend themselves to clusters of objects that can take advantage of the whole far field by avoiding gravity and occlusion. Thus, all interactive UI objects in 3OS are ghostly by default. They float and are translucent.

When these ghostly 3OS objects enter into the near field, the user does not put a hand directly inside them for manipulation. Instead, the user's hand drives a point cursor (offset a few inches forward and above the hand by a parameterized amount). The point cursor enables the user to slightly extend their near-field reach. They gain proprioception over that extended space because the cursor feels like yet another joint of their arm, which widens the near field. We call this extension the pseudo-near field (an outer sheel of the near field), and most 3OS UIs live in this pseudo space when called in from the far field. This technique is crucial for comfort and efficiency:

3OS UIs are usually contextual widgets for far-field objects, meaning they operate as remote controls meant to utilize peripheral vision. The hands with good neutral elbow position for good ergo are slightly too low for good peripheral cues if they're sharing the same space with these widgets, which means you'd have to move the widgets too low for periphery, or make the user hold their hands too high for comfort. The solution is separation of the hands from the widgets with a slight offset via point cursors. Pseudo-near field widgets driven by point cursors are the best-of-both-worlds: comfort and efficiency in one interface.

#### The Two Rings of the Far Field

The far field is gigantic. It makes sense to subdivide it as soon as we begin using it for large and complex clusters of objects. The immediate far field (radius of ~20ft), allows for exponentially more clarity via less perspective size warping and parallax stereoscopy than the rest of the far field. In 3OS, we think of the far field in two parts:

  #### The Working Ring
    This is the closer section of the far field, typically defined by ~20ft radius from the user in all directions. This range is not parameterized by 3OS because it is a mental guideline for users, not a requirement. It helps to keep a field organized by keeping objects that need frequent manipulation closer in the far field.
  #### The Storage Ring
    Likewise, objects that may sit untouched for hours, days, or weeks belong in the outer ring of the far field (beyond 20ft range) where they can be brought into the working ring once they become relevant for new tasks.

These ring subdivisions are not necessary in every context, but they exist to support the most complex enterprise interactions available, including rapid file management, 3D visual node-based programming, 3D data visualization, etc.

</details>

---

<a id="getting-started"></a>
<details>
<summary style="font-size: 1.5em; font-weight: 600; line-height: 1.3; margin-bottom: 0.6em; padding-bottom: 0.15em;">🚀 Getting Started</summary>

```bash
git clone https://github.com/scifiuiguy/3OS.git
cd 3OS

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### Pre-Alpha Demo

* To use the Pre-Alpha Demo with built-in sample files and folders, load it as is onto Quest 2 or 3:
  * Plug your Quest into a PC via USB.
  * In Quest, activate the main menu and choose the Notifications tab.
  * A fresh USB Detected notification should appear. Select it.
  * On PC, a File Explorer should launch, displaying This PC/Quest 2. Double-click it.
  * Open the Documents folder and drag in the `quest_pre_alpha_demo_v070.apk`.
  * On Quest, open the Library tab and choose Files.
  * In the Quest Files Explorer, choose the "This headset" tab in the left panel.
  * Pinch on the Documents folder.
  * Pinch on the APK file, then pinch the `...` button at the bottom right corner of the APK file.
  * Pinch **Open With → Package Installer**.
  * A prompt will request install or update permission. Grant it.
  * A prompt may request hard drive access (the demo optionally reads and writes into reserved folders in Documents).
  * To launch the app any time after first install, pinch the **Menu** button on the left. Choose **Unknown Sources**, then select **3OS_Pre_Alpha_Demo**.
* To load your own sample files and folders into the demo:
  1. On your Quest hard drive, create these folders:
     * `Documents/3OS_Demo`
     * `Documents/3OS_Demo2`
  2. Drag/drop any files of your choice into either folder and they'll appear in-scene.
     * **NOTE:** Don't test with important unarchived files. The demo WILL read/write/delete in those directories.

The shared library target is `3os_kernel` (`3os_kernel.dll` / `lib3os_kernel.so`). The console harness prints `threeos_version` (currently `0.6.0`) and checks the frozen ABI (`InteropInputFrame` = 192 bytes). Host ABI headers: `include/interop/threeos_abi.h`. See [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md); Unity Quest verification lives in [3OS_Unity](https://github.com/scifiuiguy/3OS_Unity).

*This project is dedicated to breaking the flatland illusion. Welcome to true volumetric computing.*

</details>