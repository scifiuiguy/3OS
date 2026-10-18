using UnityEngine;

/// <summary>
/// Phase 0.7 Task 7.2 — world-fixed GitHub page plane.
/// Spawn: 1.5 ft ahead of XR Origin, 1 ft below eye height, 24" wide; one-shot billboard to eyes.
/// Visibility toggled by <see cref="ThreeOSFinalDemoGitHubGesture"/> (default ON).
/// </summary>
public sealed class ThreeOSFinalDemoGitHubPlane : MonoBehaviour
{
    const float WidthMeters = 24f * 0.0254f;   // 24"
    const float AheadMeters = 1.5f * 0.3048f;  // 1.5 ft
    const float BelowEyeMeters = 1f * 0.3048f; // 1 ft

    [SerializeField] Transform xrOrigin;
    [SerializeField] Transform eyeCamera;
    [SerializeField] Texture2D pageTexture;
    [SerializeField] bool startVisible = true;

    MeshRenderer _renderer;
    bool _posed;

    public void Configure(Transform origin, Transform eye, Texture2D tex)
    {
        xrOrigin = origin;
        eyeCamera = eye;
        pageTexture = tex;
        EnsureMeshAndMaterial();
        ApplyTexture();
        PoseOnce();
        SetVisible(startVisible);
    }

    void Awake()
    {
        EnsureMeshAndMaterial();
        ApplyTexture();
    }

    void Start()
    {
        if (!_posed)
            PoseOnce();
        SetVisible(startVisible);
    }

    public void SetVisible(bool visible) => gameObject.SetActive(visible);

    public void ToggleVisible() => SetVisible(!gameObject.activeSelf);

    public void PoseOnce()
    {
        if (xrOrigin == null)
            xrOrigin = GameObject.Find("XR Origin (VR)")?.transform;
        if (eyeCamera == null)
        {
            var cam = Camera.main;
            eyeCamera = cam != null ? cam.transform : null;
        }

        if (xrOrigin == null)
            return;

        var forward = xrOrigin.forward;
        forward.y = 0f;
        if (forward.sqrMagnitude < 1e-6f)
            forward = Vector3.forward;
        forward.Normalize();

        var eyeY = eyeCamera != null ? eyeCamera.position.y : xrOrigin.position.y + 1.6f;
        var pos = xrOrigin.position + forward * AheadMeters;
        pos.y = eyeY - BelowEyeMeters;
        transform.position = pos;

        // One-shot billboard toward eyes (not continuous).
        var eyePos = eyeCamera != null ? eyeCamera.position : pos - forward;
        var toEye = eyePos - pos;
        if (toEye.sqrMagnitude > 1e-6f)
            transform.rotation = Quaternion.LookRotation(toEye.normalized, Vector3.up);

        ScaleToTextureAspect();
        _posed = true;
    }

    void EnsureMeshAndMaterial()
    {
        // Unity missing-components are "fake null" — don't use ??.
        var mf = GetComponent<MeshFilter>();
        if (mf == null)
            mf = gameObject.AddComponent<MeshFilter>();
        _renderer = GetComponent<MeshRenderer>();
        if (_renderer == null)
            _renderer = gameObject.AddComponent<MeshRenderer>();
        if (mf.sharedMesh == null)
            mf.sharedMesh = BuildQuadMesh();

        if (_renderer.sharedMaterial == null)
        {
            var shader = Shader.Find("Unlit/Texture")
                         ?? Shader.Find("Universal Render Pipeline/Unlit")
                         ?? Shader.Find("Sprites/Default")
                         ?? Shader.Find("Standard");
            _renderer.sharedMaterial = new Material(shader);
        }
    }

    void ApplyTexture()
    {
        EnsureMeshAndMaterial();
        if (pageTexture == null)
            return;
        var mat = _renderer.material;
        mat.mainTexture = pageTexture;
        if (mat.HasProperty("_BaseMap"))
            mat.SetTexture("_BaseMap", pageTexture);
        ScaleToTextureAspect();
    }

    void ScaleToTextureAspect()
    {
        var aspect = pageTexture != null
            ? pageTexture.height / (float)Mathf.Max(1, pageTexture.width)
            : 976f / 1549f;
        transform.localScale = new Vector3(WidthMeters, WidthMeters * aspect, 1f);
    }

    static Mesh BuildQuadMesh()
    {
        // Unit quad in XY, facing +Z (toward viewer when LookRotation points +Z at eyes).
        var mesh = new Mesh { name = "GitHubPageQuad" };
        mesh.vertices = new[]
        {
            new Vector3(-0.5f, -0.5f, 0f),
            new Vector3(0.5f, -0.5f, 0f),
            new Vector3(-0.5f, 0.5f, 0f),
            new Vector3(0.5f, 0.5f, 0f)
        };
        mesh.uv = new[]
        {
            new Vector2(0f, 0f),
            new Vector2(1f, 0f),
            new Vector2(0f, 1f),
            new Vector2(1f, 1f)
        };
        mesh.triangles = new[] { 0, 2, 1, 2, 3, 1 };
        mesh.RecalculateNormals();
        mesh.RecalculateBounds();
        return mesh;
    }
}
