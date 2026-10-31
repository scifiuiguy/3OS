using UnityEditor;
using UnityEngine;

public static class ThreeOSFinalDemoSplashMaterialSetup
{
    const string MaterialPath = "Assets/3OS_FinalDemo/UIToolkit/Splash_Material.mat";
    const string ShaderName = "ThreeOS/SplashOverlayUnlit";

    [MenuItem("3OS/Final Demo/Assign Splash Overlay Shader")]
    public static void AssignSplashOverlayShader()
    {
        var mat = AssetDatabase.LoadAssetAtPath<Material>(MaterialPath);
        if (mat == null)
        {
            Debug.LogError("[3OS] Splash material not found: " + MaterialPath);
            return;
        }

        var shader = Shader.Find(ShaderName);
        if (shader == null)
        {
            Debug.LogError("[3OS] Shader not found: " + ShaderName + " (wait for script compile, then retry)");
            return;
        }

        var mainTex = mat.HasProperty("_MainTex") ? mat.GetTexture("_MainTex") : null;
        var color = mat.HasProperty("_Color")
            ? mat.GetColor("_Color")
            : mat.HasProperty("_BaseColor") ? mat.GetColor("_BaseColor") : Color.white;

        mat.shader = shader;
        mat.renderQueue = (int)UnityEngine.Rendering.RenderQueue.Overlay;
        if (mainTex != null)
            mat.SetTexture("_MainTex", mainTex);
        mat.SetColor("_Color", color);

        EditorUtility.SetDirty(mat);
        AssetDatabase.SaveAssets();
        Debug.Log("[3OS] Assigned " + ShaderName + " to " + MaterialPath);
    }
}
