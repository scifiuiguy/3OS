using System.Collections.Generic;
using UnityEngine;
using UnityEngine.XR.Hands;

/// <summary>
/// Act I reset: both palms down + dual ternary (ring) pinch → restore agent / NPC for reel restart.
/// </summary>
public sealed class ThreeOSFinalDemoAgentResetGesture : MonoBehaviour
{
    [SerializeField] float pinchEnterMeters = 0.04f;
    [SerializeField] float pinchExitMeters = 0.055f;
    [SerializeField] float palmDownDot = 0.25f;
    [SerializeField] float edgeCooldownSec = 0.6f;

    readonly List<XRHandSubsystem> _subsystems = new List<XRHandSubsystem>();
    XRHandSubsystem _subsystem;
    float _cooldownUntil;
    bool _prevActive;
    bool _leftPinching;
    bool _rightPinching;

    public System.Action OnReset;

    void Update()
    {
        if (!EnsureSubsystem())
            return;

        _subsystem.TryUpdateHands(XRHandSubsystem.UpdateType.Dynamic);
        var active = IsDualPalmDownTernaryPinch();
        if (Time.unscaledTime < _cooldownUntil)
        {
            _prevActive = active;
            return;
        }

        if (active && !_prevActive)
        {
            Debug.Log("[3OS][7.4] Act I reset gesture (palms down + dual ternary)");
            OnReset?.Invoke();
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

    bool IsDualPalmDownTernaryPinch()
    {
        var left = IsPalmDownTernary(_subsystem.leftHand, ref _leftPinching);
        var right = IsPalmDownTernary(_subsystem.rightHand, ref _rightPinching);
        return left && right;
    }

    bool IsPalmDownTernary(XRHand hand, ref bool latch)
    {
        if (!hand.isTracked)
            return false;
        if (!TryGetPose(hand, XRHandJointID.Palm, out var palm)
            && !TryGetPose(hand, XRHandJointID.Wrist, out palm))
            return false;
        if (!IsPalmFacingDown(palm.rotation))
            return false;
        if (!TryGetPose(hand, XRHandJointID.ThumbTip, out var thumb)
            || !TryGetPose(hand, XRHandJointID.RingTip, out var ring))
            return false;

        var dist = Vector3.Distance(thumb.position, ring.position);
        if (latch)
            latch = dist <= pinchExitMeters;
        else
            latch = dist <= pinchEnterMeters;
        return latch;
    }

    bool IsPalmFacingDown(Quaternion palmRot)
    {
        // Inverse of palm-up checks used elsewhere.
        var a = Vector3.Dot(palmRot * Vector3.forward, Vector3.down);
        var b = Vector3.Dot(palmRot * -Vector3.up, Vector3.down);
        var c = Vector3.Dot(palmRot * Vector3.up, Vector3.down);
        return Mathf.Max(a, b, c) >= palmDownDot;
    }

    static bool TryGetPose(XRHand hand, XRHandJointID id, out Pose pose) =>
        hand.GetJoint(id).TryGetPose(out pose);
}
