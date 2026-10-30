using UnityEngine;
using UnityEngine.Events;

/// <summary>
/// World-locked GitHub page quad. Transform/material come from the scene; runtime only drives visibility.
/// </summary>
[DisallowMultipleComponent]
public sealed class ThreeOSFinalDemoGitHubPlane : MonoBehaviour
{
    [SerializeField] MeshRenderer meshRenderer;
    [SerializeField] bool visibleOnStart = true;
    [SerializeField] UnityEvent<bool> onVisibilityChanged;

    bool _mindPalaceSuppressed;
    bool _visibleBeforePalace;

    /// <summary>Palace skybox hides the quad but an enabled renderer still depth-occludes far-field glyphs.</summary>
    public bool MindPalaceSuppressed => _mindPalaceSuppressed;

    void Awake()
    {
        if (meshRenderer == null)
            meshRenderer = GetComponent<MeshRenderer>();
    }

    void Start()
    {
        SetRendererVisible(visibleOnStart);
        LogRenderState("Start");
    }

    public void SetRendererVisible(bool visible)
    {
        if (_mindPalaceSuppressed)
            return;
        ApplyRendererVisible(visible);
    }

    void ApplyRendererVisible(bool visible)
    {
        if (meshRenderer == null)
            meshRenderer = GetComponent<MeshRenderer>();
        if (meshRenderer == null)
        {
            Debug.LogError("[3OS] GitHubPagePlane: MeshRenderer missing.");
            return;
        }

        meshRenderer.enabled = visible;
        onVisibilityChanged?.Invoke(visible);
    }

    /// <summary>Called on MR↔Palace wipe — no GitHub toggle/occlusion in Mind Palace.</summary>
    public void SetMindPalaceSuppressed(bool suppressed)
    {
        if (suppressed == _mindPalaceSuppressed)
            return;
        _mindPalaceSuppressed = suppressed;
        if (suppressed)
        {
            _visibleBeforePalace = meshRenderer != null && meshRenderer.enabled;
            ApplyRendererVisible(false);
            gameObject.SetActive(false);
            Debug.Log("[3OS] GitHubPagePlane suppressed for Mind Palace");
            return;
        }

        gameObject.SetActive(true);
        ApplyRendererVisible(_visibleBeforePalace);
        Debug.Log($"[3OS] GitHubPagePlane restored after Palace (visible={_visibleBeforePalace})");
    }

    public void ToggleRendererVisible()
    {
        SetRendererVisible(!IsRendererVisible);
    }

    public bool IsRendererVisible => meshRenderer != null && meshRenderer.enabled;

    void LogRenderState(string context)
    {
        var mf = GetComponent<MeshFilter>();
        var mat = meshRenderer != null ? meshRenderer.sharedMaterial : null;
        Debug.Log(
            $"[3OS] GitHubPagePlane {context}: worldPos={transform.position} " +
            $"rendererEnabled={IsRendererVisible} hasMesh={mf != null && mf.sharedMesh != null} " +
            $"shader={(mat != null ? mat.shader.name : "null")}");
    }
}
