#if UNITY_EDITOR
using System;
using System.IO;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

/// <summary>
/// Parents Mind Palace Left/Right Hand Tracking under XR Origin Camera Offset
/// so they follow the yawed spawn rig (and show in the Final Demo hierarchy).
/// Menu: 3OS / Final Demo / Setup Hand Visuals Under XR Origin
/// Batchmode: -executeMethod ThreeOSFinalDemoHandVisualsSetup.Setup
/// </summary>
public static class ThreeOSFinalDemoHandVisualsSetup
{
    const string OriginPrefabPath = "Assets/3OS_FinalDemo/Prefabs/XR_Origin_FinalDemo.prefab";
    const string LeftHandPath = "Assets/Resources/3OS/Hands/LeftHandTracking.prefab";
    const string RightHandPath = "Assets/Resources/3OS/Hands/RightHandTracking.prefab";
    const string FinalScenePath = "Assets/Scenes/3OS_Final_DemoScene.unity";

    [MenuItem("3OS/Final Demo/Setup Hand Visuals Under XR Origin")]
    public static void SetupFromMenu() => SetupInternal(exitEditor: false);

    public static void Setup() => SetupInternal(exitEditor: true);

    static void SetupInternal(bool exitEditor)
    {
        try
        {
            if (!File.Exists(OriginPrefabPath))
                throw new FileNotFoundException(OriginPrefabPath);
            if (!File.Exists(LeftHandPath) || !File.Exists(RightHandPath))
                throw new FileNotFoundException("Missing Resources/3OS/Hands Left/RightHandTracking prefabs");

            var root = PrefabUtility.LoadPrefabContents(OriginPrefabPath);
            try
            {
                var cameraOffset = root.transform.Find("Camera Offset");
                if (cameraOffset == null)
                    throw new Exception("Camera Offset missing on XR_Origin_FinalDemo");

                PlaceHand(cameraOffset, LeftHandPath, "Left Hand Tracking");
                PlaceHand(cameraOffset, RightHandPath, "Right Hand Tracking");

                PrefabUtility.SaveAsPrefabAsset(root, OriginPrefabPath);
                Debug.Log(
                    "[3OS] Hand visuals parented under XR_Origin_FinalDemo / Camera Offset " +
                    "(Left Hand Tracking, Right Hand Tracking).");
            }
            finally
            {
                PrefabUtility.UnloadPrefabContents(root);
            }

            // Touch Final Demo so the scene instance picks up prefab children on next open.
            if (File.Exists(FinalScenePath))
            {
                var scene = EditorSceneManager.OpenScene(FinalScenePath, OpenSceneMode.Single);
                EditorSceneManager.MarkSceneDirty(scene);
                EditorSceneManager.SaveScene(scene);
            }

            AssetDatabase.SaveAssets();
            AssetDatabase.Refresh();
        }
        catch (Exception ex)
        {
            Debug.LogError($"[3OS] Hand visuals setup failed: {ex}");
            if (exitEditor)
                EditorApplication.Exit(1);
            throw;
        }

        if (exitEditor)
            EditorApplication.Exit(0);
    }

    static void PlaceHand(Transform cameraOffset, string prefabPath, string objectName)
    {
        // Remove prior copies (runtime or previous setup).
        for (var i = cameraOffset.childCount - 1; i >= 0; i--)
        {
            var child = cameraOffset.GetChild(i);
            if (child.name == objectName ||
                child.name == "3OS_LeftHandMesh" ||
                child.name == "3OS_RightHandMesh" ||
                child.name == "3OS_Runtime_" + objectName)
            {
                UnityEngine.Object.DestroyImmediate(child.gameObject);
            }
        }

        var prefab = AssetDatabase.LoadAssetAtPath<GameObject>(prefabPath);
        if (prefab == null)
            throw new Exception($"Failed to load {prefabPath}");

        var instance = (GameObject)PrefabUtility.InstantiatePrefab(prefab, cameraOffset);
        instance.name = objectName;
        instance.transform.localPosition = Vector3.zero;
        instance.transform.localRotation = Quaternion.identity;
        instance.transform.localScale = Vector3.one;
    }
}
#endif
