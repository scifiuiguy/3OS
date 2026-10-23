using System.Collections;
using ThreeOS;
using UnityEngine;

/// <summary>
/// Phase 0.7 Task 7.8 — Palace contents reveal/dismiss + MR-only WorldMesh collision,
/// driven by ModeToggle wipe start/settle.
/// </summary>
public sealed class ThreeOSFinalDemoPalaceContents : MonoBehaviour
{
    [SerializeField] ThreeOSFinalDemoStage stage;
    [SerializeField] ThreeOSFinalDemoModeToggle modeToggle;
    [SerializeField] ThreeOSStorageController storage;
    [SerializeField] ThreeOSVoodooController voodoo;
    [SerializeField] ThreeOSFinalDemoPalaceLayering layering;

    Coroutine _prepareCo;
    Coroutine _scaleCo;
    bool _hooks;

    public void Configure(ThreeOSFinalDemoStage demoStage, ThreeOSFinalDemoModeToggle toggle)
    {
        stage = demoStage;
        modeToggle = toggle;
        EnsureRefs();
        Hook();
    }

    void Awake()
    {
        EnsureRefs();
        Hook();
        // Cold open is MR — enclosure on.
        if (voodoo != null)
            voodoo.SetWorldMeshCollisionActive(true);
    }

    void OnDestroy()
    {
        Unhook();
    }

    void EnsureRefs()
    {
        if (stage == null)
            stage = FindFirstObjectByType<ThreeOSFinalDemoStage>();
        if (modeToggle == null)
            modeToggle = FindFirstObjectByType<ThreeOSFinalDemoModeToggle>();
        if (storage == null)
            storage = FindFirstObjectByType<ThreeOSStorageController>();
        if (voodoo == null)
            voodoo = FindFirstObjectByType<ThreeOSVoodooController>();
        if (layering == null)
            layering = FindFirstObjectByType<ThreeOSFinalDemoPalaceLayering>();
    }

    void Hook()
    {
        if (_hooks || modeToggle == null)
            return;
        modeToggle.WipeStarted += OnWipeStarted;
        modeToggle.WipeSettled += OnWipeSettled;
        _hooks = true;
    }

    void Unhook()
    {
        if (!_hooks || modeToggle == null)
            return;
        modeToggle.WipeStarted -= OnWipeStarted;
        modeToggle.WipeSettled -= OnWipeSettled;
        _hooks = false;
    }

    void OnWipeStarted(bool toPalace)
    {
        EnsureRefs();
        // 7.8.a — colliders MR-only; flip at wipe start (parallel with sky).
        if (voodoo != null)
            voodoo.SetWorldMeshCollisionActive(!toPalace);

        if (!toPalace)
        {
            ThreeOSStorageController.BillboardAllFarFieldLabels = false;
            // Abort any in-flight paste UI / palace prepare so MR does not inherit Palace chrome.
            storage?.CancelPasteProgressUi();
            // 7.8.c — dismiss in parallel with VR→MR wipe.
            if (_prepareCo != null)
            {
                StopCoroutine(_prepareCo);
                _prepareCo = null;
            }

            if (_scaleCo != null)
            {
                StopCoroutine(_scaleCo);
                _scaleCo = null;
            }

            _scaleCo = StartCoroutine(DismissRoutine());
            return;
        }

        // MR→VR: NPC must stay at 0% if Act I already dismissed him.
        FindFirstObjectByType<ThreeOSFinalDemoPerfPlayer>()?.EnsureFloorStaysHiddenIfDismissed();
        // MR→VR: load/hide glyphs during the wipe so settle can scale in immediately.
        if (_prepareCo != null)
            StopCoroutine(_prepareCo);
        _prepareCo = StartCoroutine(PrepareDuringWipeRoutine());
    }

    void OnWipeSettled(bool toPalace)
    {
        EnsureRefs();
        if (!toPalace)
        {
            ThreeOSStorageController.BillboardAllFarFieldLabels = false;
            // Soft-dismiss can finish after wipe settle — reassert MR hide next frames.
            storage?.EnsurePalaceSoftDismissedVisuals();
            return;
        }

        FindFirstObjectByType<ThreeOSFinalDemoPerfPlayer>()?.EnsureFloorStaysHiddenIfDismissed();

        ThreeOSStorageController.BillboardAllFarFieldLabels = true;
        if (_scaleCo != null)
            StopCoroutine(_scaleCo);
        _scaleCo = StartCoroutine(ScaleInOnSettleRoutine());
    }

    IEnumerator PrepareDuringWipeRoutine()
    {
        if (storage == null || stage == null)
            yield break;
        var spawn = stage.PlayerSpawn;
        if (spawn == null)
        {
            Debug.LogError("[3OS] Palace prepare: Player_Spawn_Point missing on Stage");
            yield break;
        }

        var deadline = Time.realtimeSinceStartup + 5f;
        while (storage != null && string.IsNullOrEmpty(storage.DemoRoot) &&
               Time.realtimeSinceStartup < deadline)
            yield return null;

        yield return storage.RevealPalaceAtSpawnRoutine(spawn);
        if (!storage.PalaceFixtureLoaded)
            Debug.LogError($"[3OS] Palace prepare failed: {storage.Status}");
        else
        {
            EnsureRefs();
            layering?.PromotePalaceWorldChrome();
            storage.HidePalaceGlyphsPendingScaleIn(storage.MrPersistedSelectionId);
        }

        _prepareCo = null;
    }

    IEnumerator ScaleInOnSettleRoutine()
    {
        if (storage == null || stage == null)
            yield break;

        // Finish any in-flight prepare (usually done before settle if wipe ≥0.5s).
        var deadline = Time.realtimeSinceStartup + 8f;
        while (_prepareCo != null && Time.realtimeSinceStartup < deadline)
            yield return null;
        while (storage != null && !storage.PalaceFixtureLoaded &&
               Time.realtimeSinceStartup < deadline)
            yield return null;

        if (storage == null || !storage.PalaceFixtureLoaded)
        {
            Debug.LogError("[3OS] Palace scale-in aborted — fixture not ready");
            _scaleCo = null;
            yield break;
        }

        EnsureRefs();
        layering?.PromotePalaceWorldChrome();
        var exclude = storage.MrPersistedSelectionId;
        storage.HidePalaceGlyphsPendingScaleIn(exclude);
        yield return storage.AnimatePalaceClustersScale(0.001f, 1f,
            ThreeOSStorageController.PalaceClusterScaleSeconds, exclude);
        layering?.PromotePalaceWorldChrome();
        _scaleCo = null;
    }

    IEnumerator DismissRoutine()
    {
        if (storage == null || stage == null)
            yield break;
        if (!storage.PalaceFixtureLoaded || storage.PalaceSoftDismissed)
            yield break;
        yield return storage.DismissPalaceToMr(stage.PlayerSpawn);
        _scaleCo = null;
    }
}
