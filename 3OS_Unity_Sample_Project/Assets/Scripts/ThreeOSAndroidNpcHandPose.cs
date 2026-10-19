using System.Collections.Generic;
using UnityEngine;

/// <summary>
/// Applies recorded SegHand root pose + 1-DOF hinge angles to an NPC hand (no XR drive).
/// </summary>
public sealed class ThreeOSAndroidNpcHandPose : MonoBehaviour
{
    [SerializeField] Transform handRoot;
    [SerializeField] Vector3 hingeAxisLocal = Vector3.left;

    readonly Dictionary<string, HingeBone> _hinges = new Dictionary<string, HingeBone>();
    bool _bound;

    sealed class HingeBone
    {
        public Transform Bone;
        public Quaternion RestLocal;
    }

    public Transform HandRoot => handRoot;

    public void SetHandRoot(Transform root)
    {
        handRoot = root;
        _bound = false;
        _hinges.Clear();
        Bind();
    }

    void Awake() => Bind();

    void Bind()
    {
        if (handRoot == null || _bound)
            return;

        foreach (var anim in handRoot.GetComponentsInChildren<Animator>(true))
            anim.enabled = false;

        foreach (var t in handRoot.GetComponentsInChildren<Transform>(true))
        {
            if (t.name.StartsWith("SegJoint_") || t.name.StartsWith("SegPart_"))
                continue;
            if (!LooksLikeJointName(t.name))
                continue;
            _hinges[t.name] = new HingeBone
            {
                Bone = t,
                RestLocal = t.localRotation
            };
        }

        _bound = true;
    }

    static bool LooksLikeJointName(string name)
    {
        return name.Contains("Thumb") || name.Contains("Index") || name.Contains("Middle")
               || name.Contains("Ring") || name.Contains("Little");
    }

    /// <summary>
    /// Applies clip data authored in XR Origin local space as local pose under the NPC root
    /// (NPC faces the user with Y=180; left/right are not mirrored).
    /// </summary>
    public void ApplyLocal(ThreeOSAndroidPerfHand hand)
    {
        if (handRoot == null || hand == null)
            return;
        if (!_bound)
            Bind();

        handRoot.localPosition = ThreeOSAndroidPerfClipUtil.GetVec3(hand.rootPos);
        handRoot.localRotation = ThreeOSAndroidPerfClipUtil.GetQuat(hand.rootRot);

        var axis = hingeAxisLocal.sqrMagnitude > 1e-8f ? hingeAxisLocal.normalized : Vector3.left;
        if (hand.hinges == null)
            return;

        for (var i = 0; i < hand.hinges.Count; i++)
        {
            var h = hand.hinges[i];
            if (string.IsNullOrEmpty(h.joint) || !_hinges.TryGetValue(h.joint, out var bone))
                continue;
            bone.Bone.localRotation = bone.RestLocal * Quaternion.AngleAxis(h.angleDeg, axis);
        }
    }

    public ThreeOSAndroidPerfHand CaptureLocal()
    {
        if (!_bound)
            Bind();
        var hand = new ThreeOSAndroidPerfHand();
        if (handRoot == null)
            return hand;

        ThreeOSAndroidPerfClipUtil.SetVec3(hand.rootPos, handRoot.localPosition);
        ThreeOSAndroidPerfClipUtil.SetQuat(hand.rootRot, handRoot.localRotation);
        var axis = hingeAxisLocal.sqrMagnitude > 1e-8f ? hingeAxisLocal.normalized : Vector3.left;
        foreach (var pair in _hinges)
        {
            var bone = pair.Value;
            var delta = Quaternion.Inverse(bone.RestLocal) * bone.Bone.localRotation;
            delta.ToAngleAxis(out var ang, out var ax);
            if (ang > 180f)
                ang -= 360f;
            if (Vector3.Dot(ax, axis) < 0f)
                ang = -ang;
            hand.hinges.Add(new ThreeOSAndroidPerfHinge
            {
                joint = pair.Key,
                angleDeg = ang
            });
        }

        return hand;
    }

    public void ApplyLocalBlend(ThreeOSAndroidPerfHand from, ThreeOSAndroidPerfHand to, float t01)
    {
        if (handRoot == null || from == null || to == null)
            return;
        if (!_bound)
            Bind();

        var t = Mathf.Clamp01(t01);
        handRoot.localPosition = Vector3.Lerp(
            ThreeOSAndroidPerfClipUtil.GetVec3(from.rootPos),
            ThreeOSAndroidPerfClipUtil.GetVec3(to.rootPos),
            t);
        handRoot.localRotation = Quaternion.Slerp(
            ThreeOSAndroidPerfClipUtil.GetQuat(from.rootRot),
            ThreeOSAndroidPerfClipUtil.GetQuat(to.rootRot),
            t);

        var axis = hingeAxisLocal.sqrMagnitude > 1e-8f ? hingeAxisLocal.normalized : Vector3.left;
        var fromMap = HingeMap(from);
        var toMap = HingeMap(to);
        foreach (var pair in _hinges)
        {
            fromMap.TryGetValue(pair.Key, out var a0);
            toMap.TryGetValue(pair.Key, out var a1);
            var ang = Mathf.Lerp(a0, a1, t);
            pair.Value.Bone.localRotation = pair.Value.RestLocal * Quaternion.AngleAxis(ang, axis);
        }
    }

    static Dictionary<string, float> HingeMap(ThreeOSAndroidPerfHand hand)
    {
        var map = new Dictionary<string, float>();
        if (hand?.hinges == null)
            return map;
        for (var i = 0; i < hand.hinges.Count; i++)
        {
            var h = hand.hinges[i];
            if (!string.IsNullOrEmpty(h.joint))
                map[h.joint] = h.angleDeg;
        }

        return map;
    }

    public void ResetToRest()
    {
        if (!_bound)
            Bind();
        foreach (var pair in _hinges)
            pair.Value.Bone.localRotation = pair.Value.RestLocal;
    }
}
