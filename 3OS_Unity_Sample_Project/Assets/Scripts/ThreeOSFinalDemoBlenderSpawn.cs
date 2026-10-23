using System.Collections;
using System.Collections.Generic;
using ThreeOS;
using UnityEngine;
using UnityEngine.XR.Hands;

/// <summary>
/// Phase 0.7 Task 7.9 — double-pinch <c>DemonA.blend</c> → spinner → palm-drop laser →
/// instantiate staged <see cref="blenderGroupPrefab"/> and run badge / logos / Demon wipe reveal.
/// </summary>
public sealed class ThreeOSFinalDemoBlenderSpawn : MonoBehaviour
{
    public const string TargetFileName = "DemonA.blend";

    [Header("Refs")]
    [SerializeField] ThreeOSFinalDemoStage stage;
    [SerializeField] ThreeOSStorageController storage;
    [SerializeField] ThreeOSBridge bridge;
    [SerializeField] ThreeOSInputRouter input;
    [SerializeField] GameObject blenderGroupPrefab;

    [Header("Arm / cancel")]
    [SerializeField] float spinnerHandOffsetM = 0.08f;
    [SerializeField] float spinnerSpinDegPerSec = 360f;

    [Header("Palm-drop arming")]
    [Tooltip("Min index↔little tip distance for fingers-splayed.")]
    [SerializeField] float splayMinTipSepM = 0.075f;
    [Tooltip("Palm normal · world-down must be >= this (1 = flat to floor).")]
    [SerializeField] float palmDownDotMin = 0.55f;
    [SerializeField] float floorRayMaxM = 4f;
    [SerializeField] LayerMask floorMask = ~0;

    [Header("Laser perimeter")]
    [SerializeField] float perimeterSeconds = 2f;
    [SerializeField] float badgeFootprintMeters = 1.6f;
    [SerializeField] Color laserColor = new Color(0.45f, 0.78f, 0.95f, 0.9f);
    [SerializeField] float laserWidthM = 0.012f;

    [Header("Reveal timing")]
    [SerializeField] float badgeScaleSeconds = 0.9f;
    [Tooltip("Full Y spins while the badge scales in (3 = 1080°).")]
    [SerializeField] float badgeSpinTurns = 3f;
    [SerializeField] float logoScaleSeconds = 0.8f;
    [SerializeField] float logoOvershoot = 1.1f;
    [SerializeField] float demonWipeSeconds = 1.5f;
    [Tooltip("Drive wipe past 1 so the plasma feather clears the crown.")]
    [SerializeField] float wipeFinishOvershoot = 0.12f;
    [Tooltip("Lift spawned prefab so badge does not Z-fight the floor.")]
    [SerializeField] float spawnFloorLiftM = 0.01f;

    enum Phase
    {
        Idle,
        Armed,
        Tracing,
        Revealing
    }

    Phase _phase = Phase.Idle;
    int _armedHand = -1;
    ulong _armedEntity;
    GameObject _spinner;
    LineRenderer _laser;
    float _traceProgress;
    Vector3 _squareCenter;
    float _squareYawDeg;
    float _squareSize;
    Coroutine _revealCo;
    readonly List<XRHandSubsystem> _subsystems = new List<XRHandSubsystem>();
    XRHandSubsystem _hands;

    public bool IsArmed => _phase == Phase.Armed || _phase == Phase.Tracing;

    public void Configure(ThreeOSFinalDemoStage demoStage, ThreeOSStorageController demoStorage,
        ThreeOSBridge demoBridge, ThreeOSInputRouter demoInput, GameObject prefab = null)
    {
        stage = demoStage;
        storage = demoStorage;
        bridge = demoBridge;
        input = demoInput;
        if (prefab != null)
            blenderGroupPrefab = prefab;
        BindStorageHandlers();
    }

    void Awake()
    {
        EnsureRefs();
        BindStorageHandlers();
        EnsureLaser();
    }

    void OnDestroy()
    {
        UnbindStorageHandlers();
        if (_revealCo != null)
            StopCoroutine(_revealCo);
        DestroySpinner();
        if (_laser != null)
            Destroy(_laser.gameObject);
    }

    void EnsureRefs()
    {
        if (stage == null)
            stage = FindFirstObjectByType<ThreeOSFinalDemoStage>();
        if (storage == null)
            storage = FindFirstObjectByType<ThreeOSStorageController>();
        if (bridge == null)
            bridge = FindFirstObjectByType<ThreeOSBridge>();
        if (input == null)
            input = FindFirstObjectByType<ThreeOSInputRouter>();
        if (blenderGroupPrefab == null)
            blenderGroupPrefab = Resources.Load<GameObject>("3OS/BlenderGroup");
    }

    void BindStorageHandlers()
    {
        if (storage == null)
            return;
        storage.FileDoublePinchHandler = OnFileDoublePinch;
        storage.ShortPinchConsumeHandler = TryConsumeCancelPinch;
    }

    void UnbindStorageHandlers()
    {
        if (storage == null)
            return;
        if (ReferenceEquals(storage.FileDoublePinchHandler, (System.Action<ulong, InteropWorkspaceItem, int>)OnFileDoublePinch))
            storage.FileDoublePinchHandler = null;
        if (ReferenceEquals(storage.ShortPinchConsumeHandler, (System.Func<int, bool>)TryConsumeCancelPinch))
            storage.ShortPinchConsumeHandler = null;
    }

    void OnFileDoublePinch(ulong entityId, InteropWorkspaceItem item, int hand)
    {
        if (_phase == Phase.Revealing)
            return;
        if (stage != null && stage.InPalace)
            return; // MR-only spawn
        if (item.Name == null ||
            !string.Equals(item.Name, TargetFileName, System.StringComparison.OrdinalIgnoreCase))
            return;
        if (blenderGroupPrefab == null)
        {
            Debug.LogError("[3OS] BlenderGroup prefab missing — assign on FinalDemoBlenderSpawn");
            return;
        }

        if (hand < 0)
            hand = 0;

        CancelArmed(keepSelection: true);
        _armedHand = hand;
        _armedEntity = entityId;
        _phase = Phase.Armed;
        _traceProgress = 0f;
        EnsureSpinner(hand);
        if (_laser != null)
            _laser.enabled = false;
        Debug.Log($"[3OS] Blender spawn armed hand={(hand == 0 ? "L" : "R")} file={item.Name}");
    }

    bool TryConsumeCancelPinch(int hand)
    {
        if (_phase != Phase.Armed && _phase != Phase.Tracing)
            return false;
        if (hand != _armedHand)
            return false;
        CancelArmed(keepSelection: true);
        Debug.Log("[3OS] Blender spawn cancelled (same-hand pinch)");
        return true;
    }

    void CancelArmed(bool keepSelection)
    {
        if (_revealCo != null)
        {
            StopCoroutine(_revealCo);
            _revealCo = null;
        }

        _phase = Phase.Idle;
        _armedHand = -1;
        _armedEntity = 0;
        _traceProgress = 0f;
        DestroySpinner();
        if (_laser != null)
        {
            _laser.positionCount = 0;
            _laser.enabled = false;
        }

        _ = keepSelection;
    }

    /// <summary>Act I reset: cancel arm/reveal and destroy any spawned BlenderGroup instance.</summary>
    public void ForceCancelArmed()
    {
        CancelArmed(keepSelection: true);
        DestroySpawnedBlenderGroups();
    }

    public static void DestroySpawnedBlenderGroups()
    {
        var named = GameObject.Find("BlenderGroup_Spawned");
        if (named != null)
            Object.Destroy(named);

        // Prefab may rename; sweep by prefix.
        var all = FindObjectsByType<Transform>(FindObjectsSortMode.None);
        for (var i = 0; i < all.Length; i++)
        {
            var t = all[i];
            if (t == null || t.parent != null)
                continue;
            if (t.name.StartsWith("BlenderGroup", System.StringComparison.Ordinal))
                Object.Destroy(t.gameObject);
        }
    }

    void Update()
    {
        if (_phase != Phase.Armed && _phase != Phase.Tracing)
            return;
        if (_armedHand < 0)
        {
            CancelArmed(true);
            return;
        }

        UpdateSpinnerPose();

        if (!TryGetPalmDropPose(_armedHand, out var palmPos, out var palmNormal, out var splayed,
                out var palmDown))
        {
            ResetTraceOnly();
            return;
        }

        if (!splayed || !palmDown)
        {
            ResetTraceOnly();
            return;
        }

        if (!TryFloorHits(palmPos, palmNormal, out var center, out var yawDeg))
        {
            ResetTraceOnly();
            return;
        }

        _squareCenter = center;
        _squareYawDeg = yawDeg;
        _squareSize = Mathf.Max(0.2f, badgeFootprintMeters);

        if (_phase == Phase.Armed)
            _phase = Phase.Tracing;

        _traceProgress = Mathf.Clamp01(_traceProgress + Time.unscaledDeltaTime / Mathf.Max(0.05f, perimeterSeconds));
        DrawLaserSquare(_squareCenter, _squareYawDeg, _squareSize, _traceProgress);

        if (_traceProgress >= 1f - 1e-4f)
            BeginReveal();
    }

    void ResetTraceOnly()
    {
        if (_phase == Phase.Tracing)
            _phase = Phase.Armed;
        _traceProgress = 0f;
        if (_laser != null)
        {
            _laser.positionCount = 0;
            _laser.enabled = false;
        }
    }

    void BeginReveal()
    {
        if (_phase == Phase.Revealing)
            return;
        _phase = Phase.Revealing;
        // Kill spinner before instantiate — must not linger / snap to the file glyph.
        DestroySpinner();
        _armedHand = -1;
        if (_laser != null)
        {
            // Leave full square visible briefly while badge catches up.
            DrawLaserSquare(_squareCenter, _squareYawDeg, _squareSize, 1f);
        }

        _revealCo = StartCoroutine(RevealRoutine());
    }

    IEnumerator RevealRoutine()
    {
        DestroySpinner();
        var spawnPos = _squareCenter + Vector3.up * spawnFloorLiftM;
        var instance = Instantiate(blenderGroupPrefab, spawnPos, Quaternion.Euler(0f, _squareYawDeg, 0f));
        instance.name = "BlenderGroup_Spawned";
        // Prefab root carries author scale; keep it. Slight Y lift avoids badge/floor Z-fight.
        instance.transform.position = spawnPos;
        instance.transform.rotation = Quaternion.Euler(0f, _squareYawDeg, 0f);

        var badge = FindChild(instance.transform, "Blender_Badge");
        var logos = FindNamedChildren(instance.transform, "Blender_Logo");
        var demon = FindChild(instance.transform, "DemonA");
        var wipe = demon != null ? demon.GetComponent<ThreeOSVerticalWipeDriver>() : null;
        if (wipe == null && demon != null)
            wipe = demon.GetComponentInChildren<ThreeOSVerticalWipeDriver>(true);

        // Opaque logos + ZWrite so transparent badge depth-sort cannot paint over the back pair.
        for (var i = 0; i < logos.Count; i++)
            ForceOpaqueDepthWrite(logos[i]);
        if (badge != null)
            EnableBadgeDepthWrite(badge);

        // Prefab may author logos/badge at full scale — hide until their beats.
        if (badge != null)
            badge.localScale = Vector3.zero;
        for (var i = 0; i < logos.Count; i++)
            logos[i].localScale = Vector3.zero;
        if (wipe != null)
        {
            wipe.RefreshBounds();
            // Negative wipe clears the feather band so feet stay hidden during badge/logos.
            wipe.SetWipe(-0.08f);
        }

        // Keep Demon meshes off until their wipe beat (belt-and-suspenders vs feather leak).
        if (demon != null)
            SetRenderersEnabled(demon, false);

        // 1) Badge: 0 → footprint match (Unity plane = 10 units; scale so world size = square).
        if (badge != null)
        {
            var rootScale = Mathf.Max(1e-4f, instance.transform.lossyScale.x);
            var planeNative = 10f;
            var targetLocal = _squareSize / (planeNative * rootScale);
            var t = 0f;
            var startYaw = badge.localEulerAngles.y;
            // LerpAngle shortest-path would collapse multi-turn spins — accumulate explicitly.
            var endYaw = startYaw + 360f * Mathf.Max(1f, badgeSpinTurns);
            while (t < badgeScaleSeconds)
            {
                t += Time.unscaledDeltaTime;
                var u = Mathf.SmoothStep(0f, 1f, Mathf.Clamp01(t / badgeScaleSeconds));
                var s = Mathf.Lerp(0f, targetLocal, u);
                badge.localScale = new Vector3(s, s, s);
                var yaw = Mathf.Lerp(startYaw, endYaw, u);
                badge.localRotation = Quaternion.Euler(0f, yaw, 0f);
                yield return null;
            }

            badge.localScale = Vector3.one * targetLocal;
            badge.localRotation = Quaternion.Euler(0f, endYaw, 0f);
        }

        if (_laser != null)
        {
            _laser.positionCount = 0;
            _laser.enabled = false;
        }

        // 2) Logos: simultaneous 0 → overshoot → 1
        if (logos.Count > 0)
        {
            for (var i = 0; i < logos.Count; i++)
                logos[i].localScale = Vector3.zero;
            var t = 0f;
            var peak = Mathf.Clamp(logoOvershoot, 1.01f, 1.5f);
            var upDur = logoScaleSeconds * 0.65f;
            var downDur = logoScaleSeconds * 0.35f;
            while (t < upDur)
            {
                t += Time.unscaledDeltaTime;
                var u = Mathf.SmoothStep(0f, 1f, Mathf.Clamp01(t / upDur));
                var s = Mathf.Lerp(0f, peak, u);
                for (var i = 0; i < logos.Count; i++)
                    logos[i].localScale = Vector3.one * s;
                yield return null;
            }

            t = 0f;
            while (t < downDur)
            {
                t += Time.unscaledDeltaTime;
                var u = Mathf.SmoothStep(0f, 1f, Mathf.Clamp01(t / downDur));
                var s = Mathf.Lerp(peak, 1f, u);
                for (var i = 0; i < logos.Count; i++)
                    logos[i].localScale = Vector3.one * s;
                yield return null;
            }

            for (var i = 0; i < logos.Count; i++)
                logos[i].localScale = Vector3.one;
        }

        // 3) Demon wipe → 1 (+ overshoot so feather clears the crown)
        if (demon != null)
            SetRenderersEnabled(demon, true);
        if (wipe != null)
        {
            wipe.RefreshBounds();
            wipe.SetWipe(-0.08f);
            var wipeEnd = 1f + Mathf.Max(0f, wipeFinishOvershoot);
            var t = 0f;
            while (t < demonWipeSeconds)
            {
                t += Time.unscaledDeltaTime;
                var u = Mathf.Clamp01(t / demonWipeSeconds);
                wipe.SetWipe(Mathf.Lerp(-0.08f, wipeEnd, u));
                yield return null;
            }

            wipe.SetWipe(wipeEnd);
        }

        _phase = Phase.Idle;
        _armedHand = -1;
        _armedEntity = 0;
        _revealCo = null;
        Debug.Log("[3OS] Blender spawn reveal complete");
    }

    void EnsureSpinner(int hand)
    {
        DestroySpinner();
        _spinner = new GameObject("3OS_AppDropSpinner");
        // Segmented arc (not a solid disc) so rotation reads clearly. Gap + bright tip.
        const int segments = 12;
        const int litSegments = 9; // leave a 90° gap
        var radius = 0.03f;
        var sh = Shader.Find("ThreeOS/EqBar") ?? Shader.Find("Sprites/Default") ??
                 Shader.Find("Unlit/Color");
        for (var i = 0; i < litSegments; i++)
        {
            var bit = GameObject.CreatePrimitive(PrimitiveType.Cube);
            bit.name = $"Seg_{i}";
            bit.transform.SetParent(_spinner.transform, false);
            foreach (var c in bit.GetComponents<Collider>())
                Destroy(c);

            var angleDeg = i * (360f / segments);
            var rad = angleDeg * Mathf.Deg2Rad;
            bit.transform.localPosition = new Vector3(Mathf.Cos(rad) * radius, Mathf.Sin(rad) * radius, 0f);
            bit.transform.localRotation = Quaternion.Euler(0f, 0f, angleDeg);
            var isTip = i == litSegments - 1;
            bit.transform.localScale = isTip
                ? new Vector3(0.01f, 0.007f, 0.003f)
                : new Vector3(0.007f, 0.0045f, 0.0025f);

            var mat = new Material(sh);
            var col = isTip
                ? new Color(0.95f, 0.55f, 0.15f, 0.95f)
                : new Color(0.15f, 0.15f, 0.18f, 0.75f);
            mat.color = col;
            if (mat.HasProperty("_Color"))
                mat.SetColor("_Color", col);
            mat.renderQueue = 5100;
            bit.GetComponent<MeshRenderer>().sharedMaterial = mat;
        }

        UpdateSpinnerPose();
    }

    void UpdateSpinnerPose()
    {
        if (_spinner == null || _armedHand < 0)
            return;
        if (_phase != Phase.Armed && _phase != Phase.Tracing)
            return;
        if (!TryGetHandCursorUnity(_armedHand, out var pos, out var rot))
            return;
        var up = Vector3.up;
        var cam = Camera.main;
        var forward = Vector3.ProjectOnPlane(rot * Vector3.forward, up);
        if (forward.sqrMagnitude < 1e-6f)
            forward = Vector3.ProjectOnPlane(cam != null ? cam.transform.forward : Vector3.forward, up);
        if (forward.sqrMagnitude < 1e-6f)
            forward = Vector3.forward;
        forward.Normalize();
        _spinner.transform.position = pos + up * spinnerHandOffsetM + forward * (spinnerHandOffsetM * 0.35f);

        // Face the camera; spin in the view plane so the arc gap reads as rotation.
        var toCam = cam != null
            ? Vector3.ProjectOnPlane(cam.transform.position - _spinner.transform.position, up)
            : -forward;
        if (toCam.sqrMagnitude < 1e-6f)
            toCam = -forward;
        toCam.Normalize();
        var spin = Time.unscaledTime * spinnerSpinDegPerSec;
        _spinner.transform.rotation = Quaternion.LookRotation(toCam, up) *
                                      Quaternion.AngleAxis(spin, Vector3.forward);
    }

    void DestroySpinner()
    {
        if (_spinner == null)
            return;
        var go = _spinner;
        _spinner = null;
        go.SetActive(false);
        Destroy(go);
    }

    void EnsureLaser()
    {
        if (_laser != null)
            return;
        var go = new GameObject("3OS_AppDropLaser");
        _laser = go.AddComponent<LineRenderer>();
        _laser.useWorldSpace = true;
        _laser.loop = false;
        _laser.widthMultiplier = laserWidthM;
        _laser.numCornerVertices = 4;
        _laser.numCapVertices = 2;
        _laser.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;
        _laser.receiveShadows = false;
        _laser.enabled = false;
        var shader = Shader.Find("ThreeOS/EqBar") ?? Shader.Find("Sprites/Default");
        var mat = new Material(shader);
        mat.color = laserColor;
        if (mat.HasProperty("_Color"))
            mat.SetColor("_Color", laserColor);
        mat.renderQueue = 5100;
        _laser.sharedMaterial = mat;
        _laser.startColor = laserColor;
        _laser.endColor = laserColor;
        _laser.sortingOrder = 40;
    }

    void DrawLaserSquare(Vector3 center, float yawDeg, float size, float progress01)
    {
        EnsureLaser();
        _laser.enabled = true;
        var half = size * 0.5f;
        var rot = Quaternion.Euler(0f, yawDeg, 0f);
        var c0 = center + rot * new Vector3(-half, 0.01f, -half);
        var c1 = center + rot * new Vector3(half, 0.01f, -half);
        var c2 = center + rot * new Vector3(half, 0.01f, half);
        var c3 = center + rot * new Vector3(-half, 0.01f, half);
        // Perimeter path length 4 sides; place points along the path by progress.
        const int segs = 64;
        var count = Mathf.Max(2, Mathf.CeilToInt(segs * Mathf.Clamp01(progress01)) + 1);
        _laser.positionCount = count;
        for (var i = 0; i < count; i++)
        {
            var u = (i / (float)(segs)) * 4f; // 0..4 along perimeter
            u = Mathf.Min(u, 4f * progress01);
            Vector3 a, b;
            float local;
            if (u < 1f)
            {
                a = c0;
                b = c1;
                local = u;
            }
            else if (u < 2f)
            {
                a = c1;
                b = c2;
                local = u - 1f;
            }
            else if (u < 3f)
            {
                a = c2;
                b = c3;
                local = u - 2f;
            }
            else
            {
                a = c3;
                b = c0;
                local = u - 3f;
            }

            _laser.SetPosition(i, Vector3.Lerp(a, b, local));
        }
    }

    bool TryGetPalmDropPose(int hand, out Vector3 palmPos, out Vector3 palmNormal, out bool splayed,
        out bool palmDown)
    {
        palmPos = default;
        palmNormal = Vector3.down;
        splayed = false;
        palmDown = false;
        EnsureHands();
        if (_hands == null)
            return false;

        var xrHand = hand == 0 ? _hands.leftHand : _hands.rightHand;
        if (xrHand.GetJoint(XRHandJointID.Palm).TryGetPose(out var palmPose))
        {
            palmPos = TrackingToWorld(palmPose.position);
            // Meta palm +Y is dorsal; negate → palmar normal (toward floor when palm-down).
            palmNormal = -(TrackingToWorldRotation(palmPose.rotation) * Vector3.up);
        }
        else if (!TryGetHandCursorUnity(hand, out palmPos, out var rot))
        {
            return false;
        }
        else
        {
            palmNormal = -(rot * Vector3.up);
        }

        palmNormal.Normalize();
        palmDown = Vector3.Dot(palmNormal, Vector3.down) >= palmDownDotMin;

        // Splay: index tip ↔ little tip separation.
        if (xrHand.GetJoint(XRHandJointID.IndexTip).TryGetPose(out var indexPose) &&
            xrHand.GetJoint(XRHandJointID.LittleTip).TryGetPose(out var littlePose))
        {
            var i = TrackingToWorld(indexPose.position);
            var l = TrackingToWorld(littlePose.position);
            splayed = Vector3.Distance(i, l) >= splayMinTipSepM;
        }
        else
        {
            // Controllers: treat as splayed when palm-down (demo stand-in).
            splayed = palmDown;
        }

        return true;
    }

    bool TryFloorHits(Vector3 palmPos, Vector3 palmNormal, out Vector3 center, out float yawDeg)
    {
        center = palmPos;
        yawDeg = 0f;
        var towardFloor = palmNormal;
        if (Vector3.Dot(towardFloor, Vector3.down) < 0.15f)
            towardFloor = Vector3.down;

        var palmHitOk = Physics.Raycast(palmPos, towardFloor.normalized, out var palmHit, floorRayMaxM,
            floorMask, QueryTriggerInteraction.Ignore);
        var gazeHitOk = false;
        RaycastHit gazeHit = default;
        var cam = Camera.main;
        if (cam != null)
        {
            gazeHitOk = Physics.Raycast(cam.transform.position, cam.transform.forward, out gazeHit,
                floorRayMaxM * 2f, floorMask, QueryTriggerInteraction.Ignore);
            // Prefer hits that are roughly floor-aligned.
            if (gazeHitOk && Vector3.Dot(gazeHit.normal, Vector3.up) < 0.5f)
                gazeHitOk = false;
        }

        if (palmHitOk && Vector3.Dot(palmHit.normal, Vector3.up) < 0.5f)
            palmHitOk = false;

        if (!palmHitOk && !gazeHitOk)
        {
            // Horizontal plane fallback at spawn / origin Y.
            var y = stage != null && stage.PlayerSpawn != null ? stage.PlayerSpawn.position.y : 0f;
            var t = (y - palmPos.y) / towardFloor.y;
            if (towardFloor.y >= -1e-4f || t < 0f || t > floorRayMaxM)
                return false;
            center = palmPos + towardFloor.normalized * t;
            center.y = y;
        }
        else if (palmHitOk && gazeHitOk)
        {
            center = (palmHit.point + gazeHit.point) * 0.5f;
        }
        else if (palmHitOk)
        {
            center = palmHit.point;
        }
        else
        {
            center = gazeHit.point;
        }

        var forward = Vector3.ProjectOnPlane(
            cam != null ? cam.transform.forward : Vector3.forward, Vector3.up);
        if (forward.sqrMagnitude < 1e-6f)
            forward = Vector3.forward;
        yawDeg = Quaternion.LookRotation(forward.normalized, Vector3.up).eulerAngles.y;
        return true;
    }

    bool TryGetHandCursorUnity(int hand, out Vector3 pos, out Quaternion rot)
    {
        pos = default;
        rot = Quaternion.identity;
        if (input == null)
            return false;

        var aimUnity = InteropStructs.ToUnityPose(input.AimPoseForHand(hand));
        pos = aimUnity.position;
        rot = aimUnity.rotation;

        // Prefer active point-cursor tip when present (AimPoseForHand falls back to head).
        var cursor = hand == 0 ? input.LastLeftCursor : input.LastRightCursor;
        if (cursor.active != 0)
            pos = InteropStructs.OpenXrToUnityPosition(cursor.position.ToVector3());
        return true;
    }

    void EnsureHands()
    {
        if (_hands != null && _hands.running)
            return;
        _subsystems.Clear();
        SubsystemManager.GetSubsystems(_subsystems);
        for (var i = 0; i < _subsystems.Count; i++)
        {
            if (_subsystems[i] != null && _subsystems[i].running)
            {
                _hands = _subsystems[i];
                return;
            }
        }

        _hands = _subsystems.Count > 0 ? _subsystems[0] : null;
    }

    static Vector3 TrackingToWorld(Vector3 trackingPos)
    {
        var origin = FindFirstObjectByType<ThreeOSFinalDemoStage>()?.XrOrigin;
        if (origin != null)
            return origin.TransformPoint(trackingPos);
        return trackingPos;
    }

    static Quaternion TrackingToWorldRotation(Quaternion trackingRot)
    {
        var origin = FindFirstObjectByType<ThreeOSFinalDemoStage>()?.XrOrigin;
        if (origin != null)
            return origin.rotation * trackingRot;
        return trackingRot;
    }

    /// <summary>
    /// Logos must be opaque + ZWrite so a transparent badge (queue 3000, ZWrite off) cannot
    /// depth-sort over the rear pair even when those logos are spatially in front of the plane.
    /// </summary>
    static void ForceOpaqueDepthWrite(Transform root)
    {
        if (root == null)
            return;
        var rs = root.GetComponentsInChildren<Renderer>(true);
        for (var i = 0; i < rs.Length; i++)
        {
            var r = rs[i];
            if (r == null)
                continue;
            var mats = r.materials;
            for (var m = 0; m < mats.Length; m++)
            {
                var mat = mats[m];
                if (mat == null)
                    continue;
                if (mat.HasProperty("_Mode"))
                    mat.SetFloat("_Mode", 0f);
                if (mat.HasProperty("_SrcBlend"))
                    mat.SetFloat("_SrcBlend", (float)UnityEngine.Rendering.BlendMode.One);
                if (mat.HasProperty("_DstBlend"))
                    mat.SetFloat("_DstBlend", (float)UnityEngine.Rendering.BlendMode.Zero);
                if (mat.HasProperty("_ZWrite"))
                    mat.SetFloat("_ZWrite", 1f);
                mat.DisableKeyword("_ALPHATEST_ON");
                mat.DisableKeyword("_ALPHABLEND_ON");
                mat.DisableKeyword("_ALPHAPREMULTIPLY_ON");
                mat.renderQueue = (int)UnityEngine.Rendering.RenderQueue.Geometry;
                if (mat.HasProperty("_Color"))
                {
                    var c = mat.color;
                    mat.color = new Color(c.r, c.g, c.b, 1f);
                }
            }
        }
    }

    static void EnableBadgeDepthWrite(Transform badge)
    {
        if (badge == null)
            return;
        var rs = badge.GetComponentsInChildren<Renderer>(true);
        for (var i = 0; i < rs.Length; i++)
        {
            var r = rs[i];
            if (r == null)
                continue;
            var mats = r.materials;
            for (var m = 0; m < mats.Length; m++)
            {
                if (mats[m] != null && mats[m].HasProperty("_ZWrite"))
                    mats[m].SetFloat("_ZWrite", 1f);
            }
        }
    }

    static void SetRenderersEnabled(Transform root, bool enabled)
    {
        if (root == null)
            return;
        var rs = root.GetComponentsInChildren<Renderer>(true);
        for (var i = 0; i < rs.Length; i++)
        {
            if (rs[i] != null)
                rs[i].enabled = enabled;
        }
    }

    static Transform FindChild(Transform root, string name)
    {
        if (root == null)
            return null;
        if (root.name == name)
            return root;
        for (var i = 0; i < root.childCount; i++)
        {
            var found = FindChild(root.GetChild(i), name);
            if (found != null)
                return found;
        }

        return null;
    }

    static List<Transform> FindNamedChildren(Transform root, string name)
    {
        var list = new List<Transform>(4);
        CollectNamed(root, name, list);
        return list;
    }

    static void CollectNamed(Transform t, string name, List<Transform> list)
    {
        if (t.name == name)
            list.Add(t);
        for (var i = 0; i < t.childCount; i++)
            CollectNamed(t.GetChild(i), name, list);
    }
}
