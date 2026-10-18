#if UNITY_EDITOR
using System;
using System.IO;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

/// <summary>
/// Phase 0.7 Task 7.2 — GitHub page plane + raw XR toggle in Final Demo scene.
/// Menu: 3OS / Final Demo / Setup 7.2 GitHub Plane
/// Batchmode: -executeMethod ThreeOSFinalDemo72Setup.Setup
/// </summary>
public static class ThreeOSFinalDemo72Setup
{
    const string FinalScenePath = "Assets/Scenes/3OS_Final_DemoScene.unity";
    const string TexturePath = "Assets/3OS_FinalDemo/Textures/github_3os_page.png";

    [MenuItem("3OS/Final Demo/Setup 7.2 GitHub Plane")]
    public static void SetupFromMenu() => SetupInternal(exitEditor: false);

    public static void Setup() => SetupInternal(exitEditor: true);

    static void SetupInternal(bool exitEditor)
    {
        try
        {
            if (!File.Exists(FinalScenePath))
                throw new FileNotFoundException(FinalScenePath);

            AssetDatabase.ImportAsset(TexturePath, ImportAssetOptions.ForceUpdate);
            var tex = AssetDatabase.LoadAssetAtPath<Texture2D>(TexturePath);
            if (tex == null)
                throw new Exception($"Missing texture {TexturePath} — run collage export first.");

            var importer = AssetImporter.GetAtPath(TexturePath) as TextureImporter;
            if (importer != null)
            {
                importer.textureType = TextureImporterType.Default;
                importer.mipmapEnabled = true;
                importer.sRGBTexture = true;
                importer.maxTextureSize = 2048;
                importer.SaveAndReimport();
                tex = AssetDatabase.LoadAssetAtPath<Texture2D>(TexturePath);
            }

            var scene = EditorSceneManager.OpenScene(FinalScenePath, OpenSceneMode.Single);

            var origin = GameObject.Find("XR Origin (VR)");
            if (origin == null)
                throw new Exception("XR Origin (VR) missing — run 7.1 setup first.");

            var eye = GameObject.Find("Main Camera");
            var eyeT = eye != null ? eye.transform : origin.transform;

            var existing = GameObject.Find("GitHubPagePlane");
            if (existing != null)
                UnityEngine.Object.DestroyImmediate(existing);

            var planeGo = new GameObject("GitHubPagePlane");
            var plane = planeGo.AddComponent<ThreeOSFinalDemoGitHubPlane>();
            plane.Configure(origin.transform, eyeT, tex);

            var gestureGo = GameObject.Find("GitHubPageGesture");
            if (gestureGo == null)
                gestureGo = new GameObject("GitHubPageGesture");
            var gesture = gestureGo.GetComponent<ThreeOSFinalDemoGitHubGesture>()
                          ?? gestureGo.AddComponent<ThreeOSFinalDemoGitHubGesture>();
            gesture.Configure(plane);

            var stage = UnityEngine.Object.FindFirstObjectByType<ThreeOSFinalDemoStage>();
            if (stage != null)
            {
                var so = new SerializedObject(stage);
                var prop = so.FindProperty("githubPlane");
                if (prop != null)
                {
                    prop.objectReferenceValue = plane;
                    so.ApplyModifiedPropertiesWithoutUndo();
                }
            }

            EditorSceneManager.MarkSceneDirty(scene);
            EditorSceneManager.SaveScene(scene, FinalScenePath);
            AssetDatabase.SaveAssets();
            Debug.Log("[3OS][7.2] GitHub plane wired (24in, 1.5ft ahead, spawn billboard, default ON).");
            if (exitEditor)
                EditorApplication.Exit(0);
        }
        catch (Exception ex)
        {
            Debug.LogError($"[3OS][7.2] Setup failed: {ex}");
            if (exitEditor)
                EditorApplication.Exit(1);
        }
    }
}
#endif
