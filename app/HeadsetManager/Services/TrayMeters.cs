using System.IO;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;
using Microsoft.Win32;
using Forms = System.Windows.Forms;

namespace HeadsetManager.Services;

public sealed class TrayMeters : IDisposable
{
    private readonly Forms.NotifyIcon _left;
    private readonly Forms.NotifyIcon _right;
    private readonly Forms.NotifyIcon _mic;
    private readonly Forms.ContextMenuStrip _menu;
    private int _leftStep = -1;
    private int _rightStep = -1;
    private int _micStep = -1;
    private bool _light;
    private bool _themeKnown;

    public TrayMeters(Action open, Action exit)
    {
        _menu = new Forms.ContextMenuStrip();
        _menu.Items.Add("Abrir", null, (_, _) => open());
        _menu.Items.Add("Sair", null, (_, _) => exit());
        _mic = Create("Microfone");
        _right = Create("Alto-falante direito");
        _left = Create("Alto-falante esquerdo");
        SetLevels(0, 0, 0);
    }

    public void SetLevels(double left, double right, double microphone)
    {
        var light = TaskbarIsLight();
        var themeChanged = !_themeKnown || _light != light;
        _themeKnown = true;
        _light = light;
        Paint(_left, false, left, ref _leftStep, "Esquerdo", light, themeChanged);
        Paint(_right, true, right, ref _rightStep, "Direito", light, themeChanged);
        PaintMic(_mic, microphone, ref _micStep, light, themeChanged);
    }

    public void Promote()
    {
        var exe = Environment.ProcessPath;
        if (string.IsNullOrWhiteSpace(exe))
        {
            return;
        }

        using var key = Registry.CurrentUser.OpenSubKey(@"Control Panel\NotifyIconSettings", writable: true);
        if (key == null)
        {
            return;
        }

        foreach (var name in key.GetSubKeyNames())
        {
            using var sub = key.OpenSubKey(name, writable: true);
            var path = sub?.GetValue("ExecutablePath") as string;
            if (sub != null && string.Equals(path, exe, StringComparison.OrdinalIgnoreCase))
            {
                sub.SetValue("IsPromoted", 1, RegistryValueKind.DWord);
            }
        }
    }

    public void Dispose()
    {
        DisposeIcon(_left);
        DisposeIcon(_right);
        DisposeIcon(_mic);
        _menu.Dispose();
    }

    private Forms.NotifyIcon Create(string tip)
    {
        return new Forms.NotifyIcon
        {
            Visible = true,
            Text = tip,
            ContextMenuStrip = _menu
        };
    }

    private static void Paint(Forms.NotifyIcon icon, bool facingRight, double level, ref int step, string name, bool light, bool themeChanged)
    {
        var next = Step(level);
        icon.Text = $"{name}  {Math.Round(Math.Clamp(level, 0, 1) * 100)}%";
        if (next == step && !themeChanged && icon.Icon != null)
        {
            return;
        }

        step = next;
        Swap(icon, Glyph.Speaker(facingRight, next, light));
    }

    private static void PaintMic(Forms.NotifyIcon icon, double level, ref int step, bool light, bool themeChanged)
    {
        var next = Step(level);
        icon.Text = $"Microfone  {Math.Round(Math.Clamp(level, 0, 1) * 100)}%";
        if (next == step && !themeChanged && icon.Icon != null)
        {
            return;
        }

        step = next;
        Swap(icon, Glyph.Microphone(next, light));
    }

    private static int Step(double level) => (int)Math.Round(Math.Clamp(level, 0, 1) * 3);

    private static bool TaskbarIsLight()
    {
        using var key = Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize");
        return key?.GetValue("SystemUsesLightTheme") is int value && value != 0;
    }

    private static void Swap(Forms.NotifyIcon icon, Icon next)
    {
        var previous = icon.Icon;
        icon.Icon = next;
        previous?.Dispose();
    }

    private static void DisposeIcon(Forms.NotifyIcon icon)
    {
        icon.Visible = false;
        icon.Icon?.Dispose();
        icon.Dispose();
    }

    private static class Glyph
    {
        public static Icon Speaker(bool facingRight, int waves, bool light)
        {
            using var bmp = Canvas();
            using var g = Graphics.FromImage(bmp);
            Prepare(g);
            var color = light ? Color.Black : Color.White;
            using var brush = new SolidBrush(color);
            using var pen = PenOf(color);
            if (!facingRight)
            {
                g.TranslateTransform(32, 0);
                g.ScaleTransform(-1, 1);
            }

            g.FillRectangle(brush, 5, 13, 6, 6);
            g.FillPolygon(brush, new[]
            {
                new PointF(11, 13),
                new PointF(18, 8),
                new PointF(18, 24),
                new PointF(11, 19)
            });
            for (var i = 0; i < waves; i++)
            {
                var size = 7 + i * 5;
                g.DrawArc(pen, 16 + i, 16 - size / 2f, size, size, -55, 110);
            }

            return Own(bmp);
        }

        public static Icon Microphone(int waves, bool light)
        {
            using var bmp = Canvas();
            using var g = Graphics.FromImage(bmp);
            Prepare(g);
            var color = light ? Color.Black : Color.White;
            using var pen = PenOf(color);
            g.DrawEllipse(pen, 12, 3, 8, 12);
            g.DrawArc(pen, 8, 8, 16, 12, 15, 150);
            g.DrawLine(pen, 16, 20, 16, 25);
            g.DrawLine(pen, 11, 25, 21, 25);
            for (var i = 0; i < waves; i++)
            {
                var size = 6 + i * 4;
                g.DrawArc(pen, 20, 8 - i, size, size + 4, -40, 80);
            }

            return Own(bmp);
        }

        private static Bitmap Canvas() => new(32, 32, System.Drawing.Imaging.PixelFormat.Format32bppArgb);

        private static void Prepare(Graphics g)
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
            g.Clear(Color.Transparent);
        }

        private static Pen PenOf(Color color) => new(color, 2f)
        {
            StartCap = LineCap.Round,
            EndCap = LineCap.Round,
            LineJoin = LineJoin.Round
        };

        private static Icon Own(Bitmap bitmap)
        {
            var handle = bitmap.GetHicon();
            try
            {
                using var temp = Icon.FromHandle(handle);
                using var stream = new MemoryStream();
                temp.Save(stream);
                stream.Position = 0;
                return new Icon(stream);
            }
            finally
            {
                DestroyIcon(handle);
            }
        }

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool DestroyIcon(IntPtr handle);
    }
}
