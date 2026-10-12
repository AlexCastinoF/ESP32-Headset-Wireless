using HidSharp;

namespace HeadsetManager.Services;

public sealed class HidLink : IDisposable
{
    public const int VendorId = 0x303A;
    public const int ProductId = 0x4453;

    private readonly object _gate = new();
    private HidStream? _stream;
    private CancellationTokenSource? _cts;
    private Task? _loop;

    public event Action<HeadsetStatus>? StatusReceived;
    public event Action<bool>? ConnectionChanged;
    public event Action<string>? Faulted;

    public bool IsOpen
    {
        get
        {
            lock (_gate)
            {
                return _stream != null;
            }
        }
    }

    public void Start()
    {
        _cts = new CancellationTokenSource();
        _loop = Task.Run(() => Run(_cts.Token));
    }

    public void Send(HeadsetConfig config)
    {
        var report = config.ToReport();
        lock (_gate)
        {
            if (_stream == null)
            {
                return;
            }

            try
            {
                _stream.SetFeature(report);
            }
            catch (Exception ex)
            {
                Faulted?.Invoke(ex.Message);
            }
        }
    }

    private void Run(CancellationToken token)
    {
        var wasOpen = false;
        while (!token.IsCancellationRequested)
        {
            try
            {
                if (!EnsureOpen())
                {
                    if (wasOpen)
                    {
                        wasOpen = false;
                        ConnectionChanged?.Invoke(false);
                    }

                    Thread.Sleep(400);
                    continue;
                }

                if (!wasOpen)
                {
                    wasOpen = true;
                    ConnectionChanged?.Invoke(true);
                }

                var buffer = new byte[64];
                int read;
                lock (_gate)
                {
                    if (_stream == null)
                    {
                        continue;
                    }

                    _stream.ReadTimeout = 500;
                    read = _stream.Read(buffer, 0, buffer.Length);
                }

                if (read >= 33 && buffer[0] == 1)
                {
                    StatusReceived?.Invoke(HeadsetStatus.FromReport(buffer));
                }
            }
            catch (TimeoutException)
            {
            }
            catch (Exception ex)
            {
                CloseStream();
                if (wasOpen)
                {
                    wasOpen = false;
                    ConnectionChanged?.Invoke(false);
                }

                Faulted?.Invoke(ex.Message);
                Thread.Sleep(400);
            }
        }
    }

    private bool EnsureOpen()
    {
        lock (_gate)
        {
            if (_stream != null)
            {
                return true;
            }
        }

        var device = DeviceList.Local
            .GetHidDevices(VendorId, ProductId)
            .FirstOrDefault(item => item.GetMaxInputReportLength() >= 33);
        if (device == null)
        {
            return false;
        }

        HidStream stream;
        try
        {
            stream = device.Open();
        }
        catch (Exception)
        {
            return false;
        }

        stream.ReadTimeout = 500;
        lock (_gate)
        {
            _stream = stream;
        }

        return true;
    }

    private void CloseStream()
    {
        lock (_gate)
        {
            _stream?.Dispose();
            _stream = null;
        }
    }

    public void Dispose()
    {
        _cts?.Cancel();
        CloseStream();
        _cts?.Dispose();
    }
}
