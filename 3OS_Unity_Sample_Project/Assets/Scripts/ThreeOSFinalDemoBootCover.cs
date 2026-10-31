using UnityEngine;
using UnityEngine.UI;

/// <summary>
/// Full-screen black overlay until XR boot yaw/XZ settle (covers Meta→Unity gap when OVR recenters).
/// </summary>
public static class ThreeOSFinalDemoBootCover
{
    const int SortOrder = 32760;

    static GameObject _root;
    static float _shownAtRealtime;
    static bool _released;

    public static bool IsCovering => _root != null && !_released;

    public static void Ensure()
    {
        if (_root != null || _released)
            return;

        _shownAtRealtime = Time.realtimeSinceStartup;
        _root = new GameObject("FinalDemoBootCover");
        Object.DontDestroyOnLoad(_root);

        var canvasGo = new GameObject("Canvas");
        canvasGo.transform.SetParent(_root.transform, false);
        var canvas = canvasGo.AddComponent<Canvas>();
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = SortOrder;
        canvasGo.AddComponent<CanvasScaler>();
        canvasGo.AddComponent<GraphicRaycaster>();

        var imgGo = new GameObject("Black");
        imgGo.transform.SetParent(canvasGo.transform, false);
        var img = imgGo.AddComponent<Image>();
        img.color = Color.black;
        img.raycastTarget = false;
        var rt = img.rectTransform;
        rt.anchorMin = Vector2.zero;
        rt.anchorMax = Vector2.one;
        rt.offsetMin = Vector2.zero;
        rt.offsetMax = Vector2.zero;

        Debug.Log($"[3OS] BootCover shown t={_shownAtRealtime:F3}s");
    }

    public static void Release(string reason)
    {
        if (_released)
            return;
        _released = true;
        var dt = Time.realtimeSinceStartup - _shownAtRealtime;
        if (_root != null)
        {
            Object.Destroy(_root);
            _root = null;
        }

        Debug.Log($"[3OS] BootCover release reason={reason} covered={dt:F3}s t={Time.realtimeSinceStartup:F3}s");
    }
}
