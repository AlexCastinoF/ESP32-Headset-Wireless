using NAudio.CoreAudioApi;
using NAudio.Wave;

namespace HeadsetManager.Services;

public sealed class LevelMeter : IDisposable
{
    private readonly object _gate = new();
    private MMDeviceEnumerator? _enumerator;
    private MMDevice? _renderDevice;
    private MMDevice? _captureDevice;
    private WasapiLoopbackCapture? _speaker;
    private WasapiCapture? _microphone;
    private const int Ring = 2048;
    private readonly float[] _leftRing = new float[Ring];
    private readonly float[] _rightRing = new float[Ring];
    private readonly float[] _micRing = new float[Ring];
    private int _leftWrite;
    private int _rightWrite;
    private int _micWrite;
    private int _leftCount;
    private int _rightCount;
    private int _micCount;
    private int _speakerRate = 48000;
    private int _micRate = 48000;
    private float _micPeak;
    private float _leftPeak;
    private float _rightPeak;
    private double _micShown;
    private double _leftShown;
    private double _rightShown;

    public bool Running { get; private set; }

    public void Start()
    {
        Stop();
        _enumerator = new MMDeviceEnumerator();
        _renderDevice = Find(DataFlow.Render);
        _captureDevice = Find(DataFlow.Capture);
        if (_renderDevice != null)
        {
            try
            {
                _speaker = new WasapiLoopbackCapture(_renderDevice);
                _speaker.DataAvailable += OnSpeaker;
                _speaker.StartRecording();
            }
            catch (Exception)
            {
                _speaker?.Dispose();
                _speaker = null;
            }
        }

        if (_captureDevice != null)
        {
            try
            {
                _microphone = new WasapiCapture(_captureDevice);
                _microphone.DataAvailable += OnMicrophone;
                _microphone.StartRecording();
            }
            catch (Exception)
            {
                _microphone?.Dispose();
                _microphone = null;
            }
        }

        Running = _speaker != null || _microphone != null;
    }

    public void Sample(out double microphone, out double left, out double right)
    {
        lock (_gate)
        {
            _micShown = Math.Max(_micPeak, _micShown * 0.72);
            _leftShown = Math.Max(_leftPeak, _leftShown * 0.72);
            _rightShown = Math.Max(_rightPeak, _rightShown * 0.72);
            _micPeak *= 0.35f;
            _leftPeak *= 0.35f;
            _rightPeak *= 0.35f;
            microphone = _micShown;
            left = _leftShown;
            right = _rightShown;
        }
    }

    public int CopyRecent(float[] left, float[] right, float[] microphone, out int speakerRate, out int microphoneRate)
    {
        lock (_gate)
        {
            Copy(_leftRing, _leftWrite, _leftCount, left);
            Copy(_rightRing, _rightWrite, _rightCount, right);
            Copy(_micRing, _micWrite, _micCount, microphone);
            speakerRate = _speakerRate;
            microphoneRate = _micRate;
            return left.Length;
        }
    }

    public void Stop()
    {
        Running = false;
        if (_speaker != null)
        {
            _speaker.DataAvailable -= OnSpeaker;
            TryStop(_speaker);
            _speaker.Dispose();
            _speaker = null;
        }

        if (_microphone != null)
        {
            _microphone.DataAvailable -= OnMicrophone;
            TryStop(_microphone);
            _microphone.Dispose();
            _microphone = null;
        }

        _renderDevice?.Dispose();
        _captureDevice?.Dispose();
        _renderDevice = null;
        _captureDevice = null;
        _enumerator?.Dispose();
        _enumerator = null;
        lock (_gate)
        {
            _micPeak = _leftPeak = _rightPeak = 0;
            _micShown = _leftShown = _rightShown = 0;
            _leftWrite = _rightWrite = _micWrite = 0;
            _leftCount = _rightCount = _micCount = 0;
        }
    }

    private void Push(float[] ring, ref int write, ref int count, float sample)
    {
        ring[write] = sample;
        write++;
        if (write >= Ring)
        {
            write = 0;
        }

        if (count < Ring)
        {
            count++;
        }
    }

    private static void Copy(float[] ring, int write, int count, float[] dest)
    {
        Array.Clear(dest, 0, dest.Length);
        var take = Math.Min(dest.Length, count);
        var start = write - take;
        if (start < 0)
        {
            start += Ring;
        }

        for (var i = 0; i < take; i++)
        {
            dest[dest.Length - take + i] = ring[(start + i) % Ring];
        }
    }

    private void OnSpeaker(object? sender, WaveInEventArgs e)
    {
        if (_speaker == null)
        {
            return;
        }

        var format = _speaker.WaveFormat;
        lock (_gate)
        {
            _speakerRate = format.SampleRate;
            Walk(e.Buffer, e.BytesRecorded, format, (index, sample) =>
            {
                if (index == 0)
                {
                    Push(_leftRing, ref _leftWrite, ref _leftCount, sample);
                    var abs = Math.Abs(sample);
                    if (abs > _leftPeak)
                    {
                        _leftPeak = abs;
                    }
                }
                else if (index == 1)
                {
                    Push(_rightRing, ref _rightWrite, ref _rightCount, sample);
                    var abs = Math.Abs(sample);
                    if (abs > _rightPeak)
                    {
                        _rightPeak = abs;
                    }
                }
            });
        }
    }

    private void OnMicrophone(object? sender, WaveInEventArgs e)
    {
        if (_microphone == null)
        {
            return;
        }

        var format = _microphone.WaveFormat;
        lock (_gate)
        {
            _micRate = format.SampleRate;
            Walk(e.Buffer, e.BytesRecorded, format, (index, sample) =>
            {
                if (index != 0)
                {
                    return;
                }

                Push(_micRing, ref _micWrite, ref _micCount, sample);
                var abs = Math.Abs(sample);
                if (abs > _micPeak)
                {
                    _micPeak = abs;
                }
            });
        }
    }

    private static void Walk(byte[] buffer, int count, WaveFormat format, Action<int, float> sample)
    {
        var channels = Math.Max(1, format.Channels);
        if (format.Encoding == WaveFormatEncoding.IeeeFloat && format.BitsPerSample == 32)
        {
            var frame = channels * 4;
            for (var i = 0; i + frame <= count; i += frame)
            {
                for (var channel = 0; channel < channels; channel++)
                {
                    sample(channel, BitConverter.ToSingle(buffer, i + channel * 4));
                }
            }
        }
        else if (format.BitsPerSample == 16)
        {
            var frame = channels * 2;
            for (var i = 0; i + frame <= count; i += frame)
            {
                for (var channel = 0; channel < channels; channel++)
                {
                    sample(channel, BitConverter.ToInt16(buffer, i + channel * 2) / 32768f);
                }
            }
        }
    }

    private MMDevice? Find(DataFlow flow)
    {
        if (_enumerator == null)
        {
            return null;
        }

        foreach (var device in _enumerator.EnumerateAudioEndPoints(flow, DeviceState.Active))
        {
            var name = device.FriendlyName ?? "";
            if (name.Contains("Headset Wireless", StringComparison.OrdinalIgnoreCase))
            {
                return device;
            }

            device.Dispose();
        }

        return null;
    }

    private static void TryStop(IWaveIn capture)
    {
        try
        {
            capture.StopRecording();
        }
        catch (Exception)
        {
        }
    }

    public void Dispose()
    {
        Stop();
    }
}
