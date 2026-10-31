using System.Collections;
using UnityEngine;
using UnityEngine.UIElements;

/// <summary>
/// Internal splash on Main Camera: progress bar masks XR yaw reorientation, then fades the splash plane.
/// </summary>
[DefaultExecutionOrder(100)]
public sealed class SplashController : MonoBehaviour
{
    [SerializeField] UIDocument uiDocument;
    [SerializeField] Transform splashPlane;
    [SerializeField] Material splashMaterial;
    [SerializeField] float progressToReorientSec = 1f;
    [SerializeField] float progressFinishSec = 0.5f;
    [SerializeField] float fadeOutSec = 0.5f;
    [SerializeField] float progressBeforeReorient = 80f;
    [Tooltip("80→100% begins when reorient finishes OR at this many seconds after level load, whichever is first.")]
    [SerializeField] float reorientDeadlineFromLevelStartSec = 3f;

    [Header("Deferred MR content (after reorient)")]
    [SerializeField] GameObject unrealGroupPrefab;
    [SerializeField] GameObject colleagueGroupPrefab;
    [SerializeField] string unrealSpawnPointName = "Unreal_Spawn_Point";
    [SerializeField] string colleagueSpawnPointName = "Colleague_Spawn_Point";
    [SerializeField] float postReorientSpawnDelaySec = 0.1f;
    [SerializeField] float betweenDeferredSpawnsSec = 0.1f;
    [SerializeField] float progressBumpPerDeferredSpawn = 2f;

    static readonly int BaseColorId = Shader.PropertyToID("_BaseColor");
    static readonly int ColorId = Shader.PropertyToID("_Color");

    ProgressBar _progressBar;
    Renderer _planeRenderer;
    Material _matInstance;
    Coroutine _sequence;
    Coroutine _reorientDriver;
    bool _reorientDriverRunning;
    float _progressValue;

    void Awake()
    {
        ResolveRefs();
        ShowSplashPlaneBlocking();
    }

    void OnEnable()
    {
        if (!Application.isPlaying)
            return;
        ShowSplashPlaneBlocking();
        if (_sequence != null)
            StopCoroutine(_sequence);
        _sequence = StartCoroutine(SplashSequenceRoutine());
    }

    void OnDisable()
    {
        if (_reorientDriver != null)
        {
            StopCoroutine(_reorientDriver);
            _reorientDriver = null;
            _reorientDriverRunning = false;
        }

        if (_sequence != null)
        {
            StopCoroutine(_sequence);
            _sequence = null;
        }
    }

    void ResolveRefs()
    {
        if (uiDocument == null)
            uiDocument = GetComponent<UIDocument>();

        if (splashPlane == null)
        {
            splashPlane = transform.Find("SplashPlane");
            if (splashPlane == null)
                splashPlane = transform.Find("Splash_Plane");
        }

        if (splashPlane != null)
            _planeRenderer = splashPlane.GetComponentInChildren<Renderer>(true);

        if (_planeRenderer != null)
        {
            _matInstance = splashMaterial != null
                ? Instantiate(splashMaterial)
                : _planeRenderer.material;
            _planeRenderer.material = _matInstance;
        }
        else if (splashMaterial != null)
        {
            _matInstance = Instantiate(splashMaterial);
        }
    }

    void BindUi()
    {
        if (uiDocument == null || uiDocument.rootVisualElement == null)
            return;

        var root = uiDocument.rootVisualElement;
        _progressBar = root.Q<ProgressBar>("ProgressBar") ?? root.Q<ProgressBar>();
        if (_progressBar == null)
            return;

        _progressBar.lowValue = 0f;
        _progressBar.highValue = 100f;
    }

    IEnumerator WaitForProgressBarReadyRoutine()
    {
        const float timeoutSec = 5f;
        var deadline = Time.unscaledTime + timeoutSec;
        while (Time.unscaledTime < deadline)
        {
            BindUi();
            if (_progressBar != null && _progressBar.panel != null)
                yield break;
            yield return null;
        }

        Debug.LogWarning("[3OS] SplashController: ProgressBar not ready — progress UI may be wrong");
    }

    void ShowSplashPlaneBlocking()
    {
        SetMaterialAlpha(1f);
        if (_planeRenderer != null)
            _planeRenderer.enabled = true;
    }

    IEnumerator SplashSequenceRoutine()
    {
        ShowSplashPlaneBlocking();

        yield return WaitForProgressBarReadyRoutine();
        SetProgress(0f);
        yield return FlushUiToRenderTextureRoutine();

        yield return LerpProgressRoutine(0f, progressBeforeReorient, progressToReorientSec);

        var facing = FindFirstObjectByType<ThreeOSFinalDemoXROriginFacing>();
        if (facing != null)
        {
            Debug.Log(
                $"[3OS] SplashController: at 80% levelT={Time.timeSinceLevelLoadAsDouble:F2}s " +
                $"reorientDeadline={reorientDeadlineFromLevelStartSec:F1}s");
            yield return WaitReorientOrLevelDeadlineRoutine(
                facing.RunReorientationForSplashRoutine(), reorientDeadlineFromLevelStartSec);
        }
        else
        {
            Debug.LogWarning("[3OS] SplashController: no ThreeOSFinalDemoXROriginFacing — skipping reorient");
        }

        yield return DeferredMrSpawnsAfterReorientRoutine();

        var finishFrom = _progressValue;
        yield return LerpProgressRoutine(finishFrom, 100f, progressFinishSec);

        yield return FadeMaterialRoutine(1f, 0f, fadeOutSec);

        if (_planeRenderer != null)
            _planeRenderer.enabled = false;

        Debug.Log("[3OS] SplashController sequence complete");
        _sequence = null;
    }

    /// <summary>
    /// Run reorient until it finishes or level time hits the deadline. Polls every frame so long
    /// inner yields (XR waits) cannot push the deadline past the configured cap.
    /// </summary>
    IEnumerator WaitReorientOrLevelDeadlineRoutine(IEnumerator reorient, float deadlineSinceLevelLoadSec)
    {
        if (reorient == null)
            yield break;

        if (deadlineSinceLevelLoadSec <= 0f)
        {
            yield return DriveReorientRoutine(reorient);
            yield break;
        }

        if (Time.timeSinceLevelLoadAsDouble >= deadlineSinceLevelLoadSec)
        {
            Debug.LogWarning(
                $"[3OS] SplashController: level time {Time.timeSinceLevelLoadAsDouble:F2}s already past " +
                $"reorient deadline {deadlineSinceLevelLoadSec:F1}s at 80% — continuing 80→100%");
            yield break;
        }

        _reorientDriverRunning = false;
        _reorientDriver = StartCoroutine(DriveReorientRoutine(reorient));

        while (_reorientDriverRunning && Time.timeSinceLevelLoadAsDouble < deadlineSinceLevelLoadSec)
            yield return null;

        if (_reorientDriverRunning && _reorientDriver != null)
        {
            StopCoroutine(_reorientDriver);
            _reorientDriver = null;
            _reorientDriverRunning = false;
            Debug.LogWarning(
                $"[3OS] SplashController: reorient level deadline {deadlineSinceLevelLoadSec:F1}s " +
                $"(levelT={Time.timeSinceLevelLoadAsDouble:F2}s) — continuing 80→100%");
        }

        _reorientDriver = null;
    }

    IEnumerator DeferredMrSpawnsAfterReorientRoutine()
    {
        if (postReorientSpawnDelaySec > 0f)
            yield return new WaitForSecondsRealtime(postReorientSpawnDelaySec);

        if (TryInstantiateAtSpawnPoint(unrealGroupPrefab, unrealSpawnPointName, out _))
        {
            _progressValue = Mathf.Min(100f, _progressValue + progressBumpPerDeferredSpawn);
            SetProgress(_progressValue);
            yield return FlushUiToRenderTextureRoutine();
        }

        if (betweenDeferredSpawnsSec > 0f)
            yield return new WaitForSecondsRealtime(betweenDeferredSpawnsSec);

        if (TryInstantiateAtSpawnPoint(colleagueGroupPrefab, colleagueSpawnPointName, out _))
        {
            _progressValue = Mathf.Min(100f, _progressValue + progressBumpPerDeferredSpawn);
            SetProgress(_progressValue);
            yield return FlushUiToRenderTextureRoutine();
        }
    }

    static bool TryInstantiateAtSpawnPoint(GameObject prefab, string spawnPointName, out GameObject instance)
    {
        instance = null;
        if (prefab == null)
        {
            Debug.LogWarning($"[3OS] SplashController: deferred spawn skipped — prefab not assigned (spawn={spawnPointName})");
            return false;
        }

        if (string.IsNullOrEmpty(spawnPointName))
        {
            Debug.LogWarning("[3OS] SplashController: deferred spawn skipped — empty spawn point name");
            return false;
        }

        var spawnGo = GameObject.Find(spawnPointName);
        if (spawnGo == null)
        {
            Debug.LogWarning($"[3OS] SplashController: deferred spawn skipped — GameObject.Find(\"{spawnPointName}\") missing");
            return false;
        }

        var t = spawnGo.transform;
        instance = Instantiate(prefab, t.position, t.rotation);
        instance.name = prefab.name;
        Debug.Log($"[3OS] SplashController: instantiated {prefab.name} at {spawnPointName}");
        return true;
    }

    IEnumerator DriveReorientRoutine(IEnumerator reorient)
    {
        _reorientDriverRunning = true;
        try
        {
            while (reorient.MoveNext())
                yield return reorient.Current;
        }
        finally
        {
            _reorientDriverRunning = false;
        }
    }

    /// <summary>Wall-clock lerp — avoids first-frame unscaledDeltaTime spikes finishing the whole segment in one tick.</summary>
    IEnumerator LerpProgressRoutine(float from, float to, float durationSec)
    {
        if (durationSec <= 0f)
        {
            SetProgress(to);
            yield return FlushUiToRenderTextureRoutine();
            yield break;
        }

        var start = Time.unscaledTimeAsDouble;
        var end = start + durationSec;
        SetProgress(from);
        yield return FlushUiToRenderTextureRoutine();

        while (Time.unscaledTimeAsDouble < end)
        {
            var u = (float)((Time.unscaledTimeAsDouble - start) / durationSec);
            SetProgress(Mathf.Lerp(from, to, u));
            yield return new WaitForEndOfFrame();
        }

        SetProgress(to);
        yield return FlushUiToRenderTextureRoutine();
    }

    IEnumerator FadeMaterialRoutine(float fromAlpha, float toAlpha, float durationSec)
    {
        if (_matInstance == null || durationSec <= 0f)
        {
            SetMaterialAlpha(toAlpha);
            DisableSplashPlaneRendererIfInvisible();
            yield break;
        }

        var start = Time.unscaledTimeAsDouble;
        var end = start + durationSec;
        SetMaterialAlpha(fromAlpha);

        while (Time.unscaledTimeAsDouble < end)
        {
            var u = (float)((Time.unscaledTimeAsDouble - start) / durationSec);
            var a = Mathf.Lerp(fromAlpha, toAlpha, u);
            SetMaterialAlpha(a);
            if (a <= 0f)
            {
                DisableSplashPlaneRendererIfInvisible();
                yield break;
            }

            yield return new WaitForEndOfFrame();
        }

        SetMaterialAlpha(toAlpha);
        DisableSplashPlaneRendererIfInvisible();
    }

    void DisableSplashPlaneRendererIfInvisible()
    {
        if (_planeRenderer == null || !_planeRenderer.enabled)
            return;

        if (_matInstance == null)
            return;

        if (TryGetMaterialAlpha(out var a) && a <= 0f)
            _planeRenderer.enabled = false;
    }

    bool TryGetMaterialAlpha(out float alpha)
    {
        alpha = 1f;
        if (_matInstance == null)
            return false;

        if (_matInstance.HasProperty(ColorId))
        {
            alpha = _matInstance.GetColor(ColorId).a;
            return true;
        }

        if (_matInstance.HasProperty(BaseColorId))
        {
            alpha = _matInstance.GetColor(BaseColorId).a;
            return true;
        }

        return false;
    }

    IEnumerator FlushUiToRenderTextureRoutine()
    {
        RequestUiRepaint();
        yield return null;
        yield return new WaitForEndOfFrame();
    }

    void RequestUiRepaint()
    {
        var root = uiDocument?.rootVisualElement;
        root?.MarkDirtyRepaint();
        _progressBar?.MarkDirtyRepaint();
    }

    void SetProgress(float value)
    {
        if (_progressBar == null)
            return;

        var clamped = Mathf.Clamp(value, _progressBar.lowValue, _progressBar.highValue);
        _progressValue = clamped;
        _progressBar.value = clamped;
        _progressBar.title = $"{Mathf.RoundToInt(clamped)}%";
        RequestUiRepaint();
    }

    void SetMaterialAlpha(float alpha)
    {
        if (_matInstance == null)
            return;

        alpha = Mathf.Clamp01(alpha);
        if (_matInstance.HasProperty(ColorId))
        {
            var c = _matInstance.GetColor(ColorId);
            c.a = alpha;
            _matInstance.SetColor(ColorId, c);
        }
        else if (_matInstance.HasProperty(BaseColorId))
        {
            var c = _matInstance.GetColor(BaseColorId);
            c.a = alpha;
            _matInstance.SetColor(BaseColorId, c);
        }
    }
}
