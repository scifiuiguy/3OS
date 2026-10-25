# Collaborative Rooms, VDI & Enterprise Edge Compute (Phase 2.0)

Design notes for multi-user 3OS in office rooms and home/edge deployments. Task checklists live in [IMPLEMENTATION_PLAN.md](../IMPLEMENTATION_PLAN.md) (Core + Unity Phase 2.0). Product overview: [README.md](../README.md).

Single-user volumetric work on a headset is only half the enterprise story. Phase **2.0** extends 3OS so it can **interface with every 2D screen in a room** — bolted wall monitors and handheld tablets alike — while heavy graphics and session authority run on **on-prem VDI** in the office or on **home/edge compute** servers akin to IoT hubs. Eyewear stays a thin interactive client; the server owns multi-user presence, display pose, spectator compositing, and real-time media sync.

## Why VDI (and edge) instead of headset-only render

Twelve workers sharing one volumetric field, plus dozens of wall feeds and guest 4K monitors, is a GPU and policy problem. An on-prem VDI pool with serious GPU headroom can host **12 simultaneous** enterprise sessions without asking each HMD to re-render every spectator monitor. The same **session / display / compositor contracts** apply at home on a smaller edge box — fewer seats, fewer screens, identical mental model. 3OS Core does not embed a vendor VDI stack; it defines the ABI and room semantics those services would implement.

## Reference office room

The design target is a **20 ft × 20 ft** room. Three walls are workstation walls; the fourth holds the entry door and a **guest couch**. The center is empty with a large carpet — that carpet is everyone’s **primary volumetric workspace**. Guests on the couch can follow work on large screens without eyewear; workers face the carpet for shared 3D work, or turn back to the wall for legacy dual-monitor focus.

### Stations & furniture

- **12 workers:** 3 walls × 4 stations; each station **50" wide**.
- **Console:** **48" wide**, two drawers (**22" wide × 10" deep**), feet raise it **4"** off the floor.
- **Standing desk dock:** A square groove **exactly 3.0625" wide** cut into the console center between the drawers accepts a **3" square-tube** standing-desk beam. The beam seats into a **pogo-pin** power/data interface with a **spring latch**. Latch release is within easy reach as the worker steps in.
- **Desk:** Off-the-shelf standing desk with power/data routing installed, including a small LiFePO4 power brick sized to power a laptop for 8 hours. Workers will not typically need to bring a laptop but it is optional. Surface **30" × 16"** — keyboard, mouse, and a flush-mount USB/power adapter only. On arrival the worker press-releases the latch, pulls the desk out of the console, and **spins it to face the room center** for volumetric work. When they need flat screens for a while (rarely necessary since they can be manifested in eyewear), they may rotate back to the wall, push the beam home, and latch it — the station becomes a conventional desk again.

### Displays

- **Workstation walls:** **8× 24"** monitors per wall on a **single rail**, ball joint for quick **portrait ↔ landscape**, with padding for rotation. **No per-monitor hardware sensors.** 3OS localizes each screen with **digital corner fiducials** rendered on the display itself, or by using the monitor's own **real-time output as the fiducial**, so the room cameras can resolve pose and 3OS always knows where that screen lives in room space (portrait vs. landscape falls out of the same solve).
- **Guest / spectator walls:** The couch wall **and** the three workstation walls each add **two large 4K screens** (above the workstation monitors), for clearer viewing by anyone without eyewear.
- **Handhelds:** Any issued touchscreen, laptop, or similar device with login credentials joins the same display registry once localized (see cameras below).

### Cameras

Four cameras, one in each corner, aimed at the carpet. Each captures **virtual content** in the shared 3D workspace and, by default, drives the monitors on the **opposite** side of the room as wide-angle **spectator** views. Cameras also help **semantically track** logged-in devices (for example a tablet) so their room coordinates stay authoritative alongside the fiducial-tracked wall monitors.

## Arrival & workspace model

1. Unlatch desk → roll and turn to face center carpet → launch the eyewear app.  
2. Log into the **3OS enterprise** interface on the VDI (or home edge session).  
3. Land in a **personal workspace** first.  
4. Join or create **shared workspaces** as invited: pairs of teammates, groups of three or four (or more), and a **full-room** workspace that can include everyone present and **control all room monitors** as spectator surfaces.

**Solo-in-shared:** A worker can stay inside a shared session while peer volumetric content stays **masked** in their eyewear — privacy and focus without leaving the collaboration graph. Unmask or co-possess when they need to see or jointly drive the same objects.

## Asset references (flyweights) vs asset copies

Enterprise collaboration has to distinguish **what is shared in server memory** from **how each workspace looks at it**. Two pairs of workers can open the “same” 3D model in two different parallel workspaces; that must not silently mean the wrong kind of sharing.

**Asset copy** — Each workspace (or each open document) holds an independent instance. Edits, material changes, and object transforms in workspace A do **not** mutate workspace B. RAM and GPU cost scale with the number of live instances. Use copies when teams need isolation, branching, or “what-if” edits.

**Asset reference (flyweight)** — Multiple workspaces (or viewers) point at **one** authoritative object in server RAM. Attribute edits and **object** transforms propagate immediately to every workspace holding that reference, even when those workspaces are not looking from the same angle and are not the same invite graph. Use flyweights when the product truth is singular (one Maya scene, one CAD assembly) and every collaborator must see the same underlying asset. **A true flyweight needs no merge:** there is only one live object, so concurrent editors are not reconciling lineages — they are co-mutating the singleton. Ordinary save/commit of that singleton to durable storage is assumed and is not a special collaboration mode.

Opening a file does not invent a third mode: the session explicitly binds each open as either a **copy** or a **reference** (product default can prefer one, but the distinction is first-class). Example: Autodesk ports Maya into 3OS; four workers open the same Maya file in **two** parallel shared workspaces (two people each). Default placement puts the document’s world origin / primary content at **room center** on the carpet for each workspace’s staging. Whether those two opens are two copies or two references to one flyweight is a deliberate choice — not an accident of “same filename.”

### Flyweight locality (latency boundary)

By default, a flyweight is only offered inside a **low-latency co-residency domain** — at minimum the **room**, and often the whole **building / campus** on the same on-prem VDI fabric. Naïve company-wide flyweights across regions (e.g. LA room and Bangalore room on ordinary WAN RTT) are a bad idea: lag and consistency failures make “one RAM singleton” a lie. Those opens should be **copies** + VCS unless the tenant explicitly enables a stronger path.

### Optional site-to-site flyweight (paid sync fabric)

MMO-class servers already prove that **low-latency synchronized** world state can span sites — at serious cost. 3OS should allow enterprises that want to **pay for that class of infrastructure** to enable **site-to-site flyweights** between chosen locations (dedicated sync mesh, regional replicas kept hot, budgeted GPU/net). Default product behavior stays room/campus-local; site-to-site is an **opt-in tenant capability**, not assumed. When it is on, the same rules apply: one logical singleton, **no merge** for live edits, mandatory co-editor roster + VOIP/avatar fly-in so distant rooms still greenlight who is on the asset. When it is off, cross-site opens remain copies.

### Why choose flyweight — and who else is on it

Teams often pick flyweight **on purpose** to skip diff/merge workflows, especially when everyone is already in the **same room** and can talk across the carpet. On a shared campus, though, another group in a **different room** can still bind the same singleton and accidentally step on live edits. So whenever a user opens (or joins) a flyweight, **3OS must show who else is currently editing that same asset** — names, room/station if known, and workspace. Presence on the flyweight is the social lock; there is still no VCS merge step for those live edits, but invisible co-editors are not allowed.

### Co-editor roster actions (cross-room)

From that roster, 3OS should let you **conference co-editors in directly over VOIP** (no hunt for a separate phone bridge) and, for the best campus workflow, **fly avatars both ways**: bring their avatars into your room’s shared field and send yours into theirs so gaze, pointing (“this / that / over there”), and live edits on the flyweight happen in a shared social space even when the bodies are in different rooms.

### Simultaneous copies imply heavy VCS integration

Copies are divergent lineages. Once two or more workspaces — especially across sites that cannot flyweight — hold copies of the same logical asset, the enterprise stack must treat them like branches: identity of the source revision, per-copy dirty state, commit/check-in, merge or rebase back to a trunk, conflict detection (geometry, materials, hierarchy, binary blobs), and optional lock or lease policies when a merge would be unsafe. 3OS does not replace Git/Perforce/Plastic/etc.; VDI/edge session services and host DCC ports **plumb** a real VCS so promote / get-latest / resolve are first-class volumetric actions — otherwise simultaneous copies are just unreconciled forks.

## View transform vs object transform

Separately from copy vs flyweight, every viewer has a **camera / view** that must not be confused with the **object’s anchor** in the document or room.

| Action | What changes | Who else sees it |
| --- | --- | --- |
| **Camera / view move** | The user’s (or workspace’s) viewpoint, orbit, or “flip the scene upside down” as a view | Only that viewer or that workspace’s shared view rig — **not** object attributes in the scene |
| **Object transform** | Anchor coordinates / orientation of the asset relative to document or room origin (e.g. spin the mesh upside down) | Everyone on a **flyweight** of that object, across workspaces; only the local instance if the open is a **copy** |

So: either user in either Maya workspace can flip their view upside down with a **camera move** and leave the scene’s object attributes untouched — the other workspace can still see the asset right-side-up. If instead someone **spins the object** itself, and both sessions hold a **flyweight**, the second session’s object flips too even though their cameras never moved. If both sessions hold **copies**, spinning the object in session one leaves session two’s instance upright.

Spectator monitors and snap-to-monitor focused views follow the same rule: a monitor can show a workspace’s current **camera** into a flyweight (angle-local) or reflect **object** state that is already shared. Compositors must not bake a view orbit into the asset’s transform.

## Displays as first-class 3OS citizens

Every registered screen is a **possessable** object in the shared field, not a dumb HDMI sink.

- **Default mode:** Spectator wide-angle view from the opposite-corner camera (or a full-room reassignment).  
- **Snap-to-monitor:** Standard 3OS transforms let any user snap volumetric content onto any monitor. That display immediately leaves spectator mode and shows an **object-focused** view — 3D asset, video, image, text surface, etc., by content type.  
- **Shared possession:** If a user possesses a monitor from a shared workspace, any other invitee to that workspace may interact with it or **close possession**, returning the monitor to its default spectator view.  
- **Legacy workstation mode:** Desk latched to the wall; user possesses the **two monitors directly above their console**. Those two behave as a normal dual-monitor desktop until released — WIMP when you need it, volumetric when you don’t.

## Handheld pull-in & media sync

Because corner cameras and the VDI/edge session track logged-in devices in 3D, a user can **pull a tablet’s live content into the shared volumetric space** with a 3OS gesture. Placement follows the localized device pose. The same real-time sync path covers **text, images, video, and 3D assets** across eyewear clients and wall/handheld displays, subject to invite and mask rules. Bandwidth and LOD ladders (12-seat VDI vs smaller edge nodes) are server policy; the kernel exposes intent, not a particular codec stack.

## Split of responsibility

| Layer | Role |
| --- | --- |
| **VDI / edge server** | Auth & tenancy, workspace invites, display registry + camera-based fiducial localization, spectator compositor, media sync, GPU-heavy shared renders, **flyweight vs copy** residency (room/campus default; optional **paid site-to-site** sync fabric), **VCS plumbing** when copies |
| **Eyewear / host client** | Login UI, personal + shared workspace interaction, gestures, local OpenXR input; view/camera rigs; copy promote/merge UX when not flyweight; consumes compositor feeds rather than re-rendering every wall |
| **3OS Core** | Session/display/workspace ABI, possession & snap semantics, mask rules, **reference vs copy** + **locality** gates, **view vs object** transform split, VCS intent hooks for copies only (not a full VCS engine), headless contracts |

The reference room dimensions and hardware numbers above are the **optimal office fixture**, not a hard-coded limit. Smaller labs and home edge deployments exercise the same contracts with fewer stations and screens. See Phase **2.0** in the [implementation plan](../IMPLEMENTATION_PLAN.md) for checklist tasks and lab gates.
