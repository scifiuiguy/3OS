using UnityEngine;

/// <summary>
/// Audio-thread tap on the mic monitor AudioSource. Quest/Android often returns
/// silence from Microphone.GetData / GetOutputData unless the clip is playing through
/// an AudioSource; OnAudioFilterRead still sees the PCM.
/// </summary>
public sealed class ThreeOSFinalDemoMicTap : MonoBehaviour
{
    readonly object _gate = new object();
    float _rms;
    float _peak;
    readonly float[] _bandEnergy = new float[10];
    int _frameCounter;

    public void ResetLevels()
    {
        lock (_gate)
        {
            _rms = 0f;
            _peak = 0f;
            for (var i = 0; i < _bandEnergy.Length; i++)
                _bandEnergy[i] = 0f;
        }
    }

    public void Read(out float rms, float[] bands01)
    {
        lock (_gate)
        {
            rms = _rms;
            if (bands01 == null)
                return;
            var n = Mathf.Min(bands01.Length, _bandEnergy.Length);
            var peak = Mathf.Max(_peak, 1e-8f);
            for (var i = 0; i < n; i++)
                bands01[i] = Mathf.Clamp01(_bandEnergy[i] / peak);
            for (var i = n; i < bands01.Length; i++)
                bands01[i] = 0f;
        }
    }

    void OnAudioFilterRead(float[] data, int channels)
    {
        if (data == null || data.Length == 0 || channels < 1)
            return;

        var frames = data.Length / channels;
        if (frames <= 0)
            return;

        double sum = 0;
        for (var i = 0; i < data.Length; i += channels)
        {
            var s = data[i];
            sum += s * s;
        }

        var rms = Mathf.Sqrt((float)(sum / frames));

        // Map time slices → bands (audio thread; keep it cheap).
        var bandCount = _bandEnergy.Length;
        var chunk = Mathf.Max(1, frames / bandCount);
        var localBands = new float[bandCount];
        var peak = 1e-8f;
        for (var b = 0; b < bandCount; b++)
        {
            double bs = 0;
            var a = b * chunk;
            var z = Mathf.Min(frames, a + chunk);
            var count = Mathf.Max(1, z - a);
            for (var f = a; f < z; f++)
            {
                var s = data[f * channels];
                bs += s * s;
            }

            var br = Mathf.Sqrt((float)(bs / count));
            localBands[b] = br;
            if (br > peak)
                peak = br;
        }

        lock (_gate)
        {
            // Light smoothing so main-thread reads aren't spiky.
            _rms = Mathf.Lerp(_rms, rms, 0.45f);
            _peak = Mathf.Max(peak, Mathf.Lerp(_peak, peak, 0.35f));
            for (var i = 0; i < bandCount; i++)
                _bandEnergy[i] = Mathf.Lerp(_bandEnergy[i], localBands[i], 0.45f);
        }

        _frameCounter++;
    }
}
