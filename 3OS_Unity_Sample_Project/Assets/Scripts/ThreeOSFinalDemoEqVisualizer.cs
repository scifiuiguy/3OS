using UnityEngine;
using UnityEngine.Rendering;

/// <summary>Row of bottom-anchored cubes driven by mic band amplitudes (Task 7.4).</summary>
public sealed class ThreeOSFinalDemoEqVisualizer : MonoBehaviour
{
    const int BarCount = 10;
    const float BarWidth = 0.018f;
    const float BarDepth = 0.018f;
    const float BarGap = 0.006f;
    const float MaxHeight = 0.14f;
    const float MinHeight = 0.028f;
    const float SmoothSpeed = 11f; // was 22 — 50% slower chase
    const float AmplitudeScale = 0.5f; // damp visual throw by half

    Transform[] _bars;
    float[] _displayH;
    bool _built;
    Material _matInstance;

    public void ShowAbove(Transform widgetRoot)
    {
        EnsureBuilt();
        if (!gameObject.activeSelf)
            gameObject.SetActive(true);
        if (widgetRoot == null)
            return;
        transform.SetParent(null, false);
        transform.position = widgetRoot.position + widgetRoot.up * 0.14f + widgetRoot.forward * -0.02f;
        transform.rotation = widgetRoot.rotation;
    }

    public void Hide()
    {
        if (_bars != null)
        {
            for (var i = 0; i < _bars.Length; i++)
            {
                if (_displayH != null)
                    _displayH[i] = MinHeight;
                ApplyBarHeight(i, MinHeight);
            }
        }

        if (gameObject.activeSelf)
            gameObject.SetActive(false);
    }

    public void SetBands(float[] bands01)
    {
        EnsureBuilt();
        if (!gameObject.activeSelf)
            gameObject.SetActive(true);
        if (_bars == null || bands01 == null)
            return;

        var dt = Time.unscaledDeltaTime;
        for (var i = 0; i < _bars.Length; i++)
        {
            var v = i < bands01.Length ? Mathf.Clamp01(bands01[i] * AmplitudeScale) : 0f;
            var target = Mathf.Lerp(MinHeight, MaxHeight, v);
            _displayH[i] = Mathf.Lerp(_displayH[i], target, 1f - Mathf.Exp(-SmoothSpeed * dt));
            ApplyBarHeight(i, _displayH[i]);
        }
    }

    /// <summary>Flat low shimmer only — never a left→right ramp (that looked like fake EQ).</summary>
    public void PulseIdle()
    {
        EnsureBuilt();
        if (!gameObject.activeSelf)
            gameObject.SetActive(true);
        var shimmer = 0.08f + 0.06f * Mathf.Abs(Mathf.Sin(Time.unscaledTime * 2.2f));
        var bands = new float[BarCount];
        for (var i = 0; i < BarCount; i++)
            bands[i] = shimmer;
        SetBands(bands);
    }

    void ApplyBarHeight(int i, float h)
    {
        var bar = _bars[i];
        if (bar == null)
            return;
        bar.localScale = new Vector3(BarWidth, Mathf.Max(1e-4f, h), BarDepth);
        var x = bar.localPosition.x;
        bar.localPosition = new Vector3(x, h * 0.5f, 0f);
    }

    void EnsureBuilt()
    {
        if (_built)
            return;
        _built = true;
        _bars = new Transform[BarCount];
        _displayH = new float[BarCount];
        var mat = BarMaterial();
        var totalW = BarCount * BarWidth + (BarCount - 1) * BarGap;
        var x0 = -totalW * 0.5f + BarWidth * 0.5f;
        for (var i = 0; i < BarCount; i++)
        {
            var cube = GameObject.CreatePrimitive(PrimitiveType.Cube);
            cube.name = $"EQ_Bar_{i}";
            cube.transform.SetParent(transform, false);
            cube.transform.localPosition = new Vector3(x0 + i * (BarWidth + BarGap), MinHeight * 0.5f, 0f);
            cube.transform.localRotation = Quaternion.identity;
            cube.transform.localScale = new Vector3(BarWidth, MinHeight, BarDepth);
            var col = cube.GetComponent<Collider>();
            if (col != null)
                Destroy(col);
            var rend = cube.GetComponent<Renderer>();
            if (rend != null)
            {
                rend.sharedMaterial = mat;
                rend.shadowCastingMode = ShadowCastingMode.Off;
                rend.receiveShadows = false;
            }

            _bars[i] = cube.transform;
            _displayH[i] = MinHeight;
        }
    }

    Material BarMaterial()
    {
        if (_matInstance != null)
            return _matInstance;

        // Resources material keeps ThreeOS/EqBar in the player build (Shader.Find alone gets stripped).
        var template = Resources.Load<Material>("3OS/EqBar");
        if (template != null)
        {
            _matInstance = new Material(template)
            {
                name = "3OS_EQ_Bar_Runtime",
                hideFlags = HideFlags.HideAndDontSave
            };
            // Force black @ 30% — never inherit HandGhost blue.
            var tint = new Color(0f, 0f, 0f, 0.30f);
            if (_matInstance.HasProperty("_Color"))
                _matInstance.SetColor("_Color", tint);
            else
                _matInstance.color = tint;
            return _matInstance;
        }

        var shader = Shader.Find("ThreeOS/EqBar");
        if (shader == null)
        {
            Debug.LogError("[3OS][7.4] Missing ThreeOS/EqBar — EQ bars will be wrong color/invisible.");
            shader = Shader.Find("Unlit/Color");
        }

        _matInstance = new Material(shader)
        {
            name = "3OS_EQ_Bar_Runtime",
            hideFlags = HideFlags.HideAndDontSave,
            color = new Color(0f, 0f, 0f, 0.30f)
        };
        if (_matInstance.HasProperty("_Color"))
            _matInstance.SetColor("_Color", new Color(0f, 0f, 0f, 0.30f));
        _matInstance.renderQueue = (int)RenderQueue.Transparent;
        return _matInstance;
    }

    void OnDestroy()
    {
        if (_matInstance != null)
            Destroy(_matInstance);
    }
}
