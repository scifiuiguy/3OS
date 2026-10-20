using System;
using System.Threading;
using UnityEngine;

/// <summary>
/// Quest mic via Android AudioRecord on a background thread.
/// Uses AndroidJNI NewShortArray/FromShortArray so PCM copies back into C#
/// (Unity AndroidJavaObject.Call does not).
/// </summary>
public sealed class ThreeOSFinalDemoAndroidMic : IDisposable
{
#if UNITY_ANDROID && !UNITY_EDITOR
    AndroidJavaObject _recorder;
    int _chunkShorts;
    int _sampleRate;
    bool _started;
    IntPtr _readMethodId = IntPtr.Zero;

    Thread _thread;
    volatile bool _threadRun;
    readonly object _gate = new object();
    float[] _latestPcm = Array.Empty<float>();
    int _latestLen;
    float _latestRms;
    int _readOk;
    int _readFail;
    string _lastError;

    const int AudioSourceMic = 1;
    const int AudioSourceVoiceRecognition = 6;
    const int AudioSourceVoiceCommunication = 7;
    const int ChannelInMono = 16;
    const int EncodingPcm16Bit = 2;
    const int StateInitialized = 1;
    const int RecordstateRecording = 3;

    public bool IsRecording => _started;
    public int SampleRate => _sampleRate;
    public int LastReadCount { get; private set; }
    public int LastReadError { get; private set; }
    public float LastRms
    {
        get { lock (_gate) return _latestRms; }
    }

    public bool TryStart(int preferredRate, out string error)
    {
        error = null;
        Stop();

        try
        {
            using (var audioRecordCls = new AndroidJavaClass("android.media.AudioRecord"))
            {
                var rates = new[] { 16000, 44100, 48000, Mathf.Max(16000, preferredRate) };
                Exception last = null;
                for (var r = 0; r < rates.Length; r++)
                {
                    for (var src = 0; src < 3; src++)
                    {
                        var source = src == 0 ? AudioSourceMic
                            : src == 1 ? AudioSourceVoiceRecognition
                            : AudioSourceVoiceCommunication;
                        try
                        {
                            if (TryStartWith(audioRecordCls, rates[r], source))
                            {
                                CacheReadMethod();
                                StartThread();
                                Debug.Log(
                                    $"[3OS][7.4] AudioRecord thread started rate={_sampleRate} source={source} " +
                                    $"chunk={_chunkShorts}");
                                return true;
                            }
                        }
                        catch (Exception ex)
                        {
                            last = ex;
                        }
                    }
                }

                error = last != null ? last.Message : (_lastError ?? "AudioRecord failed all rates/sources");
                return false;
            }
        }
        catch (Exception ex)
        {
            error = ex.Message;
            return false;
        }
    }

    void CacheReadMethod()
    {
        if (_readMethodId != IntPtr.Zero)
            return;
        try
        {
            using (var cls = new AndroidJavaClass("android.media.AudioRecord"))
            {
                _readMethodId = AndroidJNIHelper.GetMethodID(
                    cls.GetRawClass(), "read", "([SII)I", false);
            }
        }
        catch (Exception ex)
        {
            Debug.LogWarning($"[3OS][7.4] AudioRecord method cache failed: {ex.Message}");
        }
    }

    bool TryStartWith(AndroidJavaClass audioRecordCls, int rate, int audioSource)
    {
        var minBuf = audioRecordCls.CallStatic<int>("getMinBufferSize", rate, ChannelInMono, EncodingPcm16Bit);
        if (minBuf <= 0)
        {
            _lastError = $"getMinBufferSize={minBuf} rate={rate}";
            return false;
        }

        var bufBytes = Math.Max(minBuf * 2, (rate / 5) * 2);
        var recorder = new AndroidJavaObject(
            "android.media.AudioRecord",
            audioSource,
            rate,
            ChannelInMono,
            EncodingPcm16Bit,
            bufBytes);

        var state = recorder.Call<int>("getState");
        if (state != StateInitialized)
        {
            _lastError = $"AudioRecord state={state}";
            SafeRelease(recorder);
            return false;
        }

        recorder.Call("startRecording");
        var recState = recorder.Call<int>("getRecordingState");
        if (recState != RecordstateRecording)
        {
            _lastError = $"recordingState={recState}";
            SafeRelease(recorder);
            return false;
        }

        _recorder = recorder;
        _sampleRate = rate;
        // ~40ms chunks — blocking read stays responsive.
        _chunkShorts = Math.Max(512, rate / 25);
        _latestPcm = new float[_chunkShorts];
        _started = true;
        return true;
    }

    void StartThread()
    {
        _threadRun = true;
        _readOk = 0;
        _readFail = 0;
        _thread = new Thread(CaptureLoop)
        {
            Name = "3OS-AudioRecord",
            IsBackground = true
        };
        _thread.Start();
    }

    void CaptureLoop()
    {
        AndroidJNI.AttachCurrentThread();
        var scratch = new short[_chunkShorts];
        var floats = new float[_chunkShorts];
        try
        {
            while (_threadRun)
            {
                try
                {
                    if (_recorder == null)
                    {
                        Thread.Sleep(10);
                        continue;
                    }

                    var n = ReadShortsJni(scratch);
                    if (n <= 0)
                    {
                        Interlocked.Increment(ref _readFail);
                        LastReadError = n;
                        Thread.Sleep(5);
                        continue;
                    }

                    double sum = 0;
                    const float scale = 1f / 32768f;
                    for (var i = 0; i < n; i++)
                    {
                        var f = scratch[i] * scale;
                        floats[i] = f;
                        sum += f * f;
                    }

                    var rms = Mathf.Sqrt((float)(sum / n));
                    lock (_gate)
                    {
                        if (_latestPcm.Length < n)
                            _latestPcm = new float[n];
                        Array.Copy(floats, 0, _latestPcm, 0, n);
                        _latestLen = n;
                        _latestRms = rms;
                    }

                    LastReadCount = n;
                    LastReadError = 0;
                    Interlocked.Increment(ref _readOk);
                }
                catch (Exception ex)
                {
                    _lastError = ex.Message;
                    Interlocked.Increment(ref _readFail);
                    Thread.Sleep(20);
                }
            }
        }
        finally
        {
            AndroidJNI.DetachCurrentThread();
        }
    }

    int ReadShortsJni(short[] scratch)
    {
        var n = scratch.Length;
        var jArray = IntPtr.Zero;
        try
        {
            jArray = AndroidJNI.NewShortArray(n);
            if (jArray == IntPtr.Zero)
                return 0;

            if (_readMethodId == IntPtr.Zero)
                CacheReadMethod();
            if (_readMethodId == IntPtr.Zero)
                return 0;

            var args = new[]
            {
                new jvalue { l = jArray },
                new jvalue { i = 0 },
                new jvalue { i = n }
            };

            var read = AndroidJNI.CallIntMethod(_recorder.GetRawObject(), _readMethodId, args);
            if (read <= 0)
                return read;

            var filled = AndroidJNI.FromShortArray(jArray);
            if (filled == null || filled.Length == 0)
                return 0;

            var copy = Math.Min(read, Math.Min(n, filled.Length));
            Array.Copy(filled, 0, scratch, 0, copy);
            return copy;
        }
        finally
        {
            if (jArray != IntPtr.Zero)
                AndroidJNI.DeleteLocalRef(jArray);
        }
    }

    public int Read(float[] dst)
    {
        if (!_started || dst == null || dst.Length == 0)
            return 0;

        lock (_gate)
        {
            var n = Math.Min(dst.Length, _latestLen);
            if (n <= 0)
            {
                LastReadCount = 0;
                return 0;
            }

            Array.Copy(_latestPcm, 0, dst, 0, n);
            LastReadCount = n;
            return n;
        }
    }

    public string DebugStatus()
    {
        lock (_gate)
            return $"ok={_readOk} fail={_readFail} rms={_latestRms:F5} len={_latestLen} err={_lastError}";
    }

    static void SafeRelease(AndroidJavaObject recorder)
    {
        if (recorder == null)
            return;
        try { recorder.Call("release"); }
        catch { /* ignore */ }
        try { recorder.Dispose(); }
        catch { /* ignore */ }
    }

    public void Stop()
    {
        _started = false;
        _threadRun = false;
        if (_thread != null)
        {
            try { _thread.Join(750); }
            catch { /* ignore */ }
            _thread = null;
        }

        if (_recorder != null)
        {
            try
            {
                var recState = _recorder.Call<int>("getRecordingState");
                if (recState == RecordstateRecording)
                    _recorder.Call("stop");
            }
            catch { /* ignore */ }

            SafeRelease(_recorder);
            _recorder = null;
        }

        lock (_gate)
        {
            _latestLen = 0;
            _latestRms = 0f;
        }
    }

    public void Dispose() => Stop();
#else
    public bool IsRecording => false;
    public int SampleRate => 0;
    public int LastReadCount => 0;
    public int LastReadError => 0;
    public float LastRms => 0f;

    public bool TryStart(int preferredRate, out string error)
    {
        error = "AudioRecord only on Android device";
        return false;
    }

    public int Read(float[] dst) => 0;
    public string DebugStatus() => "editor";
    public void Stop() { }
    public void Dispose() { }
#endif
}
