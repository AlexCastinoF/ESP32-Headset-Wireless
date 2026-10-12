using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

namespace HeadsetManager.Services;

public static class AppTrayIcon
{
    private static Bitmap? _crest;

    public static Icon ForTray() => DrawCrest(32);

    public static Icon ForWindow() => DrawCrest(64);

    public static Icon Battery(int? percent, bool light)
    {
        using var canvas = new Bitmap(32, 32, PixelFormat.Format32bppArgb);
        using var g = Graphics.FromImage(canvas);
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        g.Clear(Color.Transparent);
        var outline = light ? Color.Black : Color.White;
        var empty = percent is null;
        var low = percent is int value && value <= 15;
        var stroke = empty ? Color.FromArgb(160, outline) : low ? Color.FromArgb(255, 220, 70, 60) : outline;
        using var pen = new Pen(stroke, 2f) { LineJoin = LineJoin.Round };
        g.DrawRectangle(pen, 3, 9, 21, 14);
        using var nub = new SolidBrush(stroke);
        g.FillRectangle(nub, 26, 13, 3, 6);
        if (percent is int level && level > 0)
        {
            var width = Math.Clamp((int)Math.Round(17 * Math.Clamp(level, 0, 100) / 100.0), 1, 17);
            using var fill = new SolidBrush(low ? Color.FromArgb(255, 220, 70, 60) : outline);
            g.FillRectangle(fill, 5, 11, width, 10);
        }

        return Own(canvas);
    }

    private static Icon DrawCrest(int size)
    {
        using var canvas = new Bitmap(size, size, PixelFormat.Format32bppArgb);
        using var g = Graphics.FromImage(canvas);
        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.Clear(Color.Transparent);
        g.DrawImage(Crest(), new Rectangle(0, 0, size, size));
        return Own(canvas);
    }

    private static Bitmap Crest()
    {
        if (_crest != null)
        {
            return _crest;
        }

        using var raw = new Bitmap(Locate());
        var minX = raw.Width;
        var minY = raw.Height;
        var maxX = 0;
        var maxY = 0;
        for (var y = 0; y < raw.Height; y++)
        {
            for (var x = 0; x < raw.Width; x++)
            {
                var pixel = raw.GetPixel(x, y);
                if (pixel.R + pixel.G + pixel.B <= 36)
                {
                    continue;
                }

                if (x < minX) minX = x;
                if (y < minY) minY = y;
                if (x > maxX) maxX = x;
                if (y > maxY) maxY = y;
            }
        }

        var side = Math.Max(1, (int)((maxY - minY) * 0.72));
        var cropX = Math.Clamp((minX + maxX) / 2 - side / 2, 0, Math.Max(0, raw.Width - side));
        var cropY = Math.Clamp(minY - 4, 0, Math.Max(0, raw.Height - side));
        var crest = raw.Clone(new Rectangle(cropX, cropY, side, side), PixelFormat.Format32bppArgb);
        for (var y = 0; y < crest.Height; y++)
        {
            for (var x = 0; x < crest.Width; x++)
            {
                var pixel = crest.GetPixel(x, y);
                if (pixel.R < 14 && pixel.G < 14 && pixel.B < 14)
                {
                    crest.SetPixel(x, y, Color.Transparent);
                }
            }
        }

        _crest = crest;
        return crest;
    }

    private static string Locate()
    {
        var beside = Path.Combine(AppContext.BaseDirectory, "icone.png");
        if (File.Exists(beside))
        {
            return beside;
        }

        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir != null)
        {
            var candidate = Path.Combine(dir.FullName, "icone.png");
            if (File.Exists(candidate))
            {
                return candidate;
            }

            dir = dir.Parent;
        }

        throw new FileNotFoundException("icone.png não encontrado.", beside);
    }

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
