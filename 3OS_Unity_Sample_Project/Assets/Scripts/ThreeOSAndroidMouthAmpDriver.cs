using UnityEngine;

/// <summary>
/// Scales a Mouth transform's local Y from AudioSource amplitude (lip-flap stand-in).
/// Driven only while <see cref="SetDriving"/> is enabled (playback in the test scene).
/// </summary>
public sealed class ThreeOSAndroidMouthAmpDriver : MonoBehaviour
{
    [SerializeField] Transform mouth;
    [SerializeField] AudioSource audioSource;

    [Tooltip("Y scale at silence (relative to rest localScale.y).")]
    [SerializeField] float closedYMul = 0.35f;

    [Tooltip("Y scale at peak amplitude (relative to rest localScale.y). 200% = fully open.")]
    [SerializeField] float openYMul = 4f;

    [SerializeField] float amplitudeGain = 48f;
    [SerializeField] float smoothSpeed = 28f;
    [SerializeField] int sampleCount = 256;

    Vector3 _restScale = Vector3.one;
    float _currentYMul = 1f;
    bool _driving;
    bool _hasRest;
    Transform _restMouth;
    float[] _samples;

    public void Configure(Transform mouthTransform, AudioSource source)
    {
        var mouthChanged = mouth != mouthTransform;
        mouth = mouthTransform;
        audioSource = source;
        // Force intended range — scene serialization can leave stale openYMul≤1.35.
        closedYMul = 0.35f;
        openYMul = 4f;
        amplitudeGain = 48f;
        smoothSpeed = 28f;
        // Only cache authored rest once per Mouth. Re-caching mid-drive (Speak during
        // clip 01) baked the open scale as the new rest → wider mouth every retry.
        if (!_hasRest || mouthChanged || _restMouth != mouth)
        {
            if (_driving)
                ResetToRest();
            CacheRest(force: true);
        }
    }

    public void SetDriving(bool driving)
    {
        _driving = driving;
        if (mouth == null)
            return;
        if (!_driving)
        {
            ResetToRest();
            return;
        }

        // Snap closed so the first open is obvious (at least 50% Y travel to peak).
        _currentYMul = closedYMul;
        ApplyY(_currentYMul);
    }

    /// <summary>Force mouth back to authored rest scale (call when a performance cue ends).</summary>
    public void ResetToRest()
    {
        _driving = false;
        _currentYMul = 1f;
        if (mouth == null)
            return;
        if (!_hasRest || _restScale.sqrMagnitude < 1e-8f)
            CacheRest(force: true);
        mouth.localScale = _restScale;
    }

    void Awake()
    {
        closedYMul = 0.35f;
        openYMul = 4f;
        amplitudeGain = 48f;
        smoothSpeed = 28f;
        if (mouth == null && transform != null)
            mouth = FindNamed(transform.root != null ? transform.root : transform, "Mouth");
        CacheRest(force: true);
        _samples = new float[Mathf.ClosestPowerOfTwo(Mathf.Max(64, sampleCount))];
    }

    void LateUpdate()
    {
        if (!_driving || mouth == null || audioSource == null)
            return;

        if (!audioSource.isPlaying || audioSource.clip == null)
        {
            _currentYMul = Mathf.Lerp(_currentYMul, closedYMul, Time.deltaTime * smoothSpeed);
            ApplyY(_currentYMul);
            return;
        }

        if (_samples == null || _samples.Length != sampleCount)
            _samples = new float[Mathf.ClosestPowerOfTwo(Mathf.Max(64, sampleCount))];

        audioSource.GetOutputData(_samples, 0);
        var sum = 0f;
        for (var i = 0; i < _samples.Length; i++)
        {
            var s = _samples[i];
            sum += s * s;
        }

        var rms = Mathf.Sqrt(sum / _samples.Length);
        var norm = Mathf.Clamp01(rms * amplitudeGain);
        var targetMul = Mathf.Lerp(closedYMul, openYMul, norm);
        _currentYMul = Mathf.Lerp(_currentYMul, targetMul, Time.deltaTime * smoothSpeed);
        ApplyY(_currentYMul);
    }

    void CacheRest(bool force = false)
    {
        if (mouth == null)
            return;
        if (_hasRest && !force && _restMouth == mouth)
            return;
        // Never sample rest while lip-flap is driving — that freezes an open pose.
        if (_driving && _hasRest && _restMouth == mouth && !force)
            return;
        _restScale = mouth.localScale;
        if (_restScale.y < 1e-4f)
            _restScale.y = 1f;
        _restMouth = mouth;
        _hasRest = true;
    }

    void ApplyY(float yMul)
    {
        var s = _restScale;
        s.y = _restScale.y * yMul;
        mouth.localScale = s;
    }

    public static Transform FindNamed(Transform root, string name)
    {
        if (root == null)
            return null;
        if (root.name == name)
            return root;
        for (var i = 0; i < root.childCount; i++)
        {
            var found = FindNamed(root.GetChild(i), name);
            if (found != null)
                return found;
        }

        return null;
    }
}
