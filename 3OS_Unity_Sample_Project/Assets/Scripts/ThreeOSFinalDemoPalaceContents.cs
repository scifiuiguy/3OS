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

    Coroutine _co;
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
            // 7.8.c — dismiss in parallel with VR→MR wipe.
            if (_co != null)
                StopCoroutine(_co);
            _co = StartCoroutine(DismissRoutine());
        }
    }

    void OnWipeSettled(bool toPalace)
    {
        EnsureRefs();
        if (!toPalace)
        {
            ThreeOSStorageController.BillboardAllFarFieldLabels = false;
            return;
        }

        ThreeOSStorageController.BillboardAllFarFieldLabels = true;
        // 7.8.b — introduce after wipe settles.
        if (_co != null)
            StopCoroutine(_co);
        _co = StartCoroutine(RevealRoutine());
    }

    IEnumerator RevealRoutine()
    {
        if (storage == null || stage == null)
            yield break;
        var spawn = stage.PlayerSpawn;
        if (spawn == null)
        {
            Debug.LogError("[3OS] Palace reveal: Player_Spawn_Point missing on Stage");
            yield break;
        }

        // Critical: yield before any FS/glyph work so WipeSettled does not hitch the settle frame
        // (Quest killed the app on a ~9s sync LoadDual+RebuildViews).
        yield return null;

        var deadline = Time.realtimeSinceStartup + 5f;
        while (storage != null && string.IsNullOrEmpty(storage.DemoRoot) &&
               Time.realtimeSinceStartup < deadline)
            yield return null;

        yield return storage.RevealPalaceAtSpawnRoutine(spawn);
        if (!storage.PalaceFixtureLoaded)
        {
            Debug.LogError($"[3OS] Palace reveal failed: {storage.Status}");
            yield break;
        }

        // Sky Overlay (queue 5000) would bury Geometry glyphs (~3000) — bump to keep-list.
        EnsureRefs();
        layering?.PromotePalaceWorldChrome();

        var exclude = storage.MrPersistedSelectionId;
        storage.HidePalaceGlyphsPendingScaleIn(exclude);
        yield return storage.AnimatePalaceClustersScale(0.001f, 1f,
            ThreeOSStorageController.PalaceClusterScaleSeconds, exclude);
        layering?.PromotePalaceWorldChrome();
        _co = null;
    }

    IEnumerator DismissRoutine()
    {
        if (storage == null || stage == null)
            yield break;
        if (!storage.PalaceFixtureLoaded || storage.PalaceSoftDismissed)
            yield break;
        yield return storage.DismissPalaceToMr(stage.PlayerSpawn);
        _co = null;
    }
}
