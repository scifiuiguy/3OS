using UnityEngine;

/// <summary>FPS A/B — logs smoothed framerate once per second (adb logcat).</summary>
public sealed class ThreeOSFinalDemoFpsLogger : MonoBehaviour
{
    float _fpsSmooth = 72f;
    float _nextLogAt;

    void Update()
    {
        var dt = Time.unscaledDeltaTime;
        if (dt > 1e-4f)
            _fpsSmooth = Mathf.Lerp(_fpsSmooth, 1f / dt, 1f - Mathf.Exp(-dt * 4f));

        if (Time.unscaledTime < _nextLogAt)
            return;

        _nextLogAt = Time.unscaledTime + 1f;
        Debug.Log($"[3OS][FPS] {_fpsSmooth:F1}  rawMs={(dt * 1000f):F1}");
    }
}
