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

    ulong _anchorId;

    public ulong AnchorId => _anchorId;

    void Awake()
    {
        if (bridge == null)
            bridge = FindObjectOfType<ThreeOSBridge>();
        if (stage == null)
            stage = FindObjectOfType<ThreeOSFinalDemoStage>();
        if (head == null)
            head = ResolveHead();
    }

    void OnEnable()
    {
        TryRegister();
    }

    void OnDisable()
    {
        Clear();
    }

    void LateUpdate()
    {
        if (bridge == null || !bridge.IsLoaded || head == null)
            return;
        if (_anchorId == 0)
        {
            TryRegister();
            return;
        }

        var pose = InteropStructs.ToOpenXrPose(new Pose(head.position, head.rotation));
        bridge.SetFocusAnchorPose(_anchorId, pose);
    }

    void TryRegister()
    {
        if (bridge == null || !bridge.IsLoaded || head == null || _anchorId != 0)
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
}
