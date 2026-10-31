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
        if (FindFirstObjectByType<SplashController>() == null)
            ThreeOSFinalDemoBootCover.Ensure();
        // Cold open is MR; Task 7.7 toggle owns later transitions.
        SetPalaceSkyboxActive(_inPalace);
        EnsureModeToggle();
        EnsureBlenderSpawn();
        EnsureChairCullFix();
        EnsureStandingSeatedLayout();
        EnsureXROriginFacing();
        EnsureXROriginPlacement();
        // XZ before splash reorient (yaw: SplashController → XROriginFacing).
        AlignPlayerToSeatedStart();
    }

    void Start()
    {
        // Bridge may spawn after Stage Awake (harness boot order).
        EnsureModeToggle();
        EnsureBlenderSpawn();
        EnsureChairCullFix();
        EnsureStandingSeatedLayout();
        EnsureXROriginFacing();
        EnsureXROriginPlacement();
    }

    /// <summary>
    /// Place XR Origin at Chair_Seated_Start (+ yaw offset). Player never spawns at Chair_Standing_Start.
    /// </summary>
    void AlignPlayerToSpawn() => AlignPlayerToSeatedStart();

    /// <summary>Chair_Seated_Start XZ only; yaw via <see cref="ThreeOSFinalDemoXROriginFacing"/>.</summary>
    public void AlignPlayerToSeatedStart() => AlignPlayerPositionToSeatedStart();

    /// <summary>
    /// Move XR Origin (rig root) so camera XZ matches Chair_Seated_Start. OVR may override — see XROriginPlacement LateUpdate.
    /// </summary>
    public void AlignPlayerPositionToSeatedStart()
    {
        if (ThreeOSFinalDemoXROriginPlacement.TryPlaceXROriginForCameraAtMarker(
                "Chair_Seated_Start", out _, out _, out _))
            return;

        EnsurePlacementRefs();
        var positionMarker = GameObject.Find("Chair_Seated_Start")?.transform;
        if (xrOrigin != null && positionMarker != null)
            AlignPlayerPositionToMarker(positionMarker);
    }

    /// <summary>Flat forward from Player_Spawn_Point (transform.forward, no rig yaw offset).</summary>
    public Vector3 GetAuthoredViewForward()
    {
        EnsurePlacementRefs();
        var marker = playerSpawn != null ? playerSpawn : GameObject.Find("Chair_Seated_Start")?.transform;
        if (marker == null)
            return Vector3.forward;
        var forward = marker.forward;
        forward.y = 0f;
        return forward.sqrMagnitude > 1e-6f ? forward.normalized : Vector3.forward;
    }

    void EnsurePlacementRefs()
    {
        if (xrOrigin == null)
        {
            var go = GameObject.Find("XR Origin (VR)");
            if (go != null)
                xrOrigin = go.transform;
        }

        if (playerSpawn == null)
        {
            var go = GameObject.Find("Player_Spawn_Point");
            if (go != null)
                playerSpawn = go.transform;
        }
    }

    public void AlignPlayerPositionToMarker(Transform positionMarker)
    {
        EnsurePlacementRefs();
        if (xrOrigin == null || positionMarker == null)
            return;
        xrOrigin.position = new Vector3(positionMarker.position.x, xrOrigin.position.y, positionMarker.position.z);
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

    void EnsureChairCullFix()
    {
        if (FindFirstObjectByType<ThreeOSFinalDemoChairCullFix>() != null)
            return;
        var go = new GameObject("FinalDemoChairCullFix");
        go.AddComponent<ThreeOSFinalDemoChairCullFix>();
    }

    void EnsureStandingSeatedLayout()
    {
        if (FindFirstObjectByType<ThreeOSFinalDemoStandingSeatedLayout>() != null)
            return;
        var go = new GameObject("FinalDemoStandingSeatedLayout");
        go.AddComponent<ThreeOSFinalDemoStandingSeatedLayout>();
    }

    void EnsureXROriginFacing()
    {
        if (FindFirstObjectByType<ThreeOSFinalDemoXROriginFacing>() != null)
            return;
        var go = new GameObject("FinalDemoXROriginFacing");
        go.AddComponent<ThreeOSFinalDemoXROriginFacing>();
    }

    void EnsureXROriginPlacement()
    {
        if (FindFirstObjectByType<ThreeOSFinalDemoXROriginPlacement>() != null)
            return;
        var go = new GameObject("FinalDemoXROriginPlacement");
        go.AddComponent<ThreeOSFinalDemoXROriginPlacement>();
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
