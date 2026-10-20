#if UNITY_EDITOR
using System;
using System.IO;
using System.Linq;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

/// <summary>
/// Phase 0.7 Task 7.1 — Final demo staging: XR Origin + NPC (no FS migrate yet).
/// Prefabs DemoScene XR Origin, hands-test PerfNPC; instances into 3OS_Final_DemoScene
/// at Player_Spawn_Point; removes Jarvis v07 placeholder; stores WispySky for Palace overlay.
/// Menu: 3OS / Final Demo / Setup 7.1 Origin + NPC
/// Batchmode: -executeMethod ThreeOSFinalDemo71Setup.Setup
/// </summary>
public static class ThreeOSFinalDemo71Setup
{
    const string DemoScenePath = "Assets/Scenes/3OS_DemoScene.unity";
    const string HandsScenePath = "Assets/Scenes/AndroidHands_ControllerTest.unity";
    const string FinalScenePath = "Assets/Scenes/3OS_Final_DemoScene.unity";
    const string PrefabFolder = "Assets/3OS_FinalDemo/Prefabs";
    const string XrOriginPrefabPath = PrefabFolder + "/XR_Origin_FinalDemo.prefab";
    const string XrImPrefabPath = PrefabFolder + "/XR_InteractionManager_FinalDemo.prefab";
    const string NpcPrefabPath = PrefabFolder + "/PerfNPC_FinalDemo.prefab";
    const string WispySkyMatPath = "Assets/Wispy Sky/Materials/WispySkyboxMat2.mat";
    const string HeadPrefabPath =
        "Assets/3OS_FinalDemo/Application Demos/Jarvis/NPC_Head_Scale_Node.prefab";
    const string LeftHandPrefabPath =
        "Assets/NekoLegends/Humanoids/Androids/Type01/Models/Hands/Prefabs/SegHand_L_Tracking.prefab";
    const string RightHandPrefabPath =
        "Assets/NekoLegends/Humanoids/Androids/Type01/Models/Hands/Prefabs/SegHand_R_Tracking.prefab";

    [MenuItem("3OS/Final Demo/Setup 7.1 Origin + NPC")]
    public static void SetupFromMenu() => SetupInternal(exitEditor: false);

    public static void Setup() => SetupInternal(exitEditor: true);

    static void SetupInternal(bool exitEditor)
    {
        try
        {
            EnsurePrefabFolder();
            PrefabXrOriginFromDemo();
            PrefabNpcFromHandsTest();
            WireFinalScene();
            EnsureBuildSettings();
            AssetDatabase.SaveAssets();
            AssetDatabase.Refresh();
            Debug.Log("[3OS][7.1] Final demo Origin + NPC wired.");
            if (exitEditor)
                EditorApplication.Exit(0);
        }
        catch (Exception ex)
        {
            Debug.LogError($"[3OS][7.1] Setup failed: {ex}");
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

    static void PrefabXrOriginFromDemo()
    {
        if (!File.Exists(DemoScenePath))
            throw new FileNotFoundException(DemoScenePath);

        var scene = EditorSceneManager.OpenScene(DemoScenePath, OpenSceneMode.Single);
        var origin = GameObject.Find("XR Origin (VR)");
        if (origin == null)
            throw new Exception("XR Origin (VR) not found in DemoScene");

        SaveReplacePrefab(origin, XrOriginPrefabPath);
        Debug.Log($"[3OS][7.1] Prefabbed XR Origin → {XrOriginPrefabPath}");

        var xrim = GameObject.Find("XR Interaction Manager");
        if (xrim != null)
        {
            SaveReplacePrefab(xrim, XrImPrefabPath);
            Debug.Log($"[3OS][7.1] Prefabbed XR Interaction Manager → {XrImPrefabPath}");
        }

        EditorSceneManager.CloseScene(scene, true);
    }

    static void PrefabNpcFromHandsTest()
    {
        if (!File.Exists(HandsScenePath))
            throw new FileNotFoundException(HandsScenePath);

        var scene = EditorSceneManager.OpenScene(HandsScenePath, OpenSceneMode.Single);
        var npc = GameObject.Find("PerfNPC");
        if (npc == null)
        {
            // Rebuild a stand-alone PerfNPC matching hands-test capture setup.
            npc = BuildPerfNpcStandalone();
            Debug.Log("[3OS][7.1] PerfNPC missing in hands scene — rebuilt from prefabs.");
        }

        // Detach from XR Origin so the prefab is world-rooted.
        npc.transform.SetParent(null, true);
        SaveReplacePrefab(npc, NpcPrefabPath);
        Debug.Log($"[3OS][7.1] Prefabbed PerfNPC → {NpcPrefabPath}");
        EditorSceneManager.CloseScene(scene, true);
    }

    static GameObject BuildPerfNpcStandalone()
    {
        var leftPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(LeftHandPrefabPath);
        var rightPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(RightHandPrefabPath);
        var headPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(HeadPrefabPath);
        if (leftPrefab == null || rightPrefab == null)
            throw new Exception("SegHand tracking prefabs missing");

        var npc = new GameObject("PerfNPC");
        var headHost = new GameObject("NPC_Head");
        headHost.transform.SetParent(npc.transform, false);
        headHost.transform.localPosition = new Vector3(0f, 1.6f, 0f);
        if (headPrefab != null)
        {
            var head = (GameObject)PrefabUtility.InstantiatePrefab(headPrefab);
            head.name = "NPC_Head_Scale_Node";
            head.transform.SetParent(headHost.transform, false);
            head.transform.localPosition = Vector3.zero;
            head.transform.localRotation = Quaternion.identity;
        }

        EnsureNpcHand(npc.transform, "NPC_Hand_L", leftPrefab, true);
        EnsureNpcHand(npc.transform, "NPC_Hand_R", rightPrefab, false);
        return npc;
    }

    static void EnsureNpcHand(Transform npc, string name, GameObject prefab, bool left)
    {
        var go = (GameObject)PrefabUtility.InstantiatePrefab(prefab);
        go.name = name;
        go.transform.SetParent(npc, false);
        go.transform.localPosition = left ? new Vector3(-0.2f, 1.2f, 0.25f) : new Vector3(0.2f, 1.2f, 0.25f);
        go.transform.localRotation = Quaternion.identity;
        go.transform.localScale = Vector3.one;
        var pose = go.GetComponent<ThreeOSAndroidNpcHandPose>() ?? go.AddComponent<ThreeOSAndroidNpcHandPose>();
        var so = new SerializedObject(pose);
        so.FindProperty("handRoot").objectReferenceValue = go.transform;
        so.FindProperty("hingeAxisLocal").vector3Value = new Vector3(-1f, 0f, 0f);
        so.ApplyModifiedPropertiesWithoutUndo();
        pose.SetHandRoot(go.transform);
    }

    static void WireFinalScene()
    {
        if (!File.Exists(FinalScenePath))
            throw new FileNotFoundException(FinalScenePath);

        var scene = EditorSceneManager.OpenScene(FinalScenePath, OpenSceneMode.Single);

        // Remove Jarvis v07 placeholder (and any other v07 duplicates).
        foreach (var go in scene.GetRootGameObjects())
            DestroyNamedRecursive(go, "v07");

        var spawn = GameObject.Find("Player_Spawn_Point");
        if (spawn == null)
            throw new Exception("Player_Spawn_Point not found in Final Demo scene");

        // XR Interaction Manager (once)
        var xrimPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(XrImPrefabPath);
        if (xrimPrefab != null && GameObject.Find("XR Interaction Manager") == null)
        {
            var xrim = (GameObject)PrefabUtility.InstantiatePrefab(xrimPrefab);
            xrim.name = "XR Interaction Manager";
        }

        // XR Origin at spawn
        foreach (var existing in UnityEngine.Object.FindObjectsByType<Transform>(FindObjectsSortMode.None)
                     .Where(t => t.name == "XR Origin (VR)" && t.parent == null)
                     .Select(t => t.gameObject)
                     .ToArray())
            UnityEngine.Object.DestroyImmediate(existing);

        var originPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(XrOriginPrefabPath);
        if (originPrefab == null)
            throw new Exception($"Missing {XrOriginPrefabPath}");

        var origin = (GameObject)PrefabUtility.InstantiatePrefab(originPrefab);
        origin.name = "XR Origin (VR)";
        origin.transform.SetPositionAndRotation(spawn.transform.position, spawn.transform.rotation);
        origin.transform.localScale = Vector3.one;
        Debug.Log($"[3OS][7.1] XR Origin at spawn {spawn.transform.position}");

        // Floor parent + NPC (5 ft ahead of spawn, facing spawn / user)
        var oldFloor = GameObject.Find("NPC_Floor_Parent");
        if (oldFloor != null)
            UnityEngine.Object.DestroyImmediate(oldFloor);

        var floor = new GameObject("NPC_Floor_Parent");
        var forward = spawn.transform.forward;
        if (forward.sqrMagnitude < 1e-4f)
            forward = Vector3.forward;
        forward.y = 0f;
        forward.Normalize();
        var npcPos = spawn.transform.position + forward * (5f * 0.3048f);
        floor.transform.position = npcPos;
        // Face the user (look toward spawn).
        floor.transform.rotation = Quaternion.LookRotation(
            (spawn.transform.position - npcPos).normalized, Vector3.up);

        var npcPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(NpcPrefabPath);
        if (npcPrefab == null)
            throw new Exception($"Missing {NpcPrefabPath}");

        var npc = (GameObject)PrefabUtility.InstantiatePrefab(npcPrefab);
        npc.name = "PerfNPC";
        npc.transform.SetParent(floor.transform, false);
        npc.transform.localPosition = Vector3.zero;
        npc.transform.localRotation = Quaternion.identity;
        npc.transform.localScale = Vector3.one;
        Debug.Log($"[3OS][7.1] PerfNPC under NPC_Floor_Parent at {npcPos}");

        // Stage marker: palace skybox ref (not applied — Act I stays office / fake PT).
        var stageGo = GameObject.Find("FinalDemoStage");
        if (stageGo == null)
            stageGo = new GameObject("FinalDemoStage");
        var stage = stageGo.GetComponent<ThreeOSFinalDemoStage>()
                    ?? stageGo.AddComponent<ThreeOSFinalDemoStage>();
        var sky = AssetDatabase.LoadAssetAtPath<Material>(WispySkyMatPath);
        if (sky == null)
            Debug.LogWarning($"[3OS][7.1] Wispy sky material missing at {WispySkyMatPath}");
        var sso = new SerializedObject(stage);
        sso.FindProperty("palaceSkybox").objectReferenceValue = sky;
        sso.FindProperty("xrOrigin").objectReferenceValue = origin.transform;
        sso.FindProperty("npcFloorParent").objectReferenceValue = floor.transform;
        sso.FindProperty("npcRoot").objectReferenceValue = npc.transform;
        sso.FindProperty("playerSpawn").objectReferenceValue = spawn.transform;
        sso.ApplyModifiedPropertiesWithoutUndo();

        // Cold open: do not apply palace skybox.
        if (RenderSettings.skybox == sky)
            RenderSettings.skybox = null;

        EditorSceneManager.MarkSceneDirty(scene);
        EditorSceneManager.SaveScene(scene, FinalScenePath);
    }

    static void EnsureBuildSettings()
    {
        var scenes = EditorBuildSettings.scenes.ToList();
        var idx = scenes.FindIndex(s => s.path == FinalScenePath);
        if (idx < 0)
        {
            scenes.Insert(0, new EditorBuildSettingsScene(FinalScenePath, true));
        }
        else
        {
            var s = scenes[idx];
            s.enabled = true;
            scenes.RemoveAt(idx);
            scenes.Insert(0, s);
        }

        EditorBuildSettings.scenes = scenes.ToArray();
        Debug.Log($"[3OS][7.1] Build settings: {FinalScenePath} enabled @ index 0");
    }

    static void SaveReplacePrefab(GameObject source, string path)
    {
        if (AssetDatabase.LoadAssetAtPath<GameObject>(path) != null)
            AssetDatabase.DeleteAsset(path);
        PrefabUtility.SaveAsPrefabAsset(source, path);
    }

    static void DestroyNamedRecursive(GameObject root, string name)
    {
        if (root == null)
            return;
        if (root.name == name)
        {
            UnityEngine.Object.DestroyImmediate(root);
            return;
        }

        for (var i = root.transform.childCount - 1; i >= 0; i--)
            DestroyNamedRecursive(root.transform.GetChild(i).gameObject, name);
    }
}
#endif
