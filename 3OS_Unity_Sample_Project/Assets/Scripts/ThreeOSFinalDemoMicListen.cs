using System.Collections;
using UnityEngine;
#if UNITY_ANDROID && !UNITY_EDITOR
using UnityEngine.Android;
#endif

/// <summary>
/// Quest mic listen + EOU via Unity <see cref="Microphone"/> → looping <see cref="AudioClip"/>.
/// Proven on device (MicEq 219): match DSP rate, play muted monitor, force unmute, avoid dual capture.
/// </summary>
public sealed class ThreeOSFinalDemoMicListen : MonoBehaviour
{
    [SerializeField] float speechRmsThreshold = 0.008f;
    [SerializeField] float silenceRmsFloor = 0.004f;
    [SerializeField] float silenceRelative = 0.42f;
    [SerializeField] float silenceHoldSec = 0.30f;
    [SerializeField] float minSpeechSec = 0.05f;
    [SerializeField] float peakDecayPerSec = 3.5f;
    [SerializeField] int bandCount = 10;
    [SerializeField] float fallbackCueSecAfterSignal = 14f;
    [SerializeField] int clipLengthSec = 1;
    [SerializeField] int readSamples = 1024;

    string _device = "";
    AudioClip _clip;
    AudioSource _monitor;
    float[] _pcm;
    readonly float[] _bands = new float[16];
    int _lastPos = -1;

    bool _armed;
    bool _waitingPermission;
    bool _heardSpeech;
    bool _sawMicEnergy;
    float _speechAccum;
    float _silenceAccum;
    float _armedElapsed;
    float _speechPeakRms;
    float _nextRmsLog;
    Coroutine _armRoutine;

    public bool IsArmed => _armed;
    public bool IsWaitingPermission => _waitingPermission;
    public bool IsArming => _armRoutine != null;
    public bool HasMicSignal => _sawMicEnergy;
    public float[] Bands => _bands;
    public float LastRms { get; private set; }

    public System.Action OnEndOfUtterance;

    public void Arm()
    {
        if (_armed && _clip != null && Microphone.IsRecording(_device))
        {
            Debug.Log("[3OS][7.4] Mic already recording (Unity Microphone)");
            return;
        }

        if (_armRoutine != null)
            StopCoroutine(_armRoutine);
        _armRoutine = StartCoroutine(ArmRoutine());
    }

    IEnumerator ArmRoutine()
    {
        _waitingPermission = false;
        TearDownCapture();
        _armed = false;

#if UNITY_ANDROID && !UNITY_EDITOR
        if (!Permission.HasUserAuthorizedPermission(Permission.Microphone))
        {
            _waitingPermission = true;
            Permission.RequestUserPermission(Permission.Microphone);
            var waited = 0f;
            while (!Permission.HasUserAuthorizedPermission(Permission.Microphone) && waited < 90f)
            {
                IdleBands();
                waited += Time.unscaledDeltaTime;
                yield return null;
            }

            _waitingPermission = false;
            if (!Permission.HasUserAuthorizedPermission(Permission.Microphone))
            {
                Debug.LogWarning("[3OS][7.4] Mic permission denied — Speak again to cue.");
                _armed = true;
                _armRoutine = null;
                yield break;
            }

            yield return null;
            yield return new WaitForSecondsRealtime(0.25f);
        }

        ForceUnmute("arm");
#endif

        if (!Application.HasUserAuthorization(UserAuthorization.Microphone))
            yield return Application.RequestUserAuthorization(UserAuthorization.Microphone);

        EnsureMonitor();
        _device = PickDevice();
        Microphone.GetDeviceCaps(_device, out var minFreq, out var maxFreq);
        var sampleRate = PickRate(minFreq, maxFreq, AudioSettings.outputSampleRate);
        if (_pcm == null || _pcm.Length != readSamples)
            _pcm = new float[readSamples];

        _clip = Microphone.Start(_device, true, Mathf.Max(1, clipLengthSec), sampleRate);
        if (_clip == null)
        {
            Debug.LogError("[3OS][7.4] Microphone.Start returned null — Speak again to cue NPC.");
            _armed = true;
            _armRoutine = null;
            yield break;
        }

        var t0 = Time.unscaledTime;
        while (Microphone.GetPosition(_device) <= 0 && Time.unscaledTime - t0 < 2f)
        {
            IdleBands();
            yield return null;
        }

        if (Microphone.GetPosition(_device) <= 0)
        {
            Debug.LogWarning("[3OS][7.4] Mic GetPosition stuck at 0 — restarting capture");
            Microphone.End(_device);
            yield return new WaitForSecondsRealtime(0.15f);
            _clip = Microphone.Start(_device, true, Mathf.Max(1, clipLengthSec), sampleRate);
            t0 = Time.unscaledTime;
            while (_clip != null && Microphone.GetPosition(_device) <= 0 && Time.unscaledTime - t0 < 2f)
            {
                IdleBands();
                yield return null;
            }
        }

        _monitor.clip = _clip;
        _monitor.loop = true;
        _monitor.volume = 0f;
        _monitor.spatialBlend = 0f;
        _monitor.Play();

#if UNITY_ANDROID && !UNITY_EDITOR
        ForceUnmute("post-start");
#endif

        _armed = true;
        _heardSpeech = false;
        _sawMicEnergy = false;
        _speechAccum = 0f;
        _silenceAccum = 0f;
        _armedElapsed = 0f;
        _speechPeakRms = 0f;
        _lastPos = -1;
        _nextRmsLog = 0f;
        _armRoutine = null;

        var pos = _clip != null ? Microphone.GetPosition(_device) : -1;
        Debug.Log(
            $"[3OS][7.4] Unity Microphone armed device='{_device}' rate={sampleRate} " +
            $"pos={pos} monPlay={_monitor.isPlaying}");
    }

    public void Disarm()
    {
        if (_armRoutine != null)
        {
            StopCoroutine(_armRoutine);
            _armRoutine = null;
        }

        _waitingPermission = false;
        _armed = false;
        _heardSpeech = false;
        _sawMicEnergy = false;
        _speechAccum = 0f;
        _silenceAccum = 0f;
        _armedElapsed = 0f;
        _speechPeakRms = 0f;
        TearDownCapture();
        for (var i = 0; i < _bands.Length; i++)
            _bands[i] = 0f;
        LastRms = 0f;
    }

    void TearDownCapture()
    {
        if (_monitor != null && _monitor.isPlaying)
            _monitor.Stop();
        if (_monitor != null)
            _monitor.clip = null;

        try
        {
            if (Microphone.IsRecording(_device))
                Microphone.End(_device);
        }
        catch { /* ignore */ }

        _clip = null;
        _lastPos = -1;
    }

    void OnDisable() => Disarm();

    void OnApplicationPause(bool paused)
    {
        if (paused)
        {
            if (_armed && _clip != null)
                TearDownCapture();
            return;
        }

        if (!_armed || _waitingPermission || _armRoutine != null)
            return;
        Debug.Log("[3OS][7.4] Focus returned — re-arming Unity Microphone");
        Arm();
    }

    void Update()
    {
        if (_waitingPermission || _armRoutine != null)
        {
            IdleBands();
            if (!_armed)
                return;
        }

        if (!_armed)
            return;

        _armedElapsed += Time.unscaledDeltaTime;
        var gotSamples = SampleUnityMic();
        var rms = LastRms;
        var dt = Time.unscaledDeltaTime;

        if (gotSamples && rms > 1e-6f)
            _sawMicEnergy = true;
        else if (rms >= speechRmsThreshold * 0.25f)
            _sawMicEnergy = true;

        if (Time.unscaledTime >= _nextRmsLog)
        {
            _nextRmsLog = Time.unscaledTime + 0.5f;
            var pos = _clip != null ? Microphone.GetPosition(_device) : -1;
            Debug.Log(
                $"[3OS][7.4] Mic rms={rms:F5} got={gotSamples} energy={_sawMicEnergy} " +
                $"heard={_heardSpeech} peak={_speechPeakRms:F5} sil={_silenceAccum:F2} " +
                $"band0={_bands[0]:F2} pos={pos}");
        }

        if (!_sawMicEnergy)
        {
            IdleBands();
            return;
        }

        if (!_heardSpeech)
        {
            if (rms >= speechRmsThreshold)
            {
                _speechAccum += dt;
                _speechPeakRms = Mathf.Max(_speechPeakRms, rms);
                if (_speechAccum >= minSpeechSec)
                {
                    _heardSpeech = true;
                    _silenceAccum = 0f;
                    Debug.Log($"[3OS][7.4] Begin-speak (rms={rms:F4})");
                }
            }
            else
            {
                _speechAccum = 0f;
            }

            if (_armedElapsed >= fallbackCueSecAfterSignal)
            {
                Debug.LogWarning("[3OS][7.4] Fallback EOU after signal timeout");
                FireEndOfUtterance();
            }

            return;
        }

        _speechPeakRms = Mathf.Max(rms, _speechPeakRms * Mathf.Exp(-peakDecayPerSec * dt));
        var silenceGate = Mathf.Max(silenceRmsFloor, _speechPeakRms * silenceRelative);
        if (rms <= silenceGate)
        {
            _silenceAccum += dt;
            if (_silenceAccum >= silenceHoldSec)
                FireEndOfUtterance();
        }
        else
        {
            _silenceAccum = 0f;
        }
    }

    public void ForceEndOfUtterance()
    {
        Debug.Log("[3OS][7.4] End-of-utterance (manual cue)");
        FireEndOfUtterance();
    }

    void FireEndOfUtterance()
    {
        Debug.Log("[3OS][7.4] End-of-utterance");
        var cb = OnEndOfUtterance;
        Disarm();
        cb?.Invoke();
    }

    bool SampleUnityMic()
    {
        if (_clip == null || _pcm == null || !Microphone.IsRecording(_device))
        {
            LastRms = 0f;
            return false;
        }

        var pos = Microphone.GetPosition(_device);
        if (pos < 0 || _clip.samples <= 0)
        {
            LastRms = 0f;
            return false;
        }

        if (pos == _lastPos || pos == 0)
        {
            LastRms = 0f;
            return false;
        }

        _lastPos = pos;
        var n = Mathf.Min(readSamples, _clip.samples);
        var start = pos - n;
        if (start < 0)
            start += _clip.samples;

        if (!_clip.GetData(_pcm, start))
        {
            LastRms = 0f;
            return false;
        }

        double sum = 0;
        for (var i = 0; i < n; i++)
        {
            var s = _pcm[i];
            sum += s * s;
        }

        LastRms = Mathf.Sqrt((float)(sum / n));
        var level = Mathf.Clamp01(LastRms * 80f);

        var chunk = Mathf.Max(1, n / bandCount);
        for (var b = 0; b < bandCount && b < _bands.Length; b++)
        {
            double bs = 0;
            var a = b * chunk;
            var z = Mathf.Min(n, a + chunk);
            for (var i = a; i < z; i++)
            {
                var s = _pcm[i];
                bs += s * s;
            }

            var br = Mathf.Sqrt((float)(bs / Mathf.Max(1, z - a)));
            var slice = Mathf.Clamp01(br * 80f);
            _bands[b] = Mathf.Clamp01(level * 0.5f + slice * 0.5f);
        }

        return true;
    }

    void IdleBands()
    {
        var shimmer = 0.08f + 0.06f * Mathf.Abs(Mathf.Sin(Time.unscaledTime * 2.2f));
        for (var i = 0; i < bandCount && i < _bands.Length; i++)
            _bands[i] = shimmer;
    }

    void EnsureMonitor()
    {
        if (_monitor != null)
            return;
        _monitor = gameObject.AddComponent<AudioSource>();
        _monitor.playOnAwake = false;
        _monitor.loop = true;
        _monitor.volume = 0f;
        _monitor.spatialBlend = 0f;
    }

#if UNITY_ANDROID && !UNITY_EDITOR
    static void ForceUnmute(string where)
    {
        try
        {
            using (var unityPlayer = new AndroidJavaClass("com.unity3d.player.UnityPlayer"))
            using (var activity = unityPlayer.GetStatic<AndroidJavaObject>("currentActivity"))
            using (var am = activity.Call<AndroidJavaObject>("getSystemService", "audio"))
            {
                if (am == null)
                    return;
                var muted = am.Call<bool>("isMicrophoneMute");
                if (muted)
                {
                    am.Call("setMicrophoneMute", false);
                    Debug.Log($"[3OS][7.4] AudioManager[{where}] forced unmute");
                }
            }
        }
        catch (System.Exception ex)
        {
            Debug.LogWarning($"[3OS][7.4] ForceUnmute[{where}] {ex.Message}");
        }
    }
#endif

    static string PickDevice()
    {
        var devices = Microphone.devices;
        if (devices == null || devices.Length == 0)
            return "";
        for (var i = 0; i < devices.Length; i++)
        {
            var d = devices[i] ?? "";
            if (d.IndexOf("Android", System.StringComparison.OrdinalIgnoreCase) >= 0)
                return d;
        }

        return devices[0] ?? "";
    }

    static int PickRate(int minF, int maxF, int prefer)
    {
        if (prefer <= 0)
            prefer = 24000;
        if (maxF <= 0)
            return prefer;
        if (minF > 0 && prefer < minF)
            return minF;
        if (prefer > maxF)
            return maxF;
        return prefer;
    }
}
