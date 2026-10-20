using System.Collections;
using System.Collections.Generic;
using System.IO;
using ThreeOS;
using UnityEngine;
using UnityEngine.XR.Hands;

/// <summary>
/// Phase 0.7 Task 7.7 — open-book glyph + dual-thumb pulse → bidirectional skybox wipe.
/// Host <see cref="ThreeOSFinalDemoOpenBookGesture"/> owns geometry + HUD; core intents
/// are OR'd when present. Host owns mode flag + FX.
/// </summary>
public sealed class ThreeOSFinalDemoModeToggle : MonoBehaviour
{
    [SerializeField] ThreeOSFinalDemoStage stage;
    [SerializeField] ThreeOSBridge bridge;
    [SerializeField] ThreeOSFinalDemoOpenBookGesture openBook;
    [SerializeField] ThreeOSFinalDemoPalaceLayering layering;
    [SerializeField] float wipeDurationSec = 0.5f;
    [SerializeField] float glyphScale = 1f;
    [SerializeField] Color wipeColor = new Color(0.45f, 0.78f, 0.95f, 0.85f);

    readonly List<XRHandSubsystem> _subsystems = new List<XRHandSubsystem>();
    XRHandSubsystem _hands;

    GameObject _glyphGo;
    MeshFilter _glyphFilter;
    MeshRenderer _glyphRenderer;
    Mesh _mrMesh;
    Mesh _vrMesh;
    Material _glyphMat;

    GameObject _wipeGo;
    Material _wipeMat;
    Coroutine _wipeCo;
    bool _wiping;
    /// <summary>Destination glyph (VR=true) for the in-flight wipe; LateUpdate must not snap back mid-wipe.</summary>
    bool _wipeShowVrLabel;
    bool _wasArmed;
    bool _pulseSeenThisArm;

    public bool InPalace => stage != null && stage.InPalace;

    public void Configure(ThreeOSFinalDemoStage demoStage, ThreeOSBridge demoBridge,
        ThreeOSFinalDemoOpenBookGesture openBookGesture = null)
    {
        stage = demoStage;
        bridge = demoBridge;
        if (openBookGesture != null)
            openBook = openBookGesture;
        EnsureOpenBook();
    }

    void Awake()
    {
        EnsureOpenBook();
        EnsureLayering();
        EnsureMeshes();
        EnsureWipeShell();
    }

    void EnsureOpenBook()
    {
        if (openBook == null)
            openBook = GetComponent<ThreeOSFinalDemoOpenBookGesture>()
                       ?? FindFirstObjectByType<ThreeOSFinalDemoOpenBookGesture>();
        if (openBook == null)
            openBook = gameObject.AddComponent<ThreeOSFinalDemoOpenBookGesture>();
        if (stage != null)
            openBook.Configure(stage);
    }

    void EnsureLayering()
    {
        if (layering == null)
            layering = GetComponent<ThreeOSFinalDemoPalaceLayering>()
                       ?? gameObject.AddComponent<ThreeOSFinalDemoPalaceLayering>();
        layering.EnsureRoots();
    }

    void OnDisable()
    {
        DismissGlyph();
        if (_wipeCo != null)
        {
            StopCoroutine(_wipeCo);
            _wipeCo = null;
        }

        _wiping = false;
        if (_wipeGo != null)
            _wipeGo.SetActive(false);
        if (layering != null)
            layering.RestoreKeepListQueues();
    }

    void LateUpdate()
    {
        // After OpenBook.Update so PulseThisFrame / StickyArmed are current.
        if (stage == null)
            return;
        if (bridge == null)
            bridge = FindFirstObjectByType<ThreeOSBridge>();
        EnsureOpenBook();

        // Host sticky arm is authoritative (core thumb heuristics must not disarm).
        var armed = openBook != null && openBook.StickyArmed;
        var pulse = openBook != null && openBook.PulseThisFrame;
        if (bridge != null && bridge.TryGetIntents(out var intents))
            pulse = pulse || (intents.flags & (uint)InteractionIntentFlags.ModeTogglePulse) != 0;

        if (!armed)
        {
            if (_wasArmed && !_wiping)
                DismissGlyph();
            _wasArmed = false;
            _pulseSeenThisArm = false;
            return;
        }

        if (!_wasArmed)
            _pulseSeenThisArm = false;
        _wasArmed = true;

        // During wipe show the destination glyph from frame 0; otherwise current mode.
        UpdateGlyphPose(showVrLabel: _wiping ? _wipeShowVrLabel : stage.InPalace);

        if (pulse && !_pulseSeenThisArm && !_wiping)
        {
            _pulseSeenThisArm = true;
            BeginToggleWipe();
        }
    }

    void BeginToggleWipe()
    {
        var toPalace = !stage.InPalace;
        // Spec: MR→Palace = reverse (inward); Palace→MR = forward (outward).
        var direction = toPalace ? -1f : 1f;
        if (_wipeCo != null)
            StopCoroutine(_wipeCo);
        _wipeCo = StartCoroutine(WipeRoutine(toPalace, direction));
    }

    IEnumerator WipeRoutine(bool toPalace, float direction)
    {
        _wiping = true;
        _wipeShowVrLabel = toPalace;
        // Swap MR/VR glyph at wipe start (not after settle).
        UpdateGlyphPose(showVrLabel: toPalace);

        EnsureLayering();
        EnsurePalaceOverlay();
        // Sky stays Overlay+Always; exclusive keep-list (hands/cursors/HUD/glyph) draws above.
        if (_wipeMat != null)
            layering.ApplySkyMaterialQueue(_wipeMat);
        layering.PromoteKeepList();

        // VR→MR: office under full sky at progress 0 (outward formula).
        if (!toPalace)
            layering.SetMrWorldVisible(true);

        if (_wipeGo != null)
            _wipeGo.SetActive(true);
        if (_wipeMat != null)
        {
            _wipeMat.SetFloat("_Direction", direction);
            _wipeMat.SetFloat("_CoverBoost", 0f);
            _wipeMat.SetFloat("_Progress", 0f);
            _wipeMat.SetColor("_Tint", Color.white);
        }

        // Glyph may have been created just now — refresh keep-list so it stays above sky.
        layering.PromoteKeepList();

        var dur = Mathf.Max(0.05f, wipeDurationSec);
        var t = 0f;
        while (t < dur)
        {
            t += Time.unscaledDeltaTime;
            var u = Mathf.Clamp01(t / dur);
            if (_wipeMat != null)
                _wipeMat.SetFloat("_Progress", u);
            yield return null;
        }

        stage.SetInPalace(toPalace);
        ApplyPalaceOverlaySettled(toPalace);
        _wiping = false;
        _wipeCo = null;
        // Allow another thumbsClosed pulse without dropping open-book arm.
        _pulseSeenThisArm = false;

        if (_wasArmed)
            UpdateGlyphPose(showVrLabel: stage.InPalace);
        else
            DismissGlyph();
    }

    void ApplyPalaceOverlaySettled(bool inPalace)
    {
        EnsureLayering();
        EnsurePalaceOverlay();
        if (_wipeGo == null || _wipeMat == null)
            return;
        if (inPalace)
        {
            // Void office; sky over world; keep-list (hands/cursors) above sky.
            layering.SetMrWorldVisible(false);
            _wipeGo.SetActive(true);
            layering.ApplySkyMaterialQueue(_wipeMat);
            layering.PromoteKeepList();
            _wipeMat.SetFloat("_Progress", 1f);
            _wipeMat.SetFloat("_Direction", -1f);
            _wipeMat.SetFloat("_CoverBoost", 1f);
        }
        else
        {
            layering.SetMrWorldVisible(true);
            layering.RestoreKeepListQueues();
            _wipeMat.SetFloat("_CoverBoost", 0f);
            _wipeMat.SetFloat("_Progress", 1f);
            _wipeGo.SetActive(false);
        }
    }

    void UpdateGlyphPose(bool showVrLabel)
    {
        EnsureMeshes();
        if (_mrMesh == null && _vrMesh == null)
            return;

        if (!TryHandsMidpoint(out var mid, out var face))
            return;

        if (_glyphGo == null)
        {
            _glyphGo = new GameObject("ModeToggleGlyph");
            _glyphFilter = _glyphGo.AddComponent<MeshFilter>();
            _glyphRenderer = _glyphGo.AddComponent<MeshRenderer>();
            if (_glyphMat == null)
            {
                var tmpl = Resources.Load<Material>("3OS/GlyphDefault");
                _glyphMat = tmpl != null ? new Material(tmpl) : new Material(Shader.Find("ThreeOS/Glyph"));
            }

            _glyphRenderer.sharedMaterial = _glyphMat;
            if (_glyphMat != null)
                _glyphMat.renderQueue = ThreeOSFinalDemoPalaceLayering.KeepRenderQueue;
        }

        _glyphGo.SetActive(true);
        var mesh = showVrLabel ? (_vrMesh != null ? _vrMesh : _mrMesh) : (_mrMesh != null ? _mrMesh : _vrMesh);
        if (_glyphFilter.sharedMesh != mesh)
            _glyphFilter.sharedMesh = mesh;

        _glyphGo.transform.position = mid;
        if (face.sqrMagnitude > 1e-6f)
        {
            // Mesh faces −Z in export; yaw 180 so the readable side faces the user.
            _glyphGo.transform.rotation =
                Quaternion.LookRotation(face, Vector3.up) * Quaternion.Euler(0f, 180f, 0f);
        }

        _glyphGo.transform.localScale = Vector3.one * glyphScale;
    }

    void DismissGlyph()
    {
        if (_glyphGo != null)
            _glyphGo.SetActive(false);
    }

    bool TryHandsMidpoint(out Vector3 mid, out Vector3 faceDir)
    {
        mid = default;
        faceDir = Vector3.forward;
        if (!EnsureHands())
            return false;

        _hands.TryUpdateHands(XRHandSubsystem.UpdateType.Dynamic);
        if (!TryPalmWorld(_hands.leftHand, out var lp) || !TryPalmWorld(_hands.rightHand, out var rp))
            return false;

        mid = (lp + rp) * 0.5f;
        var cam = Camera.main;
        if (cam != null)
        {
            faceDir = mid - cam.transform.position;
            faceDir.y = 0f;
            if (faceDir.sqrMagnitude < 1e-6f)
                faceDir = cam.transform.forward;
            faceDir.Normalize();
        }

        return true;
    }

    bool TryPalmWorld(XRHand hand, out Vector3 worldPos)
    {
        worldPos = default;
        if (!hand.isTracked)
            return false;
        Pose tracking;
        if (!hand.GetJoint(XRHandJointID.Palm).TryGetPose(out tracking)
            && !hand.GetJoint(XRHandJointID.Wrist).TryGetPose(out tracking))
            return false;

        worldPos = TrackingToWorld(tracking.position);
        return true;
    }

    Vector3 TrackingToWorld(Vector3 trackingPos)
    {
        var space = stage != null && stage.XrOrigin != null
            ? stage.XrOrigin
            : (Camera.main != null ? Camera.main.transform.root : null);
        return space != null ? space.TransformPoint(trackingPos) : trackingPos;
    }

    bool EnsureHands()
    {
        if (_hands != null && _hands.running)
            return true;
        SubsystemManager.GetSubsystems(_subsystems);
        _hands = null;
        for (var i = 0; i < _subsystems.Count; i++)
        {
            if (_subsystems[i] != null && _subsystems[i].running)
            {
                _hands = _subsystems[i];
                break;
            }
        }

        return _hands != null;
    }

    void EnsureMeshes()
    {
        if (_mrMesh == null)
            _mrMesh = LoadGlyphMesh("mr");
        if (_vrMesh == null)
            _vrMesh = LoadGlyphMesh("vr");
    }

    static Mesh LoadGlyphMesh(string resourceName)
    {
        try
        {
            byte[] bytes = null;
            var ta = Resources.Load<TextAsset>($"3OS/glyphs/{resourceName}");
            if (ta != null && ta.bytes != null && ta.bytes.Length > 0)
                bytes = ta.bytes;
            if (bytes == null)
            {
                var path = Path.Combine(Application.streamingAssetsPath, "3OS", "glyphs",
                    resourceName + ".3glyph");
                if (File.Exists(path))
                    bytes = File.ReadAllBytes(path);
            }

            if (bytes != null)
            {
                if (ThreeOSGlbMesh.TryLoadFirstMesh(bytes, out var mesh, out var err))
                {
                    mesh.name = resourceName + "_toggle_glyph";
                    return mesh;
                }

                if (!string.IsNullOrEmpty(err))
                    Debug.LogWarning($"[3OS] mode toggle glyph '{resourceName}' parse: {err}");
            }
        }
        catch (System.Exception ex)
        {
            Debug.LogWarning($"[3OS] mode toggle glyph '{resourceName}': {ex.Message}");
        }

        return null;
    }

    void EnsureWipeShell() => EnsurePalaceOverlay();

    void EnsurePalaceOverlay()
    {
        var cam = Camera.main;
        if (cam == null)
            return;

        if (_wipeGo == null)
        {
            // Inverted sphere around the HMD — samples Palace sky and draws OVER office meshes
            // (ZTest Always / Overlay queue). No office MeshRenderer queue changes required.
            _wipeGo = GameObject.CreatePrimitive(PrimitiveType.Sphere);
            _wipeGo.name = "PalaceSkyOverlay";
            Object.Destroy(_wipeGo.GetComponent<Collider>());
            _wipeGo.transform.SetParent(cam.transform, false);
            _wipeGo.transform.localPosition = Vector3.zero;
            _wipeGo.transform.localRotation = Quaternion.identity;
            _wipeGo.transform.localScale = Vector3.one * 12f;

            // Resources material keeps the shader in the player build (Shader.Find alone strips).
            var tmpl = Resources.Load<Material>("3OS/PalaceSkyOverlay");
            var shader = Shader.Find("ThreeOS/PalaceSkyOverlay");
            if (tmpl != null)
                _wipeMat = new Material(tmpl);
            else if (shader != null)
                _wipeMat = new Material(shader);
            else
            {
                Debug.LogError("[3OS] PalaceSkyOverlay shader missing from build — sky will be wrong.");
                _wipeMat = new Material(Shader.Find("Sprites/Default"));
            }

            layering?.ApplySkyMaterialQueue(_wipeMat);
            _wipeMat.renderQueue = ThreeOSFinalDemoPalaceLayering.SkyRenderQueue;
            _wipeMat.SetFloat("_Feather", 0.08f);
            _wipeMat.SetColor("_Tint", Color.white);
            BindPalaceSkyTextures(_wipeMat);

            var rend = _wipeGo.GetComponent<MeshRenderer>();
            rend.sharedMaterial = _wipeMat;
            rend.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;
            rend.receiveShadows = false;
            _wipeGo.SetActive(false);
            Debug.Log($"[3OS] Palace overlay ready shader={_wipeMat.shader.name} rq={_wipeMat.renderQueue}");
        }
        else if (_wipeMat != null)
        {
            BindPalaceSkyTextures(_wipeMat);
        }

        // Re-parent if camera changed (XR Origin respawn).
        if (_wipeGo.transform.parent != cam.transform)
        {
            _wipeGo.transform.SetParent(cam.transform, false);
            _wipeGo.transform.localPosition = Vector3.zero;
            _wipeGo.transform.localRotation = Quaternion.identity;
        }
    }

    void BindPalaceSkyTextures(Material mat)
    {
        if (mat == null || stage == null || stage.PalaceSkybox == null)
            return;
        var src = stage.PalaceSkybox;
        CopySkyTex(src, mat, "_FrontTex");
        CopySkyTex(src, mat, "_BackTex");
        CopySkyTex(src, mat, "_LeftTex");
        CopySkyTex(src, mat, "_RightTex");
        CopySkyTex(src, mat, "_UpTex");
        CopySkyTex(src, mat, "_DownTex");
    }

    static void CopySkyTex(Material src, Material dst, string prop)
    {
        if (src.HasProperty(prop) && dst.HasProperty(prop))
            dst.SetTexture(prop, src.GetTexture(prop));
    }
}
