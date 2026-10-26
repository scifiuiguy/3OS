using System.Collections;
using System.Collections.Generic;
using ThreeOS;
using UnityEngine;
using UnityEngine.XR.Hands;

/// <summary>
/// Phase 0.7 Task 7.4/7.5 — Speak → mic/EQ → clip 01 → Speak → clip 02 + floor/widget shrink;
/// palms-down dual-secondary resets Act I without deallocating the agent widget.
/// </summary>
public sealed class ThreeOSFinalDemoActIDirector : MonoBehaviour
{
    [SerializeField] ThreeOSFinalDemoStage stage;
    [SerializeField] ThreeOSFinalDemoPerfPlayer player;
    [SerializeField] ThreeOSFinalDemoMicListen mic;
    [SerializeField] ThreeOSFinalDemoEqVisualizer eq;
    [SerializeField] ThreeOSFinalDemoAgentResetGesture resetGesture;
    [SerializeField] ThreeOSFinalDemoModeToggle modeToggle;
    [SerializeField] ThreeOSAndroidMouthAmpDriver mouthAmp;
    [SerializeField] AudioSource audioSource;

    [Header("NPC bind (filled by setup if empty)")]
    [SerializeField] Transform npcFloorParent;
    [SerializeField] Transform npcRoot;
    [SerializeField] Transform npcHead;
    [SerializeField] ThreeOSAndroidNpcHandPose npcLeftHand;
    [SerializeField] ThreeOSAndroidNpcHandPose npcRightHand;

    [Header("Slots")]
    [SerializeField] TextAsset slot01Json;
    [SerializeField] TextAsset slot02Json;
    [SerializeField] AudioClip slot01Audio;
    [SerializeField] AudioClip slot02Audio;

    [Header("Timing")]
    [SerializeField] float npcCueDelaySec = 0.5f;

    /// <summary>0 = next EOU plays slot 01; 1 = next EOU plays slot 02; 2 = both done until reset.</summary>
    int _nextSlot;
    bool _listening;
    bool _clip2ShrinkUiStarted;
    Coroutine _cueDelayRoutine;
    // Ignore second-Speak while arming / settling; only end after real speech (or stuck).
    const float SecondSpeakGraceSec = 1.25f;
    const float SecondSpeakStuckSec = 4.0f;
    float _listenStartedAt;

    // Manual finish: right middle-finger pinch while listening (doesn't need re-confirming Speak).
    readonly List<XRHandSubsystem> _handSubsystems = new List<XRHandSubsystem>();
    XRHandSubsystem _hands;
    bool _finishPinchLatched;
    bool _finishPinchPrev;
    float _finishCooldownUntil;

    void Awake()
    {
        ResolveRefs();
        EnsureComponents();
        WirePlayerSlots();
        // Defer JSON/audio prefetch off the first frames — cold-open hitch on Quest.
        StartCoroutine(DeferredPrefetchRoutine());
        eq?.Hide();
    }

    IEnumerator DeferredPrefetchRoutine()
    {
        yield return null;
        yield return null;
        // Slot 0 only for cold pose — remaining clips load lazy on first play (Meta Start).
        if (player != null)
            yield return player.PrefetchSlotRoutine(0);
        yield return null;
        player?.TryApplyColdPoseSlot0();
        // Offline Demo1/2 layout for Act I reset — load once, never scan disk mid-demo.
        yield return ThreeOSFinalDemoFsSnapshot.EnsureLoaded();
    }

    void OnEnable()
    {
        ThreeOSAgentSession.SpeakSelected += HandleSpeak;
        if (player != null)
        {
            player.OnSlotFinished += HandleSlotFinished;
            player.OnExitShrinkStarted += HandleExitShrinkStarted;
        }

        if (mic != null)
        {
            mic.OnEndOfUtterance += HandleEndOfUtterance;
            mic.OnArmFailed += HandleArmFailed;
        }

        if (resetGesture != null)
            resetGesture.OnReset += ResetActI;
        EnsureModeToggle();
        if (modeToggle != null)
        {
            modeToggle.WipeStarted += HandleModeWipeStarted;
            modeToggle.WipeSettled += HandleModeWipeSettled;
        }
    }

    void OnDisable()
    {
        ThreeOSAgentSession.SpeakSelected -= HandleSpeak;
        if (mic != null)
        {
            mic.OnEndOfUtterance -= HandleEndOfUtterance;
            mic.OnArmFailed -= HandleArmFailed;
        }

        if (player != null)
        {
            player.OnSlotFinished -= HandleSlotFinished;
            player.OnExitShrinkStarted -= HandleExitShrinkStarted;
        }

        if (resetGesture != null)
            resetGesture.OnReset -= ResetActI;
        if (modeToggle != null)
        {
            modeToggle.WipeStarted -= HandleModeWipeStarted;
            modeToggle.WipeSettled -= HandleModeWipeSettled;
        }
    }

    void HandleModeWipeStarted(bool toPalace)
    {
        // Palace sky does not occlude NPC depth — keep dismissed floor at 0 until Act I reset.
        player?.EnsureFloorStaysHiddenIfDismissed();
        if (toPalace && _nextSlot >= 2)
            player?.ForceFloorHidden();
    }

    void HandleModeWipeSettled(bool toPalace)
    {
        player?.EnsureFloorStaysHiddenIfDismissed();
        if (toPalace && _nextSlot >= 2)
            player?.ForceFloorHidden();
    }

    void EnsureModeToggle()
    {
        if (modeToggle == null)
            modeToggle = FindFirstObjectByType<ThreeOSFinalDemoModeToggle>();
    }

    void Update()
    {
        // Belt-and-suspenders: never let a dismissed NPC resurrect mid-frame.
        player?.EnsureFloorStaysHiddenIfDismissed();

        if (!_listening)
            return;

        // Only freeze latch while NPC audio/anim is actually playing (no post-clip delay).
        if (mic != null)
            mic.SuppressUtteranceDetection = player != null && player.IsPlaying;

        if (eq != null)
        {
            var widget = ThreeOSAgentSession.WidgetRoot;
            if (widget != null)
                eq.ShowAbove(widget);
            // Always use mic band buffer (flat idle shimmer when no AudioRecord energy yet).
            if (mic != null)
                eq.SetBands(mic.Bands);
        }

        PollFinishPinch();
    }

    void PollFinishPinch()
    {
        // Manual middle-pinch EOU only after real speech — a Speak (index) pinch often
        // brings middle near thumb and was false-firing CommitListening on re-arm.
        if (mic == null || !mic.HeardSpeech)
        {
            _finishPinchPrev = true;
            return;
        }

        if (Time.unscaledTime - _listenStartedAt < SecondSpeakGraceSec)
        {
            _finishPinchPrev = true;
            return;
        }

        if (!EnsureHands())
            return;
        _hands.TryUpdateHands(XRHandSubsystem.UpdateType.Dynamic);
        var active = IsRightSecondaryPinch();
        if (Time.unscaledTime < _finishCooldownUntil)
        {
            _finishPinchPrev = active;
            return;
        }

        if (active && !_finishPinchPrev)
        {
            Debug.Log("[3OS][7.4] Right middle pinch → manual end-of-utterance cue");
            _finishCooldownUntil = Time.unscaledTime + 0.5f;
            CommitListening();
        }

        _finishPinchPrev = active;
    }

    bool EnsureHands()
    {
        if (_hands != null && _hands.running)
            return true;
        SubsystemManager.GetSubsystems(_handSubsystems);
        _hands = null;
        for (var i = 0; i < _handSubsystems.Count; i++)
        {
            if (_handSubsystems[i] != null && _handSubsystems[i].running)
            {
                _hands = _handSubsystems[i];
                break;
            }
        }

        return _hands != null;
    }

    bool IsRightSecondaryPinch()
    {
        var hand = _hands.rightHand;
        if (!hand.isTracked)
            return false;
        if (!hand.GetJoint(XRHandJointID.ThumbTip).TryGetPose(out var thumb) ||
            !hand.GetJoint(XRHandJointID.MiddleTip).TryGetPose(out var middle))
            return false;
        var dist = Vector3.Distance(thumb.position, middle.position);
        if (_finishPinchLatched)
            _finishPinchLatched = dist <= 0.055f;
        else
            _finishPinchLatched = dist <= 0.04f;
        return _finishPinchLatched;
    }

    void CommitListening()
    {
        if (!_listening)
            return;
        if (mic != null)
            mic.ForceEndOfUtterance();
        else
            HandleEndOfUtterance();
    }

    void HandleExitShrinkStarted()
    {
        if (_clip2ShrinkUiStarted)
            return;
        _clip2ShrinkUiStarted = true;
        ThreeOSAgentSession.RequestShrink(0.5f);
    }

    void HandleSpeak()
    {
        Debug.Log($"[3OS][7.4] SpeakSelected received (nextSlot={_nextSlot} listening={_listening})");
        if (_nextSlot >= 2)
        {
            Debug.Log("[3OS][7.4] Speak ignored — Act I clips done; use palms-down dual-secondary reset.");
            return;
        }

        // Second Speak = manual EOU only after speech was heard (or stuck recovery).
        // Grace-only end caused: start listen → bounce pinch → immediate clip cue.
        if (_listening)
        {
            if (mic != null && (mic.IsArming || mic.IsWaitingPermission))
            {
                Debug.Log("[3OS][7.4] Second Speak ignored — mic still arming");
                return;
            }

            var elapsed = Time.unscaledTime - _listenStartedAt;
            if (elapsed < SecondSpeakGraceSec)
            {
                Debug.Log($"[3OS][7.4] Second Speak ignored — grace ({elapsed:F2}s)");
                return;
            }

            if (mic != null && mic.SuppressUtteranceDetection)
            {
                Debug.Log("[3OS][7.4] Second Speak ignored — utterance suppressed (NPC audio)");
                return;
            }

            var heard = mic != null && mic.HeardSpeech;
            var armedReady = mic == null || mic.IsArmed;
            var stuck = elapsed >= SecondSpeakStuckSec;
            if (!armedReady || (!heard && !stuck))
            {
                Debug.Log(
                    $"[3OS][7.4] Second Speak ignored — need speech or stuck " +
                    $"(armed={armedReady} heard={heard} t={elapsed:F2})");
                return;
            }

            Debug.Log(
                $"[3OS][7.4] Second Speak → manual end-of-utterance cue " +
                $"(heard={heard} stuck={stuck})");
            CommitListening();
            return;
        }

        // Re-arm listen; if clip 01 is still playing, cut it and ease to last frame.
        StartListening();
    }

    void HandleArmFailed()
    {
        if (!_listening)
            return;
        Debug.LogWarning("[3OS][7.4] Mic arm failed — stopping listen (Speak again)");
        _listening = false;
        eq?.Hide();
    }

    void StartListening()
    {
        EnsureComponents();
        if (_cueDelayRoutine != null)
        {
            StopCoroutine(_cueDelayRoutine);
            _cueDelayRoutine = null;
        }

        // Speak during clip 01: stop audio, lerp live pose → last JSON frame, then listen.
        if (player != null && player.IsPlaying && player.PlayingSlotIndex == 0)
        {
            Debug.Log("[3OS][7.4] Speak during clip 01 → interrupt to last frame + listen");
            player.InterruptToLastFrame(0.4f);
        }

        _listening = true;
        _listenStartedAt = Time.unscaledTime;
        // Absorb current hand pose so Speak's index pinch can't edge middle-pinch EOU.
        _finishPinchPrev = true;
        _finishPinchLatched = false;
        _finishCooldownUntil = Time.unscaledTime + SecondSpeakGraceSec;

        // Always restart capture so a prior failed arm cannot stick.
        // Do not suppress after interrupt — reply silence must latch as soon as speech begins.
        mic?.Disarm();
        mic?.Arm();
        if (mic != null)
            mic.SuppressUtteranceDetection = player != null && player.IsPlaying;

        var widget = ThreeOSAgentSession.WidgetRoot;
        if (eq != null)
        {
            eq.gameObject.SetActive(true);
            if (widget != null)
                eq.ShowAbove(widget);
            else
            {
                // Fallback: park EQ near NPC head so it is still visible if widget root lags a frame.
                var anchor = npcHead != null ? npcHead : npcRoot;
                if (anchor != null)
                {
                    eq.transform.position = anchor.position + Vector3.up * 0.25f;
                    eq.transform.rotation = Quaternion.LookRotation(
                        Camera.main != null ? Camera.main.transform.forward : Vector3.forward);
                }
            }
        }

        Debug.Log(
            $"[3OS][7.4] Listening (next slot {_nextSlot + 1:00}) widget={(widget != null)} eq={(eq != null)}");
    }

    void OnApplicationFocus(bool hasFocus)
    {
        // Permission sheet steals focus; once we're back, keep listening + EQ if Speak was armed.
        if (!hasFocus || !_listening || mic == null)
            return;
        if (mic.IsArmed || mic.IsWaitingPermission)
            return;
        Debug.Log("[3OS][7.4] Focus returned — re-arming mic after permission UI");
        mic.Arm();
    }

    void HandleEndOfUtterance()
    {
        _listening = false;
        if (mic != null)
            mic.SuppressUtteranceDetection = false;
        eq?.Hide();

        var slot = _nextSlot;
        if (slot >= 2)
            return;

        if (_cueDelayRoutine != null)
            StopCoroutine(_cueDelayRoutine);
        _cueDelayRoutine = StartCoroutine(CueNpcAfterDelay(slot));
    }

    IEnumerator CueNpcAfterDelay(int slot)
    {
        var delay = Mathf.Max(0f, npcCueDelaySec);

        // Stop prior clip before blending into the next first frame (avoids fighting LateUpdate).
        if (slot == 1 && player != null && player.IsPlaying && player.PlayingSlotIndex == 0)
        {
            Debug.Log("[3OS][7.4] Stopping clip 01 before blend into clip 02");
            player.StopPlayback(resetMouth: true);
        }

        ThreeOSAndroidPerfFrame from = null;
        ThreeOSAndroidPerfFrame to = null;
        var canBlend = player != null && player.TryBeginBlendToFirstFrame(slot, out from, out to);

        if (delay > 0f)
        {
            Debug.Log(
                $"[3OS][7.4] NPC cue delay {delay:F2}s (slot {slot + 1:00}) blend={canBlend}");
            var t0 = Time.unscaledTime;
            while (true)
            {
                var elapsed = Time.unscaledTime - t0;
                var u = delay > 1e-4f ? Mathf.Clamp01(elapsed / delay) : 1f;
                var e = u * u * (3f - 2f * u);
                if (canBlend)
                    player.ApplyFrameBlend(from, to, e);
                if (u >= 1f)
                    break;
                yield return null;
            }
        }
        else if (canBlend)
        {
            player.ApplyFrameBlend(from, to, 1f);
        }

        if (slot == 1)
            _clip2ShrinkUiStarted = false;

        player?.PlaySlot(slot);
        _nextSlot = slot + 1;
        _cueDelayRoutine = null;
    }

    void HandleSlotFinished(int slotIndex)
    {
        mouthAmp?.ResetToRest();
        if (slotIndex == 1 && !_clip2ShrinkUiStarted)
        {
            _clip2ShrinkUiStarted = true;
            ThreeOSAgentSession.RequestShrink(0.5f);
        }
    }

    public void ResetActI()
    {
        _listening = false;
        if (_cueDelayRoutine != null)
        {
            StopCoroutine(_cueDelayRoutine);
            _cueDelayRoutine = null;
        }

        mic?.Disarm();
        eq?.Hide();
        player?.StopPlayback(resetMouth: true);
        player?.ResetFloorScale();
        player?.TryApplyColdPoseSlot0();
        mouthAmp?.ResetToRest();
        _nextSlot = 0;
        _clip2ShrinkUiStarted = false;
        ThreeOSAgentSession.RequestPopIn();

        // Full demo take reset: destroy Blender spawn + MR DemonA glyph, restore Demo1/2 from
        // the offline FS snapshot, remount palace. Safe to fire while soft-dismissed in MR.
        StartCoroutine(ResetDemoTakeRoutine());
    }

    IEnumerator ResetDemoTakeRoutine()
    {
        // 1) BlenderGroup floor spawn (if any)
        var blender = FindFirstObjectByType<ThreeOSFinalDemoBlenderSpawn>();
        if (blender != null)
            blender.ForceCancelArmed();
        else
            ThreeOSFinalDemoBlenderSpawn.DestroySpawnedBlenderGroups();

        var storage = FindFirstObjectByType<ThreeOSStorageController>();
        if (storage != null)
        {
            // 2) MR-persisted DemonA.blend glyph (grab into MR does not rewrite disk paths)
            storage.ClearMrPersistedSelectionVisual();
            storage.CancelPasteProgressUi();
            storage.ClearLastPasteBatch();
        }

        // 3) Disk: restore Demo1/2 from serialized snapshot (ThreadPool)
        yield return ThreeOSFinalDemoFsSnapshot.EnsureLoaded();
        var snap = ThreeOSFinalDemoFsSnapshot.Cached;
        if (snap != null)
        {
            var docs1 = ThreeOSStorageController.DocumentsDemoPath(
                string.IsNullOrEmpty(snap.demo1_folder) ? "3OS_Demo" : snap.demo1_folder);
            var docs2 = ThreeOSStorageController.DocumentsDemoPath(
                string.IsNullOrEmpty(snap.demo2_folder) ? "3OS_Demo2" : snap.demo2_folder);
            var docs3 = ThreeOSStorageController.DocumentsDemoPath(
                string.IsNullOrEmpty(snap.demo3_folder) ? "3OS_Demo3" : snap.demo3_folder);

            var bgDone = false;
            var restoreOk = false;
            System.Threading.ThreadPool.QueueUserWorkItem(_ =>
            {
                try
                {
                    restoreOk = ThreeOSFinalDemoFsSnapshot.RestoreDemoRootsFromSnapshot(
                        snap, docs1, docs2, docs3);
                }
                catch (System.Exception ex)
                {
                    Debug.LogError("[3OS][7.4] snapshot restore failed: " + ex.Message);
                    restoreOk = false;
                }
                finally
                {
                    System.Threading.Thread.MemoryBarrier();
                    bgDone = true;
                }
            });

            // Shell mv of ~40 DemonProject tops is usually <10s; copy fallback can be longer.
            // Remounting before bgDone left partial Demo1/2 + DemonProject duplicates.
            var waited = 0f;
            const float restoreTimeoutSec = 180f;
            while (!bgDone && waited < restoreTimeoutSec)
            {
                waited += Time.unscaledDeltaTime;
                yield return null;
            }

            if (!bgDone)
            {
                Debug.LogError(
                    $"[3OS][7.4] snapshot restore still running after {waited:F1}s — remount deferred");
                while (!bgDone)
                {
                    waited += Time.unscaledDeltaTime;
                    yield return null;
                }
            }

            Debug.Log(
                $"[3OS][7.4] snapshot restore finished ok={restoreOk} waited={waited:F1}s " +
                $"d1={docs1} d2={docs2}");
        }
        else
        {
            Debug.LogWarning("[3OS][7.4] no FS snapshot — disk not restored");
        }

        // 4) Remount palace from restored Documents. If still in MR office, keep glyphs hidden —
        // reset restores disk/kernel only; Mind Palace must not appear until the wipe to VR.
        if (storage != null)
        {
            var stayHiddenInMr = stage == null || !stage.InPalace;
            yield return storage.RemountPalaceAfterDemoResetRoutine(remainHiddenInMr: stayHiddenInMr);
        }

        // StorageClear during remount wiped core focus anchors; Unity still held a stale id.
        var npcFocus = FindFirstObjectByType<ThreeOSFinalDemoNpcFocusAnchor>();
        npcFocus?.ForceReregister();

        Debug.Log("[3OS][7.4] Act I reset + demo FS restore complete");
    }

    void ResolveRefs()
    {
        if (stage == null)
            stage = FindFirstObjectByType<ThreeOSFinalDemoStage>();
        if (stage != null)
        {
            if (npcFloorParent == null)
                npcFloorParent = stage.NpcFloorParent;
            if (npcRoot == null)
                npcRoot = stage.NpcRoot;
        }

        if (npcRoot == null)
        {
            var go = GameObject.Find("PerfNPC");
            if (go != null)
                npcRoot = go.transform;
        }

        if (npcFloorParent == null)
        {
            var go = GameObject.Find("NPC_Floor_Parent");
            if (go != null)
                npcFloorParent = go.transform;
        }

        if (npcHead == null && npcRoot != null)
        {
            var t = npcRoot.Find("NPC_Head");
            if (t == null)
                t = FindDeep(npcRoot, "NPC_Head");
            npcHead = t;
        }

        if (npcLeftHand == null && npcRoot != null)
        {
            var l = npcRoot.Find("NPC_Hand_L");
            if (l != null)
                npcLeftHand = l.GetComponent<ThreeOSAndroidNpcHandPose>()
                              ?? l.gameObject.AddComponent<ThreeOSAndroidNpcHandPose>();
        }

        if (npcRightHand == null && npcRoot != null)
        {
            var r = npcRoot.Find("NPC_Hand_R");
            if (r != null)
                npcRightHand = r.GetComponent<ThreeOSAndroidNpcHandPose>()
                               ?? r.gameObject.AddComponent<ThreeOSAndroidNpcHandPose>();
        }
    }

    void EnsureComponents()
    {
        if (audioSource == null)
            audioSource = GetComponent<AudioSource>() ?? gameObject.AddComponent<AudioSource>();
        audioSource.playOnAwake = false;

        if (mouthAmp == null)
            mouthAmp = GetComponent<ThreeOSAndroidMouthAmpDriver>()
                       ?? gameObject.AddComponent<ThreeOSAndroidMouthAmpDriver>();
        Transform mouth = null;
        if (npcHead != null)
            mouth = ThreeOSAndroidMouthAmpDriver.FindNamed(npcHead, "Mouth");
        if (mouth == null && npcRoot != null)
            mouth = ThreeOSAndroidMouthAmpDriver.FindNamed(npcRoot, "Mouth");
        // Configure once (or when Mouth/AudioSource changes). Calling every Speak re-cached
        // live open scale as rest → progressive wider mouth on each retry.
        mouthAmp.Configure(mouth, audioSource);
        if (player == null || !player.IsPlaying)
            mouthAmp.ResetToRest();

        if (player == null)
            player = GetComponent<ThreeOSFinalDemoPerfPlayer>()
                     ?? gameObject.AddComponent<ThreeOSFinalDemoPerfPlayer>();
        if (mic == null)
            mic = GetComponent<ThreeOSFinalDemoMicListen>()
                  ?? gameObject.AddComponent<ThreeOSFinalDemoMicListen>();
        if (eq == null)
        {
            var eqGo = GameObject.Find("FinalDemoEQ");
            if (eqGo == null)
                eqGo = new GameObject("FinalDemoEQ");
            eq = eqGo.GetComponent<ThreeOSFinalDemoEqVisualizer>()
                 ?? eqGo.AddComponent<ThreeOSFinalDemoEqVisualizer>();
        }

        if (resetGesture == null)
            resetGesture = GetComponent<ThreeOSFinalDemoAgentResetGesture>()
                           ?? gameObject.AddComponent<ThreeOSFinalDemoAgentResetGesture>();
    }

    void WirePlayerSlots()
    {
        var slotDefs = new[]
        {
            new ThreeOSFinalDemoPerfPlayer.Slot
            {
                label = "01",
                jsonFileName = "android_hands_perf_01.json",
                jsonAsset = slot01Json,
                audio = slot01Audio
            },
            new ThreeOSFinalDemoPerfPlayer.Slot
            {
                label = "02",
                jsonFileName = "android_hands_perf_02.json",
                jsonAsset = slot02Json,
                audio = slot02Audio
            }
        };

        // Ensure hand pose roots point at SegHand roots if binder left them empty.
        if (npcLeftHand != null && npcLeftHand.HandRoot == null)
            npcLeftHand.SetHandRoot(npcLeftHand.transform);
        if (npcRightHand != null && npcRightHand.HandRoot == null)
            npcRightHand.SetHandRoot(npcRightHand.transform);

        player.Configure(
            npcFloorParent,
            npcRoot,
            npcHead,
            npcLeftHand,
            npcRightHand,
            mouthAmp,
            audioSource,
            slotDefs);
    }

    static Transform FindDeep(Transform root, string name)
    {
        if (root == null)
            return null;
        if (root.name == name)
            return root;
        for (var i = 0; i < root.childCount; i++)
        {
            var f = FindDeep(root.GetChild(i), name);
            if (f != null)
                return f;
        }

        return null;
    }
}
