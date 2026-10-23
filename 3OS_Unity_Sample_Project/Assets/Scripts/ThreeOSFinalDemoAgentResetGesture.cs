using System.Collections.Generic;
using ThreeOS;
using UnityEngine;
using UnityEngine.XR.Hands;

/// <summary>
/// Act I reset: both palms down + dual secondary (middle) pinch.
/// Dismiss All is the same pinch with palms up — only palm orientation differs.
/// Meta XR Hands palm.+Y is dorsal; palmar normal is negated (same as ThreeOSInputRouter).
/// </summary>
public sealed class ThreeOSFinalDemoAgentResetGesture : MonoBehaviour
{
    [SerializeField] float pinchEnterMeters = 0.04f;
    [SerializeField] float pinchExitMeters = 0.055f;
    [Tooltip("Palmar normal · world-down (kernel enter Down ≈ 0.55).")]
    [SerializeField] float palmDownDot = 0.55f;
    [Tooltip("Reject if palmar normal · world-up is this strong (Dismiss All zone).")]
    [SerializeField] float palmUpRejectDot = 0.30f;
    [SerializeField] float edgeCooldownSec = 0.6f;

    [SerializeField] ThreeOSInputRouter inputRouter;

    readonly List<XRHandSubsystem> _subsystems = new List<XRHandSubsystem>();
    XRHandSubsystem _subsystem;
    float _cooldownUntil;
    bool _prevActive;
    bool _leftPinching;
    bool _rightPinching;

    public System.Action OnReset;

    void Awake()
    {
        if (inputRouter == null)
            inputRouter = FindFirstObjectByType<ThreeOSInputRouter>();
    }

    void Update()
    {
        if (!EnsureSubsystem())
            return;

        // Kernel already classified this frame's dismiss-all — never also reset.
        if (inputRouter != null)
        {
            var flags = (InteractionIntentFlags)inputRouter.LastIntents.flags;
            if ((flags & InteractionIntentFlags.VoodooDismissAllPulse) != 0 ||
                (flags & InteractionIntentFlags.VoodooDismissPulse) != 0)
            {
                _prevActive = false;
                return;
            }
        }

        _subsystem.TryUpdateHands(XRHandSubsystem.UpdateType.Dynamic);
        var active = IsDualPalmDownSecondaryPinch();
        if (Time.unscaledTime < _cooldownUntil)
        {
            _prevActive = active;
            return;
        }

        if (active && !_prevActive)
        {
            Debug.Log("[3OS][7.4] Act I reset gesture (palms down + dual secondary)");
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

    bool IsDualPalmDownSecondaryPinch()
    {
        // Prefer kernel palm modes (same quantizer as Dismiss All) when cursors are live.
        if (inputRouter != null &&
            inputRouter.LastLeftCursor.active != 0 &&
            inputRouter.LastRightCursor.active != 0)
        {
            var leftDown = (PalmMode)inputRouter.LastLeftCursor.palmMode == PalmMode.Down;
            var rightDown = (PalmMode)inputRouter.LastRightCursor.palmMode == PalmMode.Down;
            if (!leftDown || !rightDown)
                return false;
            var left = IsSecondaryPinch(_subsystem.leftHand, ref _leftPinching);
            var right = IsSecondaryPinch(_subsystem.rightHand, ref _rightPinching);
            return left && right;
        }

        var l = IsPalmDownSecondary(_subsystem.leftHand, ref _leftPinching);
        var r = IsPalmDownSecondary(_subsystem.rightHand, ref _rightPinching);
        return l && r;
    }

    bool IsPalmDownSecondary(XRHand hand, ref bool latch)
    {
        if (!hand.isTracked)
            return false;
        if (!TryGetPose(hand, XRHandJointID.Palm, out var palm)
            && !TryGetPose(hand, XRHandJointID.Wrist, out palm))
            return false;
        if (!IsPalmFacingDown(palm.rotation))
            return false;
        return IsSecondaryPinch(hand, ref latch);
    }

    bool IsSecondaryPinch(XRHand hand, ref bool latch)
    {
        if (!hand.isTracked)
            return false;
        if (!TryGetPose(hand, XRHandJointID.ThumbTip, out var thumb)
            || !TryGetPose(hand, XRHandJointID.MiddleTip, out var middle))
            return false;

        var dist = Vector3.Distance(thumb.position, middle.position);
        if (latch)
            latch = dist <= pinchExitMeters;
        else
            latch = dist <= pinchEnterMeters;
        return latch;
    }

    bool IsPalmFacingDown(Quaternion palmRot)
    {
        // Meta XR Hands palm.+Y = dorsal (back of hand). Kernel uses palmar = -dorsal.
        var palmar = -(palmRot * Vector3.up);
        if (Vector3.Dot(palmar, Vector3.up) >= palmUpRejectDot)
            return false;
        return Vector3.Dot(palmar, Vector3.down) >= palmDownDot;
    }

    static bool TryGetPose(XRHand hand, XRHandJointID id, out Pose pose) =>
        hand.GetJoint(id).TryGetPose(out pose);
}
