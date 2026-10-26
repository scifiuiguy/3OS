using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

/// <summary>
/// Final Demo (7.4/7.5) playlist player — applies archived JSON to PerfNPC under NPC_Floor_Parent.
/// JSON is seeded into <c>persistentDataPath/3OS_FinalDemoPerf/</c> (never AndroidHandsPerf).
/// </summary>
public sealed class ThreeOSFinalDemoPerfPlayer : MonoBehaviour
{
    public const string PersistFolder = "3OS_FinalDemoPerf";

    [System.Serializable]
    public sealed class Slot
    {
        public string label = "01";
        public string jsonFileName = "android_hands_perf_01.json";
        public TextAsset jsonAsset;
        public AudioClip audio;
    }

    [Header("NPC")]
    [SerializeField] Transform npcFloorParent;
    [SerializeField] Transform npcRoot;
    [SerializeField] Transform npcHead;
    [SerializeField] ThreeOSAndroidNpcHandPose npcLeftHand;
    [SerializeField] ThreeOSAndroidNpcHandPose npcRightHand;
    [SerializeField] ThreeOSAndroidMouthAmpDriver mouthAmp;
    [SerializeField] AudioSource audioSource;

    [Header("Slots")]
    [SerializeField] Slot[] slots =
    {
        new Slot { label = "01", jsonFileName = "android_hands_perf_01.json" },
        new Slot { label = "02", jsonFileName = "android_hands_perf_02.json" }
    };

    readonly Dictionary<int, ThreeOSAndroidPerfClip> _loaded = new Dictionary<int, ThreeOSAndroidPerfClip>();
    int _playingSlot = -1;
    float _playStartUnscaled;
    int _playFrameIndex;
    bool _exitShrinkArmed;
    bool _exitShrinkStarted;
    float _exitShrinkStartUnscaled = -1f;
    Vector3 _floorRestScale = Vector3.one;
    Coroutine _interruptCo;

    public bool IsPlaying => _playingSlot >= 0;
    public int PlayingSlotIndex => _playingSlot;
    public float PlayingElapsedSec =>
        _playingSlot >= 0 ? Time.unscaledTime - _playStartUnscaled : 0f;

    /// <summary>Fired once when clip-02 last-1s floor shrink begins (widget should shrink in parallel).</summary>
    public System.Action OnExitShrinkStarted;
    public System.Action<int> OnSlotFinished;

    public void Configure(
        Transform floorParent,
        Transform root,
        Transform head,
        ThreeOSAndroidNpcHandPose left,
        ThreeOSAndroidNpcHandPose right,
        ThreeOSAndroidMouthAmpDriver mouth,
        AudioSource source,
        Slot[] slotDefs)
    {
        npcFloorParent = floorParent;
        npcRoot = root;
        npcHead = head;
        npcLeftHand = left;
        npcRightHand = right;
        mouthAmp = mouth;
        audioSource = source;
        if (slotDefs != null && slotDefs.Length > 0)
            slots = slotDefs;
        if (npcFloorParent != null)
            _floorRestScale = npcFloorParent.localScale;
    }

    void Awake()
    {
        if (audioSource == null)
            audioSource = GetComponent<AudioSource>() ?? gameObject.AddComponent<AudioSource>();
        audioSource.playOnAwake = false;
        if (npcFloorParent != null)
            _floorRestScale = npcFloorParent.localScale.sqrMagnitude > 1e-6f
                ? npcFloorParent.localScale
                : Vector3.one;
        // Prefetch deferred by ActIDirector (cold-open hitch).
    }

    public void PrefetchSlots()
    {
        if (slots == null)
            return;
        for (var i = 0; i < slots.Length; i++)
            TryLoadSlot(i);
    }

    /// <summary>Load one slot only (cold open uses slot 0; others load on first play).</summary>
    public void PrefetchSlot(int slotIndex) => TryLoadSlot(slotIndex);

    /// <summary>Non-blocking seed+load for Meta Start (yields jar StreamingAssets read).</summary>
    public System.Collections.IEnumerator PrefetchSlotRoutine(int slotIndex)
    {
        if (slots == null || slotIndex < 0 || slotIndex >= slots.Length)
            yield break;
        var slot = slots[slotIndex];
        if (slot == null || string.IsNullOrEmpty(slot.jsonFileName))
            yield break;
        yield return EnsurePersistCopyRoutine(slot);
        TryLoadSlot(slotIndex);
    }

    public bool TryApplyColdPoseSlot0()
    {
        if (!TryGetClip(0, out var clip) || clip.frames == null || clip.frames.Count == 0)
            return false;
        ApplyFrame(clip.frames[0]);
        mouthAmp?.ResetToRest();
        return true;
    }

    /// <summary>Capture current NPC pose and first JSON frame for a pre-play blend.</summary>
    public bool TryBeginBlendToFirstFrame(int slotIndex, out ThreeOSAndroidPerfFrame from, out ThreeOSAndroidPerfFrame to)
    {
        from = CaptureCurrentFrame();
        to = null;
        if (!TryGetClip(slotIndex, out var clip) || clip.frames == null || clip.frames.Count == 0)
            return false;
        to = clip.frames[0];
        return to != null;
    }

    public void ApplyFrameBlend(ThreeOSAndroidPerfFrame from, ThreeOSAndroidPerfFrame to, float t01)
    {
        if (from == null || to == null)
            return;
        var t = Mathf.Clamp01(t01);
        if (npcHead != null)
        {
            npcHead.localPosition = Vector3.Lerp(
                ThreeOSAndroidPerfClipUtil.GetVec3(from.headPos),
                ThreeOSAndroidPerfClipUtil.GetVec3(to.headPos),
                t);
            npcHead.localRotation = Quaternion.Slerp(
                ThreeOSAndroidPerfClipUtil.GetQuat(from.headRot),
                ThreeOSAndroidPerfClipUtil.GetQuat(to.headRot),
                t);
        }

        npcLeftHand?.ApplyLocalBlend(from.left, to.left, t);
        npcRightHand?.ApplyLocalBlend(from.right, to.right, t);
    }

    ThreeOSAndroidPerfFrame CaptureCurrentFrame()
    {
        var frame = new ThreeOSAndroidPerfFrame();
        if (npcHead != null)
        {
            ThreeOSAndroidPerfClipUtil.SetVec3(frame.headPos, npcHead.localPosition);
            ThreeOSAndroidPerfClipUtil.SetQuat(frame.headRot, npcHead.localRotation);
        }

        frame.left = npcLeftHand != null ? npcLeftHand.CaptureLocal() : new ThreeOSAndroidPerfHand();
        frame.right = npcRightHand != null ? npcRightHand.CaptureLocal() : new ThreeOSAndroidPerfHand();
        return frame;
    }

    public bool PlaySlot(int slotIndex)
    {
        if (!TryGetClip(slotIndex, out var clip) || clip.frames == null || clip.frames.Count == 0)
        {
            Debug.LogWarning($"[3OS][7.4] Missing performance slot {slotIndex}");
            return false;
        }

        StopPlayback(resetMouth: true);
        _playingSlot = slotIndex;
        _playStartUnscaled = Time.unscaledTime;
        _playFrameIndex = 0;
        _exitShrinkArmed = slotIndex == 1;
        _exitShrinkStarted = false;
        _exitShrinkStartUnscaled = -1f;

        var audio = slots[slotIndex].audio;
        mouthAmp?.SetDriving(true);
        if (audioSource != null && audio != null)
        {
            audioSource.clip = audio;
            audioSource.time = 0f;
            audioSource.Play();
        }

        ApplyFrame(clip.frames[0]);
        Debug.Log($"[3OS][7.4] Playing slot {slots[slotIndex].label}");
        return true;
    }

    public void StopPlayback(bool resetMouth)
    {
        if (_interruptCo != null)
        {
            StopCoroutine(_interruptCo);
            _interruptCo = null;
        }

        if (audioSource != null && audioSource.isPlaying)
            audioSource.Stop();
        if (resetMouth)
            mouthAmp?.ResetToRest();
        else
            mouthAmp?.SetDriving(false);
        _playingSlot = -1;
        _exitShrinkArmed = false;
        _exitShrinkStartUnscaled = -1f;
    }

    /// <summary>
    /// Cut clip audio immediately and ease the NPC from the live pose to that clip's last
    /// JSON frame (Speak-during-clip-01 reply path).
    /// </summary>
    public void InterruptToLastFrame(float blendSec = 0.4f)
    {
        if (_playingSlot < 0)
            return;
        if (_interruptCo != null)
            StopCoroutine(_interruptCo);
        _interruptCo = StartCoroutine(InterruptToLastFrameRoutine(blendSec));
    }

    IEnumerator InterruptToLastFrameRoutine(float blendSec)
    {
        var slot = _playingSlot;
        if (!TryGetClip(slot, out var clip) || clip.frames == null || clip.frames.Count == 0)
        {
            StopPlayback(resetMouth: true);
            _interruptCo = null;
            yield break;
        }

        var from = CaptureCurrentFrame();
        var to = clip.frames[clip.frames.Count - 1];

        if (audioSource != null && audioSource.isPlaying)
            audioSource.Stop();
        mouthAmp?.SetDriving(false);
        // Clear playing so LateUpdate cannot fight the blend.
        _playingSlot = -1;
        _exitShrinkArmed = false;
        _exitShrinkStartUnscaled = -1f;

        var dur = Mathf.Max(0.05f, blendSec);
        var t0 = Time.unscaledTime;
        while (true)
        {
            var u = Mathf.Clamp01((Time.unscaledTime - t0) / dur);
            var e = u * u * (3f - 2f * u);
            ApplyFrameBlend(from, to, e);
            if (u >= 1f)
                break;
            yield return null;
        }

        ApplyFrame(to);
        mouthAmp?.ResetToRest();
        _interruptCo = null;
        Debug.Log($"[3OS][7.4] Interrupted slot {slot} → last frame ({dur:F2}s)");
    }

    /// <summary>True after clip-02 shrink until Act I reset restores the NPC.</summary>
    public bool FloorDismissed { get; private set; }

    /// <summary>Restore floor parent scale after Act I reset.</summary>
    public void ResetFloorScale()
    {
        if (npcFloorParent == null)
            return;
        FloorDismissed = false;
        if (!npcFloorParent.gameObject.activeSelf)
            npcFloorParent.gameObject.SetActive(true);
        npcFloorParent.localScale = _floorRestScale.sqrMagnitude > 1e-6f
            ? _floorRestScale
            : Vector3.one;
        // Re-enable NPC head focus anchor for agent Multi-Choice snap.
        FindFirstObjectByType<ThreeOSFinalDemoNpcFocusAnchor>()?.ForceReregister();
    }

    void LateUpdate()
    {
        if (_playingSlot < 0)
            return;
        if (!TryGetClip(_playingSlot, out var clip) || clip.frames == null || clip.frames.Count == 0)
        {
            FinishCurrentSlot();
            return;
        }

        var t = Time.unscaledTime - _playStartUnscaled;
        var frames = clip.frames;
        while (_playFrameIndex + 1 < frames.Count && frames[_playFrameIndex + 1].t <= t)
            _playFrameIndex++;
        ApplyFrame(frames[_playFrameIndex]);

        var endT = clip.durationSec > 0.01f ? clip.durationSec : frames[frames.Count - 1].t;

        // Clip 02: lerp floor parent → 0 over the last 0.5 s of JSON time.
        const float exitShrinkSec = 0.5f;
        if (_exitShrinkArmed && npcFloorParent != null)
        {
            var shrinkStart = Mathf.Max(0f, endT - exitShrinkSec);
            if (t >= shrinkStart)
            {
                if (!_exitShrinkStarted)
                {
                    _exitShrinkStarted = true;
                    _exitShrinkStartUnscaled = Time.unscaledTime;
                    OnExitShrinkStarted?.Invoke();
                }

                var u = Mathf.Clamp01((t - shrinkStart) / exitShrinkSec);
                if (u >= 0.999f)
                {
                    // Explicit snap — smoothstep + float noise left a visible blue speck before.
                    npcFloorParent.localScale = Vector3.zero;
                }
                else
                {
                    var e = u * u * (3f - 2f * u);
                    npcFloorParent.localScale = Vector3.Lerp(_floorRestScale, Vector3.zero, e);
                }
            }
        }

        var audioDone = audioSource == null || !audioSource.isPlaying;
        if (t >= endT && audioDone)
            FinishCurrentSlot();
    }

    void FinishCurrentSlot()
    {
        var wasSlot = _playingSlot;
        if (audioSource != null && audioSource.isPlaying)
            audioSource.Stop();
        mouthAmp?.ResetToRest();
        _playingSlot = -1;
        _exitShrinkArmed = false;
        if (wasSlot == 1)
            ForceFloorHidden();
        Debug.Log($"[3OS][7.4] Finished slot {wasSlot}");
        OnSlotFinished?.Invoke(wasSlot);
    }

    /// <summary>Snap floor parent to exact zero scale and deactivate so no ghost geo remains.</summary>
    public void ForceFloorHidden()
    {
        if (npcFloorParent == null)
            return;
        FloorDismissed = true;
        npcFloorParent.localScale = Vector3.zero;
        if (npcFloorParent.gameObject.activeSelf)
            npcFloorParent.gameObject.SetActive(false);
        // Drop focus-anchor registration so CentroidDebug / snap cannot leave a floor node.
        FindFirstObjectByType<ThreeOSFinalDemoNpcFocusAnchor>()?.SuppressWhileNpcHidden();
    }

    /// <summary>
    /// Re-assert zero scale after MR↔VR wipes (parent hierarchy / world toggles must not
    /// resurrect a full-size NPC that clips Palace glyphs through the sky).
    /// </summary>
    public void EnsureFloorStaysHiddenIfDismissed()
    {
        if (!FloorDismissed || npcFloorParent == null)
            return;
        npcFloorParent.localScale = Vector3.zero;
        if (npcFloorParent.gameObject.activeSelf)
            npcFloorParent.gameObject.SetActive(false);
    }

    void ApplyFrame(ThreeOSAndroidPerfFrame frame)
    {
        if (npcHead != null)
        {
            npcHead.localPosition = ThreeOSAndroidPerfClipUtil.GetVec3(frame.headPos);
            npcHead.localRotation = ThreeOSAndroidPerfClipUtil.GetQuat(frame.headRot);
        }

        npcLeftHand?.ApplyLocal(frame.left);
        npcRightHand?.ApplyLocal(frame.right);
    }

    bool TryGetClip(int slotIndex, out ThreeOSAndroidPerfClip clip)
    {
        clip = null;
        if (slots == null || slotIndex < 0 || slotIndex >= slots.Length)
            return false;
        if (_loaded.TryGetValue(slotIndex, out clip) && clip != null)
            return true;
        return TryLoadSlot(slotIndex, out clip);
    }

    bool TryLoadSlot(int slotIndex) => TryLoadSlot(slotIndex, out _);

    bool TryLoadSlot(int slotIndex, out ThreeOSAndroidPerfClip clip)
    {
        clip = null;
        var slot = slots[slotIndex];
        if (slot == null || string.IsNullOrEmpty(slot.jsonFileName))
            return false;

        try
        {
            EnsurePersistCopy(slot);
            var path = PersistPath(slot.jsonFileName);
            if (!File.Exists(path))
            {
                Debug.LogWarning($"[3OS][7.5] Missing {path}");
                return false;
            }

            clip = JsonUtility.FromJson<ThreeOSAndroidPerfClip>(File.ReadAllText(path));
            if (clip == null || clip.frames == null || clip.frames.Count == 0)
                return false;
            _loaded[slotIndex] = clip;
            return true;
        }
        catch (System.Exception ex)
        {
            Debug.LogWarning($"[3OS][7.5] load {slot.jsonFileName}: {ex.Message}");
            return false;
        }
    }

    static void EnsurePersistCopy(Slot slot)
    {
        var dir = Path.Combine(Application.persistentDataPath, PersistFolder);
        Directory.CreateDirectory(dir);
        var dst = Path.Combine(dir, slot.jsonFileName);
        if (File.Exists(dst))
            return; // never clobber — distinct from AndroidHandsPerf/

        byte[] bytes = null;
        if (slot.jsonAsset != null)
            bytes = slot.jsonAsset.bytes;

        if (bytes == null || bytes.Length == 0)
        {
            var resName = "3OS/FinalDemoPerf/" + Path.GetFileNameWithoutExtension(slot.jsonFileName);
            var res = Resources.Load<TextAsset>(resName);
            if (res != null)
                bytes = res.bytes;
        }

        if (bytes == null || bytes.Length == 0)
        {
#if UNITY_ANDROID && !UNITY_EDITOR
            // Last resort for mid-demo Play (slot N not prefetched). Cold open uses
            // PrefetchSlotRoutine instead — do not call this on Meta Start frames.
            var uri = Path.Combine(Application.streamingAssetsPath, "3OS", "FinalDemoPerf",
                slot.jsonFileName);
            using (var req = UnityEngine.Networking.UnityWebRequest.Get(uri))
            {
                req.SendWebRequest();
                var guard = 0;
                while (!req.isDone && guard++ < 6000)
                    System.Threading.Thread.Sleep(1);
                if (req.result == UnityEngine.Networking.UnityWebRequest.Result.Success)
                    bytes = req.downloadHandler.data;
            }
#else
            var streaming = Path.Combine(Application.streamingAssetsPath, "3OS", "FinalDemoPerf",
                slot.jsonFileName);
            if (File.Exists(streaming))
                bytes = File.ReadAllBytes(streaming);
#endif
        }

        if (bytes == null || bytes.Length == 0)
        {
            Debug.LogWarning($"[3OS][7.5] Could not seed {slot.jsonFileName}");
            return;
        }

        File.WriteAllBytes(dst, bytes);
        Debug.Log($"[3OS][7.5] Seeded {dst} ({bytes.Length} bytes)");
    }

    static System.Collections.IEnumerator EnsurePersistCopyRoutine(Slot slot)
    {
        var dir = Path.Combine(Application.persistentDataPath, PersistFolder);
        Directory.CreateDirectory(dir);
        var dst = Path.Combine(dir, slot.jsonFileName);
        if (File.Exists(dst))
            yield break;

        byte[] bytes = null;
        if (slot.jsonAsset != null)
            bytes = slot.jsonAsset.bytes;

        if (bytes == null || bytes.Length == 0)
        {
            var resName = "3OS/FinalDemoPerf/" + Path.GetFileNameWithoutExtension(slot.jsonFileName);
            var res = Resources.Load<TextAsset>(resName);
            if (res != null)
                bytes = res.bytes;
        }

        if (bytes == null || bytes.Length == 0)
        {
#if UNITY_ANDROID && !UNITY_EDITOR
            var uri = Path.Combine(Application.streamingAssetsPath, "3OS", "FinalDemoPerf", slot.jsonFileName);
            using (var req = UnityEngine.Networking.UnityWebRequest.Get(uri))
            {
                yield return req.SendWebRequest();
                if (req.result == UnityEngine.Networking.UnityWebRequest.Result.Success)
                    bytes = req.downloadHandler.data;
            }
#else
            var streaming = Path.Combine(Application.streamingAssetsPath, "3OS", "FinalDemoPerf",
                slot.jsonFileName);
            if (File.Exists(streaming))
                bytes = File.ReadAllBytes(streaming);
#endif
        }

        if (bytes == null || bytes.Length == 0)
        {
            Debug.LogWarning($"[3OS][7.5] Could not seed {slot.jsonFileName}");
            yield break;
        }

        File.WriteAllBytes(dst, bytes);
        Debug.Log($"[3OS][7.5] Seeded {dst} ({bytes.Length} bytes)");
    }

    public static string PersistPath(string fileName) =>
        Path.Combine(Application.persistentDataPath, PersistFolder, fileName);
}
