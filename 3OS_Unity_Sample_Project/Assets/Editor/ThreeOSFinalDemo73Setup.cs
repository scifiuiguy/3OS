#if UNITY_EDITOR
using System;
using System.IO;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;
using ThreeOS;

/// <summary>
/// Phase 0.7 Task 7.3 — migrate DemoScene harness (Bridge / Input / Storage / Voodoo / MultiChoice)
/// into Final Demo and wire NPC head focus anchor.
/// Menu: 3OS / Final Demo / Setup 7.3 Harness + NPC Focus Anchor
/// </summary>
public static class ThreeOSFinalDemo73Setup
{
    const string DemoScenePath = "Assets/Scenes/3OS_DemoScene.unity";
    const string FinalScenePath = "Assets/Scenes/3OS_Final_DemoScene.unity";
    const string PrefabFolder = "Assets/3OS_FinalDemo/Prefabs";
    const string HarnessPrefabPath = PrefabFolder + "/Harness_FinalDemo.prefab";

    [MenuItem("3OS/Final Demo/Setup 7.3 Harness + NPC Focus Anchor")]
    public static void SetupFromMenu() => SetupInternal(exitEditor: false);

    public static void Setup() => SetupInternal(exitEditor: true);

    static void SetupInternal(bool exitEditor)
    {
        try
        {
            EnsurePrefabFolder();
            PrefabHarnessFromDemo();
            WireFinalScene();
            AssetDatabase.SaveAssets();
            AssetDatabase.Refresh();
            Debug.Log("[3OS][7.3] Final demo harness + NPC focus anchor wired.");
            if (exitEditor)
                EditorApplication.Exit(0);
        }
        catch (Exception ex)
        {
            Debug.LogError($"[3OS][7.3] Setup failed: {ex}");
            if (exitEditor)
                EditorApplication.Exit(1);
        }
    }

    static void EnsurePrefabFolder()
    {
        if (!AssetDatabase.IsValidFolder("Assets/3OS_FinalDemo"))
            AssetDatabase.CreateFolder("Assets", "3OS_FinalDemo");
        if (!AssetDatabase.IsValidFolder(PrefabFolder))
            AssetDatabase.CreateFolder("Assets/3OS_FinalDemo", "Prefabs");
    }

    static void PrefabHarnessFromDemo()
    {
        if (!File.Exists(DemoScenePath))
            throw new FileNotFoundException(DemoScenePath);

        EditorSceneManager.OpenScene(DemoScenePath, OpenSceneMode.Single);
        var harness = GameObject.Find("3OS_Harness");
        if (harness == null)
            throw new Exception("3OS_Harness not found in DemoScene");

        // Ensure VoodooController exists (StorageController owns MultiChoice).
        if (harness.GetComponent<ThreeOSVoodooController>() == null)
            harness.AddComponent<ThreeOSVoodooController>();

        SaveReplacePrefab(harness, HarnessPrefabPath);
        Debug.Log($"[3OS][7.3] Prefabbed harness → {HarnessPrefabPath}");
    }

    static void WireFinalScene()
    {
        if (!File.Exists(FinalScenePath))
            throw new FileNotFoundException(FinalScenePath);

        var scene = EditorSceneManager.OpenScene(FinalScenePath, OpenSceneMode.Single);

        var existing = GameObject.Find("3OS_Harness");
        if (existing != null)
            Undo.DestroyObjectImmediate(existing);

        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(HarnessPrefabPath);
        if (prefab == null)
            throw new Exception($"Missing harness prefab {HarnessPrefabPath}");

        var instance = (GameObject)PrefabUtility.InstantiatePrefab(prefab, scene);
        instance.name = "3OS_Harness";
        Undo.RegisterCreatedObjectUndo(instance, "Instantiate Final Demo Harness");

        var bridge = instance.GetComponent<ThreeOSBridge>()
                     ?? instance.GetComponentInChildren<ThreeOSBridge>(true);
        var stage = UnityEngine.Object.FindObjectOfType<ThreeOSFinalDemoStage>();
        if (stage == null)
            throw new Exception("ThreeOSFinalDemoStage missing — run 7.1 setup first");

        var focusGo = GameObject.Find("NPC_Focus_Anchor");
        if (focusGo == null)
        {
            focusGo = new GameObject("NPC_Focus_Anchor");
            Undo.RegisterCreatedObjectUndo(focusGo, "Create NPC_Focus_Anchor");
        }

        var focus = focusGo.GetComponent<ThreeOSFinalDemoNpcFocusAnchor>()
                    ?? focusGo.AddComponent<ThreeOSFinalDemoNpcFocusAnchor>();
        var so = new SerializedObject(focus);
        so.FindProperty("bridge").objectReferenceValue = bridge;
        so.FindProperty("stage").objectReferenceValue = stage;
        Transform head = null;
        if (stage.NpcRoot != null)
        {
            head = stage.NpcRoot.Find("NPC_Head");
            if (head == null)
                head = stage.NpcRoot.Find("NPC_Head/NPC_Head_Scale_Node");
        }
        so.FindProperty("head").objectReferenceValue = head;
        so.ApplyModifiedPropertiesWithoutUndo();

        EditorSceneManager.MarkSceneDirty(scene);
        EditorSceneManager.SaveScene(scene);
        Debug.Log("[3OS][7.3] Final scene wired with harness + NPC_Focus_Anchor.");
    }

    static void SaveReplacePrefab(GameObject source, string path)
    {
        if (File.Exists(path))
            AssetDatabase.DeleteAsset(path);
        PrefabUtility.SaveAsPrefabAsset(source, path);
    }
}
#endif
