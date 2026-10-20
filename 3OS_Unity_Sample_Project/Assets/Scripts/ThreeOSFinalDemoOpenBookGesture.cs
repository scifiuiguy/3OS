using System.Collections.Generic;
using System.Text;
using UnityEngine;
using UnityEngine.XR.Hands;

/// <summary>
/// Phase 0.7 Task 7.7 — open-book pose from raw XR Hands + world HUD diagnostics.
/// Enter: palms face + fingers parallel/ortho + tip cluster + thumbs opposite/ortho.
/// Continuous base (cancel): same without thumbs-opposite (so thumbs can close to confirm).
/// </summary>
public sealed class ThreeOSFinalDemoOpenBookGesture : MonoBehaviour
{
    [Header("Refs")]
    [SerializeField] ThreeOSFinalDemoStage stage;
    [Tooltip("Full OpenBook diagnostics (expensive TextMesh). Off for FPS A/B.")]
    [SerializeField] bool showHud = false;
    [Tooltip("Keep a one-line FPS meter even when showHud is off.")]
    [SerializeField] bool showFps = true;
    [Tooltip("Off = no XR Hands eval / arm / pulse (MR↔VR toggle inert). Was false for FPS A/B.")]
    [SerializeField] bool gestureEnabled = true;

    [Header("Palm → face")]
    [SerializeField] float palmFaceDotMin = 0.55f;

    [Header("Index/middle/ring directions")]
    [Tooltip("Min pairwise tip←proximal direction dot (parallel).")]
    [SerializeField] float fingerParallelDotMin = 0.75f;
    [Tooltip("Max |fingerDir·palmNormal| (orthogonal to palm).")]
    [SerializeField] float fingerPalmOrthoAbsMax = 0.35f;

    [Header("Unsplayed tip cluster (enter + continuous cancel)")]
    [SerializeField] float tipClusterMaxMeters = 0.055f;

    [Header("Thumbs — ENTER only (never used to cancel once armed)")]
    [Tooltip("Max |thumb·palm| / |thumb·fingers| to ARM. Higher = easier. Breaking ortho after arm does NOT cancel.")]
    [SerializeField] float thumbPalmOrthoAbsMax = 0.98f;
    [SerializeField] float thumbFingerOrthoAbsMax = 0.98f;
    [Tooltip("Optional opposite thumbs (enter only). Soft — set >=1 to ignore.")]
    [SerializeField] float thumbOppositeDotMax = 0.55f;

    [Header("Thumb close (confirm)")]
    [SerializeField] float thumbCloseEnterM = 0.048f;
    [SerializeField] float thumbCloseExitM = 0.072f;

    [Header("Arm cancel hysteresis")]
    [Tooltip("Maintain-base must fail this many frames before disarm. Thumbs never count.")]
    [SerializeField] int baseFailFramesToCancel = 20;

    [Header("Book framing")]
    [SerializeField] float bookSepMinM = 0.06f;
    [SerializeField] float bookSepMaxM = 0.50f;
    [SerializeField] float bookHeightDeltaMaxM = 0.20f;

    readonly List<XRHandSubsystem> _subsystems = new List<XRHandSubsystem>();
    XRHandSubsystem _hands;
    TextMesh _hud;
    Transform _hudFollow;
    float _fpsSmooth = 72f;
    float _msSmooth = 13.9f;
    float _fpsHudNextAt;

    bool _stickyArmed;
    bool _thumbLatch;
    bool _leftThumbClosed;
    bool _rightThumbClosed;
    int _baseFailFrames;
    bool _prevBase;
    bool _prevEnter;
    bool _prevArmed;
    bool _prevPulse;

    public struct HandDiag
    {
        public bool tracked;
        public float palmFaceDot;
        public bool palmFaceOk;
        public float fingerParallelMin;
        public bool fingerParallelOk;
        public float fingerPalmOrthoMaxAbs;
        public bool fingerPalmOrthoOk;
        public float tipClusterMax;
        public bool tipClusterOk;
        public float thumbPalmOrthoAbs;
        public float thumbFingerOrthoAbs;
        public bool thumbOrthoOk;
        public Vector3 palmPos;
        public Vector3 palmNormal;
        public Vector3 fingerMean;
        public Vector3 thumbDir;
        public float thumbKnuckleDist;
    }

    public struct FrameDiag
    {
        public HandDiag left;
        public HandDiag right;
        public float thumbOppositeDot;
        public bool thumbOppositeOk;
        public float bookSep;
        public bool bookSepOk;
        public bool baseOk;
        public bool enterOk;
        public bool stickyArmed;
        public bool bothThumbsClosed;
        public bool pulse;
        public string failReason;
    }

    public FrameDiag Last { get; private set; }
    public bool StickyArmed => _stickyArmed;
    public bool PulseThisFrame { get; private set; }

    public void Configure(ThreeOSFinalDemoStage demoStage) => stage = demoStage;

    void Update()
    {
        PulseThisFrame = false;
        if (!gestureEnabled)
        {
            _stickyArmed = false;
            Last = default;
            if (showFps || showHud)
                UpdateHud(default);
            return;
        }

        var diag = Evaluate();
        ApplyStickyAndPulse(ref diag);
        Last = diag;

        if (diag.baseOk != _prevBase || diag.enterOk != _prevEnter || diag.stickyArmed != _prevArmed ||
            diag.pulse != _prevPulse)
        {
            Debug.Log(
                $"[3OS][OpenBook] base={(diag.baseOk ? 1 : 0)} enter={(diag.enterOk ? 1 : 0)} " +
                $"armed={(diag.stickyArmed ? 1 : 0)} pulse={(diag.pulse ? 1 : 0)} fail={diag.failReason} " +
                $"Lface={diag.left.palmFaceDot:F2} Rface={diag.right.palmFaceDot:F2} " +
                $"LfPar={diag.left.fingerParallelMin:F2} RfPar={diag.right.fingerParallelMin:F2} " +
                $"Ltip={diag.left.tipClusterMax:F3} Rtip={diag.right.tipClusterMax:F3} " +
                $"thOpp={diag.thumbOppositeDot:F2}");
            _prevBase = diag.baseOk;
            _prevEnter = diag.enterOk;
            _prevArmed = diag.stickyArmed;
            _prevPulse = diag.pulse;
        }

        if (showFps || showHud)
            UpdateHud(diag);
    }

    void ApplyStickyAndPulse(ref FrameDiag diag)
    {
        // ENTER: base + thumbs ortho (+ soft opposite). MAINTAIN/CANCEL: base only —
        // thumbs leaving ortho must NEVER disarm (needed so thumbs can close to confirm).
        _leftThumbClosed = ThumbClosed(diag.left.thumbKnuckleDist, _leftThumbClosed);
        _rightThumbClosed = ThumbClosed(diag.right.thumbKnuckleDist, _rightThumbClosed);
        diag.bothThumbsClosed = _leftThumbClosed && _rightThumbClosed;

        var wasArmed = _stickyArmed;

        // Maintain/cancel = baseOk only (palm face, finger parallel, finger⊥palm, tip cluster).
        // Thumbs are enter/confirm only — never block base cancel (a wide thumb-near-knuckle
        // hold previously zeroed fail frames and made the gesture uncancellable).
        if (diag.enterOk)
        {
            _stickyArmed = true;
            _baseFailFrames = 0;
        }
        else if (!diag.baseOk)
        {
            _baseFailFrames++;
            if (_baseFailFrames >= Mathf.Max(1, baseFailFramesToCancel))
                _stickyArmed = false;
        }
        else
        {
            _baseFailFrames = 0;
        }

        diag.stickyArmed = _stickyArmed;
        if (_stickyArmed && !diag.enterOk)
            diag.failReason = "armed_hold";

        var canConfirm = _stickyArmed || wasArmed;
        if (!canConfirm)
        {
            _thumbLatch = false;
        }
        else if (_thumbLatch)
        {
            if (!diag.bothThumbsClosed)
                _thumbLatch = false;
        }
        else if (diag.bothThumbsClosed)
        {
            _thumbLatch = true;
            diag.pulse = true;
            PulseThisFrame = true;
            _stickyArmed = true;
            _baseFailFrames = 0;
            diag.stickyArmed = true;
        }
    }

    bool ThumbClosed(float dist, bool was)
    {
        if (float.IsNaN(dist) || dist < 0f)
            return false;
        return was ? dist <= thumbCloseExitM : dist <= thumbCloseEnterM;
    }

    FrameDiag Evaluate()
    {
        var d = new FrameDiag { failReason = "ok" };
        if (!EnsureHands())
        {
            d.failReason = "no_hands_subsys";
            return d;
        }

        _hands.TryUpdateHands(XRHandSubsystem.UpdateType.Dynamic);
        var head = ResolveHead();
        d.left = EvalHand(_hands.leftHand, head);
        d.right = EvalHand(_hands.rightHand, head);

        if (!d.left.tracked || !d.right.tracked)
        {
            d.failReason = !d.left.tracked && !d.right.tracked ? "hands_lost"
                : (!d.left.tracked ? "left_lost" : "right_lost");
            return d;
        }

        d.bookSep = Vector3.Distance(d.left.palmPos, d.right.palmPos);
        d.bookSepOk = d.bookSep >= bookSepMinM && d.bookSep <= bookSepMaxM &&
                      Mathf.Abs(d.left.palmPos.y - d.right.palmPos.y) <= bookHeightDeltaMaxM;

        d.thumbOppositeDot = Vector3.Dot(d.left.thumbDir, d.right.thumbDir);
        // Enter gate: thumbs ortho (required). Opposite is soft assist only.
        var thumbsOrthoBoth = d.left.thumbOrthoOk && d.right.thumbOrthoOk;
        d.thumbOppositeOk = thumbsOrthoBoth && d.thumbOppositeDot <= thumbOppositeDotMax;

        // ENTER framing includes book sep; MAINTAIN does not (closing thumbs moves palms).
        var enterFrameOk = d.left.palmFaceOk && d.right.palmFaceOk &&
                           d.left.fingerParallelOk && d.right.fingerParallelOk &&
                           d.left.fingerPalmOrthoOk && d.right.fingerPalmOrthoOk &&
                           d.left.tipClusterOk && d.right.tipClusterOk &&
                           d.bookSepOk;

        // Continuous cancel — NO thumb rules, NO bookSep (thumb close shifts palms).
        d.baseOk = d.left.palmFaceOk && d.right.palmFaceOk &&
                   d.left.fingerParallelOk && d.right.fingerParallelOk &&
                   d.left.fingerPalmOrthoOk && d.right.fingerPalmOrthoOk &&
                   d.left.tipClusterOk && d.right.tipClusterOk;

        d.enterOk = enterFrameOk && thumbsOrthoBoth && d.thumbOppositeDot <= thumbOppositeDotMax;

        if (!d.baseOk)
        {
            if (!d.left.palmFaceOk || !d.right.palmFaceOk) d.failReason = "palm_face";
            else if (!d.left.fingerParallelOk || !d.right.fingerParallelOk) d.failReason = "finger_parallel";
            else if (!d.left.fingerPalmOrthoOk || !d.right.fingerPalmOrthoOk) d.failReason = "finger_palm_ortho";
            else if (!d.left.tipClusterOk || !d.right.tipClusterOk) d.failReason = "tip_splay";
            else d.failReason = "base";
        }
        else if (!d.enterOk)
        {
            d.failReason = !enterFrameOk ? "enter_frame" :
                (!thumbsOrthoBoth ? "thumbs_ortho_enter" : "thumbs_opp_enter");
        }

        return d;
    }

    HandDiag EvalHand(XRHand hand, Pose head)
    {
        var h = new HandDiag { thumbKnuckleDist = float.NaN };
        if (!hand.isTracked)
            return h;
        h.tracked = true;

        if (!TryJoint(hand, XRHandJointID.Palm, out var palmPose)
            && !TryJoint(hand, XRHandJointID.Wrist, out palmPose))
            return h;

        h.palmPos = TrackingToWorld(palmPose.position);
        // Meta: palm +Y is dorsal → negate for palmar normal (same as InputRouter).
        h.palmNormal = -(TrackingToWorldRot(palmPose.rotation) * Vector3.up);
        if (h.palmNormal.sqrMagnitude < 1e-8f)
            return h;
        h.palmNormal.Normalize();

        var toHead = head.position - h.palmPos;
        if (toHead.sqrMagnitude < 1e-8f)
            return h;
        toHead.Normalize();
        h.palmFaceDot = Vector3.Dot(h.palmNormal, toHead);
        h.palmFaceOk = h.palmFaceDot >= palmFaceDotMin;

        if (!TryFingerDir(hand, XRHandJointID.IndexProximal, XRHandJointID.IndexTip, out var iDir, out var iTip)
            || !TryFingerDir(hand, XRHandJointID.MiddleProximal, XRHandJointID.MiddleTip, out var mDir, out var mTip)
            || !TryFingerDir(hand, XRHandJointID.RingProximal, XRHandJointID.RingTip, out var rDir, out var rTip))
            return h;

        var pIm = Vector3.Dot(iDir, mDir);
        var pMr = Vector3.Dot(mDir, rDir);
        var pIr = Vector3.Dot(iDir, rDir);
        h.fingerParallelMin = Mathf.Min(pIm, Mathf.Min(pMr, pIr));
        h.fingerParallelOk = h.fingerParallelMin >= fingerParallelDotMin;

        var oI = Mathf.Abs(Vector3.Dot(iDir, h.palmNormal));
        var oM = Mathf.Abs(Vector3.Dot(mDir, h.palmNormal));
        var oR = Mathf.Abs(Vector3.Dot(rDir, h.palmNormal));
        h.fingerPalmOrthoMaxAbs = Mathf.Max(oI, Mathf.Max(oM, oR));
        h.fingerPalmOrthoOk = h.fingerPalmOrthoMaxAbs <= fingerPalmOrthoAbsMax;

        h.tipClusterMax = Mathf.Max(Vector3.Distance(iTip, mTip),
            Mathf.Max(Vector3.Distance(mTip, rTip), Vector3.Distance(iTip, rTip)));
        h.tipClusterOk = h.tipClusterMax <= tipClusterMaxMeters;

        h.fingerMean = (iDir + mDir + rDir).normalized;

        if (TryFingerDir(hand, XRHandJointID.ThumbProximal, XRHandJointID.ThumbTip, out var tDir, out _)
            || TryFingerDir(hand, XRHandJointID.ThumbMetacarpal, XRHandJointID.ThumbTip, out tDir, out _))
        {
            h.thumbDir = tDir;
            h.thumbPalmOrthoAbs = Mathf.Abs(Vector3.Dot(h.thumbDir, h.palmNormal));
            h.thumbFingerOrthoAbs = Mathf.Abs(Vector3.Dot(h.thumbDir, h.fingerMean));
            h.thumbOrthoOk = h.thumbPalmOrthoAbs <= thumbPalmOrthoAbsMax &&
                             h.thumbFingerOrthoAbs <= thumbFingerOrthoAbsMax;
        }

        if (TryJoint(hand, XRHandJointID.IndexProximal, out var knuckle)
            && TryJoint(hand, XRHandJointID.ThumbTip, out var thumbTip))
        {
            h.thumbKnuckleDist = Vector3.Distance(TrackingToWorld(thumbTip.position),
                TrackingToWorld(knuckle.position));
        }

        return h;
    }

    bool TryFingerDir(XRHand hand, XRHandJointID proximal, XRHandJointID tip, out Vector3 dir,
        out Vector3 tipWorld)
    {
        dir = default;
        tipWorld = default;
        if (!TryJoint(hand, proximal, out var p) || !TryJoint(hand, tip, out var t))
            return false;
        var pw = TrackingToWorld(p.position);
        tipWorld = TrackingToWorld(t.position);
        dir = tipWorld - pw;
        if (dir.sqrMagnitude < 1e-8f)
            return false;
        dir.Normalize();
        return true;
    }

    static bool TryJoint(XRHand hand, XRHandJointID id, out Pose pose) =>
        hand.GetJoint(id).TryGetPose(out pose);

    Pose ResolveHead()
    {
        var cam = Camera.main;
        if (cam != null)
            return new Pose(cam.transform.position, cam.transform.rotation);
        if (stage != null && stage.XrOrigin != null)
        {
            var t = stage.XrOrigin;
            return new Pose(t.position + Vector3.up * 1.6f, t.rotation);
        }

        return new Pose(Vector3.up * 1.6f, Quaternion.identity);
    }

    Vector3 TrackingToWorld(Vector3 trackingPos)
    {
        var space = Playspace();
        return space != null ? space.TransformPoint(trackingPos) : trackingPos;
    }

    Quaternion TrackingToWorldRot(Quaternion trackingRot)
    {
        var space = Playspace();
        return space != null ? space.rotation * trackingRot : trackingRot;
    }

    Transform Playspace()
    {
        if (stage != null && stage.XrOrigin != null)
            return stage.XrOrigin;
        var cam = Camera.main;
        if (cam == null)
            return null;
        var t = cam.transform;
        while (t.parent != null)
        {
            t = t.parent;
            if (t.name.IndexOf("XR Origin", System.StringComparison.OrdinalIgnoreCase) >= 0)
                return t;
        }

        return cam.transform.root;
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

    void UpdateHud(FrameDiag d)
    {
        EnsureHud();
        if (_hud == null)
            return;

        var dt = Time.unscaledDeltaTime;
        if (dt > 1e-4f)
        {
            var instFps = 1f / dt;
            _fpsSmooth = Mathf.Lerp(_fpsSmooth, instFps, 1f - Mathf.Exp(-dt * 4f));
            _msSmooth = Mathf.Lerp(_msSmooth, dt * 1000f, 1f - Mathf.Exp(-dt * 4f));
        }

        var sb = new StringBuilder(showHud ? 512 : 64);
        sb.Append("FPS ").Append(_fpsSmooth.ToString("F0"))
            .Append("  ").Append(_msSmooth.ToString("F1")).Append("ms");
        if (!showHud)
        {
            // Throttle TextMesh rebuilds so the meter itself isn't the cost.
            if (Time.unscaledTime >= _fpsHudNextAt)
            {
                _fpsHudNextAt = Time.unscaledTime + 0.25f;
                _hud.text = sb.ToString();
                _hud.color = Color.white;
            }
            return;
        }

        sb.Append("  raw ").Append((dt * 1000f).ToString("F1")).Append("ms")
            .Append("  tgt ").Append(Application.targetFrameRate);
        sb.AppendLine();
        sb.AppendLine("OpenBook 7.7");
        sb.Append("ARMED ").Append(Yn(d.stickyArmed));
        sb.Append("  ENTER ").Append(Yn(d.enterOk));
        sb.Append("  BASE ").Append(Yn(d.baseOk));
        sb.Append("  PULSE ").Append(Yn(d.pulse));
        sb.AppendLine();
        sb.Append("fail=").Append(d.failReason ?? "");
        sb.Append("  baseFailFr=").AppendLine(_baseFailFrames.ToString());
        AppendHand(sb, "L", d.left);
        AppendHand(sb, "R", d.right);
        sb.Append("thOrtho enter-only L").Append(Yn(d.left.thumbOrthoOk))
            .Append(" R").Append(Yn(d.right.thumbOrthoOk));
        sb.Append("  thOpp ").Append(d.thumbOppositeDot.ToString("F2"))
            .AppendLine(" (enter soft)");
        sb.Append("bookSep ").Append(Yn(d.bookSepOk)).Append(' ')
            .Append(d.bookSep.ToString("F2")).Append("m");
        sb.Append("  thumbsClosed ").Append(Yn(d.bothThumbsClosed));
        sb.Append(" L").Append(d.left.thumbKnuckleDist.ToString("F3"))
            .Append(" R").AppendLine(d.right.thumbKnuckleDist.ToString("F3"));
        sb.AppendLine("maintain=BASE only (thumbs ignored after arm)");
        _hud.text = sb.ToString();
        _hud.color = d.stickyArmed ? new Color(0.45f, 0.95f, 0.55f) :
            (d.baseOk ? new Color(0.95f, 0.9f, 0.4f) : Color.white);
    }

    void AppendHand(StringBuilder sb, string tag, HandDiag h)
    {
        if (!h.tracked)
        {
            sb.Append(tag).AppendLine(" LOST");
            return;
        }

        sb.Append(tag).Append(" face ").Append(Yn(h.palmFaceOk)).Append(' ')
            .Append(h.palmFaceDot.ToString("F2"));
        sb.Append("  par ").Append(Yn(h.fingerParallelOk)).Append(' ')
            .Append(h.fingerParallelMin.ToString("F2"));
        sb.Append("  f⊥p ").Append(Yn(h.fingerPalmOrthoOk)).Append(' ')
            .Append(h.fingerPalmOrthoMaxAbs.ToString("F2"));
        sb.AppendLine();
        sb.Append(tag).Append(" tip ").Append(Yn(h.tipClusterOk)).Append(' ')
            .Append(h.tipClusterMax.ToString("F3")).Append("m");
        sb.Append("  th⊥ ").Append(Yn(h.thumbOrthoOk))
            .Append(" p").Append(h.thumbPalmOrthoAbs.ToString("F2"))
            .Append(" f").Append(h.thumbFingerOrthoAbs.ToString("F2"));
        sb.AppendLine();
    }

    static string Yn(bool v) => v ? "OK" : "--";

    void EnsureHud()
    {
        if (_hud != null)
        {
            if (_hudFollow == null && Camera.main != null)
            {
                _hudFollow = Camera.main.transform;
                _hud.transform.SetParent(_hudFollow, false);
                _hud.transform.localPosition = new Vector3(0.05f, 0.12f, 1.15f);
                _hud.transform.localRotation = Quaternion.identity;
            }

            return;
        }

        var go = new GameObject("OpenBook_HUD");
        var cam = Camera.main;
        if (cam != null)
        {
            _hudFollow = cam.transform;
            go.transform.SetParent(_hudFollow, false);
        }

        go.transform.localPosition = new Vector3(0.05f, 0.12f, 1.15f);
        go.transform.localRotation = Quaternion.identity;
        go.transform.localScale = Vector3.one;
        _hud = go.AddComponent<TextMesh>();
        _hud.fontSize = 36;
        _hud.characterSize = 0.008f;
        _hud.anchor = TextAnchor.UpperLeft;
        _hud.alignment = TextAlignment.Left;
        _hud.color = Color.white;
        _hud.text = "OpenBook…";
        // Draw after Palace sky overlay (rq 2400) so HUD stays readable in VR mode.
        var hudRend = go.GetComponent<MeshRenderer>();
        if (hudRend != null)
        {
            var fontMat = hudRend.material; // instance
            if (fontMat != null)
                fontMat.renderQueue = ThreeOSFinalDemoPalaceLayering.KeepRenderQueue;
        }
    }
}
