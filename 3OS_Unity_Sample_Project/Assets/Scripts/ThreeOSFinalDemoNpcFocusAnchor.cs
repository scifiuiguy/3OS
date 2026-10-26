using UnityEngine;
using ThreeOS;

/// <summary>
/// Phase 0.7 Task 7.3 — register NPC head as a world focus anchor and stream its pose to core.
/// Snap/free-range voodoo release on this anchor opens the agent Multi-Choice (no portal).
/// </summary>
public sealed class ThreeOSFinalDemoNpcFocusAnchor : MonoBehaviour
{
    [SerializeField] ThreeOSBridge bridge;
    [SerializeField] Transform head;
    [Tooltip("Optional; defaults to searching PerfNPC/NPC_Head under Stage.")]
    [SerializeField] ThreeOSFinalDemoStage stage;
    [SerializeField] ThreeOSFinalDemoPerfPlayer perfPlayer;
    [Tooltip("Body yaw parent — faces the player when voodoo opens the agent.")]
    [SerializeField] Transform floorParent;
    [SerializeField] float faceYawSeconds = 0.25f;

    ulong _anchorId;
    bool _suppressed;
    Coroutine _faceRoutine;

    public ulong AnchorId => _anchorId;

    void Awake()
    {
        if (bridge == null)
            bridge = FindObjectOfType<ThreeOSBridge>();
        if (stage == null)
            stage = FindObjectOfType<ThreeOSFinalDemoStage>();
        if (perfPlayer == null)
            perfPlayer = FindObjectOfType<ThreeOSFinalDemoPerfPlayer>();
        if (head == null)
            head = ResolveHead();
        if (floorParent == null)
            floorParent = ResolveFloor();
    }

    void OnEnable()
    {
        ThreeOSAgentSession.AgentOpened += OnAgentOpened;
        if (!_suppressed)
            TryRegister();
    }

    void OnDisable()
    {
        ThreeOSAgentSession.AgentOpened -= OnAgentOpened;
        if (_faceRoutine != null)
        {
            StopCoroutine(_faceRoutine);
            _faceRoutine = null;
        }

        Clear();
    }

    void OnAgentOpened()
    {
        if (floorParent == null)
            floorParent = ResolveFloor();
        if (floorParent == null)
            return;
        if (_faceRoutine != null)
            StopCoroutine(_faceRoutine);
        _faceRoutine = StartCoroutine(FacePlayerYawRoutine());
    }

    System.Collections.IEnumerator FacePlayerYawRoutine()
    {
        var cam = Camera.main;
        if (cam == null || floorParent == null)
            yield break;

        var to = cam.transform.position - floorParent.position;
        to.y = 0f;
        if (to.sqrMagnitude < 1e-6f)
            yield break;

        var target = Quaternion.LookRotation(to.normalized, Vector3.up);
        var start = floorParent.rotation;
        var dur = Mathf.Max(0.05f, faceYawSeconds);
        var t = 0f;
        while (t < 1f)
        {
            t += Time.deltaTime / dur;
            floorParent.rotation = Quaternion.Slerp(start, target, Mathf.SmoothStep(0f, 1f, t));
            yield return null;
        }

        floorParent.rotation = target;
        _faceRoutine = null;
    }

    void LateUpdate()
    {
        if (bridge == null || !bridge.IsLoaded || head == null)
            return;

        // NPC floor scaled away — drop the kernel anchor so it cannot sit as a floor blob
        // (CentroidDebug also skips FocusAnchor markers for the same reason).
        var dismissed = perfPlayer != null && perfPlayer.FloorDismissed;
        if (dismissed)
        {
            if (!_suppressed)
            {
                Clear();
                _suppressed = true;
            }

            return;
        }

        if (_suppressed)
        {
            _suppressed = false;
            TryRegister();
        }

        if (_anchorId == 0)
        {
            TryRegister();
            return;
        }

        var pose = InteropStructs.ToOpenXrPose(new Pose(head.position, head.rotation));
        // Act I reset remounts via StorageClear, which wipes core focus anchors. A stale
        // _anchorId would silently fail SetFocusAnchorPose forever — force re-register.
        if (!bridge.SetFocusAnchorPose(_anchorId, pose))
        {
            _anchorId = 0;
            TryRegister();
        }
    }

    /// <summary>Drop and re-create after Act I reset / StorageClear / NPC restore.</summary>
    public void ForceReregister()
    {
        _suppressed = false;
        Clear();
        TryRegister();
    }

    /// <summary>Clear without re-register (NPC dismissed / floor hidden).</summary>
    public void SuppressWhileNpcHidden()
    {
        _suppressed = true;
        Clear();
    }

    void TryRegister()
    {
        if (_suppressed || bridge == null || !bridge.IsLoaded || head == null || _anchorId != 0)
            return;
        var pose = InteropStructs.ToOpenXrPose(new Pose(head.position, head.rotation));
        _anchorId = bridge.RegisterFocusAnchor("npc_head", pose);
        if (_anchorId != 0)
            Debug.Log($"[3OS][7.3] NPC head focus anchor id={_anchorId}");
    }

    void Clear()
    {
        if (_anchorId == 0 || bridge == null || !bridge.IsLoaded)
        {
            _anchorId = 0;
            return;
        }
        bridge.ClearFocusAnchor(_anchorId);
        _anchorId = 0;
    }

    Transform ResolveHead()
    {
        if (stage != null && stage.NpcRoot != null)
        {
            var t = stage.NpcRoot.Find("NPC_Head");
            if (t != null)
                return t;
            t = stage.NpcRoot.Find("NPC_Head/NPC_Head_Scale_Node");
            if (t != null)
                return t;
        }

        var go = GameObject.Find("NPC_Head");
        return go != null ? go.transform : null;
    }

    Transform ResolveFloor()
    {
        if (stage != null && stage.NpcFloorParent != null)
            return stage.NpcFloorParent;
        if (stage != null && stage.NpcRoot != null)
            return stage.NpcRoot;
        var go = GameObject.Find("NPC_Floor_Parent");
        return go != null ? go.transform : null;
    }
}
