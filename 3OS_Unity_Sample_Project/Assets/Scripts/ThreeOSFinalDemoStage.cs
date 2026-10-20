using ThreeOS;
using UnityEngine;

/// <summary>
/// Phase 0.7 staging refs for the Final Demo scene.
/// Act I/III: office geo = fake passthrough (no real PT). Palace skybox is applied later by the wipe (7.7).
/// </summary>
public sealed class ThreeOSFinalDemoStage : MonoBehaviour
{
    [Header("Placement")]
    [SerializeField] Transform playerSpawn;
    [SerializeField] Transform xrOrigin;
    [SerializeField] Transform npcFloorParent;
    [SerializeField] Transform npcRoot;

    [Header("Mind Palace (Act II)")]
    [Tooltip("WispySky Mat2 (6-face) — applied over office geo when Palace wipe runs; not at cold open.")]
    [SerializeField] Material palaceSkybox;

    [Header("Act I — GitHub plane")]
    [SerializeField] ThreeOSFinalDemoGitHubPlane githubPlane;

    /// <summary>Host mode flag for Task 7.7 — false = MR office, true = Palace/VR skybox.</summary>
    bool _inPalace;

    public Transform PlayerSpawn => playerSpawn;
    public Transform XrOrigin => xrOrigin;
    public Transform NpcFloorParent => npcFloorParent;
    public Transform NpcRoot => npcRoot;
    public Material PalaceSkybox => palaceSkybox;
    public ThreeOSFinalDemoGitHubPlane GithubPlane => githubPlane;
    public bool InPalace => _inPalace;

    /// <summary>
    /// Ambient skybox only. Visible Palace cover is the camera-locked overlay in
    /// <see cref="ThreeOSFinalDemoModeToggle"/> (draws over office geo without changing mesh queues).
    /// </summary>
    public void SetPalaceSkyboxActive(bool active)
    {
        if (active)
        {
            if (palaceSkybox != null)
                RenderSettings.skybox = palaceSkybox;
        }
        else if (palaceSkybox != null && RenderSettings.skybox == palaceSkybox)
        {
            RenderSettings.skybox = null;
        }
    }

    /// <summary>Set MR/Palace mode. Overlay settle is owned by ModeToggle wipe / sync.</summary>
    public void SetInPalace(bool inPalace)
    {
        _inPalace = inPalace;
        SetPalaceSkyboxActive(inPalace);
    }

    void Awake()
    {
        // Cold open is MR; Task 7.7 toggle owns later transitions.
        SetPalaceSkyboxActive(_inPalace);
        EnsureModeToggle();
    }

    void Start()
    {
        // Bridge may spawn after Stage Awake (harness boot order).
        EnsureModeToggle();
    }

    void EnsureModeToggle()
    {
        var toggle = FindFirstObjectByType<ThreeOSFinalDemoModeToggle>();
        if (toggle == null)
        {
            var go = new GameObject("FinalDemoModeToggle");
            toggle = go.AddComponent<ThreeOSFinalDemoModeToggle>();
        }

        var bridge = FindFirstObjectByType<ThreeOSBridge>();
        toggle.Configure(this, bridge);
    }
}
