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
    [Tooltip("WispySky (or swap) — applied over office geo when Palace wipe runs; not at cold open.")]
    [SerializeField] Material palaceSkybox;

    [Header("Act I — GitHub plane")]
    [SerializeField] ThreeOSFinalDemoGitHubPlane githubPlane;

    public Transform PlayerSpawn => playerSpawn;
    public Transform XrOrigin => xrOrigin;
    public Transform NpcFloorParent => npcFloorParent;
    public Transform NpcRoot => npcRoot;
    public Material PalaceSkybox => palaceSkybox;
    public ThreeOSFinalDemoGitHubPlane GithubPlane => githubPlane;

    /// <summary>Apply or clear the Palace skybox (used by future VR↔MR wipe).</summary>
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
}
