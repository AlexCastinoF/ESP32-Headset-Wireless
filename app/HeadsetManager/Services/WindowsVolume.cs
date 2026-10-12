using NAudio.CoreAudioApi;

namespace HeadsetManager.Services;

public sealed class WindowsVolume : IDisposable
{
    private readonly MMDeviceEnumerator _enumerator = new();
    private MMDevice? _speaker;
    private MMDevice? _microphone;
    private bool _writing;

    public bool HasSpeaker => _speaker != null;
    public bool HasMicrophone => _microphone != null;

    public event Action? Changed;

    public void Refresh()
    {
        Detach();
        _speaker = Find(DataFlow.Render);
        _microphone = Find(DataFlow.Capture);
        Attach(_speaker);
        Attach(_microphone);
    }

    public double SpeakerLevel
    {
        get => Read(_speaker);
        set => Write(_speaker, value);
    }

    public double MicrophoneLevel
    {
        get => Read(_microphone);
        set => Write(_microphone, value);
    }

    private MMDevice? Find(DataFlow flow)
    {
        foreach (var device in _enumerator.EnumerateAudioEndPoints(flow, DeviceState.Active))
        {
            var name = device.FriendlyName ?? "";
            if (name.Contains("Headset Wireless", StringComparison.OrdinalIgnoreCase))
            {
                return device;
            }
        }

        return null;
    }

    private void Attach(MMDevice? device)
    {
        if (device == null)
        {
            return;
        }

        device.AudioEndpointVolume.OnVolumeNotification += OnNotification;
    }

    private void OnNotification(AudioVolumeNotificationData data)
    {
        if (_writing)
        {
            return;
        }

        Changed?.Invoke();
    }

    private static double Read(MMDevice? device)
    {
        if (device == null)
        {
            return 1;
        }

        return device.AudioEndpointVolume.MasterVolumeLevelScalar;
    }

    private void Write(MMDevice? device, double level)
    {
        if (device == null)
        {
            return;
        }

        level = Math.Clamp(level, 0, 1);
        _writing = true;
        try
        {
            device.AudioEndpointVolume.MasterVolumeLevelScalar = (float)level;
        }
        finally
        {
            _writing = false;
        }
    }

    private void Detach()
    {
        if (_speaker != null)
        {
            _speaker.AudioEndpointVolume.OnVolumeNotification -= OnNotification;
            _speaker.Dispose();
            _speaker = null;
        }

        if (_microphone != null)
        {
            _microphone.AudioEndpointVolume.OnVolumeNotification -= OnNotification;
            _microphone.Dispose();
            _microphone = null;
        }
    }

    public void Dispose()
    {
        Detach();
        _enumerator.Dispose();
    }
}
