#if UNITY_EDITOR
using System.Collections.Generic;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

/// <summary>
/// Prepares Final Demo office geometry for baked / mixed lighting.
/// Does not bake by default — run Generate Lighting from the Lighting window
/// (or use the Bake menu item) after reviewing static flags.
/// </summary>
public static class ThreeOSFinalDemoLightBakePrep
{
    const string FinalDemoScene = "Assets/Scenes/3OS_Final_DemoScene.unity";

    [MenuItem("3OS/Final Demo/Lighting/Prep Office For Bake")]
    public static void PrepOfficeForBake()
    {
        var scene = EnsureFinalDemoSceneOpen();
        if (!scene.IsValid())
            return;

        EnableBakedLightmaps();
        ConvertRealtimeLightsToMixed();
        MarkOfficeContributorsStatic();
        LogMaterialChecklist();

        EditorSceneManager.MarkSceneDirty(scene);
        Debug.Log("[3OS] Light bake prep done. Open Window → Rendering → Lighting → Generate Lighting when ready.");
    }

    [MenuItem("3OS/Final Demo/Lighting/Bake Now (Progressive Async)")]
    public static void BakeNow()
    {
        PrepOfficeForBake();
        // Never use Lightmapping.Bake() from a MenuItem — it blocks the Editor main thread
        // ("Waiting for user code in mscorlib.dll…") until the whole bake finishes.
        if (!Lightmapping.BakeAsync())
        {
            Debug.LogError("[3OS] BakeAsync failed to start. Cancel any in-progress bake first.");
            return;
        }

        Debug.Log("[3OS] Progressive bake started asynchronously. Watch the Lighting window; Cancel there if needed. Save the scene when it completes.");
    }

    [MenuItem("3OS/Final Demo/Lighting/Cancel Bake")]
    public static void CancelBake()
    {
        if (Lightmapping.isRunning)
        {
            Lightmapping.Cancel();
            Debug.Log("[3OS] Bake cancelled.");
        }
        else
        {
            Debug.Log("[3OS] No bake running.");
        }
    }

    /// <summary>
    /// Instant brighten after a dark bake: Mixed lights keep realtime direct, so intensity
    /// + ambient + indirectScale recover the pre-bake look without waiting on a full rebake.
    /// Bumped GI scales apply fully on the next bake.
    /// </summary>
    [MenuItem("3OS/Final Demo/Lighting/Brighten Office Exposure")]
    public static void BrightenOfficeExposure()
    {
        var scene = EnsureFinalDemoSceneOpen();
        if (!scene.IsValid())
            return;

        RenderSettings.ambientIntensity = Mathf.Max(RenderSettings.ambientIntensity, 2.4f);

        var lights = 0;
        foreach (var light in Object.FindObjectsByType<Light>(FindObjectsSortMode.None))
        {
            if (light.type != LightType.Point && light.type != LightType.Spot &&
                light.type != LightType.Directional)
                continue;
            light.intensity = Mathf.Max(light.intensity, 1.75f);
            light.bounceIntensity = Mathf.Max(light.bounceIntensity, 1.75f);
            light.lightmapBakeType = LightmapBakeType.Mixed;
            EditorUtility.SetDirty(light);
            lights++;
        }

        var ls = Lightmapping.lightingSettings;
        if (ls != null)
        {
            ls.mixedBakeMode = MixedLightingMode.IndirectOnly;
            ls.albedoBoost = Mathf.Max(ls.albedoBoost, 1.5f);
            ls.indirectScale = Mathf.Max(ls.indirectScale, 2f);
            EditorUtility.SetDirty(ls);
        }

        // Re-link LightingData if the bake folder exists but the scene dropped the reference.
        var lightingData = AssetDatabase.LoadAssetAtPath<LightingDataAsset>(
            "Assets/Scenes/3OS_Final_DemoScene/LightingData.asset");
        if (lightingData != null && Lightmapping.lightingDataAsset == null)
        {
            Lightmapping.lightingDataAsset = lightingData;
            Debug.Log("[3OS] Re-linked LightingData.asset to the open scene.");
        }

        EditorSceneManager.MarkSceneDirty(scene);
        Debug.Log(
            $"[3OS] Brightened exposure ({lights} lights ≥1.75, ambient ≥2.4, indirectScale ≥2). " +
            "Realtime direct updates immediately; rebake to refresh lightmaps with the new GI scales.");
    }

    static Scene EnsureFinalDemoSceneOpen()
    {
        var active = SceneManager.GetActiveScene();
        if (active.path == FinalDemoScene)
            return active;

        if (!EditorSceneManager.SaveCurrentModifiedScenesIfUserWantsTo())
            return default;

        return EditorSceneManager.OpenScene(FinalDemoScene, OpenSceneMode.Single);
    }

    static void EnableBakedLightmaps()
    {
        Lightmapping.lightingSettings ??= new LightingSettings { name = "3OS_FinalDemo_Lighting" };
        var ls = Lightmapping.lightingSettings;
        ls.bakedGI = true;
        ls.realtimeGI = false;
        // Soft realtime-looking mix: baked GI + realtime specular/direct where Mixed lights allow.
        ls.mixedBakeMode = MixedLightingMode.IndirectOnly;
        // GPU progressive; CPU + Final Gather (old JapanOffice defaults) can run for hours.
        ls.lightmapper = LightingSettings.Lightmapper.ProgressiveGPU;
        ApplyQuickDemoBakeSettings(ls);
        EditorUtility.SetDirty(ls);
        Debug.Log("[3OS] Enabled baked GI (Progressive GPU, quick-demo samples, Final Gather off).");
    }

    /// <summary>
    /// Demo-office bake defaults. JapanOffice LightBakeSettings shipped with Final Gather
    /// @ 4096 rays + 4K atlases — that is production Enlighten-era quality, not a 5–15 min check.
    /// </summary>
    static void ApplyQuickDemoBakeSettings(LightingSettings ls)
    {
        ls.lightmapResolution = 20f;       // texels / unit (was 10 with huge atlas → slow pack)
        ls.lightmapMaxSize = 1024;
        ls.lightmapPadding = 2;
        ls.ao = false;
        ls.directSampleCount = 32;
        ls.indirectSampleCount = 256;
        ls.environmentSampleCount = 64;
        ls.maxBounces = 2;
        ls.minBounces = 1;
        // Dark vinyl + intensity-1 points bake underexposed — boost GI fill for demo.
        ls.albedoBoost = 1.5f;
        ls.indirectScale = 2f;
        // finalGather is obsolete (Enlighten-only) and errors under Progressive — leave it off
        // in LightBakeSettings.lighting (m_FinalGather: 0), do not set it from script.
    }

    static void ConvertRealtimeLightsToMixed()
    {
        var n = 0;
        foreach (var light in Object.FindObjectsByType<Light>(FindObjectsSortMode.None))
        {
            if (light.lightmapBakeType != LightmapBakeType.Realtime)
                continue;
            light.lightmapBakeType = LightmapBakeType.Mixed;
            EditorUtility.SetDirty(light);
            n++;
        }

        Debug.Log($"[3OS] Converted {n} Realtime light(s) → Mixed.");
    }

    static void MarkOfficeContributorsStatic()
    {
        // ContributeGI = 1 << 0 in StaticEditorFlags (value 1). Keep batching optional.
        const StaticEditorFlags gi = StaticEditorFlags.ContributeGI;
        var tagged = 0;
        foreach (var r in Object.FindObjectsByType<MeshRenderer>(FindObjectsSortMode.None))
        {
            var go = r.gameObject;
            var path = GetPath(go.transform);
            if (!IsOfficeBakeCandidate(path, go.name))
                continue;

            var flags = GameObjectUtility.GetStaticEditorFlags(go);
            if ((flags & gi) == 0)
            {
                GameObjectUtility.SetStaticEditorFlags(go, flags | gi);
                tagged++;
            }
        }

        Debug.Log($"[3OS] Marked Contribute GI on {tagged} renderer(s) (consoles / desks / Japan office).");
    }

    static bool IsOfficeBakeCandidate(string path, string name)
    {
        if (name.Contains("StandingDesk") || name.Contains("Console") || name.Contains("TracWall"))
            return true;
        if (path.Contains("Japan") || path.Contains("Office") || path.Contains("Furniture"))
            return true;
        // Avoid XR rig / Demon / UI / sky / wipe props.
        if (path.Contains("XR Origin") || path.Contains("Demon") || path.Contains("Wipe") ||
            path.Contains("Canvas") || path.Contains("Hand"))
            return false;
        return false;
    }

    static string GetPath(Transform t)
    {
        var parts = new List<string>();
        while (t != null)
        {
            parts.Add(t.name);
            t = t.parent;
        }

        parts.Reverse();
        return string.Join("/", parts);
    }

    static void LogMaterialChecklist()
    {
        Debug.Log(
            "[3OS] Material checklist for bake (Built-in Standard):\n" +
            "  • Albedo/_Color — dark vinyl is fine; avoid baked GI maps in albedo\n" +
            "  • Smoothness — keep ≤~0.35 for vinyl or realtime specular still flares\n" +
            "  • Normal — optional; bake does not replace micro-normals (still for realtime specular)\n" +
            "  • Metallic — 0 for vinyl/plastic\n" +
            "  • Emission — only if lit; set Global Illumination = Baked\n" +
            "  • Mesh — Generate Lightmap UVs (enabled on ThreeOS_StandingDeskConsole.fbx)\n" +
            "  • Renderer — Contribute GI static; Scale In Lightmap ~1");
    }
}
#endif
