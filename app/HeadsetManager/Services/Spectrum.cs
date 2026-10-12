namespace HeadsetManager.Services;

public static class Spectrum
{
    public const int Size = 1024;
    public const int Bands = 36;

    public static void BandsOf(float[] samples, int sampleRate, float[] bands)
    {
        var re = new float[Size];
        var im = new float[Size];
        var copy = Math.Min(Size, samples.Length);
        for (var i = 0; i < copy; i++)
        {
            var window = 0.5f - 0.5f * MathF.Cos(2f * MathF.PI * i / (Size - 1));
            re[i] = samples[samples.Length - copy + i] * window;
        }

        Fft(re, im);
        Array.Clear(bands, 0, bands.Length);
        var rate = Math.Max(sampleRate, 1);
        for (var bin = 1; bin < Size / 2; bin++)
        {
            var hz = bin * rate / (float)Size;
            if (hz < 40 || hz > 16000)
            {
                continue;
            }

            var position = Math.Log(hz / 40.0) / Math.Log(16000.0 / 40.0);
            var band = (int)(position * (bands.Length - 1));
            band = Math.Clamp(band, 0, bands.Length - 1);
            var mag = MathF.Sqrt(re[bin] * re[bin] + im[bin] * im[bin]) / (Size / 4f);
            if (mag > bands[band])
            {
                bands[band] = mag;
            }
        }
    }

    private static void Fft(float[] re, float[] im)
    {
        var n = re.Length;
        for (int i = 1, j = 0; i < n; i++)
        {
            var bit = n >> 1;
            for (; (j & bit) != 0; bit >>= 1)
            {
                j ^= bit;
            }

            j ^= bit;
            if (i < j)
            {
                (re[i], re[j]) = (re[j], re[i]);
                (im[i], im[j]) = (im[j], im[i]);
            }
        }

        for (var len = 2; len <= n; len <<= 1)
        {
            var angle = -2 * Math.PI / len;
            var wLenRe = MathF.Cos((float)angle);
            var wLenIm = MathF.Sin((float)angle);
            for (var i = 0; i < n; i += len)
            {
                var wRe = 1f;
                var wIm = 0f;
                for (var k = 0; k < len / 2; k++)
                {
                    var u = i + k;
                    var v = u + len / 2;
                    var tRe = wRe * re[v] - wIm * im[v];
                    var tIm = wRe * im[v] + wIm * re[v];
                    re[v] = re[u] - tRe;
                    im[v] = im[u] - tIm;
                    re[u] += tRe;
                    im[u] += tIm;
                    var nextRe = wRe * wLenRe - wIm * wLenIm;
                    wIm = wRe * wLenIm + wIm * wLenRe;
                    wRe = nextRe;
                }
            }
        }
    }
}
