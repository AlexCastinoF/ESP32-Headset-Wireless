namespace HeadsetManager.Services;

public static class LinkMath
{
    public const int ProfileGame = 0;
    public const int ProfileBalance = 1;
    public const int ProfileStable = 2;
    public const int ProfileCustom = 3;

    public static int OneWayMs(int frameMs, int jitter) => frameMs + 1 + jitter * frameMs + 11;

    public static int RoundTripMs(int frameMs, int jitter) => 2 * OneWayMs(frameMs, jitter) + 8;

    public static void Profile(int profile, ref int frameMs, ref int jitter)
    {
        if (profile == ProfileGame)
        {
            frameMs = 5;
            jitter = 1;
        }
        else if (profile == ProfileStable)
        {
            frameMs = 10;
            jitter = 2;
        }
        else if (profile == ProfileBalance)
        {
            frameMs = 5;
            jitter = 2;
        }
    }
}
