using System;
using System.Collections.Generic;
using UnityEngine;

/// <summary>JSON-serializable head+hands performance clip for the Android hands test scene.</summary>
[Serializable]
public sealed class ThreeOSAndroidPerfClip
{
    public int version = 1;
    public string audioClipName;
    public float durationSec;
    public List<ThreeOSAndroidPerfFrame> frames = new List<ThreeOSAndroidPerfFrame>();
}

[Serializable]
public sealed class ThreeOSAndroidPerfFrame
{
    public float t;
    public float[] headPos = new float[3];
    public float[] headRot = new float[4];
    public ThreeOSAndroidPerfHand left = new ThreeOSAndroidPerfHand();
    public ThreeOSAndroidPerfHand right = new ThreeOSAndroidPerfHand();
}

[Serializable]
public sealed class ThreeOSAndroidPerfHand
{
    public float[] rootPos = new float[3];
    public float[] rootRot = new float[4];
    public List<ThreeOSAndroidPerfHinge> hinges = new List<ThreeOSAndroidPerfHinge>();
}

[Serializable]
public sealed class ThreeOSAndroidPerfHinge
{
    public string joint;
    public float angleDeg;
}

public static class ThreeOSAndroidPerfClipUtil
{
    public static void SetVec3(float[] dst, Vector3 v)
    {
        dst[0] = v.x;
        dst[1] = v.y;
        dst[2] = v.z;
    }

    public static void SetQuat(float[] dst, Quaternion q)
    {
        dst[0] = q.x;
        dst[1] = q.y;
        dst[2] = q.z;
        dst[3] = q.w;
    }

    public static Vector3 GetVec3(float[] src)
    {
        if (src == null || src.Length < 3) return Vector3.zero;
        return new Vector3(src[0], src[1], src[2]);
    }

    public static Quaternion GetQuat(float[] src)
    {
        if (src == null || src.Length < 4) return Quaternion.identity;
        return new Quaternion(src[0], src[1], src[2], src[3]);
    }

    public static ThreeOSAndroidPerfHand FromCapture(in ThreeOSAndroidControllerHandBinder.HandCaptureSample sample, Transform space)
    {
        var hand = new ThreeOSAndroidPerfHand();
        var pos = space != null ? space.InverseTransformPoint(sample.RootPosition) : sample.RootPosition;
        var rot = space != null
            ? Quaternion.Inverse(space.rotation) * sample.RootRotation
            : sample.RootRotation;
        SetVec3(hand.rootPos, pos);
        SetQuat(hand.rootRot, rot);
        if (sample.Hinges != null)
        {
            for (var i = 0; i < sample.Hinges.Length; i++)
            {
                hand.hinges.Add(new ThreeOSAndroidPerfHinge
                {
                    joint = sample.Hinges[i].JointName,
                    angleDeg = sample.Hinges[i].AngleDeg
                });
            }
        }

        return hand;
    }
}
