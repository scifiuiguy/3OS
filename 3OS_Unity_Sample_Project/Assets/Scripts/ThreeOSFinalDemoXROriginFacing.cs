using System;
using System.Collections;
using UnityEngine;
using UnityEngine.XR;
using UnityEngine.XR.Management;
using Unity.XR.CoreUtils;

/// <summary>
/// Align view to authored spawn forward via <see cref="XROrigin.MatchOriginUpCameraForward"/>.
/// Waits for HMD tracking + <see cref="XRInputSubsystem.trackingOriginUpdated"/>, then holds
/// boot cover until yaw is stable. When <see cref="SplashController"/> is present,
/// reorientation runs from the splash progress callback instead.
/// </summary>
[DefaultExecutionOrder(101)]
public sealed class ThreeOSFinalDemoXROriginFacing : MonoBehaviour
{
    [SerializeField] float maxWaitTrackingSec = 15f;
    [SerializeField] float maxWaitOriginUpdateSec = 8f;
    [SerializeField] float ovrRewriteGuardSec = 2.5f;
    [SerializeField] float settleAngleDeg = 1.25f;
    [SerializeField] int stableFramesRequired = 3;
    [SerializeField] float maxYawSnapDeg = 45f;
    [Tooltip("Optional fine-tune (degrees). playerSpawnYawOffsetDeg is not applied here.")]
    [SerializeField] float extraYawOffsetDeg;

    float _bootRealtime;
    int _bootFrame;
    float _ovrGuardUntilUnscaled;
    bool _primaryAlignDone;
    bool _bootCoverReleased;
    XRInputSubsystem _inputSubsystem;
    bool _trackingOriginUpdated;

    void OnEnable()
    {
        _bootRealtime = Time.realtimeSinceStartup;
        _bootFrame = Time.frameCount;
        _primaryAlignDone = false;
        _bootCoverReleased = false;
        _trackingOriginUpdated = false;
        _ovrGuardUntilUnscaled = 0f;
        if (FindFirstObjectByType<SplashController>() != null)
            return;
        StartCoroutine(AlignWhenReadyRoutine());
    }

    /// <summary>Splash progress at ~80% — full yaw/XZ align while splash plane still visible.</summary>
    public IEnumerator RunReorientationForSplashRoutine()
    {
        yield return AlignCoreRoutine(releaseBootCoverAtEnd: false);
    }

    void OnDisable()
    {
        UnhookOriginUpdated();
        ReleaseBootCoverOnce("disabled");
    }

    void LateUpdate()
    {
        if (Time.unscaledTime > _ovrGuardUntilUnscaled || !_primaryAlignDone || !_bootCoverReleased)
            return;

        ApplyPlacementFirst();
        TryMatchAuthoredForward("ovr-guard", force: false);
    }

    IEnumerator AlignWhenReadyRoutine()
    {
        yield return AlignCoreRoutine(releaseBootCoverAtEnd: true);
    }

    IEnumerator AlignCoreRoutine(bool releaseBootCoverAtEnd)
    {
        var deadline = Time.realtimeSinceStartup + maxWaitTrackingSec;

        XROrigin xrOrigin = null;
        while (xrOrigin == null || xrOrigin.Camera == null)
        {
            xrOrigin = FindXROrigin();
            if (xrOrigin != null && xrOrigin.Camera != null)
                break;
            if (Time.realtimeSinceStartup > deadline)
            {
                Debug.LogWarning("[3OS] XROriginFacing: timed out waiting for XROrigin+Camera");
                if (releaseBootCoverAtEnd)
                    ReleaseBootCoverOnce("timeout-no-xr-origin");
                yield break;
            }

            yield return null;
        }

        LogMilestone("xr-origin-camera");

        while (!HeadTrackingReady())
        {
            if (Time.realtimeSinceStartup > deadline)
            {
                Debug.LogWarning("[3OS] XROriginFacing: timed out waiting for HMD Position+Rotation tracking");
                break;
            }

            yield return null;
        }

        LogMilestone("hmd-tracking-ready");

        ApplyPlacementFirst();
        TryMatchAuthoredForward("align-tracking-ready", force: true);

        _primaryAlignDone = true;
        _ovrGuardUntilUnscaled = Time.unscaledTime + ovrRewriteGuardSec;

        HookOriginUpdated();
        var originWaitStart = Time.realtimeSinceStartup;
        while (!_trackingOriginUpdated &&
               Time.realtimeSinceStartup - originWaitStart < maxWaitOriginUpdateSec)
        {
            yield return null;
        }

        if (_trackingOriginUpdated)
        {
            LogMilestone("tracking-origin-updated");
            yield return null;
            ApplyPlacementFirst();
            TryMatchAuthoredForward("align-after-origin-update", force: true);
        }
        else
        {
            LogMilestone("no-tracking-origin-update-within-wait");
        }

        UnhookOriginUpdated();

        yield return WaitForStableYawRoutine();
        if (releaseBootCoverAtEnd)
            ReleaseBootCoverOnce("yaw-stable");
    }

    IEnumerator WaitForStableYawRoutine()
    {
        var stable = 0;
        var end = Time.realtimeSinceStartup + ovrRewriteGuardSec;
        while (Time.realtimeSinceStartup < end)
        {
            yield return new WaitForEndOfFrame();
            ApplyPlacementFirst();
            var err = PeekAngleErrorDeg();
            if (err < settleAngleDeg)
            {
                stable++;
                if (stable >= stableFramesRequired)
                {
                    LogMilestone($"yaw-stable-{stableFramesRequired}frames err={err:F2}");
                    yield break;
                }
            }
            else
            {
                stable = 0;
                if (err <= maxYawSnapDeg)
                    TryMatchAuthoredForward("stable-wait", force: true);
                else
                    Debug.LogWarning($"[3OS] XROriginFacing: defer yaw snap errDeg={err:F1} (placement-only this frame)");
            }
        }

        LogMilestone("yaw-stable-timeout");
    }

    void ReleaseBootCoverOnce(string reason)
    {
        if (_bootCoverReleased)
            return;
        _bootCoverReleased = true;
        ThreeOSFinalDemoBootCover.Release(reason);
    }

    void HookOriginUpdated()
    {
        UnhookOriginUpdated();
        _inputSubsystem = GetLoadedInputSubsystem();
        if (_inputSubsystem == null)
        {
            LogMilestone("no-xr-input-subsystem");
            return;
        }

        _inputSubsystem.trackingOriginUpdated += OnTrackingOriginUpdated;
    }

    void UnhookOriginUpdated()
    {
        if (_inputSubsystem == null)
            return;
        _inputSubsystem.trackingOriginUpdated -= OnTrackingOriginUpdated;
        _inputSubsystem = null;
    }

    void OnTrackingOriginUpdated(XRInputSubsystem subsystem)
    {
        _trackingOriginUpdated = true;
        LogMilestone("tracking-origin-updated-callback");
    }

    static void ApplyPlacementFirst()
    {
        ThreeOSFinalDemoXROriginPlacement.TryPlaceXROriginForCameraAtMarker(
            "Chair_Seated_Start", out _, out _, out _);
    }

    static XRInputSubsystem GetLoadedInputSubsystem()
    {
        var settings = XRGeneralSettings.Instance;
        if (settings == null || settings.Manager == null || settings.Manager.activeLoader == null)
            return null;
        return settings.Manager.activeLoader.GetLoadedSubsystem<XRInputSubsystem>();
    }

    static XROrigin FindXROrigin()
    {
        var originGo = GameObject.Find("XR Origin (VR)");
        return originGo != null ? originGo.GetComponent<XROrigin>() : null;
    }

    static bool HeadTrackingReady()
    {
        var device = InputDevices.GetDeviceAtXRNode(XRNode.Head);
        if (!device.isValid)
            return Camera.main != null;

        if (!device.TryGetFeatureValue(CommonUsages.trackingState, out var state))
            return device.isValid;

        return state.HasFlag(InputTrackingState.Position) && state.HasFlag(InputTrackingState.Rotation);
    }

    void LogMilestone(string name)
    {
        var dt = Time.realtimeSinceStartup - _bootRealtime;
        var df = Time.frameCount - _bootFrame;
        Debug.Log(
            $"[3OS] XROriginFacing milestone={name} dt={dt:F3}s frame+{df} (since load t={Time.realtimeSinceStartup:F3}s)");
    }

    float PeekAngleErrorDeg()
    {
        var stage = UnityEngine.Object.FindFirstObjectByType<ThreeOSFinalDemoStage>();
        var xrOrigin = FindXROrigin();
        if (stage == null || xrOrigin == null || xrOrigin.Camera == null)
            return 180f;

        var desiredForward = stage.GetAuthoredViewForward();
        if (Mathf.Abs(extraYawOffsetDeg) > 0.01f)
            desiredForward = Quaternion.Euler(0f, extraYawOffsetDeg, 0f) * desiredForward;
        desiredForward.y = 0f;
        if (desiredForward.sqrMagnitude < 1e-6f)
            desiredForward = Vector3.forward;
        else
            desiredForward.Normalize();

        var camForward = xrOrigin.Camera.transform.forward;
        camForward.y = 0f;
        if (camForward.sqrMagnitude < 1e-6f)
            return 180f;
        camForward.Normalize();
        return Vector3.Angle(camForward, desiredForward);
    }

    bool TryMatchAuthoredForward(string pass, bool force)
    {
        var stage = UnityEngine.Object.FindFirstObjectByType<ThreeOSFinalDemoStage>();
        if (stage == null)
            return false;

        var xrOrigin = FindXROrigin();
        if (xrOrigin == null || xrOrigin.Camera == null)
            return false;

        var angleErr = PeekAngleErrorDeg();
        if (!force && angleErr < settleAngleDeg)
            return false;

        if (!force && angleErr > maxYawSnapDeg)
        {
            Debug.LogWarning(
                $"[3OS] XROriginFacing {pass}: skip MatchOrigin errDeg={angleErr:F1} (> {maxYawSnapDeg})");
            return false;
        }

        var desiredForward = stage.GetAuthoredViewForward();
        if (Mathf.Abs(extraYawOffsetDeg) > 0.01f)
            desiredForward = Quaternion.Euler(0f, extraYawOffsetDeg, 0f) * desiredForward;
        desiredForward.y = 0f;
        if (desiredForward.sqrMagnitude < 1e-6f)
            desiredForward = Vector3.forward;
        else
            desiredForward.Normalize();

        var matched = xrOrigin.MatchOriginUpCameraForward(Vector3.up, desiredForward);
        var camForward = xrOrigin.Camera.transform.forward;
        Debug.Log(
            $"[3OS] XROriginFacing {pass}: matched={matched} errDeg={angleErr:F2} " +
            $"dt={Time.realtimeSinceStartup - _bootRealtime:F3}s frame+{Time.frameCount - _bootFrame} " +
            $"desired={desiredForward} camForward={camForward}");
        return matched;
    }
}
