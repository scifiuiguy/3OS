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
    [Tooltip("XR Origin yaw = spawn yaw + this. Demo was ~180° off spawn forward.")]
    [SerializeField] float playerSpawnYawOffsetDeg = 180f;

    [Header("Mind Palace (Act II)")]
    [Tooltip("WispySky Mat2 (6-face) — applied over office geo when Palace wipe runs; not at cold open.")]
    [SerializeField] Material palaceSkybox;
    [Tooltip("WispyCubemap2 — seamless sample source for the Palace overlay (same sky set as Mat2).")]
    [SerializeField] Cubemap palaceSkyCubemap;

    [Header("Act I — GitHub plane")]
    [SerializeField] ThreeOSFinalDemoGitHubPlane githubPlane;

    /// <summary>Host mode flag for Task 7.7 — false = MR office, true = Palace/VR skybox.</summary>
    bool _inPalace;

    public Transform PlayerSpawn => playerSpawn;
    public Transform XrOrigin => xrOrigin;
    public Transform NpcFloorParent => npcFloorParent;
    public Transform NpcRoot => npcRoot;
    public Material PalaceSkybox => palaceSkybox;
    public Cubemap PalaceSkyCubemap => palaceSkyCubemap;
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
        AlignPlayerToSpawn();
        EnsureModeToggle();
        EnsureBlenderSpawn();
    }

    void Start()
    {
        // Bridge may spawn after Stage Awake (harness boot order).
        AlignPlayerToSpawn();
        EnsureModeToggle();
        EnsureBlenderSpawn();
    }

    /// <summary>
    /// Face spawn forward (+ optional yaw offset) without moving scene props / NPC / palace anchors.
    /// </summary>
    void AlignPlayerToSpawn()
    {
        if (xrOrigin == null || playerSpawn == null)
            return;
        var yaw = Quaternion.Euler(0f, playerSpawn.eulerAngles.y + playerSpawnYawOffsetDeg, 0f);
        xrOrigin.SetPositionAndRotation(
            new Vector3(playerSpawn.position.x, xrOrigin.position.y, playerSpawn.position.z),
            yaw);
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

        var contents = FindFirstObjectByType<ThreeOSFinalDemoPalaceContents>();
        if (contents == null)
        {
            var go = new GameObject("FinalDemoPalaceContents");
            contents = go.AddComponent<ThreeOSFinalDemoPalaceContents>();
        }

        contents.Configure(this, toggle);
    }

    void EnsureBlenderSpawn()
    {
        var spawn = FindFirstObjectByType<ThreeOSFinalDemoBlenderSpawn>();
        if (spawn == null)
        {
            var go = new GameObject("FinalDemoBlenderSpawn");
            spawn = go.AddComponent<ThreeOSFinalDemoBlenderSpawn>();
        }

        var storage = FindFirstObjectByType<ThreeOSStorageController>();
        var bridge = FindFirstObjectByType<ThreeOSBridge>();
        var input = FindFirstObjectByType<ThreeOSInputRouter>();
        var prefab = Resources.Load<GameObject>("3OS/BlenderGroup");
        spawn.Configure(this, storage, bridge, input, prefab);
    }
}
