using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using System.Windows.Threading;
using HeadsetManager.Services;
using Microsoft.Win32;
using Forms = System.Windows.Forms;
using Color = System.Windows.Media.Color;
using Button = System.Windows.Controls.Button;
using Brush = System.Windows.Media.Brush;
using Brushes = System.Windows.Media.Brushes;
using ComboBox = System.Windows.Controls.ComboBox;
using Point = System.Windows.Point;

namespace HeadsetManager;

public partial class MainWindow : Window
{
    private const string RunKeyPath = @"Software\Microsoft\Windows\CurrentVersion\Run";
    private const string RunValueName = "HeadsetWireless";
    private static readonly byte[] PresetPeer = { 0x90, 0xDA, 0x72, 0x84, 0xB0, 0x20 };

    private readonly HidLink _link = new();
    private readonly WindowsVolume _volume = new();
    private readonly LevelMeter _levels = new();
    private readonly float[] _waveLeft = new float[Spectrum.Size];
    private readonly float[] _waveRight = new float[Spectrum.Size];
    private readonly float[] _waveMic = new float[Spectrum.Size];
    private readonly float[] _bandLeft = new float[Spectrum.Bands];
    private readonly float[] _bandRight = new float[Spectrum.Bands];
    private readonly float[] _bandMic = new float[Spectrum.Bands];
    private readonly float[] _showLeft = new float[Spectrum.Bands];
    private readonly float[] _showRight = new float[Spectrum.Bands];
    private readonly float[] _showMic = new float[Spectrum.Bands];
    private TrayMeters? _meters;
    private bool _spectrum;
    private readonly DispatcherTimer _sendTimer;
    private readonly DispatcherTimer _toneTimer;
    private readonly DispatcherTimer _meterTimer;
    private readonly List<int> _rssi = new();
    private readonly List<int> _loss = new();
    private HeadsetStatus? _status;
    private HeadsetConfig _config = new();
    private bool _ready;
    private bool _loading;
    private long _editHoldUntil;
    private bool _presetSent;
    private bool _exit;
    private Forms.NotifyIcon? _tray;
    private Forms.NotifyIcon? _batteryIcon;
    private int _batteryMark = int.MinValue;
    private bool? _batteryLight;

    public MainWindow()
    {
        InitializeComponent();
        for (var channel = 1; channel <= 13; channel++)
        {
            ChannelBox.Items.Add(channel);
        }

        ChannelBox.SelectedItem = 6;
        _sendTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(160) };
        _sendTimer.Tick += (_, _) =>
        {
            _sendTimer.Stop();
            PushConfig(save: true);
        };
        _toneTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(3) };
        _toneTimer.Tick += (_, _) =>
        {
            _toneTimer.Stop();
            ToneBox.IsChecked = false;
            TestResult.Text = "Tom de 1 kHz desligado.";
        };
        _meterTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(40) };
        _meterTimer.Tick += (_, _) => PaintMeters();
        Loaded += OnLoaded;
        Closing += OnClosing;
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        _loading = true;
        StartWithWindows.IsChecked = IsStartupEnabled();
        _loading = false;
        ShowPage(PageHome, NavHome);
        CreateTray();
        _volume.Changed += () => Dispatcher.BeginInvoke(PullWindowsVolume);
        _link.ConnectionChanged += open => Dispatcher.BeginInvoke(() => OnConnection(open));
        _link.StatusReceived += status => Dispatcher.BeginInvoke(() => OnStatus(status));
        _link.Faulted += message => Dispatcher.BeginInvoke(() => StatusLine.Text = message);
        _link.Start();
        _volume.Refresh();
        PullWindowsVolume();
        TryStartMeters();
        UpdateLatencyPreview();
        _ready = true;
        if (TrayIcons.IsChecked == true)
        {
            ShowTrayMeters();
        }
    }

    private void CreateTray()
    {
        var menu = new Forms.ContextMenuStrip();
        menu.Items.Add("Abrir", null, (_, _) => Dispatcher.BeginInvoke(RestoreFromTray));
        menu.Items.Add("Sair", null, (_, _) => Dispatcher.BeginInvoke(Close));
        _tray = new Forms.NotifyIcon
        {
            Visible = true,
            Text = "Castino",
            ContextMenuStrip = menu
        };
        _tray.DoubleClick += (_, _) => Dispatcher.BeginInvoke(RestoreFromTray);
        _tray.Icon = AppTrayIcon.ForTray();
        using (var windowIcon = AppTrayIcon.ForWindow())
        {
            Icon = Imaging.CreateBitmapSourceFromHIcon(windowIcon.Handle, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions());
        }

        var batteryMenu = new Forms.ContextMenuStrip();
        batteryMenu.Items.Add("Abrir", null, (_, _) => Dispatcher.BeginInvoke(RestoreFromTray));
        batteryMenu.Items.Add("Sair", null, (_, _) => Dispatcher.BeginInvoke(Close));
        _batteryIcon = new Forms.NotifyIcon
        {
            Visible = true,
            Text = "Bateria",
            ContextMenuStrip = batteryMenu
        };
        _batteryIcon.DoubleClick += (_, _) => Dispatcher.BeginInvoke(RestoreFromTray);
        PaintBattery(null);
    }

    private void PaintBattery(int? percent)
    {
        if (_batteryIcon == null)
        {
            return;
        }

        var light = TaskbarIsLight();
        var mark = percent is int value ? Math.Clamp(value, 0, 100) / 5 : -1;
        _batteryIcon.Text = percent is int exact ? $"Bateria {exact}%" : "Bateria sem leitura";
        if (mark == _batteryMark && _batteryLight == light && _batteryIcon.Icon != null)
        {
            return;
        }

        _batteryMark = mark;
        _batteryLight = light;
        var previous = _batteryIcon.Icon;
        _batteryIcon.Icon = AppTrayIcon.Battery(percent, light);
        previous?.Dispose();
    }

    private static bool TaskbarIsLight()
    {
        using var key = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize");
        return key?.GetValue("SystemUsesLightTheme") is int value && value != 0;
    }

    private void RestoreFromTray()
    {
        Show();
        WindowState = WindowState.Normal;
        Activate();
    }

    private void OnClosing(object? sender, System.ComponentModel.CancelEventArgs e)
    {
        if (_exit)
        {
            return;
        }

        _exit = true;
        _tray?.Dispose();
        _tray = null;
        _batteryIcon?.Dispose();
        _batteryIcon = null;
        _meterTimer.Stop();
        _meters?.Dispose();
        _meters = null;
        _levels.Dispose();
        _link.Dispose();
        _volume.Dispose();
    }

    private void OnConnection(bool open)
    {
        EmptyState.Visibility = open ? Visibility.Collapsed : Visibility.Visible;
        DongleState.Text = open ? "Na USB" : "Ausente";
        SidebarLink.Text = open ? "Dongle na USB" : "Dongle ausente";
        StatusLine.Text = open ? "Dongle encontrado. Esperando o fone." : "Ligue o cabo no USB nativo do S3.";
        if (open)
        {
            _presetSent = false;
            _volume.Refresh();
            PullWindowsVolume();
            TryStartMeters();
        }
        else
        {
            _levels.Stop();
            _meterTimer.Stop();
            PaintMeters();
            LinkValue.Text = "Sem enlace";
            BatteryValue.Text = "—";
            BatteryBar.Value = 0;
            RssiValue.Text = "Sem sinal";
            PowerValue.Text = "—";
            LossValue.Text = "—";
            RttValue.Text = "—";
            PaintBars(0);
            if (_tray != null)
            {
                _tray.Text = "Castino · sem o fone";
            }

            PaintBattery(null);
        }
    }

    private void OnStatus(HeadsetStatus status)
    {
        _status = status;
        DongleState.Text = "Na USB";
        LinkValue.Text = status.Linked ? "No ar" : "Sem enlace";
        BatteryValue.Text = status.Linked ? status.Battery + "%" : "Sem leitura";
        BatteryBar.Value = status.Linked ? status.Battery : 0;
        MuteValue.Text = status.MicMuted ? "Microfone mudo" : "Microfone aberto";
        var bars = status.Linked ? SignalBars(status.Rssi) : 0;
        PaintBars(bars);
        RssiValue.Text = status.Linked ? $"{SignalWord(bars)}  ·  {status.Rssi} dBm" : "Sem sinal";
        PowerValue.Text = $"{status.TxPowerQdbm / 4.0:0.00} dBm";
        LossValue.Text = status.LossPermille + " / 1000";
        RttValue.Text = status.RttMs + " ms";
        VersionValue.Text = string.IsNullOrWhiteSpace(status.Version) ? "Firmware —" : "Firmware " + status.Version;
        SidebarLink.Text = status.Linked ? "Fone no ar" : "Dongle sem o fone";
        var tip = status.Linked ? $"Castino · {status.Rssi} dBm" : "Castino · sem o fone";
        if (tip.Length > 63)
        {
            tip = tip[..63];
        }

        if (_tray != null)
        {
            _tray.Text = tip;
        }

        PaintBattery(status.Linked ? status.Battery : null);
        OfferPreset(status);

        if (Editing())
        {
            _rssi.Add(status.Rssi);
            _loss.Add(status.LossPermille);
            if (_rssi.Count > 60)
            {
                _rssi.RemoveAt(0);
                _loss.RemoveAt(0);
            }

            DrawPlot();
            return;
        }

        _loading = true;
        if (!MacBox.IsKeyboardFocusWithin)
        {
            SyncMacChoice(status.PeerMac);
        }

        if (!ChannelBox.IsKeyboardFocusWithin && !ChannelBox.IsDropDownOpen)
        {
            ChannelBox.SelectedItem = status.Channel is >= 1 and <= 13 ? status.Channel : 6;
        }

        if (!PowerSlider.IsMouseCaptureWithin)
        {
            PowerSlider.Value = Math.Clamp(status.TxPowerQdbm, 8, 80);
        }

        ApplyTransportFromDevice(status);
        if (!SidetoneSlider.IsMouseCaptureWithin)
        {
            SidetoneSlider.Value = status.SidetoneDb;
        }

        if (!BoostSlider.IsMouseCaptureWithin)
        {
            BoostSlider.Value = status.MicBoostDb;
        }

        MuteBox.IsChecked = status.MicMuted;
        ToneBox.IsChecked = status.ToneOn;
        _loading = false;
        Remember(status);
        _rssi.Add(status.Rssi);
        _loss.Add(status.LossPermille);
        if (_rssi.Count > 60)
        {
            _rssi.RemoveAt(0);
            _loss.RemoveAt(0);
        }

        DrawPlot();
    }

    private void ApplyTransportFromDevice(HeadsetStatus status)
    {
        if (AdvancedPanel.IsKeyboardFocusWithin || ProfileGame.IsMouseOver || ProfileBalance.IsMouseOver || ProfileStable.IsMouseOver)
        {
            return;
        }

        if (status.Profile == LinkMath.ProfileCustom)
        {
            AdvancedTransport.IsChecked = true;
            SelectTag(FrameBox, status.FrameMs);
            SelectTag(JitterBox, status.JitterFrames);
        }
        else
        {
            AdvancedTransport.IsChecked = false;
            ProfileGame.IsChecked = status.Profile == LinkMath.ProfileGame;
            ProfileBalance.IsChecked = status.Profile == LinkMath.ProfileBalance;
            ProfileStable.IsChecked = status.Profile == LinkMath.ProfileStable;
        }

        UpdateLatencyPreview();
    }

    private void Remember(HeadsetStatus status)
    {
        _config.FrameMs = status.FrameMs == 0 ? 5 : status.FrameMs;
        _config.JitterFrames = status.JitterFrames == 0 ? 2 : status.JitterFrames;
        _config.Channel = status.Channel == 0 ? 6 : status.Channel;
        _config.TxPowerQdbm = status.TxPowerQdbm == 0 ? 60 : status.TxPowerQdbm;
        _config.SpeakerDb = status.SpeakerDb;
        _config.MicDb = status.MicDb;
        _config.SidetoneDb = status.SidetoneDb;
        _config.MicBoostDb = status.MicBoostDb;
        _config.Flags = (byte)(status.Flags & 0x03);
        _config.PeerMac = status.PeerMac;
        _config.Profile = status.Profile;
    }

    private void LinkTest_Click(object sender, RoutedEventArgs e)
    {
        if (_status == null || !_link.IsOpen)
        {
            TestResult.Text = "O dongle não está na USB nativa. Sem ele não há enlace para testar.";
            return;
        }

        if (!_status.Linked)
        {
            TestResult.Text = "Dongle presente, fone sem enlace. Ligue a chave da bateria e aproxime o fone. O primeiro pacote grava o par.";
            return;
        }

        var bars = SignalBars(_status.Rssi);
        var signalOk = bars >= 2;
        var lossOk = _status.LossPermille < 50;
        var batteryOk = _status.Battery > 0;
        var verdict = signalOk && lossOk ? "Enlace bom." : "Enlace fraco.";
        TestResult.Text = $"{verdict} Sinal {SignalWord(bars)} ({_status.Rssi} dBm), perdas {_status.LossPermille} por mil, bateria {_status.Battery}%"
            + (batteryOk ? "." : ". A bateria não está sendo lida.");
    }

    private void ToneTest_Click(object sender, RoutedEventArgs e)
    {
        if (!_link.IsOpen)
        {
            TestResult.Text = "Conecte o dongle no USB nativo para ouvir o tom.";
            return;
        }

        ToneBox.IsChecked = true;
        TestResult.Text = "Tom de 1 kHz ligado por 3 segundos. Ouça no fone.";
        _toneTimer.Stop();
        _toneTimer.Start();
    }

    private static int SignalBars(int rssi)
    {
        if (rssi >= -55) return 5;
        if (rssi >= -65) return 4;
        if (rssi >= -75) return 3;
        if (rssi >= -85) return 2;
        if (rssi >= -95) return 1;
        return 0;
    }

    private static string SignalWord(int bars) => bars switch
    {
        >= 4 => "forte",
        3 => "bom",
        2 => "fraco",
        1 => "mínimo",
        _ => "nenhum"
    };

    private void MeterMode_Checked(object sender, RoutedEventArgs e)
    {
        if (!_ready || _loading || ModeSpectrum == null || AudioModeSpectrum == null)
        {
            return;
        }

        _spectrum = sender == ModeSpectrum || sender == AudioModeSpectrum;
        _loading = true;
        ModeBars.IsChecked = !_spectrum;
        ModeSpectrum.IsChecked = _spectrum;
        AudioModeBars.IsChecked = !_spectrum;
        AudioModeSpectrum.IsChecked = _spectrum;
        _loading = false;
        HomeBars.Visibility = _spectrum ? Visibility.Collapsed : Visibility.Visible;
        AudioBars.Visibility = _spectrum ? Visibility.Collapsed : Visibility.Visible;
        HomeSpectrum.Visibility = _spectrum ? Visibility.Visible : Visibility.Collapsed;
        AudioSpectrum.Visibility = _spectrum ? Visibility.Visible : Visibility.Collapsed;
        SpectrumLegend.Visibility = _spectrum ? Visibility.Visible : Visibility.Collapsed;
    }

    private void TrayIcons_Changed(object sender, RoutedEventArgs e)
    {
        if (!_ready || _loading || TrayIcons == null)
        {
            return;
        }

        if (TrayIcons.IsChecked == true)
        {
            ShowTrayMeters();
        }
        else
        {
            _meters?.Dispose();
            _meters = null;
        }
    }

    private void ShowTrayMeters()
    {
        if (_meters != null)
        {
            return;
        }

        _meters = new TrayMeters(
            () => Dispatcher.BeginInvoke(RestoreFromTray),
            () => Dispatcher.BeginInvoke(Close));
        _meters.Promote();
        var promote = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
        promote.Tick += (_, _) =>
        {
            promote.Stop();
            _meters?.Promote();
        };
        promote.Start();
    }

    private void TryStartMeters()
    {
        try
        {
            _levels.Start();
            _meterTimer.Start();
            LiveHint.Text = _levels.Running
                ? "As barras seguem o que o Windows envia ao dongle e o que o microfone devolve."
                : "O Windows ainda não listou o alto-falante Headset Wireless. As barras ficam paradas até ele aparecer.";
        }
        catch (Exception ex)
        {
            LiveHint.Text = ex.Message;
        }
    }

    private void PaintMeters()
    {
        _levels.Sample(out var mic, out var left, out var right);
        ApplyMeter(HomeLeftMeter, HomeLeftLabel, "Alto-falante esquerdo", left);
        ApplyMeter(HomeRightMeter, HomeRightLabel, "Alto-falante direito", right);
        ApplyMeter(HomeMicMeter, HomeMicLabel, "Microfone", mic);
        ApplyMeter(AudioLeftMeter, AudioLeftLabel, "Alto-falante esquerdo", left);
        ApplyMeter(AudioRightMeter, AudioRightLabel, "Alto-falante direito", right);
        ApplyMeter(AudioMicMeter, AudioMicLabel, "Microfone", mic);
        _meters?.SetLevels(left, right, mic);

        if (_spectrum)
        {
            PaintSpectrum();
        }
    }

    private void PaintSpectrum()
    {
        _levels.CopyRecent(_waveLeft, _waveRight, _waveMic, out var speakerRate, out var micRate);
        Spectrum.BandsOf(_waveLeft, speakerRate, _bandLeft);
        Spectrum.BandsOf(_waveRight, speakerRate, _bandRight);
        Spectrum.BandsOf(_waveMic, micRate, _bandMic);
        Ease(_bandLeft, _showLeft);
        Ease(_bandRight, _showRight);
        Ease(_bandMic, _showMic);
        DrawSpectrum(HomeSpectrum);
        DrawSpectrum(AudioSpectrum);
    }

    private void DrawSpectrum(Canvas canvas)
    {
        canvas.Children.Clear();
        var width = canvas.ActualWidth;
        var height = canvas.ActualHeight;
        if (width < 20 || height < 20)
        {
            return;
        }

        AddTrace(_showLeft, width, height, Color.FromRgb(0x0E, 0x7C, 0x72));
        AddTrace(_showRight, width, height, Color.FromRgb(0xC4, 0x6B, 0x2D));
        AddTrace(_showMic, width, height, Color.FromRgb(0x3D, 0x7E, 0xA6));

        void AddTrace(float[] bands, double w, double h, Color color)
        {
            var line = new Polyline
            {
                Stroke = new SolidColorBrush(color),
                StrokeThickness = 2
            };
            for (var i = 0; i < bands.Length; i++)
            {
                var x = w * i / (bands.Length - 1);
                var y = h - Math.Clamp(bands[i], 0, 1) * (h - 8) - 4;
                line.Points.Add(new Point(x, y));
            }

            canvas.Children.Add(line);
        }
    }

    private static void Ease(float[] target, float[] shown)
    {
        for (var i = 0; i < target.Length; i++)
        {
            shown[i] = target[i] > shown[i] ? target[i] : shown[i] * 0.72f;
        }
    }

    private static void ApplyMeter(System.Windows.Controls.ProgressBar bar, TextBlock label, string name, double level)
    {
        var percent = (int)Math.Round(Math.Clamp(level, 0, 1) * 100);
        bar.Value = percent;
        label.Text = $"{name}  {percent}%";
    }

    private void PaintBars(int level)
    {
        var fill = new SolidColorBrush(Color.FromRgb(0x0E, 0x7C, 0x72));
        var empty = new SolidColorBrush(Color.FromRgb(0xD5, 0xDD, 0xE4));
        var bars = new[] { Bar1, Bar2, Bar3, Bar4, Bar5 };
        for (var i = 0; i < bars.Length; i++)
        {
            bars[i].Background = i < level ? fill : empty;
        }
    }

    private void Nav_Click(object sender, RoutedEventArgs e)
    {
        if (sender == NavHome) ShowPage(PageHome, NavHome);
        else if (sender == NavAudio) ShowPage(PageAudio, NavAudio);
        else if (sender == NavTransport) ShowPage(PageTransport, NavTransport);
        else if (sender == NavRadio) ShowPage(PageRadio, NavRadio);
        else
        {
            ShowPage(PageDiag, NavDiag);
            Dispatcher.BeginInvoke(DrawPlot, DispatcherPriority.Loaded);
        }
    }

    private void ShowPage(StackPanel page, Button nav)
    {
        foreach (var panel in new[] { PageHome, PageAudio, PageTransport, PageRadio, PageDiag })
        {
            panel.Visibility = panel == page ? Visibility.Visible : Visibility.Collapsed;
        }

        foreach (var button in new[] { NavHome, NavAudio, NavTransport, NavRadio, NavDiag })
        {
            button.Background = button == nav ? new SolidColorBrush(Color.FromRgb(0x0E, 0x7C, 0x72)) : Brushes.Transparent;
            button.Foreground = Brushes.White;
        }
    }

    private void VolumeSlider_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (!_ready || _loading)
        {
            return;
        }

        if (sender == SpeakerSlider)
        {
            SpeakerLabel.Text = HasWindowsSpeaker()
                ? $"Alto-falante  {SpeakerSlider.Value:0}%"
                : $"Alto-falante no firmware  {SpeakerDb()} dB";
            if (HasWindowsSpeaker())
            {
                _volume.SpeakerLevel = SpeakerSlider.Value / 100.0;
            }
        }
        else
        {
            MicLabel.Text = HasWindowsMic()
                ? $"Microfone  {MicSlider.Value:0}%"
                : $"Microfone no firmware  {MicDb()} dB";
            if (HasWindowsMic())
            {
                _volume.MicrophoneLevel = MicSlider.Value / 100.0;
            }
        }

        ScheduleSend();
    }

    private void FirmwareSlider_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (SidetoneLabel == null || BoostLabel == null)
        {
            return;
        }

        var side = (int)SidetoneSlider.Value;
        SidetoneLabel.Text = side <= -60 ? "Retorno da própria voz  desligado" : $"Retorno da própria voz  {side} dB";
        BoostLabel.Text = $"Avanço do microfone  {(int)BoostSlider.Value} dB";
        if (_ready && !_loading)
        {
            ScheduleSend();
        }
    }

    private void Flag_Changed(object sender, RoutedEventArgs e)
    {
        if (_ready && !_loading)
        {
            ScheduleSend();
        }
    }

    private void Profile_Checked(object sender, RoutedEventArgs e)
    {
        if (!_ready || _loading || AdvancedTransport.IsChecked == true)
        {
            UpdateLatencyPreview();
            return;
        }

        UpdateLatencyPreview();
        ScheduleSend();
    }

    private void Advanced_Changed(object sender, RoutedEventArgs e)
    {
        if (AdvancedPanel == null)
        {
            return;
        }

        AdvancedPanel.Visibility = AdvancedTransport.IsChecked == true ? Visibility.Visible : Visibility.Collapsed;
        UpdateLatencyPreview();
        if (_ready && !_loading)
        {
            ScheduleSend();
        }
    }

    private void CustomTransport_Changed(object sender, SelectionChangedEventArgs e)
    {
        UpdateLatencyPreview();
        if (_ready && !_loading && AdvancedTransport.IsChecked == true)
        {
            ScheduleSend();
        }
    }

    private void PowerSlider_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (PowerLabel == null)
        {
            return;
        }

        PowerLabel.Text = $"Potência  {PowerSlider.Value / 4.0:0.00} dBm";
        if (_ready && !_loading)
        {
            ScheduleSend();
        }
    }

    private void Channel_Changed(object sender, SelectionChangedEventArgs e)
    {
        if (_ready && !_loading)
        {
            ScheduleSend();
        }
    }

    private void MacChoice_Changed(object sender, RoutedEventArgs e)
    {
        if (MacBox == null || MacCustom == null)
        {
            return;
        }

        MacBox.Visibility = MacCustom.IsChecked == true ? Visibility.Visible : Visibility.Collapsed;
        if (!_ready || _loading || MacPreset.IsChecked != true)
        {
            return;
        }

        _presetSent = true;
        HoldEdits();
        PushConfig(save: true);
        StatusLine.Text = "Fone C3 90:DA:72:84:B0:20 enviado ao dongle.";
    }

    private void SaveRadio_Click(object sender, RoutedEventArgs e)
    {
        if (MacCustom.IsChecked == true && !string.IsNullOrWhiteSpace(MacBox.Text) && !TryParseMac(MacBox.Text, out _))
        {
            StatusLine.Text = "MAC em seis bytes, por exemplo AA:BB:CC:DD:EE:FF.";
            return;
        }

        _config.Channel = ChannelBox.SelectedItem is int channel ? channel : 6;
        _config.TxPowerQdbm = (int)PowerSlider.Value;
        HoldEdits();
        PushConfig(save: true);
        StatusLine.Text = MacPreset.IsChecked == true
            ? "Fone C3 enviado. O enlace aparece quando o fone está ligado no canal 6."
            : "MAC personalizado enviado. O canal troca quando o fone confirma.";
    }

    private void OfferPreset(HeadsetStatus status)
    {
        if (_presetSent || MacPreset.IsChecked != true || IsRealMac(status.PeerMac))
        {
            return;
        }

        _presetSent = true;
        PushConfig(save: true);
        StatusLine.Text = "Fone C3 90:DA:72:84:B0:20 enviado ao dongle.";
    }

    private void SyncMacChoice(byte[] peer)
    {
        if (MacEquals(peer, PresetPeer))
        {
            MacPreset.IsChecked = true;
            return;
        }

        if (!IsRealMac(peer))
        {
            return;
        }

        MacCustom.IsChecked = true;
        MacBox.Text = FormatMac(peer);
    }

    private byte[] PeerMacFromChoice()
    {
        if (MacCustom.IsChecked == true)
        {
            return TryParseMac(MacBox.Text, out var mac) ? mac : new byte[6];
        }

        return PresetPeer.ToArray();
    }

    private static bool IsRealMac(byte[] mac) => mac.Length == 6 && mac.Any(b => b != 0) && mac.Any(b => b != 0xFF);

    private static bool MacEquals(byte[] left, byte[] right) =>
        left.Length == right.Length && left.AsSpan().SequenceEqual(right);

    private void StartWithWindows_Changed(object sender, RoutedEventArgs e)
    {
        if (_loading)
        {
            return;
        }

        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(RunKeyPath, writable: true);
            if (key == null)
            {
                return;
            }

            if (StartWithWindows.IsChecked == true)
            {
                key.SetValue(RunValueName, Environment.ProcessPath ?? "");
            }
            else
            {
                key.DeleteValue(RunValueName, throwOnMissingValue: false);
            }
        }
        catch (Exception ex)
        {
            StatusLine.Text = ex.Message;
        }
    }

    private void HoldEdits()
    {
        _editHoldUntil = Environment.TickCount64 + 1500;
    }

    private bool Editing() => Environment.TickCount64 < _editHoldUntil;

    private void ScheduleSend()
    {
        HoldEdits();
        _sendTimer.Stop();
        _sendTimer.Start();
    }

    private void PushConfig(bool save)
    {
        CollectTransport();
        _config.TxPowerQdbm = (int)PowerSlider.Value;
        if (ChannelBox.SelectedItem is int channel)
        {
            _config.Channel = channel;
        }

        _config.SidetoneDb = (int)SidetoneSlider.Value;
        _config.MicBoostDb = (int)BoostSlider.Value;
        _config.SpeakerDb = HasWindowsSpeaker() ? 0 : SpeakerDb();
        _config.MicDb = HasWindowsMic() ? 0 : MicDb();
        var flags = 0;
        if (MuteBox.IsChecked == true) flags |= 0x01;
        if (ToneBox.IsChecked == true) flags |= 0x02;
        if (save) flags |= 0x04;
        _config.Flags = (byte)flags;
        _config.PeerMac = PeerMacFromChoice();

        _link.Send(_config);
    }

    private void CollectTransport()
    {
        if (AdvancedTransport.IsChecked == true)
        {
            _config.Profile = LinkMath.ProfileCustom;
            _config.FrameMs = TagOf(FrameBox, 5);
            _config.JitterFrames = TagOf(JitterBox, 2);
            return;
        }

        if (ProfileGame.IsChecked == true)
        {
            _config.Profile = LinkMath.ProfileGame;
        }
        else if (ProfileStable.IsChecked == true)
        {
            _config.Profile = LinkMath.ProfileStable;
        }
        else
        {
            _config.Profile = LinkMath.ProfileBalance;
        }

        var frame = _config.FrameMs;
        var jitter = _config.JitterFrames;
        LinkMath.Profile(_config.Profile, ref frame, ref jitter);
        _config.FrameMs = frame;
        _config.JitterFrames = jitter;
    }

    private void UpdateLatencyPreview()
    {
        if (LatencyPreview == null)
        {
            return;
        }

        int frame = 5;
        int jitter = 2;
        if (AdvancedTransport?.IsChecked == true)
        {
            frame = TagOf(FrameBox, 5);
            jitter = TagOf(JitterBox, 2);
        }
        else if (ProfileGame?.IsChecked == true)
        {
            frame = 5;
            jitter = 1;
        }
        else if (ProfileStable?.IsChecked == true)
        {
            frame = 10;
            jitter = 2;
        }

        LatencyPreview.Text = $"Ida estimada {LinkMath.OneWayMs(frame, jitter)} ms · ida e volta {LinkMath.RoundTripMs(frame, jitter)} ms";
    }

    private void PullWindowsVolume()
    {
        _loading = true;
        if (Editing())
        {
            _loading = false;
            return;
        }

        if (_volume.HasSpeaker && !SpeakerSlider.IsMouseCaptureWithin)
        {
            SpeakerSlider.Value = _volume.SpeakerLevel * 100.0;
        }

        if (_volume.HasMicrophone && !MicSlider.IsMouseCaptureWithin)
        {
            MicSlider.Value = _volume.MicrophoneLevel * 100.0;
        }

        _loading = false;
        SpeakerLabel.Text = _volume.HasSpeaker
            ? $"Alto-falante  {SpeakerSlider.Value:0}%"
            : $"Alto-falante no firmware  {SpeakerDb()} dB";
        MicLabel.Text = _volume.HasMicrophone
            ? $"Microfone  {MicSlider.Value:0}%"
            : $"Microfone no firmware  {MicDb()} dB";
        VolumeHint.Text = _volume.HasSpeaker
            ? "O slider move o volume do Windows. O ganho do firmware fica em 0 dB para não atenuar duas vezes."
            : "O Windows ainda não listou o alto-falante Headset Wireless. O slider vai direto para o ganho do dongle.";
    }

    private bool HasWindowsSpeaker() => _volume.HasSpeaker;
    private bool HasWindowsMic() => _volume.HasMicrophone;

    private int SpeakerDb() => (int)Math.Round(-60.0 * (1.0 - SpeakerSlider.Value / 100.0));
    private int MicDb() => (int)Math.Round(-60.0 * (1.0 - MicSlider.Value / 100.0));

    private void DrawPlot()
    {
        Plot.Children.Clear();
        if (_rssi.Count < 2 || Plot.ActualWidth < 10)
        {
            PlotCaption.Text = _rssi.Count == 0 ? "Aguardando amostras." : "Aguardando mais amostras.";
            return;
        }

        var width = Plot.ActualWidth;
        var height = Plot.ActualHeight;
        var pen = new SolidColorBrush(Color.FromRgb(0x0E, 0x7C, 0x72));
        var lossPen = new SolidColorBrush(Color.FromRgb(0xC4, 0x6B, 0x2D));
        AddLine(_rssi, -90, -30, width, height, pen);
        AddLine(_loss, 0, 100, width, height, lossPen);
        PlotCaption.Text = $"Verde: RSSI de {_rssi.Min()} a {_rssi.Max()} dBm. Laranja: perdas por mil.";
    }

    private void AddLine(IReadOnlyList<int> values, double min, double max, double width, double height, Brush stroke)
    {
        var figure = new Polyline { Stroke = stroke, StrokeThickness = 2 };
        var span = Math.Max(1, max - min);
        for (var i = 0; i < values.Count; i++)
        {
            var x = width * i / (values.Count - 1);
            var y = height - (Math.Clamp(values[i], min, max) - min) / span * (height - 8) - 4;
            figure.Points.Add(new Point(x, y));
        }

        Plot.Children.Add(figure);
    }

    private static int TagOf(ComboBox box, int fallback)
    {
        if (box.SelectedItem is ComboBoxItem item && item.Tag != null && int.TryParse(item.Tag.ToString(), out var value))
        {
            return value;
        }

        return fallback;
    }

    private static void SelectTag(ComboBox box, int value)
    {
        foreach (var entry in box.Items)
        {
            if (entry is ComboBoxItem item && item.Tag != null && int.TryParse(item.Tag.ToString(), out var tag) && tag == value)
            {
                box.SelectedItem = item;
                return;
            }
        }
    }

    private static string FormatMac(byte[] mac)
    {
        if (mac.All(b => b == 0) || mac.All(b => b == 0xFF))
        {
            return "";
        }

        return string.Join(":", mac.Select(b => b.ToString("X2")));
    }

    private static bool TryParseMac(string text, out byte[] mac)
    {
        mac = new byte[6];
        var parts = text.Split(new[] { ':', '-', ' ' }, StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length != 6)
        {
            return false;
        }

        for (var i = 0; i < 6; i++)
        {
            if (!byte.TryParse(parts[i], NumberStyles.HexNumber, CultureInfo.InvariantCulture, out mac[i]))
            {
                return false;
            }
        }

        return true;
    }

    private static bool IsStartupEnabled()
    {
        using var key = Registry.CurrentUser.OpenSubKey(RunKeyPath, writable: false);
        return key?.GetValue(RunValueName) != null;
    }
}
