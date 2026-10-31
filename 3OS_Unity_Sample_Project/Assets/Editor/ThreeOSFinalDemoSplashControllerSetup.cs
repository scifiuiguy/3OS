using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.UIElements;

/// <summary>Wires SplashController on Main Camera hierarchy (UIDocument + SplashPlane refs).</summary>
public static class ThreeOSFinalDemoSplashControllerSetup
{
    const string SplashMaterialPath = "Assets/3OS_FinalDemo/UIToolkit/Splash_Material.mat";
    const string UnrealGroupPrefabPath = "Assets/3OS_FinalDemo/Prefabs/Unreal Group.prefab";
    const string ColleagueGroupPrefabPath = "Assets/3OS_FinalDemo/Prefabs/Colleague_Group.prefab";

    [MenuItem("3OS/Final Demo/Wire SplashController on Main Camera")]
    public static void WireSplashController()
    {
        Transform splashT = null;
        var xr = GameObject.Find("XR Origin (VR)");
        if (xr != null)
            splashT = xr.transform.Find("Camera Offset/Main Camera/SplashController");
        if (splashT == null)
        {
            var cam = GameObject.Find("Main Camera");
            splashT = cam != null ? cam.transform.Find("SplashController") : null;
        }

        if (splashT == null)
            splashT = GameObject.Find("SplashController")?.transform;
        var splashGo = splashT != null ? splashT.gameObject : null;
        if (splashGo == null)
        {
            Debug.LogError("[3OS] Wire SplashController: SplashController child not found under Main Camera");
            return;
        }

        var controller = splashGo.GetComponent<SplashController>();
        if (controller == null)
            controller = splashGo.AddComponent<SplashController>();

        var doc = splashGo.GetComponent<UIDocument>();
        if (doc == null)
            doc = splashGo.AddComponent<UIDocument>();

        var plane = splashGo.transform.Find("SplashPlane");
        if (plane == null)
            plane = splashGo.transform.Find("Splash_Plane");

        var mat = AssetDatabase.LoadAssetAtPath<Material>(SplashMaterialPath);

        var so = new SerializedObject(controller);
        so.FindProperty("uiDocument").objectReferenceValue = doc;
        if (plane != null)
            so.FindProperty("splashPlane").objectReferenceValue = plane;
        if (mat != null)
            so.FindProperty("splashMaterial").objectReferenceValue = mat;

        var unrealPrefab = AssetDatabase.LoadAssetAtPath<GameObject>(UnrealGroupPrefabPath);
        var colleaguePrefab = AssetDatabase.LoadAssetAtPath<GameObject>(ColleagueGroupPrefabPath);
        if (unrealPrefab != null)
            so.FindProperty("unrealGroupPrefab").objectReferenceValue = unrealPrefab;
        else
            Debug.LogWarning("[3OS] Wire SplashController: missing " + UnrealGroupPrefabPath);
        if (colleaguePrefab != null)
            so.FindProperty("colleagueGroupPrefab").objectReferenceValue = colleaguePrefab;
        else
            Debug.LogWarning("[3OS] Wire SplashController: missing " + ColleagueGroupPrefabPath);

        so.ApplyModifiedPropertiesWithoutUndo();

        EditorSceneManager.MarkSceneDirty(splashGo.scene);
        Debug.Log("[3OS] SplashController wired on " + splashGo.name);
    }
}
