using System.Collections.Generic;
using ThreeOS;
using UnityEngine;

/// <summary>
/// Phase 0.7 Task 7.7 — keep-list vs Palace sky without per-mesh ShaderLab wars.
///
/// Product model: Mind Palace is a VR void. When settled in Palace, MR office roots are
/// hidden so transparent office materials cannot sort over the sky. Keep-list
/// (hands / cursors / HUD / mode glyph) stays active and is force-queued above the sky
/// during the wipe. This mirrors the portable half of Meta's compositor story: system
/// icons are OpenXR composition layers on top of the app; a sky *under* hands is done by
/// voiding the world (or a future equirect composition layer behind the eye buffer), not
/// by fighting every Transparent queue in the room.
/// </summary>
public sealed class ThreeOSFinalDemoPalaceLayering : MonoBehaviour
{
    /// <summary>Overlay queue — sky above office/world; keep-list draws after this.</summary>
    public const int SkyRenderQueue = 5000;
    /// <summary>Hands, cursors, HUD, mode glyph — exclusive allowlist above sky.</summary>
    public const int KeepRenderQueue = 5100;

    [Tooltip("MR world roots hidden while InPalace (auto-fills Office if empty).")]
    [SerializeField] GameObject[] hideInPalaceRoots;

    readonly Dictionary<Material, int> _savedQueues = new Dictionary<Material, int>();
    readonly List<Renderer> _keepRenderers = new List<Renderer>(64);
    bool _keepPromoted;

    public void Configure(GameObject[] roots)
    {
        if (roots != null && roots.Length > 0)
            hideInPalaceRoots = roots;
    }

    public void EnsureRoots()
    {
        if (hideInPalaceRoots != null && hideInPalaceRoots.Length > 0)
            return;
        var office = GameObject.Find("Office");
        if (office != null)
            hideInPalaceRoots = new[] { office };
    }

    /// <summary>Hide/show MR office (and similar) for settled Palace void.</summary>
    public void SetMrWorldVisible(bool visible)
    {
        EnsureRoots();
        if (hideInPalaceRoots == null)
            return;
        for (var i = 0; i < hideInPalaceRoots.Length; i++)
        {
            var go = hideInPalaceRoots[i];
            if (go != null && go.activeSelf != visible)
                go.SetActive(visible);
        }
    }

    public void ApplySkyMaterialQueue(Material skyMat)
    {
        if (skyMat == null)
            return;
        skyMat.renderQueue = SkyRenderQueue;
    }

    /// <summary>Bump hands/cursors/HUD/glyphs above sky for the wipe window.</summary>
    public void PromoteKeepList()
    {
        RefreshKeepRenderers();
        ApplyKeepQueueToRenderers(_keepRenderers);
        _keepPromoted = true;
    }

    /// <summary>
    /// 7.8 — Mind Palace FS glyphs/boxes + voodoo chrome use Geometry (~3000) and sit under
    /// the sky Overlay (5000 + ZTest Always). Promote them to the keep-list queue.
    /// </summary>
    public void PromotePalaceContentGlyphs()
    {
        PromotePalaceWorldChrome();
    }

    /// <summary>Glyphs, labels, voodoo wires/arrows/snap dots — anything that must clear the sky.</summary>
    public void PromotePalaceWorldChrome()
    {
        _palaceGlyphRends.Clear();
        var roots = FindObjectsByType<Transform>(FindObjectsSortMode.None);
        for (var i = 0; i < roots.Length; i++)
        {
            var t = roots[i];
            if (t == null)
                continue;
            var n = t.name;
            var isGlyphRoot = t.parent == null &&
                              (n.StartsWith("Glyph_", System.StringComparison.Ordinal) ||
                               n.StartsWith("Ghost_", System.StringComparison.Ordinal));
            var isClusterPivot = n.StartsWith("3OS_PalaceClusterPivot",
                System.StringComparison.Ordinal);
            var isVoodooChrome =
                n.StartsWith("3OS_Voodoo", System.StringComparison.Ordinal) ||
                n.StartsWith("3OS_InsertArrow", System.StringComparison.Ordinal) ||
                n.StartsWith("3OS_VoodooDragArrow", System.StringComparison.Ordinal) ||
                n.StartsWith("3OS_GhostDragArrow", System.StringComparison.Ordinal) ||
                n.StartsWith("3OS_PortalDismiss", System.StringComparison.Ordinal) ||
                n.StartsWith("3OS_PortalScissors", System.StringComparison.Ordinal) ||
                n.StartsWith("3OS_BoxDismiss", System.StringComparison.Ordinal) ||
                n.IndexOf("SnapDot", System.StringComparison.OrdinalIgnoreCase) >= 0 ||
                n.StartsWith("3OS_Arrow", System.StringComparison.Ordinal);
            if (!isGlyphRoot && !isClusterPivot && !isVoodooChrome)
                continue;
            // Voodoo chrome may nest — only root or matching named transforms.
            if (isVoodooChrome && t.parent != null &&
                t.parent.name.StartsWith("3OS_", System.StringComparison.Ordinal))
            {
                // Still include nested line/head children via GetComponentsInChildren from parent
                // when we hit the root; skip nested name matches to avoid duplicate work.
                if (!IsVoodooChromeRoot(t))
                    continue;
            }

            t.GetComponentsInChildren(true, _tmpRends);
            for (var r = 0; r < _tmpRends.Count; r++)
            {
                if (_tmpRends[r] != null && !_palaceGlyphRends.Contains(_tmpRends[r]))
                    _palaceGlyphRends.Add(_tmpRends[r]);
            }
        }

        ApplyKeepQueueToRenderers(_palaceGlyphRends);
        _tmpRends.Clear();
        _palaceGlyphRends.Clear();
        _keepPromoted = true;
    }

    static bool IsVoodooChromeRoot(Transform t)
    {
        if (t.parent == null)
            return true;
        var p = t.parent.name;
        return !(p.StartsWith("3OS_Voodoo", System.StringComparison.Ordinal) ||
                 p.StartsWith("3OS_InsertArrow", System.StringComparison.Ordinal) ||
                 p.StartsWith("3OS_VoodooDragArrow", System.StringComparison.Ordinal) ||
                 p.StartsWith("3OS_GhostDragArrow", System.StringComparison.Ordinal) ||
                 p.StartsWith("3OS_PortalDismiss", System.StringComparison.Ordinal) ||
                 p.StartsWith("3OS_PortalScissors", System.StringComparison.Ordinal) ||
                 p.StartsWith("3OS_BoxDismiss", System.StringComparison.Ordinal) ||
                 p.StartsWith("3OS_Arrow", System.StringComparison.Ordinal));
    }

    void ApplyKeepQueueToRenderers(List<Renderer> renderers)
    {
        for (var i = 0; i < renderers.Count; i++)
        {
            var r = renderers[i];
            if (r == null)
                continue;
            // Mutate shared materials in place — avoid r.materials instancing, which can
            // bleach runtime-owned arrow/wire colors while leaving the original at 3000.
            // Only bump UP to keep-list; never demote chrome already above (e.g. X/scissors
            // at 5120 so they draw over translucent dismiss shells at 5100).
            var shared = r.sharedMaterials;
            for (var m = 0; m < shared.Length; m++)
            {
                var mat = shared[m];
                if (mat == null || mat.renderQueue >= KeepRenderQueue)
                    continue;
                if (!_savedQueues.ContainsKey(mat))
                    _savedQueues[mat] = mat.renderQueue;
                mat.renderQueue = KeepRenderQueue;
            }
        }
    }

    public void RestoreKeepListQueues()
    {
        if (!_keepPromoted)
            return;
        foreach (var kv in _savedQueues)
        {
            if (kv.Key != null)
                kv.Key.renderQueue = kv.Value;
        }

        _savedQueues.Clear();
        _keepPromoted = false;
    }

    void RefreshKeepRenderers()
    {
        _keepRenderers.Clear();

        void AddGo(GameObject go)
        {
            if (go == null)
                return;
            go.GetComponentsInChildren(true, _tmpRends);
            for (var i = 0; i < _tmpRends.Count; i++)
            {
                if (_tmpRends[i] != null && !_keepRenderers.Contains(_tmpRends[i]))
                    _keepRenderers.Add(_tmpRends[i]);
            }

            _tmpRends.Clear();
        }

        // Mode glyph + open-book HUD (this hierarchy / siblings under ModeToggle).
        AddGo(gameObject);
        var hud = GameObject.Find("OpenBook_HUD");
        AddGo(hud);
        var glyph = GameObject.Find("ModeToggleGlyph");
        AddGo(glyph);
        var sky = GameObject.Find("PalaceSkyOverlay");
        // sky intentionally not keep-listed

        AddGo(GameObject.Find("3OS_LeftCursor"));
        AddGo(GameObject.Find("3OS_RightCursor"));

        var handVis = FindFirstObjectByType<ThreeOSHandVisuals>();
        if (handVis != null)
            AddGo(handVis.gameObject);

        // Meta / XR Hands mesh visuals under XR Origin.
        var stage = FindFirstObjectByType<ThreeOSFinalDemoStage>();
        if (stage != null && stage.XrOrigin != null)
        {
            var hands = stage.XrOrigin.GetComponentsInChildren<SkinnedMeshRenderer>(true);
            for (var i = 0; i < hands.Length; i++)
            {
                var n = hands[i].name;
                if (n.IndexOf("Hand", System.StringComparison.OrdinalIgnoreCase) >= 0 ||
                    n.IndexOf("OVR", System.StringComparison.OrdinalIgnoreCase) >= 0)
                {
                    if (!_keepRenderers.Contains(hands[i]))
                        _keepRenderers.Add(hands[i]);
                }
            }
        }

        // Any XRHandMeshController driven meshes.
        foreach (var mb in FindObjectsByType<MonoBehaviour>(FindObjectsSortMode.None))
        {
            if (mb == null)
                continue;
            var tn = mb.GetType().Name;
            if (tn.IndexOf("Hand", System.StringComparison.OrdinalIgnoreCase) < 0)
                continue;
            if (tn.IndexOf("Mesh", System.StringComparison.OrdinalIgnoreCase) < 0 &&
                tn.IndexOf("Visual", System.StringComparison.OrdinalIgnoreCase) < 0)
                continue;
            AddGo(mb.gameObject);
        }
    }

    readonly List<Renderer> _tmpRends = new List<Renderer>(32);
    readonly List<Renderer> _palaceGlyphRends = new List<Renderer>(128);

    void OnDisable()
    {
        RestoreKeepListQueues();
        SetMrWorldVisible(true);
    }
}
