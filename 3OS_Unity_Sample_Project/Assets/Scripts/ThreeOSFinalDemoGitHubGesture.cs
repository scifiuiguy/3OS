using System.Collections.Generic;
using UnityEngine;
using UnityEngine.XR.Hands;

/// <summary>
/// Phase 0.7 Task 7.2 — right palm-up + right secondary (middle) pinch toggles the GitHub plane.
/// Raw XR Hand joint tips only (same approach as the hands-test detector).
/// </summary>
public sealed class ThreeOSFinalDemoGitHubGesture : MonoBehaviour
{
    [SerializeField] ThreeOSFinalDemoGitHubPlane plane;
    [SerializeField] float pinchEnterMeters = 0.04f;
    [SerializeField] float pinchExitMeters = 0.055f;
    [SerializeField] float palmUpDot = 0.25f;
    [SerializeField] float edgeCooldownSec = 0.45f;

    readonly List<XRHandSubsystem> _subsystems = new List<XRHandSubsystem>();
    XRHandSubsystem _subsystem;
    float _cooldownUntil;
    bool _prevActive;
    bool _rightPinching;

    public void Configure(ThreeOSFinalDemoGitHubPlane githubPlane)
    {
        plane = githubPlane;
    }

    void Update()
    {
        if (plane == null)
            return;
        if (!EnsureSubsystem())
            return;

        _subsystem.TryUpdateHands(XRHandSubsystem.UpdateType.Dynamic);
        var active = IsRightPalmUpSecondaryPinch();
        if (Time.unscaledTime < _cooldownUntil)
        {
            _prevActive = active;
            return;
        }

        if (active && !_prevActive)
        {
            plane.ToggleVisible();
            _cooldownUntil = Time.unscaledTime + edgeCooldownSec;
        }

        _prevActive = active;
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

    bool IsRightPalmUpSecondaryPinch()
    {
        var hand = _subsystem.rightHand;
        if (!hand.isTracked)
            return false;

        if (!TryGetPose(hand, XRHandJointID.Palm, out var palm)
            && !TryGetPose(hand, XRHandJointID.Wrist, out palm))
            return false;

        if (!IsPalmFacingUp(palm.rotation))
            return false;

        if (!TryGetPose(hand, XRHandJointID.ThumbTip, out var thumb)
            || !TryGetPose(hand, XRHandJointID.MiddleTip, out var middle))
            return false;

        var dist = Vector3.Distance(thumb.position, middle.position);
        if (_rightPinching)
            _rightPinching = dist <= pinchExitMeters;
        else
            _rightPinching = dist <= pinchEnterMeters;
        return _rightPinching;
    }

    static bool TryGetPose(XRHand hand, XRHandJointID id, out Pose pose)
    {
        return hand.GetJoint(id).TryGetPose(out pose);
    }

    bool IsPalmFacingUp(Quaternion palmRot)
    {
        var a = Vector3.Dot(palmRot * Vector3.forward, Vector3.up);
        var b = Vector3.Dot(palmRot * -Vector3.up, Vector3.up);
        var c = Vector3.Dot(palmRot * Vector3.up, Vector3.up);
        return Mathf.Max(a, b, c) >= palmUpDot;
    }
}
