using System.Collections.Generic;
using UnityEngine;
using UnityEngine.XR.Hands;

/// <summary>
/// Both palms sideways + both ternary (thumb–ring) pinches toggles GitHub plane visibility.
/// </summary>
public sealed class ThreeOSFinalDemoGitHubGesture : MonoBehaviour
{
    [SerializeField] ThreeOSFinalDemoGitHubPlane plane;
    // Not serialized — inserting a field before pinch* broke scene layout vs player (build 398).
    ThreeOSFinalDemoStage _stage;
    [SerializeField] float pinchEnterMeters = 0.04f;
    [SerializeField] float pinchExitMeters = 0.055f;
    [Tooltip("Max |palmarNormal·up| — palms must be sideways, not palm-up/down.")]
    [SerializeField] float palmSidewaysUpAbsMax = 0.38f;
    [SerializeField] float edgeCooldownSec = 0.45f;
    [SerializeField] float startupMuteSec = 1.75f;
    [SerializeField] float trackingEngagePadSec = 0.4f;

    readonly List<XRHandSubsystem> _subsystems = new List<XRHandSubsystem>();
    XRHandSubsystem _subsystem;
    float _cooldownUntil;
    float _startupMuteUntil;
    float _engagePadUntil = -1f;
    bool _prevActive;
    bool _leftTernaryPinching;
    bool _rightTernaryPinching;
    bool _leftWasTracked;
    bool _rightWasTracked;
    bool _forcedStartVisible;

    public void Configure(ThreeOSFinalDemoGitHubPlane githubPlane) => plane = githubPlane;

    void Awake()
    {
        if (plane == null)
            plane = FindFirstObjectByType<ThreeOSFinalDemoGitHubPlane>(FindObjectsInactive.Include);
        if (_stage == null)
            _stage = FindFirstObjectByType<ThreeOSFinalDemoStage>();
    }

    void OnEnable()
    {
        _startupMuteUntil = Time.unscaledTime + startupMuteSec;
        _prevActive = false;
        _leftTernaryPinching = false;
        _rightTernaryPinching = false;
        _forcedStartVisible = false;
        plane?.SetRendererVisible(true);
    }

    void Update()
    {
        if (plane == null)
            return;

        if (IsMindPalaceActive() || plane.MindPalaceSuppressed)
        {
            _prevActive = false;
            _leftTernaryPinching = false;
            _rightTernaryPinching = false;
            return;
        }

        if (!_forcedStartVisible && Time.unscaledTime < _startupMuteUntil)
            plane.SetRendererVisible(true);
        else if (!_forcedStartVisible)
        {
            _forcedStartVisible = true;
            plane.SetRendererVisible(true);
        }

        if (!EnsureSubsystem())
            return;

        _subsystem.TryUpdateHands(XRHandSubsystem.UpdateType.Dynamic);
        var leftTracked = _subsystem.leftHand.isTracked;
        var rightTracked = _subsystem.rightHand.isTracked;
        if ((leftTracked && !_leftWasTracked) || (rightTracked && !_rightWasTracked))
            _engagePadUntil = Time.unscaledTime + trackingEngagePadSec;
        if (!leftTracked && !rightTracked)
            _engagePadUntil = -1f;
        _leftWasTracked = leftTracked;
        _rightWasTracked = rightTracked;

        if (Time.unscaledTime < _startupMuteUntil ||
            (_engagePadUntil > 0f && Time.unscaledTime < _engagePadUntil))
        {
            _prevActive = false;
            _leftTernaryPinching = false;
            _rightTernaryPinching = false;
            return;
        }

        if (IsLeftPrimaryPinchHeld())
        {
            _prevActive = false;
            _leftTernaryPinching = false;
            _rightTernaryPinching = false;
            return;
        }

        var active = IsDualPalmsSidewaysTernaryPinch();
        if (Time.unscaledTime < _cooldownUntil)
        {
            _prevActive = active;
            return;
        }

        if (active && !_prevActive)
        {
            plane.ToggleRendererVisible();
            _cooldownUntil = Time.unscaledTime + edgeCooldownSec;
        }

        _prevActive = active;
    }

    bool IsMindPalaceActive()
    {
        if (_stage == null)
            _stage = FindFirstObjectByType<ThreeOSFinalDemoStage>();
        return _stage != null && _stage.InPalace;
    }

    bool EnsureSubsystem()
    {
        if (_subsystem != null && _subsystem.running)
            return true;
        SubsystemManager.GetSubsystems(_subsystems);
        _subsystem = null;
        for (var i = 0; i < _subsystems.Count; i++)
        {
            if (_subsystems[i].running)
            {
                _subsystem = _subsystems[i];
                break;
            }
        }

        return _subsystem != null;
    }

    bool IsLeftPrimaryPinchHeld()
    {
        var hand = _subsystem.leftHand;
        if (!hand.isTracked)
            return false;
        if (!TryGetPose(hand, XRHandJointID.ThumbTip, out var thumb)
            || !TryGetPose(hand, XRHandJointID.IndexTip, out var index))
            return false;
        return Vector3.Distance(thumb.position, index.position) <= pinchEnterMeters;
    }

    bool IsDualPalmsSidewaysTernaryPinch()
    {
        var leftOk = IsHandSidewaysTernaryPinch(_subsystem.leftHand, true);
        var rightOk = IsHandSidewaysTernaryPinch(_subsystem.rightHand, false);
        return leftOk && rightOk;
    }

    bool IsHandSidewaysTernaryPinch(XRHand hand, bool left)
    {
        if (!hand.isTracked)
            return false;

        if (!TryGetPose(hand, XRHandJointID.Palm, out var palm)
            && !TryGetPose(hand, XRHandJointID.Wrist, out palm))
            return false;

        if (!IsPalmSideways(palm.rotation))
            return false;

        if (!TryGetPose(hand, XRHandJointID.ThumbTip, out var thumb)
            || !TryGetPose(hand, XRHandJointID.RingTip, out var ring))
            return false;

        var dist = Vector3.Distance(thumb.position, ring.position);
        ref var pinching = ref left ? ref _leftTernaryPinching : ref _rightTernaryPinching;
        if (pinching)
            pinching = dist <= pinchExitMeters;
        else
            pinching = dist <= pinchEnterMeters;
        return pinching;
    }

    static bool TryGetPose(XRHand hand, XRHandJointID id, out Pose pose)
    {
        return hand.GetJoint(id).TryGetPose(out pose);
    }

    static Vector3 PalmarNormal(Quaternion palmRot)
    {
        // Match OpenBook / Meta: dorsal ≈ +Y → palmar ≈ −Y in palm joint space.
        var n = palmRot * -Vector3.up;
        if (n.sqrMagnitude < 1e-8f)
            return Vector3.forward;
        return n.normalized;
    }

    bool IsPalmSideways(Quaternion palmRot)
    {
        var n = PalmarNormal(palmRot);
        return Mathf.Abs(Vector3.Dot(n, Vector3.up)) <= palmSidewaysUpAbsMax;
    }
}
