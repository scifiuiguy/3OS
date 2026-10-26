using Unity.Profiling;
using UnityEngine;
using UnityEngine.Profiling;

/// <summary>FPS A/B — logs smoothed framerate, worst frame, and sub-60 frame count once per second.</summary>
public sealed class ThreeOSFinalDemoFpsLogger : MonoBehaviour
{
    const float HitchMs = 1000f / 60f;

    float _fpsSmooth = 72f;
    float _nextLogAt;
    float _worstMs;
    float _worstAt;
    int _worstFrame;
    int _hitchCount;

    readonly FrameTiming[] _timing = new FrameTiming[1];
    readonly System.Collections.Generic.List<ProfilerRecorder> _recorders = new();
    readonly System.Collections.Generic.List<string> _recorderNames = new();
    int _lastGc;
    long _lastAlloc;
    int _detailLogged;
    float _detailWindowStart;

    static readonly (ProfilerCategory cat, string name)[] Markers =
    {
        (ProfilerCategory.Scripts, "Update.ScriptRunBehaviourUpdate"),
        (ProfilerCategory.Scripts, "PreLateUpdate.ScriptRunBehaviourLateUpdate"),
        (ProfilerCategory.Scripts, "Update.ScriptRunDelayedDynamicFrameRate"),
        (ProfilerCategory.Internal, "Update.ScriptRunBehaviourUpdate"),
        (ProfilerCategory.Physics, "Physics.Simulate"),
        (ProfilerCategory.Internal, "FixedUpdate.PhysicsFixedUpdate"),
        (ProfilerCategory.Gui, "PostLateUpdate.PlayerUpdateCanvases"),
        (ProfilerCategory.Render, "Camera.Render"),
        (ProfilerCategory.Memory, "GC.Collect"),
        (ProfilerCategory.Animation, "PreLateUpdate.DirectorUpdateAnimationBegin"),
    };

    void OnEnable()
    {
        foreach (var (cat, name) in Markers)
        {
            _recorders.Add(ProfilerRecorder.StartNew(cat, name, 1));
            _recorderNames.Add(name);
        }
        _lastGc = System.GC.CollectionCount(0);
        _lastAlloc = Profiler.GetMonoUsedSizeLong();
    }

    void OnDisable()
    {
        foreach (var r in _recorders) r.Dispose();
        _recorders.Clear();
        _recorderNames.Clear();
    }

    // Capped to a few lines per second so logging itself does not distort frame times.
    void LogHitchDetail(float ms)
    {
        if (Time.unscaledTime - _detailWindowStart > 1f)
        {
            _detailWindowStart = Time.unscaledTime;
            _detailLogged = 0;
        }
        if (_detailLogged++ >= 3) return;

        FrameTimingManager.CaptureFrameTimings();
        var n = FrameTimingManager.GetLatestTimings(1, _timing);
        var t = n > 0 ? _timing[0] : default;
        var gc = System.GC.CollectionCount(0);
        var mono = Profiler.GetMonoUsedSizeLong();
        var sb = new System.Text.StringBuilder(256);
        sb.Append($"[3OS][HITCHDETAIL] f={Time.frameCount} ms={ms:F1} cpuFrame={t.cpuFrameTime:F1} " +
                  $"cpuMain={t.cpuMainThreadFrameTime:F1} cpuRender={t.cpuRenderThreadFrameTime:F1} " +
                  $"gpu={t.gpuFrameTime:F1} gcDelta={gc - _lastGc} monoDeltaKB={(mono - _lastAlloc) / 1024}");
        for (var i = 0; i < _recorders.Count; i++)
        {
            var r = _recorders[i];
            if (r.Valid && r.LastValue > 0)
                sb.Append($" {_recorderNames[i]}={r.LastValue / 1e6:F1}ms");
        }
        Debug.Log(sb.ToString());
        _lastGc = gc;
        _lastAlloc = mono;
    }

    void Update()
    {
        var dt = Time.unscaledDeltaTime;
        if (dt > 1e-4f)
            _fpsSmooth = Mathf.Lerp(_fpsSmooth, 1f / dt, 1f - Mathf.Exp(-dt * 4f));

        var ms = dt * 1000f;
        if (ms > _worstMs)
        {
            _worstMs = ms;
            _worstAt = Time.realtimeSinceStartup;
            _worstFrame = Time.frameCount;
        }
        if (ms > HitchMs)
        {
            _hitchCount++;
            LogHitchDetail(ms);
        }

        if (Time.unscaledTime < _nextLogAt)
            return;

        _nextLogAt = Time.unscaledTime + 1f;
        Debug.Log($"[3OS][FPS] {_fpsSmooth:F1} rawMs={ms:F1} worstMs={_worstMs:F1} " +
                  $"worstFrame={_worstFrame} worstAt={_worstAt:F2} hitches={_hitchCount}");
        _worstMs = 0f;
        _worstFrame = 0;
        _hitchCount = 0;
    }
}
