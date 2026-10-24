# 3OS: A Volumetric Operating System Interface

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![OpenXR](https://img.shields.io/badge/API-OpenXR-orange.svg)](https://www.khronos.org/openxr/)

Johnny Mnemonic showed a 3D VR replacement for 2D desktop computers in 1995:
<a href="https://www.youtube.com/watch?v=SoxGazX_kkw" target="_blank" rel="noopener noreferrer">
  <img src="mnemonic.jpeg" alt="Johnny Mnemonic — 3D VR internet (1995)" />
</a>

This movie was shy of the mark of dreaming up an actual usable interface, but it hinted at a 1990s sense of optimism toward 3D interfaces becoming better, faster, and more efficient than 2D desktop equivalents. Obviously, that didn't happen. YET. What happened to that optimism though?

Was it proven to be bunk? Are 3D interfaces destined to be slower than desktop PCs? Or did we stop investing in the VR industry for a decade after the dot-com bust, allowing mountains of incredible 90s university research to collect dust in SIGCHI archives, never to be drawn upon again by modern industry titans? Until...

<hr style="border: 1px solid gray;">

<a id="meta-start-reviewers"></a>
<details>
<summary><b>🕶️ Hello, Meta Start Review Team!</b></summary>
<hr style="border: 1px solid gray;">

Welcome to **3OS**! It's an enterprise-grade volumetric XR operating system interface built from the ground up to push the boundaries of spatial multitasking and ergonomics at the office. Since you're probably reviewing lots of apps, I tried to consolidate the <i>NOVEL</i> that is this README into a quick set of bullet points to help you out:

### 🌟 Core Architectural Highlights for Meta Start

<blockquote>
<details>
<summary style="font-size: 1.15em; display: list-item;">
<b>Hands-First Interaction Model Obsessed with Speed and Best-in-Class Ergonomics</b>
</summary>

Using low-fatigue, volumetric, isometric, proprioceptive hand tracking, 3OS eliminates "gorilla arm" potential by relying on small, localized gestures and a proprioceptive merging of the near field and far field via a careful amalgam of techniques derived from a treasure trove of classic HCI studies going all the way back to 1991. This interaction technique formula was inspired by a dozen overlooked SIGCHI gems. (see Prior Art)

</details>
</blockquote>

<blockquote>
<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Author Background (former Technicolor XR Tech Lead & Magic Leap Prototyping Engineer)</span></span>
</summary>

  I was deeply frustrated with state-of-the-art XR UI/UX design while working as a Lead Product Prototyping Engineer at Magic Leap in 2019 when I decided there must be <i>SOMEONE</i> in Academia who solved the myriad of problems slowing down our XR UI/UX. That's when I discovered SIGCHI. I had no idea of the rich history of UI research in Academia before that, and I became obsessed with mining for the gold that would unlock speed in 3D UI. It took a while to synthesize an end-to-end solution.

</details>
</blockquote>
  
<blockquote>
<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">The Solution In A Nutshell</span></span>
</summary>

Raycast models and floating 2D windows waste most of XR’s adjacency/multitasking space and encourage slow, single-handed interaction. 3OS makes a compelling case for a volumetric isometric OS model (instead of a raycast model) as <i>THE</i> solution to the many speed bottlenecks preventing 3D UI adoption in enterprise spatial computing by encouraging snap-twitch voxel-based actions that <i>can</i> work with one hand but get faster for enterprise power users with two hands in concert. While mining SIGCHI archives, I gravitated toward perf-op studies involving Fitts' Law variations. Ergonomic studies battling "gorilla arm" also fascinated me because I was deeply familiar with the problematic muscle groups in the shoulders (especially the coracobrachialis) since I targeted those same muscle groups with relaxation techniques in college while teaching one-on-one drum lessons to hundreds of students.

</details>
</blockquote>

<blockquote>
<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Unchaining the Desk to Access the Whole Room</span></span>
</summary>

Built explicitly for the modern near-future work environment, 3OS integrates fiducially-tracked rolling standing desk consoles that detach from dual wall monitors to allow seamless transitions between seated and standing configurations in any position in a dedicated XR room where MR content may encompass part of a desk, part of the room, shared space with multiple MR apps, the whole room, or even a whole VR field. 3OS gives each task the exact amount of space it needs for ASAP completion with rapid context switching e.g. carrying files across MR/VR transitions, enabling high throughput for tasks that only achieve speed boosts when utilizing the whole field, yet never requiring users to reach or cross the room.

</details>
</blockquote>

<blockquote>
<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">High-Performance Native Architecture</span></span>
</summary>

  The heart of the 3OS Pre-Alpha Demo is a native C++ kernel with C ABI marshalled into Unity for rapid prototyping today, but it is prepped to eventually run on vWS VMs to OpenXR to CloudXR in an enterprise context. On an enterprise server (e.g. AMD EPYC 9004 + Pro 6000 Blackwell), each team member will get a streamed headset plus a rolling standing desk that clicks into a dual-monitor wall console or detaches to reposition anywhere in the room for collab work, sporting optional virtual monitors and anchorable virtual 2D and 3D widgets. All legacy 2D software will be supported. 3OS will target SDK partnerships with 3D editing software titans for early adoption. Apps like Solidworks, Maya, Unreal, etc. will render into workspace containers (volume managers) rather than talking to HMDs directly. Colocated teams will run multiple MR apps x multiple workspaces via thin HMD streaming clients, flying solo and/or multiplayer as needed while the VDI edge stack offloads processing.

</details>
</blockquote>

### 🗺️ Anatomy of the Demo
* **Demo Video First:** I encourage you to watch the 3-minute tutorial before launching the Pre-Alpha Demo, which contains a near-future mockup of an enterprise dev room running 3OS Server that supports 2-12 team members.
* **Most Demo Features Are Mockups:** Agentic avatar integration and app multitasking are narrative scaffolding so far, because 3OS is less than two months old.
* **Operational Features:** UI and controls described in the Basic Controls section below are mostly operational and in sync with the Quest Documents directory for the demo. These features are still very much in pre-alpha condition with lots of refinement coming. They will be much more snappy when processing is offloaded to an EPYC 9004 to free up the Snapdragon XR2 to focus solely on rendering and input.

I hope you enjoy the demo!

</details>

<hr style="border: 1px solid gray;">

<a id="what-is-3os"></a>
<details>
<summary><b>🧊 Introduction</b></summary>
<hr style="border: 1px solid gray;">

<a id="what-is-3os"></a>
<blockquote>
<details>
<summary><b>🌐 What is 3OS?</b></summary>
<hr style="border: 1px solid gray;">

3OS (pronounced **"three-oh-ess"**) is inspired by cult classic sci-fi flicks like Johnny Mnemonic, The Matrix, Minority Report, Iron Man, and The Expanse, but it also grounds its efficacy in our rich history of HCI research.

3OS is a voxel-first operating system interface that attempts to take us back to our optimistic roots by drawing upon nearly-forgotten 90s HCI studies (see the Prior Art section). It aims to be the first 3D interface that's faster than desktop PCs for enterprise teams using the HCI road less traveled (A.K.A. volumetric isometric interaction, NOT raycast cursors), but it also supports hybrid integration of all legacy 2D apps with virtual and physical monitors in a collaborative development room.

### Target hardware: 
  * XR HMDs
  * holographic displays
  * 2D virtual and physical monitors for legacy app backwards compatibility
  * on-prem edge VDI server running surround-spectator monitors and HMD streams

</details>
</blockquote>

---

<a id="what-exists-today"></a>
<blockquote>
<details>
<summary><b>🧪 What exists today?</b></summary>
<hr style="border: 1px solid gray;">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">A Prototype Interface</span></span>
</summary>

A fast-twitch volumetric isometric interaction model (called voodoo volumes) that merges near-field/far-field content for rapid whole-field interaction.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">A Prototype 3D File System</span></span>
</summary>

One of the clearest ways to demonstrate the power of 3OS is to show how common 2D desktop tasks (e.g. file management) can be faster in 3OS. That is the purpose of the pre-alpha demo on Meta Quest (see Getting Started for demo instructions).

The author of 3OS led the team that built the Spotify App on Magic Leap. Placing virtual album art on walls in physical space was truly magical. Ever since, he has been dreaming of a way to do similar things with ALL files and folders. Now we can!

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">A mockup scene representing a near-future office running 3OS Server</span></span>
</summary>

The mockup office demonstrates the ability for any team working with 3D content to transition into a hybrid 2D+3D interface via the 3OS Server. The 3OS Server has a technical specification in-progress but its first prototype has not been built yet. The pre-alpha mockup is designed to show the value of the 3OS Server to investors and partners so that prototype development can begin.

</details>


</details>
</blockquote>

---

<a id="market-size"></a>
<blockquote>
<details>
<summary><b>📈 Market Opportunity</b></summary>
<hr style="border: 1px solid gray;">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">How big is the market for an enterprise-grade 3D OS?</span></span>
</summary>

There are 100 million potential early adopters in all fields of professional 3D content creation (CAD/game-dev/VFX/software). The TAM for an enterprise-grade 3D OS that has the horsepower and throughput to convince them all to upgrade is:

* ~50 million CAD engineers
* ~1 million game developers
* ~1 million VFX artists
* ~50 million software engineers
* $500/month per 3OS Server seat license 

#### TAM: $50 billion ARR

...with massive future growth potential due to the advent of GenAI, agentic toolchains, and the weight/size optimization of VR and MR headsets toward sunglasses form-factor. Exciting times! Let's do this!

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">How to take 3OS to the next level</span></span>
</summary>

The server code will be written with or without enterprise-grade hardware. A Pro 6000 Blackwell is not in the author's current budget. He is actively seeking industry partners who share his vision of the near future, especially if they have office space in which to allocate 18 months of R&D and dogfooding before beginning a private beta with SDK partners.

</details>

</details>
</blockquote>

---

<a id="why-software-engineers-3d"></a>
<blockquote>
<details>
<summary><b>💻 Why Would All Software Engineers Work in 3D?</b></summary>
<hr style="border: 1px solid gray;">

Most people could imagine CAD engineers enjoying an Iron-Man-style interface if it legitimiately made work faster, but software engineers? What would it even mean for THEM to "work in 3D"?


<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Confusion About Agents</span></span>
</summary>

Today, everyone understands that the nature of software development is rapidly changing. Most developers believe <i>AGENTS</i> are the newest interface layer on the top of the abstraction stack (binary, assembly, two layers of low-level languages, and high-level languages... and now agents?), but that's only <i>HALF</i> the story.

Most of us recognize that agents place human programmers yet another layer away from raw binary, but this is easy to confuse. Agents are only <b>PART</b> of a new stack layer that isn't baked yet.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">The New Stack Layer Must Be Visual</span></span>
</summary>

Every layer of the abstraction stack has been <b><i>visual</i></b> for a reason. Our eyes are the highest-throughput way to put data into to our brains by a long-shot. Agents do not currently give us a new <b><i>visual</i></b> representation of the stack. They reply in a chat thread, but that chat thread is not the program. They can show us the code they changed, but that layer of the stack was already here long before they arrived. That's not the new layer either. There is no <i>NEW</i> layer of the stack to look at. <i>YET</i>.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;"><i>What does the new layer <b>LOOK</b> like?</i></span></span>
</summary>

<b>Agentic Visual Programming Languages (VPLs)</b> <i>ARE</i> the new top layer of the abstraction stack. See the history of 2D VPLs below to understand why they MUST go 3D to effectively visualize agentic work output.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">History of 2D VPLs</span></span>
</summary>

Node-based languages are incredibly popular and growing more robust each day in VFX toolchains e.g. Nuke, Houdini, Unreal, Substance, Blender, ComfyUI, etc. These never became Turing-complete because they never needed to be. The tech artists using them always had human programmers on the team who preferred text-based code because image-based nodes were bulky, making instruction sets more verbose, which meant fewer instructions could fit in one viewport. It felt annoying and unnecessary to "actual" programmers.

But we "actual" programmers wouldn't want to admit that there was job security in preventing VPLs from becoming Turing-complete, and that job security is disappearing with agents. A tech artist can now task an agent to make a new Blueprint node in seconds that would have taken days to move through a human teammate's workflow. If it were possible to streamline a VPL's info density and flesh it out to the same level of capability as a high-level language (it IS possible), then VPLs might one day unlock a thing text can't offer:

<b><i>ICONOGRAPHY</i></b>

We use icons for many things, but not programming. YET. Why write a class name when an agent can do that in the background and then also present you a symbol (or an origami-style glyph in 3D)? The symbol takes up less space on screen, and then it takes up even less space in a cloud of other symbols in 3D space. You can still identify it at a glance, and you can fit 10X as many data structures in the same viewport.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Why VPLs must go 3D</span></span>
</summary>

Anyone familiar with Unreal Blueprints will know there's something magical about the way it visualzes execution flow. The connectors glow as data flows through them. However, it's really not useful yet, because you must zoom all the way in to read the text on the nodes, and zooming that far in will make you miss potential issues elsewhere in the application when the execution flow breaks at the wrong time. 

The nodes are all rectangles in all current VPLs. This is their downfall. You can't tell any node apart at a distance. None use iconography yet. If they did, then we could see the entire app lighting up like a Christmas tree all at once, or like clusters of synapses, and we'd realize we WANT to zoom out all the time, making it obvious that 3D clusters will give yet another order of magnitude of zoom-out capability. This would be a better debugger than anything we've ever had in an IDE. THIS is the new layer of the stack, a real-time VISUALIZER of agentic changes to the code.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Floating Clusters of Synapses Are the New Code</span></span>
</summary>

The new layer of the stack is 3D node-based collections of synapses where each synapse represents a class, method, or property, and each unit glows on activation like a realtime MRI machine for your application. That's what Unreal Blueprints are, except the text and rectangles make us look at it only through a magnifying glass, which breaks the purpose. Turing-complete 3D VPLs will start popping up like flies in the next few years, and one of them will become the new Javascript, taking the spatial computing industry by storm and finding its way into the back-end of most software systems. 

These new languages will be nothing more than visual wrappers on top of high-level and low-level languages (Javascript, C#, Swift, C++, Rust, etc.), but one could argue that C was nothing more than an assembly wrapper in similar fashion. 3D VPLs will be the primary way new generations of programmers engage with code. Just as the elder Millenial author of 3OS has never touched Fortran, it's possible some of the youngest Gen-Alpha software kids will rarely need to touch Javascript.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Agents Are Too Fast For Us Without 3D VPLs</span></span>
</summary>

Fixing this is the only way to keep up with agents as they make sweeping changes to our code in real time. It takes hours to review the code they write line-by-line. 3D VPLs are simply real-time visualizers of the CPU as it runs your program. They are the only way to ensure code stays clean mid-development without falling behind your competitors. Line-by-line code review is the new low-level perf op. It must be done for safety prior to release, but we can't do it for every changelist anymore. There's no time. Full code review will be a low-level task for advanced human programmers. It will occur approaching a release candidate. 3D node-based debugging will replace full code review in the early-to-mid dev cycle.

</details>

</details>
</blockquote>

---


<a id="wimp-is-wimpy"></a>
<blockquote>
<details>
<summary><b>🪟 WIMP is wimpy</b></summary>
<hr style="border: 1px solid gray;">

Dealing with complex matrices and clusters of files and data structures in 3D is impossible with today's default interface. 3OS rejects the default bias of legacy WIMP (Windows, Icons, Menus, and Pointers) and multi-touch paradigms that have been a crutch in modern XR for far too long.

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Minimalism is Inappropriate for Enterprise</span></span>
</summary>

Legacy paradigms are supported for full real-time integration, but they are not the primary interface. 3OS aims to knock the dust off of the works of HCI giants whose volumetric contributions fly circles around WIMP, yet their research got buried by the fervor of the mobile era's obsession with minimalism. 

<b>Minimalism has its place in consumer software.</b> For enterprise, it's time to unbolt the training wheels of minimalism to prove that 90s 3D UI optimism was not misplaced.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">WIMP Is To XR As Comannd Line Was to Windows 95</span></span>
</summary>

The WIMP paradigm is a backwards compatibility tool in XR. It should not be front-and-center in any spatial computing OS. A VR or MR HMD that uses WIMP as its default metaphor set is like Windows 95 launching into a command line with no Desktop, no Start button, and no File Explorer, except at least the command line still brought extra speed for power users. WIMP interfaces do the opposite in XR. Their primary value is familiarity comfort (appropriate for consumer apps), but their incredible UX latency baggage comes with it. Even so, they're currently the ONLY XR OS interface available in 2026.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Enterprise Professionals Want To Be Iron Man</span></span>
</summary>

We cannot expect enterprise software suite companies to consider porting their robust systems into XR using stone tools. Their customers want what's "cool" in sci-fi. Their customers want to toss holograms around the room. 3OS aims to prove they can have that dream in a way that makes them faster at their jobs, not slower, without uprooting their workflows from any legacy 2D software that must remain 2D.

</details>

</details>
</blockquote>

---



<a id="the-problem"></a>
<blockquote>
<details>
<summary><b>⚠️ The Problem</b></summary>
<hr style="border: 1px solid gray;">

Software companies (Adobe, Autodesk, Epic, NVIDIA, etc.) assess whether they should port their legacy platforms into a new OS frequently. Their software is typically web-browser first for cross-platform efficiency, but even with cross-platform apps, the most robust productivity tools typically run on the desktop version only.

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Why is desktop productivity king?</span></span>
</summary>

These companies understand that touchscreens (and by association XR HMDS) are more of an entertainment consumption platform than an enterprise productivity platform. There are exceptions, but a majority do not port all the desktop features of enterprise software onto mobile devices because...

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Multi-touch interfaces...</span></span>
</summary>

1) ...are physically smaller. Shrinking the number of available interactive fragments (especially when hands are constantly occluding some of them) reduces multitasking capability exponentially, hence why iOS requires developers to severely limit the number of user choices per screen controller.

2) ...cannot compete with desktop interfaces due to mouse/keyboard blowing multi-touch out of the water on throughput.

3) ...actively discourage manual raw data management by obfuscating direct data access, implying that some OS makers do not want the developer community thinking of touchscreens (or other new platforms) as capable of cannibalizing desktop/laptop sales through competitive levels of productivity. 

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Mobile Went Smaller... XR Goes Infinitely Bigger</span></span>
</summary>

It follows that if you <i>increase</i> the number of interactive fragments (or voxels) instead of decreasing it, shortened total adjacency and multitasking capability should explode exponentially beyond the capability of the best desktop interfaces. Enterprise workers understand the simple virtue of multi-screen setups, but their value barely scratches the throughput surface of multiple screens times multiple layers of depth in XR, IF we can unlock those layers. It's intuitive and obvious that XR <i>will</i> be faster than desktop when we get the interface right.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Why then is the boot interface of EVERY XR OS patterned after Apple and Android?</span></span>
</summary>

The author of 3OS suspects it's primarily because companies like Meta and HTC hired the smartest people in the world on the subject of UI, and they were too smart for their own good. People who have their doctorates in HCI probably told everyone at these companies that raycast has 2X the throughput of any other option in XR, which is technically true according to most research (most of which was looking in the wrong direction), therefore everything BUT raycast is DOA. Case closed?

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">HCI Needs More Musicians</span></span>
</summary>

The case needs to be reopened. HCI PhDs were right about the data, but wrong about the conclusion. Raycast discourages bimanual (two-hand) interaction, so its 2X throughput is cancelled by nearly always being used one-hand-at-a-time in practice, whereas volumetric isometric models feel effortless with two hands. This was obvious to the author of 3OS years before he started nerding out on ancient SIGCHI papers because, once upon a time, he taught over 300 students how to play the drums. Nearly every student started with no bimanual coordination whatsoever, a lot like most HCI research study participants. Yet suddenly, after a few good words of advice, bimanual talent started coming out of the woodwork for every human being who learned a sliver of technique. Consumers may be less willing to adopt a stick-shift car, but what about CAD engineers, computer programmers, game devs, and VFX artists? They'll want paddle-shifters on their Ferraris if we can figure out how to build them.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Raycast Was DOA Before Getting Outed As a Dual-Hand Brain-Teaser</span></span>
</summary>

Aside from feeling wrong with two hands, raycast squashes the field to a plane, destroying the entire value proposition of XR as a productivity booster when it can only use <10% of its native voxels for adjacency and multitasking. Thus, here we are, 10 years after the launch of modern VR, and we still can't copy/paste two files into a folder at once.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">We Took The Path The Research Told Us to Take</span></span>
</summary>

The author of 3OS doesn't blame anyone on the design teams at Meta or Apple or Steam or Microsoft. If the path to becoming Iron Man or Tom Cruise were easy to see, thousands of us would have built it and no one would care, because it was just too easy. But our best UI scientists led us in the footsteps of Steve Jobs and Jony Ive with their best efforts AND mountains of valid scientific data, which all implied "this XR platform will NEVER do the things your desktop PC can do."

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Enterprise Software Teams Are Rightfully Jaded, If Still Employed in XR</span></span>
</summary>

Which enterprise company would consider porting their desktop features into XR in the 2020s under this state of play? They tried in the 20-teens, but they all used raycast cursors because the OS interface suggested, "this is what apps look like in XR." My favorite attempts were Unity EditorXR and Unreal VR Mode. These VR ports all hit a zero-user wall, not because XR is DOA for enterprise, but because they never got over the "slower than PC" hurdle, the app multitasking hurdle, the hybrid 2D+3D integration hurdle, or the heavy HMD hurdle. Today, these companies don't even employ many of the folks who built those awesome-yet-now-dusty VR port codebases. Their CEOs believe XR is not viable for enterprise because the boot interface was their template, and that template yielded apps that crashed and burned.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Hope Is Not Lost</span></span>
</summary>

XR has always had greater enterprise potential than desktop computers. Those VR ports will be resurrected with new MR life. This will be obvious as soon as the right OS interface enables the medium to shine with all of its advantages. AI interfaces are part of that story, but we can't always speak our work to an agent. Sometimes, work needs to be more private, even among colleagues in a shared office. We can't adopt XR in the office if the interaction model slows us to a crawl in those quiet moments. Thus, the XR industry must present an interface that is faster than desktop interfaces WITH OR WITHOUT an AI assistant. Only then can we put the interface on steroids with agentic integration.

</details>

</details>
</blockquote>

---

<a id="the-solution"></a>
<blockquote>
<details>
<summary><b>💡 The Solution</b></summary>
<hr style="border: 1px solid gray;">

The "right" XR interface must solve these three glaring challenges:

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">1) No Near-Field/Far-Field Parity:</span></span>
</summary>

The user has instant intuitive access to the near field, but extremely slow/cumbersome (if any) access to the far field. Since the far field is more than 90% of the whole field, the total productivity canvas in XR ends up smaller with lower throughput than its desktop equivalent despite having exponentially more usable voxels than 2D has usable fragments. A 2D desktop screen has hundreds of pixel fragment zones in which to place colocated buttons, text labels, images, etc. with rapid access to every one of them at once.

XR has millions of interaction zones in each frustum which would enable adjacency efficiency and multi-tasking that blows desktop interfaces out of the water, but most of these zones are unreachable without silly raycast fishing reel actions, an impossible wrench in the gears for throughput. Imagine requiring 30 seconds to click an icon in Windows. Furthermore, we can't easily see how ridiculously slow our XR interfaces are because this throughput drag is hidden by #2.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">2) WIMP is Insufficient:</span></span>
</summary>

The crutch of WIMP denies the use of the depth vector in concert with common 2D table layouts (i.e. WIMP makes 3D matrix and cluster layouts impossible), which occludes the vast potential utility of the XR field almost entirely, which in turn masks the throughput jank of the default raycasting interaction model. Raycasting successfully hides how terribly slow it is by forcing us to drive our Ferrari at 5mph as though that's how fast all racecars drive. A model that can enable hands or cursors to hover in any subdivision of the field with equivalent effort must replace the legacy model. 

Raycast has won the XR OS implementation battle so far by putting up HCI speed test numbers that double volumetric isometric competitors only because the tests in research studies are never done apples-to-apples according to real-world applications. In the real world, users only use raycast with one hand, not two, and apples-to-apples interfaces built to take full advantage of adjacency have rarely been tested in HCI studies at all. The author of 3OS can build apples-to-apples tests that prove raycast actually has slower throughput in the real world than volumetric isometric techniques under fair criteria, and that's before accounting for the value of volumetric isometric models having access to every depth layer of the far field simultaneously.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">3) Perspective Projection vs. Isometric Proprioception:</span></span>
</summary>

We see in perspective, but perspective can warp our mind's eye of Euclidean space. Our short arms define the near field where perspective causes almost unnoticeable size warping, thus our mind's eye for hand coordination is formed isometrically. We think of 3D space the way it actually is... true width, depth, and height have an equal relationship, not a scalar one. We know the object moving away is not actually shrinking, but if we need to use a magical interface to reach out and touch that object, we should not expect our hands to easily convert to angular space where farther objects require scaled hand motions. To access the far field with equivalent efficacy to the near field, we need to use a volumetric isometric interaction model, one that treats hand motion in the near field identically to the far field and unlocks our proprioceptive instincts.

</details>

### Long story short - 

An XR OS interface that works at rapid speed for enterprise MUST use <b>a volumetric isometric model</b>, NOT a raycast model. It must be volume-based, not surface-based. Raycast has no chance of achieving speeds competitive to 2D desktops. Volumetric isometric models can beat desktop speeds if they're built to the kinds of careful specs that have rarely been tested in Academia. Solve this problem, and enterprise adoption of XR can begin as soon as we have sunglasses-form-factor HMDs capable of MR and ergo-safe hand tracking (e.g. the hand tracking FOV of Quest is sufficient).

</details>
</blockquote>

---

<a id="3os-endgame"></a>
<blockquote>
<details>
<summary><b>🎯 The 3OS Endgame</b></summary>
<hr style="border: 1px solid gray;">

A single hand gesture interface orchestrates all of the following:

* Everyone does work faster while feeling like the star of their own sci fi flick
* A 3OS Bay in every University Comp. Sci. Immersive Lab & Engineering School
* A 3OS Bay running every dev room creating 3D assets and software in enterprise
* A hybrid 2D/3D toolchain for all multimedia & software dev teams
* A better interface for CAD, Game Dev, VFX, and software engineering/QA/debugging
* Seamless legacy 2D app support with physical and virtual monitors
* 3D remote control of all 2D monitors with hand gestures
* Rolling standing desks with keyboard, mouse, and fiducial trackers
* Fast-connect consoles to attach/detach standing desks to physical wall monitor pairs
* Rapid 3D file management with hand gestures
* On-Prem Agentic Avatars that can snapshot visual context of any XR workspace
* Agent-driven 3D visual node-based programming & debugging
* Agent-driven version control and project management
* XR App multitasking with ability to minimize/maximize app volumes
* Run on an on-prem 2D+3D hybrid VDI stack via wireless streaming
* Up to 12 simultaneous user workstations (XR HMD + console) per room
* Rapid-access workspaces for multiplayer collab, comms, presentation, and solo work


The 3OS Bay can be productized as a roomscale supercomputer that increases productivity for professionals already working with 3D content. It is very specifically <b>NOT</b> a conference room. It's not for meetings. It's for simultaneous colocated collab and solo development. It can be used to present work progress, but a majority of the time it is best dedicated to the 12 workers working in small teams together to complete a large range of tasks involving 3D assets. One user can work in a solo workspace using the same space as 11 other users who are in a separate collab workspace. Early adopters are teams of developers and artists in product dev, engineering, VFX, game dev, architecture, auto, aerospace, etc.


</details>
</blockquote>

</details>

<hr style="border: 1px solid gray;">

<a id="what-is-3os"></a>
<details>
<summary><b>🏗️ Architecture</b></summary>
<hr style="border: 1px solid gray;">

<a id="3os-basics"></a>
<blockquote>
<details>
<summary><b>📐 3OS Basics</b></summary>
<hr style="border: 1px solid gray;">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">It's All Voxels</span></span>
</summary>

Instead of forcing users to aim virtual laser pointers at floating 2D windows, 3OS treats the entire environment as a high-capacity voxel field where the parameterized resolution defaults to a 3-inch voxel, the approximate size that satisfies high selection accuracy at rapid speed with 6DOF 10-finger hand motion (see the Trivariate validation below). The goal is to provide users a voxel grid representing all the subdivisions of 3D space in which they can hover, select, or otherwise interact with any voxel in under 0.25 seconds regardless of their body's distance away from that voxel. This is an impossible feat in all current spatial computing operating systems because of the false assumption that OS interfaces must use WIMP.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">How Many Interaction-ready Voxels in a Field?</span></span>
</summary>

At 3 inch voxel resolution and a default 50-foot radius from head pose to the grid's outer shell, the grid contains on the order of ~15 million voxels if the user is standing in a wide open field. Theoretically, it could be larger as some viewers have accurate parallax stereo vision up to several hundred feet, but 3-inch objects side-by-side shrink into uselessness via perspective long before that distance, thus 50ft is a decent parameterized ballpark limit.

</details>

</details>
</blockquote>

---

<a id="3os-core-implementation-philosophy"></a>
<blockquote>
<details>
<summary><b>🏛️ 3OS Core Implementation Philosophy</b></summary>
<hr style="border: 1px solid gray;">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Voxels, Not Surfaces</span></span>
</summary>

The fundamental building block of UI/UX in 3OS is a discrete coordinate volume (optimized at $\approx 3\text{ in}^3$ for reliable human motor capability under rapid hand motion), not a pixel fragment or a flat projection plane. Everything that was surface-based in WIMP is assumed volume-based in 3OS. Where WIMP gave us Windows, Icons, Menus, and Pointers, 3OS gives Volumes, Objects, Matrices, and Points. 3OS is a VOMP interface.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">The Drummer's Rule</span></span>
</summary>

3OS volumes adhere to strict ergonomic criteria. A spatial OS should never subordinate micro-precision tasks to macro-stability muscles. First and foremost, this means the user should never be required to walk across a room to engage with an object simply because that object is outside the near field. The lost energy and time of closing the gap to bring the object into reach is a cardinal enemy of productivity. In 3OS, we compress far field spatial volumes into near field proxies called Voodoo Volumes instantaneously so that interaction is entirely driven by high-speed, low-fatigue motor groups (eyes, wrists, fingers), keeping the shoulder girdle (**coracobrachialis**) kinetically neutral for a vast majority of each session. The user can walk during use of the interface if the interface is anchored to their body rather than to the room, but large full-body "walk-across-the-room" actions are never required to manipulate a given element. At most, ten-finger or 6DOF-controlled actions require the elbow to move no more than 6 inches out of its neutral pose forward or backward (never outward), and developers should encourage the elbow to return to neutral pose for a majority of any experience. This rule is derived from the Moeller Method that enables drummers to play for hours without fatigue. This methodology comes from the author's personal experience as a drum teacher relating drum student arm fatigue to common "gorilla arm" issues in HCI research.

</details>


<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Topology vs. Kinematics</span></span>
</summary>

Spatial interaction in 3OS is comprised of...
    **Voodoo Volumes** (macro-topology; managing *where* data lives via instant coordinate remapping) and...
    **Telekinesis** (micro-kinematics; managing *how* data is moved on smaller scales via velocity-controlled telekinesis). Telekinesis and direct grabbing transition seamlessly as soon as an object enters or exits the near field. 
Voodoo volumes (large gaps instantly) and Telekinesis (small translations) follow 80/20 Pareto distribution in prevalence of recommended use, where most objects occupying field space can and should be teleported into position via Voodoo Portals, though the user may occasionally desire to shift them telekinetically if the distance is fairly short.

</details>

</details>
</blockquote>

---

<a id="ergonomics-determine-interactivity-resolution"></a>
<blockquote>
<details>
<summary><b>✋ Ergonomics Determine Interactivity Resolution</b></summary>
<hr style="border: 1px solid gray;">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">The limits of the body determine ideal interaction volume bounds</span></span>
</summary>

The Trivariate Target paper by Grossman and Balakrishnan (see Prior Art #5) was instrumental in determining how large the voxels should be for fast-moving hands in a voxel-based OS. The default assumption in 3OS is that hands will be moving rapidly at various moments within the bounding box of a given voodoo volume. Elimination of overshoot errors is paramount. Also, the constraint of elbow position for "gorilla arm" prevention requires a specific bounding box size for that volume.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Derived Interaction Volume Parameters</span></span>
</summary>

Every new user to spatial computing should be taught to begin by rolling out the shoulders and shaking the flex out of their arms. Using 3D UI for hours is all about getting loose and staying loose.

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">The Floor of the "Ergo-Safe" Volume</span></span>
</summary>

They start with hands pointing straight forward from a neutral-posed elbow. The palms-up blades of the hands define the bottom plane of the ideal imaginary bounding box at the navel-level. The bottom center of the bounding box is approximately 12 inches forward of the navel (give or take an small extra forward offset for preferred distance between hands and cursors), allowing the user to place fingertips in the center of the box's bottom plane simply by touching their fingertips together with both hands while elbows are still neutral. This is an imaginary reference point to show approximately where the box anchor lives in 3D space. Wherever the upper body turns, that floating box is directly ahead, axis-aligned with the shoulders.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">The Height of the "Ergo-Safe" Volume</span></span>
</summary>

In 3OS, the user can instantiate multiple voodoo volumes side-by-side. 3OS may use IPD and/or user height to estimate the ideal default forward offset of the voodoo volume's center anchor. A height of approximately 12-18 inches above that bottom center point constitutes the total height of the voodoo volume because lifting fingers above eyeline height immediately overtaxes the coracobrachialis (collar-bone-to-bicep) muscle. This muscle can handle hours of flex with fingertips below eye-level, which gives us approximately ~18 inches max for the "ergo-safe" volume. The box may have a small offset away from the body defined by 3OS near-field point cursor parameters. For instance, if the near-field cursor offset in front of the hand is 3", then the ergo-safe volume in total is extend 3" forward away from the body.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Left/Right Bounds of the "Ergo-Safe" Volume</span></span>
</summary>

The left and right bounds of the "ergo-safe" volume are marked by the user's shoulder width, since twisting hands outward beyond that range is a shoulder cuff hyperextension, or an opening of the wingspan, which engages pecs and lats (generally a mistake of wasted energy and speed). To access to the left and to the right, the user should be trained to turn the entire upper body as a unit, effectively retargeting their "ergo-safe" volume.

Shoulder widths vary drastically. Thus, for 3OS, we target a lowest-common denominator of 12", which allows for most body sizes to instantiate up to three voodoo volumes in immediate "ergo-safe" positions, one in center, and one each on left and right flank. The flankin volumes are always in periphery, and engaging with them requires only a tiny twist of the spine to retarget both hands at once. User's should be encouraged to bring both hands together toward each target volume by aligning the shoulders on it, since this encourages restful elbows and allows them to take full advantage of the dual-hand power user features of 3OS.

</details>


<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top; padding-bottom:10px"><span style="font-weight: 600;">Near/Far Bounds of the "Ergo-Safe" Volume</span></span>
</summary>

The near and far bounds of the "ergo-safe" volume are marked by the reach of the fingertips when the elbow has moved out of neutral pose by no more than 6 inches, defining an "ergo-safe" depth of 12" for the whole volume.

</details>


The total volume is a <b>12"x12"x18" (Width x Depth x Height) imaginary bounding box</b> which fits a <b>voxel grid of 4 x 4 x 6 with 3" voxels</b>. 

These bounding measurements are agreed-upon by drum teachers to enable human hands to execute rapid-fire actions in quick succession in 3D space for several hours without fatigue, confirming that less-rigorous spatial computing hand gestures fit well within the limits of the same scope.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Research-backed Voxel Size</span></span>
</summary>

See [spatial_os_trivariate_validation.md](spatial_os_trivariate_validation.md) for proof that 3" voxel size is ideal for any spatial computing OS in which users want to move hands voxel-to-voxel without overshoot, yielding an ideal 4x4x6 matrix as our default voodoo volume. Objects can certainly be bigger and smaller than this resolution, but the underlying default size of objects and spaces between objects if grid-snapping is ever in use will follow this default format, and developers of 3OS apps must note that if they want to unlock the fastest possible user hand motion, they should build elements no smaller than this approximate target. 3OS parameterizes all of these default assumptions.

</details>

</details>
</blockquote>

---

<a id="field-interaction-state-machine"></a>
<blockquote>
<details>
<summary><b>🔄 Field Unification State Machine</b></summary>
<hr style="border: 1px solid gray;">

To eliminate physical exhaustion ("Gorilla Arm") and sluggishness of legacy XR selection systems, 3OS routes all user intent through a deterministic, cascading three-stage state machine, the first two stages of which can be traversed in under 0.5 seconds for any voxel volume in any field.

```
[State 1: Macro-Targeting] ──> [State 2: Micro-Resolution] ──> [State 3: Possession / Transform]
(Voodoo Drop gesture)           (Double Proxy Voxel Grid)       (Velocity-Mapped CRUD Action)
```

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">State 1: Macro-Targeting (The World-in-Miniature Proxy A.K.A. "voodoo drop")</span></span>
</summary>

* **The Problem:** 

  Humans cannot easily gaze at or hand-raycast to an empty voxel in which to instantiate or manage far field elements. Perspective-based techniques are insufficient for fast-twitch control of the far field. A single stationary gaze vector penetrates thousands of voxels simultaneously, potentially intersecting hundreds of objects, or none. Raycast can handle only one. The ability to choose any one voxel in the entire field must be rapid and intuitive for enterprise speed.

* **The Solution:** 
  
  Via gesture or button press, the user instantiates a localized, downscaled **Invisible Isometric Field Proxy** within comfortable, elbow-nearly-at-rest reach. The proxy is one of two types, snap or free-range, each mapped to a different gesture. The snap proxy contains a cluster of focus anchor visualizers to target, whereas the free-range proxy has no widget, allowing unbiased manual placement of the far-field voodoo volume anywhere in the field. The default ratio of the actual field to the invisible proxy miniature is 50-to-1 at default parameters in an open field e.g. the Mind Palace. These parameters flex to the size of the room, typically constraining down in MR since most MR indoor rooms are less than 50' wide. 
  
* **Isometric, Not Perspective:**

  For every 1' the user moves their hand on X, Y, or Z, the new voodoo volume moves 50' on that same axis. This world-to-user proxy is technically a World-In-Miniature technique except it provides no mini-map widget because none is needed. The technique is meant for fast-twitch action to rapidly grab a chunk of volume from anywhere in the far field. It's a technique not intended to be held long. It has a small offset allowing the new far-field voodoo volume to instantiate a few feet forward of the user, just out of reach and out of the way of its near-field voodoo copy. Importantly, this is not a "go-go" solution, a term popularized by a 90s research study that referenced Go-Go-Gadget, though it might easily be mistaken for one. Go-go variations are perspective-based. The Voodoo drop is isometric, a key distinction for accessing the whole field uniformly. It feels like egocentric raycast or go-go in that it feels like your intent is shooting out of your hand or body, but then the wacky and unpredictable fast-far-and-slow-near speed variations of the target are magically normalized by the isometric mapping.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">State 2: Micro-Resolution (The Double-Proxied Voxel Lattice A.K.A. "voodoo volume")</span></span>
</summary>

* **The Mechanism:**

  The moment a coarse region is selected via the voodoo drop, that specific chunk of space instantly blooms into a **Double-Proxy Voxel Lattice** directly in front of the user's primary workspace. The chunk of empty space is proxied to the user. Simultaneously, the user's point cursors are proxied out into that chunk in the far field.
* **Lattice Visualization:** 

  The hidden coordinate structure of that far field volume can be rendered locally as a subtle, wireframe volumetric lattice, indicating the snappable layout as new objects enter that space.
* **Decoupled Selection:** 

  The user drives a point cursor that is a 1:1 Euclidean replica of the thumb. Since other digits typically move through space to meet a mostly stationary thumb during pinch, the thumb is the ideal target for an offset point cursor. The offset relative to the user's body puts the cursor slightly forward and upward of the hand, so the hand does not occlude the near-field voodoo content. The user is free to gaze at the actual fragment in the far field, using the peripheral proxy as a remote control device, or if the fragment is too far away for good visual fidelity, the user can look primarily at the proxy instead. Cursor hover and selection can initiate CRUD actions, especially CREATE functions if the current proxy contains empty voxel space. Menu options e.g. Create New Box (3D equivalent of a folder) or Create New Object (3D equivalent of a file) are typically presented in matrix layout as long as there are more than four options.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">State 3: Possession & Transformation (Telekinesis Engine)</span></span>
</summary>

* **The Mechanism:** 

  An already created object can be hovered by sending a point cursor inside its volume rather than raycasting onto its surface. Once hovered, the user can select it with primary pinch. Once selected, the user has the option to use the proxy (a voodoo doll) as a transformation remote control for either direct manipulation or remote **Velocity-Based translation and/or rotation**.
* **Kinetic Economy:** 

  Tiny, low-fatigue wrist gestures generate acceleration and velocity vectors that scale proportionally, gliding objects effortlessly across the field in straight lines rather than the unpredictable arcs of raycast fishing line actions.
* **Momentum & Friction:** 

  The system incorporates virtual inertia. Releasing an asset with momentum allows it to slide naturally through the voxels and settle based on an OS-level spatial friction coefficient.

</details>

</details>
</blockquote>

---

<a id="voodoo-volumes"></a>
<blockquote>
<details>
<summary><b>🌀 Voodoo Volumes</b></summary>
<hr style="border: 1px solid gray;">

3OS bypasses the slow, physics-based fishing-line-reeling or lassoing of legacy XR raycasting techniques by utilizing **Non-Euclidean Voodoo Volumes**, A.K.A. rectangular chunks of volume that act as wormholes between the near field and far field. The technique is heavily inspired by the Poros Framework (see Prior Art #8).


<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">The Voodoo Drop</span></span>
</summary>

The user sees an object or cluster of objects in the far field and decides to interact. They initiate the voodoo drop gesture (see Control Basics). The object or cluster is likely to belong to a focus anchor recognized by 3OS, enabling a rapid drag motion to snap-lock onto all targets of that anchor in one volume. The user releases the gesture. A voodoo volume appears in hand's reach containing voodoo dolls of that object or cluster, or the incoming voodoo volume can optionally be replaced by a custom widget if the target is configured to launch an OS widget, or if it is within an application volume and was configured for a widget launch by the application's developer. In addition, the developer can configure focus anchor LODs so that their app volume need not become overly cluttered by dense clusters of focus anchors when a high-LOD anchor can reveal lower-LOD anchor groups and/or individual anchors, which can drill down via nested voodoo volumes.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Voodoo Volume Dragging</span></span>
</summary>

If a double-proxy voodoo volume is open, dragging an object from the near field into the voodoo volume teleports that object out into the far-field. Vice versa, dragging an object out of a voodoo volume teleports it from its far field volume into the near field. The need to grab onto a far field object with raycast and reel it in is entirely eliminated. The concept is heavily inspired by the Poros Framework (see Prior Art #8).

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Cross-Proxy Voodoo Volume Dragging</span></span>
</summary>

If multiple double-proxy voodoo volumes are open, dragging an asset out of one local volume and dropping it into another triggers an instantaneous teleportation from volume A to volume B in the far field, essentially letting the object travel through two wormholes at once.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Teleportation</span></span>
</summary>

An optional feature. If the user selects an object within a voodoo volume and then executes a primary pinch outside that voodoo, or vice versa, the object teleports to that new pinch location. If that location is inside another voodoo, the destination is the far field equivalent of that location. This is essentially identical to dragging in and out of voodoos or across voodoos, except no drag is necessary. Instead, the user just pinches to mark the destination. This feature can be toggled on/off since some users may find it confusing. With teleportation on, an item they selected is only selected within its voodoo context, which means deselection only works in that context. Outside of that context, pinch becomes teleport instead of deselection. New users may confuse the two, but power users will enjoy the effortlessness of teleport needing no drag.

</details>

</details>
</blockquote>

---

<a id="world-mesh"></a>
<blockquote>
<details>
<summary><b>🌍 World Mesh</b></summary>
<hr style="border: 1px solid gray;">

### Sitting/Standing vs. Walking Mode
3OS assumes the OS origin $(0,0,0)$ is a fluid variable, allowing users to toggle the MR field between walking and world modes, but it also anticipates that this may be handled via HMD drivers by the OEM and leaves room to integrate with unique hardware via API:
* **Walking Mode (Egocentric - User-Locked):** The global reference frame for the OS becomes a hit point at dead-center on the floor below the user's HMD pose and heavily dampened on global Y (vertical). All virtual content that is not world-locked  fixes itself to this global reference until this mode toggles off. Objects already anchored in the primary dev room reference frame will de-anchor to cache dual walking+stationary anchors for seamless transition between an office and travel outside e.g. a trip to a coffee shop. These dual anchor transitions will only occur if the object is in MR, not VR, and only if not flagged as world-locked.
* **Worldspace (Default) Mode (Allocentric - World-Locked):** Grounded permanently into the physical world mesh, mapped by SLAM tracking and cached on the server.

* **World Mesh Caching:** World meshes will be cached on the server in Worldspace mode if possible. The server expects a primary default room mesh of up to 50'x50'. It can store larger, but it defers to the caching capability of the HMDs. Attempting to scan adjacent rooms may result in the server caching each room separately in the future for appropriate occlusion culling, but this is also anticipated to be handled by the HMD OEM. The server and streaming client also may reference GPS data and/or world mesh digital twin aggregation data in the future to support long-distance travel, but 3OS Server by default will permanently store a scan of the primary dev room for rapid anchoring under the assumption that much of this caching work will be done on each HMD by the OEM. The server will make itself available as expansion storage for the HMD's local world mesh caching algorithm if the OEM allows it via API, including caching one shared room mesh for all HMDs sahring the room if possible.



</details>
</blockquote>

---

<a id="field-rings"></a>
<blockquote>
<details>
<summary><b>⭕ Field Rings</b></summary>
<hr style="border: 1px solid gray;">

For decades, HCI research has subdivided the interactable spatial field into two main categories:

**Near Field:** 
  everything your fingertips can reach

**Far Field:** 
  everything your fingertips can't reach

These are the root categories of an interactive 3D field because they require two different interaction techniques. Users may grab objects in the near field and may expect them to have physics-based and/or skeuomorphic properties, whereas some form of raycasting (not preferred in 3OS) or telekinesis (preferred) is needed to reach far-field objects.

In 3OS, we need to take these two subdivisions and divide them a bit further.

### Deeper Subcategories:


<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 900;">The Pseudo-Near Field</span></span>
</summary>

<blockquote>
<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">UI is Ghostly For Good Reason</span></span>
</summary>

<blockquote>
In 3OS, the goal is always to merge chunks of the far field into the near field instantly to take advantage of the proprioceptive power of near field interaction, yet 3OS also avoids physics-based and skeuomorphic interactions by default since neither lend themselves to clusters of objects that can take advantage of the whole far field by avoiding gravity and occlusion. Thus, all interactive UI objects in 3OS are ghostly by default. They float and are translucent.
</blockquote>

</details>
</blockquote>

<blockquote>
<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Keep It Close But Not Too Close</span></span>
</summary>

<blockquote>
When these ghostly 3OS objects enter into the near field, the user does not put a hand directly inside them for manipulation. Instead, the user's hand drives a point cursor (offset a few inches forward and above the hand by a parameterized amount). The point cursor enables the user to slightly extend their near-field reach. They gain proprioception over that extended space because the cursor feels like yet another joint of their arm, which widens the near field. We call this extension the pseudo-near field (an outer shell of the near field), and most 3OS UIs live in this pseudo space when called in from the far field. This technique is crucial for comfort and efficiency:
</blockquote>

</details>
</blockquote>

<blockquote>
<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Point Cursors Create the Pseudo-Near Field</span></span>
</summary>


<blockquote>
3OS UIs are usually contextual widgets for far-field objects, meaning they operate as remote controls meant to utilize <b>peripheral vision</b>, which is only possible via proprioception (i.e. you know where your hand is without looking). Hands with good neutral elbow position are slightly too low for good peripheral cues if they're sharing the same space with these widgets, which means you'd have to move the widgets too low for periphery to get good ergo, or make the user hold their hands too high to get good peripheral cues. The solution is separation of the hands from the widgets with a slight offset via point cursors. Pseudo-near field widgets driven by point cursors are the best-of-both-worlds: comfort and efficiency in one interface.
</blockquote>

</details>
</blockquote>

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 900;">The Two Rings of the Far Field</span></span>
</summary>

<blockquote>
The far field is gigantic. It makes sense to subdivide it as soon as we begin using it for large and complex clusters of objects. The immediate far field (radius of ~20ft), allows for exponentially more clarity via less perspective size warping and parallax stereoscopy than the rest of the far field. In 3OS, we think of the far field in two parts:
</blockquote>
<blockquote>
<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">The Working Ring</span></span>
</summary>

<blockquote>
This is the closer section of the far field, typically defined by ~20ft radius from the user in all directions. This range is not parameterized by 3OS because it is a mental guideline for users, not a requirement. It helps to keep a field organized by keeping objects that need frequent manipulation closer in the far field.
</blockquote>

</details>
</blockquote>

<blockquote>
<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">The Storage Ring</span></span>
</summary>

<blockquote>
Likewise, objects that may sit untouched for hours, days, or weeks belong in the outer ring of the far field (beyond 20ft range) where they can be brought into the working ring once they become relevant for new tasks.
</blockquote>

</details>
</blockquote>

<blockquote>
These ring subdivisions are not necessary in every context, but they exist to support the most complex enterprise interactions available, including rapid file management, 3D visual node-based programming, 3D data visualization, etc.
</blockquote>

</details>

### Object Layout, Padding, Margins, and Capacity Across the Field:

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Concentric Data Ring Layout</span></span>
</summary>

The far field is non-uniform, but it can often be logically and conceptually subdivided especially during instantiation of large numbers of objects that must align in matrices and also avoid collision with each other. The rule of thumb for incoming objects is to instantiate in the near field if the total number of objects is fewer than 6 and the near field is clear of potential collisions. Otherwise, priority instantiation happens at ground level, and matrices lay out isometrically relative to themselves but concentrically relative to each other. For example, if two dozen matrices each containing 125 objects must instantiate simultaneously, they would do so in a panorama of 5x5x5 matrices side by side in the far field as close as possible with adequate padding and margins.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Maximum Single-Field Capacity</span></span>
</summary>

Until extensive UX testing can be done on densely populated fields, it is assumed that the maximum number of objects that can occupy an entire single field is 1.5 million. We back into this napkin math by assuming a field of cylindrical shape could fill the ground level with matrices starting with concentric rings from the user out to the radial limit of the field, then stack multiple more layers of matrices on top until the field is full. At the ideal minimum size of 3" for each voxel, we assume the objects should not be smaller, and we assume that ~90% of the voxel space is always dedicated to padding and margins between the objects. A field of 50' radius and 30' height can accommodate ~15 million voxels, thus ~1.5 million objects completely fill that field.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Best Practices On Per-Field Capacity</span></span>
</summary>

It is less than ideal to fill an entire field and leave no breathing room to utilize extra empty space just as it is less than ideal to fill a 2D desktop with hundreds of icons until there is no room to move an icon anywhere. Thus, we assum the functional working range of the total number of objects in a field can be measured in six figures at maximum capacity, though most users will probably opt for sparse fields in the three-to-four figure range since they can jump from one field to another as easily as a change-directory command in a CLI.

</details>

</details>
</blockquote>

---

<a id="mr-versus-vr"></a>
<blockquote>
<details>
<summary><b>🥽 MR versus VR</b></summary>
<hr style="border: 1px solid gray;">

3OS provides the enterprise user the space they need for the task. Sometimes, that's a small widget anchored to their desk as they mostly work on their physical 2D monitor. Sometimes, that's a remoted control widget driving MR content filling half the room. Yet other times, that's a sea of objects or data visualization nodes from all across the user's databases and hard drives. Rapid access to data is key, unlike in consumer XR. This is why 3OS places heavy emphasis on the ability to carry files or folders from MR to VR and back again. Folders are boxes in 3OS. Files are objects shaped in the glyph of their application logo, just as applications define file types and icons in other operating systems.

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 900;">Field Roots vs. Child Boxes</span></span>
</summary>

<blockquote>
If the user opens a box (i.e. drills into a folder) the contents of that box will emerge out of the box without dismissing the contents of the parent file structure. If the user wants to isolate the field to visualize just the contents of that box, they can favorite it and open it as a Field Root, meaning the current field itself IS that box. In this way, the Mind Palace begins by mapping the default field to the root directory of the first hard drive it can access, but any descendent box/folder can become a Field Root.
</blockquote>

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 900;">Rigidbody Matrix</span></span>
</summary>

<blockquote>
Set to `Disabled` by default for each object. The user can opt in a given object's properties to toggle physics on for that object, allowing it to receive gravity, collide with world geo, etc. The 3OS SDK will also give 3rd parties the ability to generate objects that may have basic 3OS physics toggled on by default, or they may be driven by a physics engine defined by the app. Newly created objects float statically by default. For OS objects, the user must manually turn gravity on if desired, but they should understand that full use of 3OS capability means allowing most objects to float.
</blockquote>

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 900;">Floor Collision</span></span>
</summary>

<blockquote>
Active by default with a `Strict` policy. Leveraging semantic AI mesh pipelines, the system enforces a hard spatial floor constraint—preventing floating voxels from clipping through physical floorboards or sinking into unreachable space even if their rigidbodies are disabled or they have none. 3OS assumes by default that OS objects will never intend to clip through the user's floor plane. This restriction can be toggled off per-object.
</blockquote>

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 900;">Transitions between MR and VR</span></span>
</summary>

<blockquote>
3OS seamlessly toggles between MR and VR, assuming that MR is an immediate application staging workspace while VR is by default a file management workspace. That said, apps and files are able to cross-pollinate at will. You can run an app in the VR Mind Palace and you can bring files and folders (objects and boxes) from the VR Mind Palace into the MR workspace. In MR, the environment is typically constrained by indoor world mesh provided by the HMD's scan of the room, typically a square or rectangular office space with four walls, a ceiling, and a floor. This space may have basic contours that turn it non-rectangular, and 3OS will accommodate a variety of room shapes. 3OS will also attenuate field range sensitivity of all of its controls in MR when less total field space is accessible so that control sensitivity maps well to each environment, sort of like mouse sensitivity needing to map to screens of different resolutions or multi-screen configurations.
</blockquote>

</details>


</details>
</blockquote>

---

<a id="Apps-vs-Objects-vs-Assets-vs-Icons"></a>
<blockquote>
<details>
<summary><b>🎲 Apps vs. Objects vs. Assets vs. Icons</b></summary>
<hr style="border: 1px solid gray;">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 900;">What is an object?</span></span>
</summary>

<blockquote>
Spatial computing can naturally blur the lines between all metaphors for categorizing data. If a file is an object now, what is the 3D mesh that becomes visible when you open that object? It's also an object, but it's inside the context/volume of an app instance. For example, if you double-pinch a Blender file in 3OS, you're double-pinching a Blender object. A Blender volume will open. Inside, it will display a 3D asset, e.g. a t-rex mesh. That t-rex mesh is also an object within the Blender app. The Blender logo glyph is an object in the OS just outside that volume that was used to launch an instance of Blender, which displayed another object. They're two types of objects sharing the same 3D space. This blurring of metaphors may take some time for users accustomed to clear distinctions between icons versus images versus 3D assets. In 3OS, icons are 3D glyphs, a type of 3D asset. In 2D, icons are technically just low-resolution images, but they tend not to share canvas space in confusing ways because 2D windows occlude the OS window that launched them, whereas in 3D it will be common for the launcher and the launchee to hold persistent, adjacent space.
</blockquote>

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 900;">What is an app?</span></span>
</summary>

<blockquote>
Technically, in 3OS, an app is still an executable list of instructions. In fact, it's the exact same list of instructions before, except now it uses the 3OS SDK to modify it's render pipeline to be MR-responsive. But in similar fashion to the confusing nature of glyphs versus assets, an app could be a complex toolchain like Blender, or it could be a simple widget program designed to do nothing more than animate a t-rex object at the OS level. That t-rex executable is not a glyph, and it's not just a static 3D asset. It's an asset with unique behaviors. It's an app object, operating outside the bounds of an app volume because the object IS the app and the app IS the object. These distinctions that seemed pertinent between icons, objects, assets, and apps in 2D computing may break down entirely one day in spatial computing, but for now they will hold as long as we live in a world where 2D and 3D computing coexist in hybrid workflows, as in 3OS.
</blockquote>

</details>

</details>
</blockquote>

---


<a id="common-3d-element-types"></a>
<blockquote>
<details>
<summary><b>📦 Common 3D Element Types</b></summary>
<hr style="border: 1px solid gray;">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Files... but 3D</i></td></tr></table><b>1) Objects</b>
</summary>

<blockquote>
3OS is intended for full cross-compatibility with desktop OS interfaces because the file metaphor remains intact in an extruded 3D-ified form. A file is an OBJECT in 3OS. A folder is a BOX. Their icons are no longer icons. They're glyphs, low-poly prismatic pieces of geometry that can represent the brand of the app that generated the object. In addition, 3OS will support treating media as objects as well, including audio clips, video clips, images, 3D assets, and point-cloud-based assets (particle effects and/or Gaussian splat assets).
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Icons... but 3D</i></td></tr></table><b>2) Glyphs</b>
</summary>

<blockquote>
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
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Folders... but 3D</i></td></tr></table><b>3) Boxes</b>
</summary>

<blockquote>
Boxes in 3OS are the 3D equivalent of folders in legacy platforms. They can expand/collapse all of their contents into the current field, and they are the control source for a variety of spatial arrangements. 3OS has a global box mode and a local box mode, meaning expansion of a box can automatically trigger collapse of any currently-expanded other boxes in local mode, or expansion can be cumulative with all other expanded boxes in global mode. 3OS deals with spatial collision resolution of nearest neighbors by bumping newcomers into the closest available adjacent space. Global and Local box modes are only relevant in Manual Arrangement Mode. In Matrix and Cluster Modes, nested child boxes expand to fill space in separate matrices or clusters above the parent matrices or clusters.

**Options** (equivalent of View options in File Explorer):

* **Manual:** All objects within the box store their own list of world anchors if they each recognize the current environment; otherwise they default to a matrix layout near and above the box on expand. This is the only mode in which the object's spatial position is stored within the object and recalled in future sessions.
* **Matrix:** All objects default to a matrix layout near and above the box.
* **Multi-Matrix:** All objects default to a matrix of matrices categorized by a chosen attribute (file name, application, age in years/months/weeks/days, etc.).
* **Cluster:** All objects default to a cluster layout near and above the box.
* **Multi-Cluster:** All objects default to a cluster of clusters categorized by a chosen attribute.
* **Agent-Generated:** All objects default to a custom 3D layout generated by an AI assistant per user instructions, including advanced filtering, combinations of clusters and matrices of multi-condition criteria, etc.
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Command Line Directory Navigation... but 3D</i></td></tr></table><b>4) Voodoo Volumes</b>
</summary>

<blockquote>
These are the workhorse of 3OS, merging the near and far field via non-Euclidean portal behaviors. To engage one, the user performs a <b>voodoo drop</b> (see Field Unification State Machine): an invisible isometric field proxy maps near-hand motion to far-field placement—snap mode for focus-anchor clusters, free-range for unbiased drops. While the gesture is held, a far-field voodoo volume instantiates and moves in concert around the actual field. Releasing the gesture parks that far volume and opens the matching near-field voodoo volume in hand's reach, completing the portal. Now any contents or empty space in that part of the far field are suddenly reachable in the near field. Objects or boxes encapsulated by the volume show ghostly copies within reach; placing a point cursor or hand into the near volume projects a ghost of that cursor/hand out into the far field. The user may gaze at either end of the portal. The volume's bounding box can be pinched/stretched to scale the far-field box while the near box stays ergonomically fixed. If an object is dragged in real far-field space, the far box grows to keep it in scope while near contents appear to scale down. The box can also be single-hand dragged for velocity-based far translation. A control at the top back right corner collapses the portal.
</blockquote>
</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Sort of like sidebars in apps... but 3D</i></td></tr></table><b>5) Voodoo Widgets</b>
</summary>

<blockquote>
These are peripheral remote control devices. They appear temporarily just forward of the hand that engages the associated action, and they disappear as soon as the action is complete. The most common use is for remote transformations (dragging/rotating/changing state for far-field objects). These widgets visualize the hand's current action state e.g. a vector arrow to confirm drag is currently active at a specific velocity. For rotation, the widget is a miniature voodoo copy of the object so that any tumbling action can happen accurately even if the actual object is too distant to make out.
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Sort of like LOD-based GPS pins in map search... but 3D</i></td></tr></table><b>6) Focus Anchors</b>
</summary>

<blockquote>
These are the invisible backbone enabling voodoo snapping to work so rapidly. At the top level of 3OS, each open directory has contents potentially strewn about in a matrix or cluster, but a voodoo volume can latch onto that entire cluster instantly because it contains an invisible centroid, a Focus Anchor. Every directory has a focus anchor allowing voodoo volumes to snap to them and summon a voodoo doll of their contents into the near field for easy control. Focus Anchors are a key feature that will be available in the 3OS 3rd party SDK as well. When a 3rd party app launches, it is in multitasking mode by default, meaning it shares the same global field as everything else, including the file system and other open apps. But if the user sends a voodoo volume into the 3rd party app's bounding box, the OS won't have any default anchors to snap to, leaving the user to fish around the app's volume manually. Thus, it is important for 3rd party apps to define focus anchors early on because it enables the user to instantly summon access to any collection of elements within the app. This is a key advantage of 3OS because it enables the app content to BE the interface. If, for example, it's a CAD program, it can be designed such that every part automatically has a focus anchor, thus the developer can allow the user to pull/inspect/edit any part instantly without needing to code a custom UI. The OS already enables it.
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Application Windows... but 3D</i></td></tr></table><b>7) Application Volumes</b>
</summary>

<blockquote>
In 3OS, applications exist within 3D volumes rather than within 2D windows. Unmaximized volumes default to the 2x2x3 (WxDxH) aspect ratio of the default 3OS voodoo volume. 3rd party developers can use the 3OS SDK to configure their app manifest to launch minimized or maximized. Unlike 2D windows that have three states (minimized, default, and maximized), 3OS app volumes have four states: minimized, default, max_MR, and max_VR. This is because an app maximized in MR is assumed to take priority over all available space in the room, yet still must respect the boundaries of the room as its bounding box, whereas in VR the app's volume can expand to encapsulate infinite space. 3OS can be toggled from MR to VR with a gesture, and applications can be launched in either mode, so the maximize button dynamically fills the available space in either MR or VR. Maximizing one app minimizes all other open apps, sending them each a message that their render pipelines and tick loops can sleep, and they can await a re-focus message in standby. Minimized apps retain memory allocation, but minimize and re-focus callbacks can be used to perform appropriate memory management for perf op.
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Mouse Cursors... but 3D</i></td></tr></table><b>8) Point Cursors</b>
</summary>

<blockquote>
In 3OS, point cursors are nodes in 3D space tied to the thumb of each hand by an offset slightly forward of the body and upward. Point cursors are designed to resolve the conflicting needs of good ergonomics (restful elbows = low hands) versus peripheral widgets (too low for good peripheral visibility if ergo is good). Point cursors also resolve hand occlusion issues. Similarly to the magnifier popover in touchscreen texting when large fingers occlude letters and the letters need to be blown up and lifted above the fingers, point cursors allow near field widgets to be lifted slightly so they can be controlled without the hands covering up the value of their visible changes during hover, selection, etc.
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Buttons... but 3D</i></td></tr></table><b>9) 3D Buttons</b>
</summary>

<blockquote>
In 3OS, buttons are typically depicted as translucent cubes or other simple volumetric prisms, often with smoothed edges. Text labels may hover near, above, below, or inside the geometry, typically shaded over the top of their geo to enable clear legibility even if geo occludes any part of the text. Buttons in 3OS are volumetric, meaning cursors don't hover ONTO them; they hover INTO them. If touched directly by virtual hands, the button surface has no physics properties. The hand travels through it as though it is a ghost. We call it a button to avoid confusion around functionality. It is an event trigger handler just like any other button, but it does not reside on a surface, nor is it to be pressed. It floats in mid air, needing no flat panel to rest upon. A cursor enters into it from any vector and triggers a hover-state glow effect or other visual cue. During hover, the user performs a gesture, usually index-to-thumb pinch, to activate the button.
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Table layouts... but 3D</i></td></tr></table><b>10) Matrices</b>
</summary>

<blockquote>
Elements in legacy UI lay out on X and Y dimensions in table form. In a voxel-based OS, utilizing even one more interactive plane ahead of or behind the first plane unlocks more grid positions with less travel distance. For instance, a 4x4 or 2x8 table of elements in a 2D UI is much better laid out as a 2x2x2 matrix in 3OS. A minimum adjacency analysis on any collection of items will show matrices are ergonomically better and faster than tables.
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>2D clusters... but 3D</i></td></tr></table><b>11) Clusters</b>
</summary>

<blockquote>
Similar to matrices, but spatial positioning is more loose in that the objects need not align on any kind of local Euclidean grid. Instead, they operate on a set of min-max adjacency parameters from their nearest neighbors, usually grouped in close proximity due to some shared attribute.
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Radio buttons... but 3D</i></td></tr></table><b>12) Spherical Multi-Choice Nodes</b>
</summary>

<blockquote>
A 3D-ified radio button element. 3OS supports easy creation of these spherical interfaces which are similar to desktop radio button lists in that alternate choices (A or B or C etc.) are arranged near each other, but in 3OS they can occupy uniform angular space around a center point. They may render as segments of a sphere, or each as 3D buttons floating in a formation. The cursor's vector relative to the center point changes hover state enabling highlight of one of the elements in that collection. This interface style is the highest-performance voxel-based multiple-choice option because it eliminates any possibility of overshoot errors. The multi-choice node may have an extra core node at its center which could be an extra multi-choice option or a neutral "none" option, and it may also have an out-of-bounds limit radius encapsulating the group.
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>2D UI brought into 3D</i></td></tr></table><b>13) Sync Planes</b>
</summary>

<blockquote>
Generally, planar UI in 3OS is discouraged. Default UI elements are volumetric and isometric, whereas planar elements typically require perspective-based raycasting for far field interaction, which is not impossible, but it must be custom-built case-by-case at the service and application layers since 3OS offers no built-in raycast and recommends against 3rd parties using it too. In general, if a planar UI exists in 3OS, it typically represents the virtual twin of a physical screen in an MR context. For instance, in a room with 12 2-monitor workstations mounted to its walls, any pair of monitors might be in use with WIMP-style UI elements on-screen. 3OS will be able to render that screen's output at any size via a sync plane, but more likely the sync plane will be invisible and superimposed over the physical screen it represents, enabling remote control interaction. Sync planes will feature API integration and transition between 2D and 3D elements. The WIMP instance running on the 2D screens would be rendered by the same server as the OS driving the eyewear, thus it holds an inventory of all shared interactive elements, which means an HMD user could use a voodoo volume to target the screen itself, or target one of its open windows, e.g. a File Explorer. Once the 2D File Explorer window is targeted, it would automatically be converted to its 3D representation in the near field in the same fashion as using a 3D-to-3D voodoo volume. This 2D-to-3D transition can work identically for any 3rd party software. For example, a control widget in a 2D Autodesk Maya window might be pulled into 3D MR space via a voodoo volume if Autodesk developers used the 3OS SDK to enable that transition.
</blockquote>

</details>
<br clear="all">

</details>
</blockquote>

---
<a id="the-application-layer"></a>
<blockquote>
<details>
<summary><b>🧩 The Application Layer</b></summary>
<hr style="border: 1px solid gray;">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<b>Early Adopters</b>
</summary>

<blockquote>
3OS is designed first-and-foremost for professionals who are already working in 3D editing software (CAD, VFX, and Gaming), but it is also designed to be able to grow into mass market adoption after establishing 3D professionals as early adopters. Eventually, a Home IoT version of 3OS could be a thing. To approach early adopters though, 3OS must start by natively supporting XR ports of a few dozen professional software suites.
</blockquote>

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 900;">Early VR Ports in the 20-teens</span></span>
</summary>

<blockquote>
At the advent of modern VR with the Oculus Kickstarter, millions of us dreamed of being able to use Maya or Fusion or Unreal from inside a VR headset, but so far to date, these systems don't feel compatible with HMDs for productive eight-hour work yet because there is always some part of the task flow that requires constant removal of the HMD for fallback to a desktop interface, not to mention the ergonomic issues of bulky headsets worn all day.
</blockquote>

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 900;">The Hardware is Nearly Out of the Oven</span></span>
</summary>

<blockquote>
Hardware engineers are doing their part to shrink the technology to near-sunglasses form factor. As we approach elimination of this ergo bottleneck, it's well-past time for the OS layer to have provided a uniform 3D UI paradigm capable of enabling all 3D software compatibility, including CAD, VFX tools, and game engines to run in a cohesive single environment with multitasking (e.g. Maya and Unreal both running in a shared field).
</blockquote>

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 900;">The Content IS the Interface</span></span>
</summary>

<blockquote>
3OS encourages 3rd parties not to think of their ported app as a floating window. In 3OS, floating windows are like command-line tools. Power users may need them occasionally, but most users should prefer engaging directly with the 3D content. For example, it may be challenging to picture all of Autodesk Maya's many features working without the panoply of sidebars and menus and dropdowns, but it can be done with a little imagination. It is likely to include conversion of 2D sidebars into 3D widgets that are summon-able per-context relative to the content itself. Widgets are also anchorable to any rolling standing desk as a feature of the 3OS Server spec, but for the most part, <b>the content itself IS the interface.</b>
</blockquote>

</details>


<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 900;">App Multitasking</span></span>
</summary>

<blockquote>
3OS aims to support XR app multitasking despite its immense perf op challenges. The first professional deployments of 3OS are intended to run on on-prem VDI servers with plenty of horsepower to handle the job, and 3OS aims to provide an SDK for enterprise 3rd parties to finally port their apps natively into XR.</b>
</blockquote>

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 900;">Deep Agentic Integration</span></span>
</summary>

<blockquote>
3OS assumes that every app in the future will not only have an MCP server or similar protocol installed for agentic integration, but eventually, basic agent servers will evolve into full-blown concierge agents themselves. Every app could theoretically engage with every other app where appropriate based on data that didn't exist until the app instance arrived in a 3OS context, e.g. the internal agent of a Maya instance may realize it is sitting in an MR room in close proximity to an Unreal instance agent, and they can automatically establish a series of pipeline handshakes without any work from the user. The user placed them side-by-side, implying they will soon want pipeline capabilities between them. This evolution of every app becoming conversational with every other app implies that app multitasking may evolve beyond simple pipeline toolchaining into real-time improvisation, desire, personality, etc.
</blockquote>

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 900;">Blurring the Lines of All Previous Metaphors</span></span>
</summary>

<blockquote>
The most basic example is a T-rex app that understands it is a predator, and it actively hunts through a room at the OS level, seeking another app that broadcasts itself as prey. Such an app has no enterprise utility, but it is not architecturally much different from a coding agent. It simply wears a t-rex skin. The lines between agents, apps, and objects may blur significantly. They may all become variations on the same theme eventually.
</blockquote>

</details>

</details>
</blockquote>

---

<a id="basic-transformation"></a>
<blockquote>
<details>
<summary><b>🔀 Basic Transformation</b></summary>
<hr style="border: 1px solid gray;">

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
</blockquote>

---

<a id="the-mind-palace"></a>
<blockquote>
<details>
<summary><b>🧠 The Mind Palace</b></summary>
<hr style="border: 1px solid gray;">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">History</span></span>
</summary>

Roman statesmen, philosophers, and lawyers used a technique called <b>"the Mind Palace"</b> to deliver long, unscripted speeches in the Roman Senate or courts. It was an imaginary place where they could stash chunks of information in specific spatial locations, which, when they imagined retrieval, would assist in rapid recall.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Keeping the MR Office Clean</span></span>
</summary>

The Mind Palace in 3OS is a transitional multi-field construct that bridges MR and VR. The default field in 3OS is an MR office room where professionals are doing collaborative creative 3D work (CAD, VFX, game engine dev, etc.) with each other and with 3D agents. If each user can drop a folder on the floor or a desk or pin to the wall in that singular MR space, the space may become cluttered quickly.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">The Mind Palace is Your 3D Hard Drive</span></span>
</summary>

Alternatively, in 3OS, each user can gesture to toggle their Mind Palace, which begins as an empty VR void that retains any selections and/or voodoo volume(s) already open from the MR scene, allowing users to quickly bring data from the real world into their Mind Palace and back out again. The Mind Palace begins as an empty void, but the user can associate any directory of any hard drive in the system as the root node of a new field, and navigate between any number of those fields similarly to shortcuts on a desktop.

</details>

<details>
<summary style="font-size: 1.15em; display: list-item;">
<span style="display: inline-flex; justify-content: space-between; align-items: baseline; gap: 1.5rem; width: calc(100% - 1.25em); vertical-align: top;"><span style="font-weight: 600;">Organizing Furniture and/or Straight-up Raw Data</span></span>
</summary>

Any given field can be decorated with static 3D objects (walls, furniture, etc.) if those structures help the user mentally organize the data, but most users will simply place floating files (objects) and folders (boxes) in a certain azimuth, depth, and height relative to their body. Like a desktop PC, hundreds of files can easily cause clutter in one field, but each field allows an order of magnitude more breathing room than a desktop before needing reorganization.

</details>

</details>
</blockquote>

---

<a id="collab-rooms-vdi-enterprise-edge-compute-phase-20"></a>
<blockquote>
<details>
<summary><b>🏢 Collab Rooms, VDI & Enterprise Edge Compute (Phase 2.0)</b></summary>
<hr style="border: 1px solid gray;">

Phase **2.0** takes 3OS into multi-user enterprise rooms: every wall and handheld screen joins a shared volumetric field, sessions run on on-prem **VDI** or home **edge** compute, and collaboration semantics cover personal/shared/full-room workspaces, flyweight vs copy assets, view vs object transforms, spectator cameras, and optional site-to-site sync. The full design — reference 20×20 office fixture, desk docks, IMU monitors, co-editor VOIP/avatars, and VCS for simultaneous copies — is in [docs/COLLABORATIVE_ROOMS.md](docs/COLLABORATIVE_ROOMS.md); task checklists are Phase 2.0 in [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md).


</details>
</blockquote>

---

<a id="AI-integration"></a>
<blockquote>
<details>
<summary><b>🤖 Agentic Integration (Phase 3.0)</b></summary>
<hr style="border: 1px solid gray;">

It is assumed 3OS will host agentic integration, though that is not the focus for the early alpha prototype. The endgame is to enable agents to exist as avatars who can converse with team members in real time before diving lower in the stack to complete their work, then return to the visual field for reporting and for more instructions.

Though agents will be heavily integrated, 3OS must stand as an interface on its own without agentic text-to-speech and speech-to-text as the primary interaction models due to a funny quirk of the human psyche:

### Impostor Syndrome!

One of the biggest value propositions agents bring at work is the ability to let junior team members ask infinite stupid questions with no fear of judgement. Juniors may be surprised to learn that this syndrome never goes away. Grizzled 20-year veterans in every field are also quietly asking agents stupid questions they'd be embarrassed to ask out loud.

Even in an office where every coworker is very familiar with every other coworker in the room, verbal agent conversations cannot be the only, nor even the primary, interface during work hours. In dealing with agents, every team member needs to be able to ask judgement-free stupid questions frequently and quietly.

Therefore, if teams are ever able to transition to an agentic XR interface that replaces desktop PCs, it must be one that enables silent communication as a core option. A 3D gesture-based form of communication that operates equally well on any data in the entire field is necessary to limit the amount of prompt typing required, whether the user is communicating with agents or team members. Thus, 3OS early prototypes focus on the raw manual interface to ensure it can stand on its own without agents, and then its processes can be accelerated once agents arrive as team members.

</details>
</blockquote>

---

<a id="structural-outline"></a>
<blockquote>
<details>
<summary><b>📁 Source Structural Outline</b></summary>
<hr style="border: 1px solid gray;">

```
# Repo root — CMake project 3os_kernel (shared lib + harness)
├── include/
│   ├── openxr/                        # OpenXR header placeholder
│   ├── core/
│   │   ├── types.hpp                  # Vec3/Pose/EntityId, voxel & field primitives
│   │   ├── interaction_facade.hpp     # Hover / Select / multi-select callback facade
│   │   ├── topology.hpp               # Voodoo-drop proxy remap & double-proxy grids
│   │   ├── kinematics.hpp             # Grab-anchored stick velocity (telekinetic control law)
│   │   ├── hand_interaction.hpp       # Palm modes, pinch channels, interaction intents
│   │   ├── multi_choice.hpp           # Drive / folder / agent radial menus
│   │   ├── centroid_registry.hpp      # Mind-palace, box, temp, and focus-anchor centroids
│   │   ├── mind_palace_layout.hpp     # Orbit / ring / stack placement for mind-palace roots
│   │   ├── voodoo_volume.hpp          # Snap & free-range voodoo session orchestrator
│   │   ├── voodoo_geom.hpp            # Near/far BB helpers (header-only)
│   │   ├── voodoo_portal_space.hpp    # Far↔near portal transforms & ghost near-pose
│   │   ├── voodoo_pose_lerp.hpp       # Summon / insert / cross-portal travel lerps
│   │   ├── voodoo_orbit_packer.hpp    # Near-BB ellipse packing & clutter push
│   │   ├── voodoo_stick_viz.hpp       # Deadzone arming + stick-ray fill (session / ghost)
│   │   ├── voodoo_ghost_drag.hpp      # Blue-channel ghost-scale / audition drag
│   │   └── voodoo_cut_paste.hpp       # Scissors cut/paste (armed portals → paste I/O)
│   ├── storage/
│   │   ├── workspace.hpp              # Objects/boxes, field membership, storage events
│   │   └── glyph_registry.hpp         # THREEOS_glyph / GLB packs → extension map
│   ├── text/                          # Reserved (text I/O / labels)
│   ├── ai/                            # Reserved (OEM agent service layer)
│   └── interop/
│       ├── threeos_abi.h              # Frozen host ABI (InteropInputFrame, tick API)
│       └── threeos_marshaller.hpp     # Includes threeos_abi.h for C++ hosts
├── src/
│   ├── interaction_facade.cpp
│   ├── topology.cpp
│   ├── kinematics.cpp
│   ├── hand_interaction.cpp
│   ├── multi_choice.cpp
│   ├── centroid_registry.cpp
│   ├── mind_palace_layout.cpp
│   ├── workspace.cpp
│   ├── glyph_registry.cpp
│   ├── voodoo_volume.cpp
│   ├── voodoo_portal_space.cpp
│   ├── voodoo_pose_lerp.cpp
│   ├── voodoo_orbit_packer.cpp
│   ├── voodoo_stick_viz.cpp
│   ├── voodoo_ghost_drag.cpp
│   ├── voodoo_cut_paste.cpp
│   ├── threeos_marshaller.cpp         # DLL/SO exports (threeos_tick, …)
│   ├── storage/                       # Reserved
│   ├── text/                          # Reserved
│   └── ai/                            # Reserved
├── shaders/
│   └── SemanticWallMask.hlsl          # Vertex/pixel wall-clip mask
├── tests/
│   └── harness_main.cpp               # ctest harness for ABI / voodoo behaviors
├── .github/
│   └── workflows/
│       └── build-binaries.yml         # Windows + Linux cmake build & ctest
├── CMakeLists.txt
└── README.md

```

</details>
</blockquote>

</details>

<hr style="border: 1px solid gray;">

<a id="prior-art"></a>
<details>
<summary><b>📚 Academic Lineage & Prior Art</b></summary>
<hr style="border: 1px solid gray;">

Piles of incredible Human-Computer Interaction (HCI) literature have been left to languish by the modern spatial computing industry. In 2026, the following papers do not currently influence OS interfaces running on Meta Quest, Meta VR Glasses, Apple Vision Pro, Magic Leap, HoloLens, Snap Specs, XReal Aura, Vive, Pico, PlayStation VR, Lynx, Varjo, Bigscreen, or Steam Frame. They should.

By grounding **3OS** in this seminal research, we bridge the historic gap between the late-1990s proprioceptive research labs and modern non-Euclidean spatial interaction theories to break the productivity chains of WIMP. The following research shows how and why volumetric isometric techniques are superior to raycasting.

---

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>1994</i></td></tr></table><b>1. The "Silk Cursor"</b>
</summary>

<blockquote>
<p><b>Citation:</b> Zhai, S., Buxton, W., & Milgram, P. (1994). <i>The 'silk cursor': investigating visual cues on target acquisition in 3D user interfaces. Proceedings of the SIGCHI Conference on Human Factors in Computing Systems (CHI '94)</i>, 185–192.</p>
<p><b>Significance to 3OS:</b> Introduced a future where raycast cursors must be abandoned in order to access the entire XR field, implying that interaction via perspective is inferior to interaction via isometry. The "silk" metaphor heavily emphasized the need for most interactive elements to utilize semitransparency in such a system to prevent occlusion from causing confusion. The same is true in 3OS.</p>
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>1995</i></td></tr></table><b>2. Worlds in Miniature (WIM)</b>
</summary>

<blockquote>
<p><b>Citation:</b> Stoakley, R., Conway, M. J., & Pausch, R. (1995). "Virtual reality on a WIM: interactive worlds in miniature." <i>Proceedings of the 22nd annual conference on Computer graphics and interactive techniques (SIGGRAPH '95)</i>, 265–272.</p>
<p><b>Significance to 3OS:</b> This paper pioneered the paradigm of scaling down a massive, immersive 3D environment into a hand-held, near-field miniature copy to bypass the physical limitations of far-field targeting. It is the original world-to-user proxy, a voodoo doll of the entire virtual space. 3OS modernizes this into the <b>voodoo drop</b>: an invisible isometric field proxy (no mini-map widget) that places far-field <b>voodoo volumes</b>, tracking empty volumetric coordinate spaces and/or objects within those spaces.</p>
</blockquote>

</details>
<br clear="all">


<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>1997</i></td></tr></table><b>3. Proprioceptive Interaction Models</b>
</summary>

<blockquote>
<p><b>Citation:</b> Mine, M. R., Brooks Jr, F. P., & Sequin, C. H. (1997). "Moving objects in space: Exploiting proprioception in virtual-environment interaction." <i>Proceedings of the 24th annual conference on Computer graphics and interactive techniques (SIGGRAPH '97)</i>, 19–26.</p>
<p><b>Significance to 3OS:</b> Proved that humans possess a highly accurate, innate biological awareness of their hands' positions relative to their body frames without requiring constant visual feedback (<b>proprioception</b>). This serves as the scientific bedrock for the <b>3OS Drummer's Law</b>—keeping the shoulder girdle (coracobrachialis) completely neutral while the wrist and fingers drive intent inside near-field workspaces, i.e. peripheral remote controls in the near field manipulate objects in the far field.</p>
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>1999</i></td></tr></table><b>4. The Foundational Voodoo Dolls Paper</b>
</summary>

<blockquote>
<p><b>Citation:</b> Pierce, J. S., Stearns, B. C., & Pausch, R. (1999). "Voodoo Dolls: Seamless Interaction at Multiple Scales in Virtual Environments." <i>Proceedings of the 1999 symposium on Interactive 3D graphics (I3D '99)</i>, 141–145.</p>
<p><b>Significance to 3OS:</b> Introduced the first dynamic object-proxy architecture, allowing users to instantiate scale-adjusted, hand-held replicas ("dolls") to manipulate distant assets bidirectionally. 3OS adopts this fundamental workflow but eliminates its rigid, skeuomorphic, object-bound limits—expanding proxies to target unconstrained voxel coordinate structures rather than just pre-existing 3D models.</p>
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>2004</i></td></tr></table><b>5. Pointing at Trivariate Targets</b>
</summary>

<blockquote>
<p><b>Citation:</b> Grossman, T., & Balakrishnan, R. (2004). "Pointing at trivariate targets in 3D environments." <i>Proceedings of the SIGCHI Conference on Human Factors in Computing Systems (CHI '04)</i>, 447–454.</p>
<p><b>Significance to 3OS:</b> Introduced an important equation derived from Fitts' Law that models a 6DOF hand's ability to hit voxel-based targets in 3D space at maximum speed without overshoot. 3OS relies on a slight modification of this equation to determine the default 3" voxel size (see spatial_os_trivariate_validation.md).</p>
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>2014</i></td></tr></table><b>6. Conserving Ergonomic Energy in Mid-Air Interaction</b>
</summary>

<blockquote>
<p><b>Citation:</b> Hincapié-Ramos, J. D., Guo, X., Moghadasian, P., & Irani, P. (2014). "Conserving ergonomic energy in mid-air interaction." <i>Proceedings of the SIGCHI Conference on Human Factors in Computing Systems (CHI '14)</i>, 919–928.</p>
<p><b>Significance to 3OS:</b> Introduced the Consumed Endurance (CE) metric, a foundational framework for algorithmically predicting the onset of "gorilla arm" fatigue without the need for medical-grade sensors. By demonstrating a direct correlation between real-time skeletal joint torque and a user's physical limit, it backs up the 3OS author's instinct as a drum teacher to enable low-fatigue micro-gestures by designing the interface to encourage neutral-pose elbows more than 90% of the time.</p>
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>2017</i></td></tr></table><b>7. Predicting Mid-Air Interaction Fatigue Using Biomechanical Modeling</b>
</summary>

<blockquote>
<p><b>Citation:</b> Jang, S., Ambike, S., Stuerzlinger, W., & Ramani, K. (2017). "Predicting mid-air interaction fatigue using biomechanical modeling." <i>Proceedings of the 2017 CHI Conference on Human Factors in Computing Systems (CHI '17)</i>, 1640–1652.</p>
<p><b>Significance to 3OS:</b> Provided a dynamic, state-based approach to spatial computing fatigue by adapting a Three-Compartment Muscle Model to track active, rested, and fatigued muscle units over time. This paper validated the 3OS philosophy that users can operate outside an "ergo-safe" volume for short stints, but must return to that "ergo-safe" volume to let neutral pose recharge their muscles for a majority of a spatial compute session.</p>
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>2021</i></td></tr></table><b>8. The Poros Framework</b>
</summary>

<blockquote>
<p><b>Citation:</b> Pohl, H., Lilija, K., McIntosh, J., & Hornbæk, K. (2021). "Poros: Configurable Proxies for Distant Interactions in VR." <i>Proceedings of the 2021 CHI Conference on Human Factors in Computing Systems (CHI '21)</i>, Article 357, 1–13.</p>
<p><b>Significance to 3OS:</b> The direct conceptual and geometric peer to 3OS. This paper broke the legacy "Flatland" window paradigm by showing that users can select arbitrary volumetric bounding regions in the far field and mirror them as non-Euclidean near-field workspace portals. 3OS adapts this concept for bimanual selection frustums that are cubic instead of spherical. The most salient difference between Poros and 3OS voodoo volumes is that Poros still used raycasting to generate portals, whereas 3OS engages a modified world-in-miniature technique as its creation model, yielding rapid cross-proxy multi-portal manipulation intended as a competitive volumetric corollary to a 2D desktop File Explorer.</p>
</blockquote>

</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>2025</i></td></tr></table><b>9. Reality Proxy</b>
</summary>

<blockquote>
<p><b>Citation:</b> Al-Sadoun, A., et al. (2025). "Reality Proxy: Fluid Interactions with Real-World Objects in MR." <i>Proceedings of the 2025 CHI Conference on Human Factors in Computing Systems (CHI '25)</i>.</p>
<p><b>Significance to 3OS:</b> Evaluated and validated that using spatial computer vision and semantic machine intelligence are crucial for streamlining the associations between world mesh geo and virtual content that fully utilizes the context of that geo.</p>
</blockquote>

</details>
<br clear="all">


<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>2025</i></td></tr></table><b>10. Fitts' Law + XR Review of 122 Studies</b>
</summary>

<blockquote>
<p><b>Citation:</b> Amini, M., Stuerzlinger, W., Teather, R. J., & Batmaz, A. U. (2025). "A Systematic Review of Fitts' Law in 3D Extended Reality." <i>Proceedings of the 2025 CHI Conference on Human Factors in Computing Systems (CHI '25)</i>.</p>
<p><b>Significance to 3OS:</b> Identified that only 36% of XR HCI studies of the past several decades (44 total) that focused on throughput performance targeted virtual hand (point cursor) interaction models, whereas more than 50% targeted raycast techniques. The other 14% were eye tracking studies. Many of the 36% either intentionally or unintentionally eliminated evaluation of the depth vector by arranging UI elements in a flat plane configuration. Such tests were classified as technically volumetric and isometric since the plane was not raycast-able, yet such studies show very little salient data pertaining to the competitive efficacy of volumetric isometric techniques. To establish truly competitive volumetric isometry in hand tracking, each of the three axes must be treated uniformly. It's clear that volumetric isometric methods have been under-explored not just by industry, but by Academia as well for the entire history of HCI. It's as though we all had an innate instinct to chase Steve Jobs and Jony Ive down the wrong rabbit hole looking for the next next big thing.</p>
</blockquote>

</details>
<br clear="all">

</details>

<hr style="border: 1px solid gray;">

<a id="control-basics"></a>
<details>
<summary><b>🎮 Control Basics</b></summary>
<hr style="border: 1px solid gray;">

<details>
<summary><b>Primary, Secondary, and Ternary Triggers</b></summary>
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

</details>

<details>
<summary><b>Ulnar Wrist Rotation Modes</b></summary>
3OS defines these ulnar wrist rotation modes:
* ### Palm up
* ### Palm down
* ### Palm sideways

Each ulnar wrist mode has the option to modify the primary, secondary, or ternary event trigger, yielding a total of nine event combinations for each hand:
* ### Primary-palm-up
* ### Primary-palm-down
* ### Primary-palm-sideways
* ### Secondary-palm-up
etc.

</details>

<details>
<summary><b>Unique Hand Gestures</b></summary>
* These are gestures that don't use the standard trigger+wrist-mode combos, but instead they imply some kind of story or metaphor that feels intuitive.

### "Use The Force" Gesture

**Hand Tracking:** Point the palm in the vicinity of the same target as gaze and splay all fingers.

**6DOD Controller:** Point the palm in the vicinity of the same target as gaze while still holding the controller.


### "Open Book" Gesture

**Hand Tracking:** Point both palms toward your own face, fingers bladed (opposite of splayed) but thumbs pointed outwarded away from each other to the left and right.

**6DOF Controller:** Point both palms toward your own face while still holding both controllers and make sure thumbs are not touching X or A.

### "Open Book" Trigger

**Hand Tracking:** From Open Book position, close thumbs toward the bladed hands until they touch, then open again.

**6DOF Controller:** From Open Book position, press X and A simultaneously.

The 3OS interface layer reserves all primary event triggers and secondary-palm-down for selection, voodoo control, and other OS actions everywhere in the field outside the bounding box of a 3rd party app and a voodoo volume accessing a 3rd party app. 3rd party devs are encouraged to implement these features similarly within their app bounds via the 3OS SDK.

</details>



<hr style="border: 1px solid gray;">

### Gesture Combos

<hr style="border: 1px solid gray;">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Primary-Pinch</i></td></tr></table><b>Selection/Deselection</b>
</summary>

<blockquote>
Buttons in 3OS are 3D and volumetric by default, meaning point cursors hover INTO them rather than raycast cursors hovering ONTO them. When a point cursor enters or exits the volume of a 3D button, its hover state toggles. While hovered, <b>primary pinch</b> activates the button. Similarly, hover and pinch on an object flags that object as currently selected. Multi-choice nodes work in the same way, except hover is defined by an angular relationship between the cursor and a center point between a group of buttons. Primary Pinch outside a selection will deselect it.
</blockquote>


</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Primary-Drag (palm sideways or up)</i></td></tr></table><b>Multi-Selection</b>
</summary>

<blockquote>
Drag-selection is also volumetric. The objects encapsulated by the cubic volume drawn from corner-to-corner with a <b>primary-pinch-drag</b> action will all be flagged as selected simultaneously.
</blockquote>


</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Primary-Drag While Selected</i></td></tr></table><b>Telekinesis</b>
</summary>

<blockquote>
Once an object or box is selected, if it is still within reach of the point cursor, the user can place the point cursor inside it and drag it directly with <b>Primary-Pinch-Hold</b>.

If the object is out of reach or the cursor is outside the object but still selected, the user can still <b>Primary-Pinch-Hold</b> and drag the object from range via Telekinesis. Telekinesis is accompanied by an arrow showing velocity from its own starting point. The user can shorten that arrow to zero by returning the hand to the starting point of the gesture, aided by the shrinking of the arrow to a dot. Telekinesis is essentially a 3D joystick enabling linear remote-control motion with variabl velocity. The bigger the drag, the faster the object flies in that direction.
</blockquote>


</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Primary Double-Pinch</i></td></tr></table><b>App/Object Activation or Box Opening/Expansion</b>
</summary>

<blockquote>
<b>Primary Double-Pinch</b> is the 3D equivalent of left mouse double-click. It activates an object or expands the contents of a box. If the object is an instance of an application, the app will launch by instantiating a loading glyph above that hand. That hand is considered "armed" with the app instance. That hand can now choose where on the ground to place the app instance (see Launching Applications below).
</blockquote>


</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Palm Down + Primary-Drag</i></td></tr></table><b>Main (Snap) Voodoo Drop</b>
</summary>

<blockquote>
<b>Palm-down + primary-pinch-hold</b> begins voodoo volume selection. The drag position of the pinched fingertips is a voodoo doll of the far-field voodoo volume during drag, meaning the hand can move in 3D to place the volume in 3D with a multiplier of 50-to-1. This means a fully outstretched arm can place the far-field voodoo ~50 feet away in any direction with default sensitivity, and sensitivity is adjustable. Once the volume is dropped in the far field, its near-field companion volume (often referenced as the near BB — short for bounding box) appears in the ergonomically safe intimate space directly in front of the user.
</blockquote>


</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Palm Down + Secondary-Drag</i></td></tr></table><b>Free-Range (Non-Snap) Voodoo Drop</b>
</summary>

<blockquote>
<b>Palm-down + secondary-pinch-hold</b> begins voodoo volume selection without any focus anchor targets. This allows a user to target an empty region of the far field that may be nearby focus anchors which were pulling at the voodoo with unwanted snap gravity. Other than the lack of snap targets, th Free-Range Voodoo Drop is identical to the Main Voodoo Drop.
</blockquote>


</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Gaze + "Use The Force"</i></td></tr></table><b>Solo Voodoo Drop</b>
</summary>

<blockquote>
This is not the recommended way to drop voodoo volumes in most situations, since it is the only raycast function in all of 3OS, and raycast is heavily discouraged as an interaction model across 3OS. This gesture can also tax the shoulder if done too frequently for too long. However, it is a decent one-off shortcut for solo objects in the far field that aren't snappable. For snappable objects (i.e. objects that have a focus anchor - they're part of a matrix, cluster, or collection of any kind) use the Main Voodoo Drop feature above.

If the object does not have a focus anchor, then the main voodoo drop will gravitate toward other nearby anchors, or if no anchors are nearby, it will fall back to fully manual control like the Free-Range Voodoo Drop, which can be slow, tedious, and error-prone when targeting objects further than ~15-20' away.

If the object is solo, and there are no other objects occluding it, the user can use Gaze + "Use The Force" to select it. "Use The Force" is open palm toward the object with splayed fingers. This gesture will snap a voodoo volume onto the object immediately if it senses both gaze and "Use The Force" are targeting it simultaneously. From there, Primary Pinch will drop the voodoo officially. The user can simply cease the gesture without pinch to cancel the voodoo.
</blockquote>


</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Palm Up + Release Drag</i></td></tr></table><b>Voodoo Cancellation</b>
</summary>

<blockquote>
If the user decides in the midst of dropping a voodoo volume that they want to cancel it, they can turn their palm upward and let go of the drag. The voodoo will dismiss early.
</blockquote>


</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Palm Up + Secondary Pinch</i></td></tr></table><b>Voodoo Dismissal</b>
</summary>

<blockquote>
If a voodoo volume was generated previously with the right hand, the right hand can dismiss that volume with <b>palm-up + secondary pinch</b>, likewise with left. If the user does this Dismissal gesture with both hands at the same time, it will Dismiss All open voodoo volumes.
</blockquote>


</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Palm Sideways + Primary-Pinch Across Voodoo Bounds</i></td></tr></table><b>Voodoo Warp (Teleportation)</b>
</summary>

<blockquote>
If you select an object in a voodoo volume and then Primary-Pinch outside that voodoo, the object will warp to that location. If your Primary-Pinch is on a box, the selected object(s) will warp into that box. Warping works from the far field to the near field, vice versa, or through one voodoo into another, allowing objects to cross the entire field instantaneously. 

This feature will be a togglable power user tool in 3OS, though it is on by default in the pre-alpha demo. It potentially causes confusion with deselection, so new users may prefer it off. It requires the power user to understand that selection is in context of the current voodoo reference frame. If selection occurs inside a voodoo volume, deselection can only happen from also within that volume while this feature is on, since moving outside of that volume causes deselection to be overwritten by warp. Vice versa, a selection outside a voodoo volume will change deselection to warp when the cursor travels inside a voodoo volume. Deselection will still be available when warp is active wvia Palm UP + Primary Pinch.
</blockquote>


</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Open Book Gesture</i></td></tr></table><b>Toggling the Mind Palace</b>
</summary>

<blockquote>
To toggle the Mind Palace (MR-office-to-VR-hard-drive or vice versa), the user does the <b>Open Book</b> gesture. They turn both <b>flat palms toward their own face like an open book, thumbs out in opposite directions</b>. This calls up the current toggle status. It will read either "MR" or "VR". Closing the thumbs simultaneously will toggle MR to VR or vice versa. There will be accessibility modes that allow this gesture to activate with only one hand.
</blockquote>


</details>
<br clear="all">

<details>
<summary style="font-size: 1.15em; display: list-item;">
<table align="right" border="0"><tr><td><i>Gaze + "Use The Force" on the Floor</i></td></tr></table><b>Launching Applications</b>
</summary>

<blockquote>
See "App/Object Activation" above as the precursor to this gesture. The user has just double-pinched a file from an app that has implemented the 3OS SDK. 3OS will let the user choose where to launch the app. If the app is legacy 2D (i.e. has not implemented the 3OS SDK), it will launch either in a virtual desktop window, or on a paired VM if it is designated as that user's active 2D workstation. But an MR instance needs a gesture from the user to designate where it should be anchored in the room, as indicated by the loading glyph that appears above the hand that double-pinched to activate it. The <b>App Drop</b> gesture is <b>palm-toward-floor with fingers splayed</b>, a.k.a. the superhero "use the force" gesture. The app will target the floor where the palm aims.
</blockquote>


</details>
<br clear="all">

</details>

<hr style="border: 1px solid gray;">

<a id="getting-started"></a>
<details>
<summary><b>🚀 Getting Started</b></summary>
<hr style="border: 1px solid gray;">

For Programmers:

```bash
git clone https://github.com/scifiuiguy/3OS.git
cd 3OS

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

For Artists:

```text
1) Go to https://github.com/scifiuiguy/3OS in a web browser.
2) Click the green Code dropdown and choose Download ZIP.
3) Extract the 3OS codebase anywhere locally.
4) Find the Quest build at 3OS/builds/threeos-pre-alpha-demo.apk.
5) Follow the instructions below to sideload the APK.
```

### Pre-Alpha Demo Sideloading Instructions:

* To use the Pre-Alpha Demo with built-in sample files and folders, load it as is onto Quest 2 or 3:
  * Plug your Quest into a PC via USB.
  * In Quest, activate the main menu and choose the Notifications tab.
  * A fresh USB Detected notification should appear. Select it.
  * On PC, a File Explorer should launch, displaying This PC/Quest 2. Double-click it.
  * Open the Documents folder and drag in the `threeos-pre-alpha-demo.apk`.
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
     * `Documents/3OS_Demo3`
  2. Drag/drop any files of your choice into any of these folders and they'll appear in-scene.
     * **NOTE:** Don't test with important unarchived files. The demo WILL read/write/delete in those directories.

The shared library target is `3os_kernel` (`3os_kernel.dll` / `lib3os_kernel.so`). The console harness prints `threeos_version` (currently `0.6.0`) and checks the frozen ABI (`InteropInputFrame` = 192 bytes). Host ABI headers: `include/interop/threeos_abi.h`. See [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md); Unity Quest verification lives in a separate repo: [3OS_Unity](https://github.com/scifiuiguy/3OS_Unity).

</details>
