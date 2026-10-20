#if UNITY_EDITOR
using System;
using System.IO;
using ThreeOS;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

/// <summary>
/// Phase 0.7 Task 7.7 — wire MR↔VR mode toggle (open-book glyph + skybox wipe).
/// Menu: 3OS / Final Demo / Setup 7.7 Mode Toggle Wipe
/// Batchmode: -executeMethod ThreeOSFinalDemo77Setup.Setup
/// </summary>
public static class ThreeOSFinalDemo77Setup
{
    const string FinalScenePath = "Assets/Scenes/3OS_Final_DemoScene.unity";

    [MenuItem("3OS/Final Demo/Setup 7.7 Mode Toggle Wipe")]
    public static void SetupFromMenu() => SetupInternal(exitEditor: false);

    public static void Setup() => SetupInternal(exitEditor: true);

    static void SetupInternal(bool exitEditor)
    {
        try
        {
            if (!File.Exists(FinalScenePath))
                throw new FileNotFoundException(FinalScenePath);

            var scene = EditorSceneManager.OpenScene(FinalScenePath, OpenSceneMode.Single);
            var stage = UnityEngine.Object.FindFirstObjectByType<ThreeOSFinalDemoStage>();
            if (stage == null)
                throw new Exception("FinalDemoStage missing — run 7.1 setup first.");

            var bridge = UnityEngine.Object.FindFirstObjectByType<ThreeOSBridge>();
            if (bridge == null)
                Debug.LogWarning("[3OS] 7.7 setup: ThreeOSBridge not found yet — ModeToggle will need it at runtime.");

            var go = GameObject.Find("FinalDemoModeToggle");
            if (go == null)
                go = new GameObject("FinalDemoModeToggle");

            var toggle = go.GetComponent<ThreeOSFinalDemoModeToggle>()
                         ?? go.AddComponent<ThreeOSFinalDemoModeToggle>();
            toggle.Configure(stage, bridge);

            // Cold open stays MR (null skybox); mode flag false.
            stage.SetInPalace(false);

            EditorSceneManager.MarkSceneDirty(scene);
            EditorSceneManager.SaveScene(scene);
            Debug.Log("[3OS] Final Demo 7.7 mode toggle wired (open-book + 0.5s wipe).");
            if (exitEditor)
                EditorApplication.Exit(0);
        }
        catch (Exception ex)
        {
            Debug.LogError($"[3OS] Final Demo 7.7 setup failed: {ex}");
            if (exitEditor)
                EditorApplication.Exit(1);
            throw;
        }
    }
}
#endif
