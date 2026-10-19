#if UNITY_EDITOR
using System;
using System.IO;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

/// <summary>
/// Phase 0.7 Task 7.4/7.5 — wire Act I Speak/mic/EQ/playlist + seed Final Demo perf JSON.
/// Menu: 3OS / Final Demo / Setup 7.4 Speak + Perf Playlist
/// Batchmode: -executeMethod ThreeOSFinalDemo74Setup.Setup
/// </summary>
public static class ThreeOSFinalDemo74Setup
{
    const string FinalScenePath = "Assets/Scenes/3OS_Final_DemoScene.unity";
    const string Perf01 = "Assets/3OS_FinalDemo/Performances/android_hands_perf_01.json";
    const string Perf02 = "Assets/3OS_FinalDemo/Performances/android_hands_perf_02.json";
    const string Audio01 = "Assets/Audio/jarvis_voice_clip_01.wav";
    const string Audio02 = "Assets/Audio/jarvis_voice_clip_02.wav";
    const string StreamingDir = "Assets/StreamingAssets/3OS/FinalDemoPerf";

    [MenuItem("3OS/Final Demo/Setup 7.4 Speak + Perf Playlist")]
    public static void SetupFromMenu() => SetupInternal(exitEditor: false);

    public static void Setup() => SetupInternal(exitEditor: true);

    static void SetupInternal(bool exitEditor)
    {
        try
        {
            SeedStreamingAssets();

            if (!File.Exists(FinalScenePath))
                throw new FileNotFoundException(FinalScenePath);

            var scene = EditorSceneManager.OpenScene(FinalScenePath, OpenSceneMode.Single);

            var stage = UnityEngine.Object.FindFirstObjectByType<ThreeOSFinalDemoStage>();
            if (stage == null)
                throw new Exception("FinalDemoStage missing — run 7.1 setup first.");

            var directorGo = GameObject.Find("FinalDemoActI");
            if (directorGo == null)
                directorGo = new GameObject("FinalDemoActI");

            var director = directorGo.GetComponent<ThreeOSFinalDemoActIDirector>()
                           ?? directorGo.AddComponent<ThreeOSFinalDemoActIDirector>();

            var npcRoot = stage.NpcRoot;
            if (npcRoot == null)
            {
                var npc = GameObject.Find("PerfNPC");
                npcRoot = npc != null ? npc.transform : null;
            }

            if (npcRoot == null)
                throw new Exception("PerfNPC missing");

            var floor = stage.NpcFloorParent;
            if (floor == null)
            {
                var f = GameObject.Find("NPC_Floor_Parent");
                floor = f != null ? f.transform : null;
            }

            var head = npcRoot.Find("NPC_Head") ?? FindDeep(npcRoot, "NPC_Head");
            var left = npcRoot.Find("NPC_Hand_L");
            var right = npcRoot.Find("NPC_Hand_R");
            var leftPose = left != null
                ? left.GetComponent<ThreeOSAndroidNpcHandPose>() ?? left.gameObject.AddComponent<ThreeOSAndroidNpcHandPose>()
                : null;
            var rightPose = right != null
                ? right.GetComponent<ThreeOSAndroidNpcHandPose>() ?? right.gameObject.AddComponent<ThreeOSAndroidNpcHandPose>()
                : null;

            if (leftPose != null && leftPose.HandRoot == null)
                leftPose.SetHandRoot(left);
            if (rightPose != null && rightPose.HandRoot == null)
                rightPose.SetHandRoot(right);

            var json01 = AssetDatabase.LoadAssetAtPath<TextAsset>(Perf01);
            var json02 = AssetDatabase.LoadAssetAtPath<TextAsset>(Perf02);
            var clip01 = AssetDatabase.LoadAssetAtPath<AudioClip>(Audio01);
            var clip02 = AssetDatabase.LoadAssetAtPath<AudioClip>(Audio02);
            if (json01 == null || json02 == null)
                throw new Exception("Performance JSON TextAssets missing — check Performances/");
            if (clip01 == null || clip02 == null)
                throw new Exception("jarvis_voice_clip_01/02 missing under Assets/Audio");

            var so = new SerializedObject(director);
            so.FindProperty("stage").objectReferenceValue = stage;
            so.FindProperty("npcFloorParent").objectReferenceValue = floor;
            so.FindProperty("npcRoot").objectReferenceValue = npcRoot;
            so.FindProperty("npcHead").objectReferenceValue = head;
            so.FindProperty("npcLeftHand").objectReferenceValue = leftPose;
            so.FindProperty("npcRightHand").objectReferenceValue = rightPose;
            so.FindProperty("slot01Json").objectReferenceValue = json01;
            so.FindProperty("slot02Json").objectReferenceValue = json02;
            so.FindProperty("slot01Audio").objectReferenceValue = clip01;
            so.FindProperty("slot02Audio").objectReferenceValue = clip02;
            so.ApplyModifiedPropertiesWithoutUndo();

            EditorSceneManager.MarkSceneDirty(scene);
            EditorSceneManager.SaveScene(scene);
            AssetDatabase.SaveAssets();
            Debug.Log("[3OS][7.4] Act I Speak/mic/EQ/playlist wired on FinalDemoActI.");
        }
        catch (Exception ex)
        {
            Debug.LogError($"[3OS][7.4] setup failed: {ex}");
            if (exitEditor)
                EditorApplication.Exit(1);
            throw;
        }

        if (exitEditor)
            EditorApplication.Exit(0);
    }

    static void SeedStreamingAssets()
    {
        Directory.CreateDirectory(StreamingDir);
        CopyIfNeeded(Perf01, Path.Combine(StreamingDir, "android_hands_perf_01.json"));
        CopyIfNeeded(Perf02, Path.Combine(StreamingDir, "android_hands_perf_02.json"));
        AssetDatabase.Refresh();
    }

    static void CopyIfNeeded(string srcAsset, string dstAsset)
    {
        var srcFull = Path.GetFullPath(srcAsset);
        var dstFull = Path.GetFullPath(dstAsset);
        if (!File.Exists(srcFull))
            throw new FileNotFoundException(srcAsset);
        if (File.Exists(dstFull))
        {
            var srcInfo = new FileInfo(srcFull);
            var dstInfo = new FileInfo(dstFull);
            if (srcInfo.Length == dstInfo.Length && srcInfo.LastWriteTimeUtc <= dstInfo.LastWriteTimeUtc)
                return;
        }

        File.Copy(srcFull, dstFull, overwrite: true);
        Debug.Log($"[3OS][7.5] StreamingAssets seeded {dstAsset}");
    }

    static Transform FindDeep(Transform root, string name)
    {
        if (root.name == name)
            return root;
        for (var i = 0; i < root.childCount; i++)
        {
            var f = FindDeep(root.GetChild(i), name);
            if (f != null)
                return f;
        }

        return null;
    }
}
#endif
