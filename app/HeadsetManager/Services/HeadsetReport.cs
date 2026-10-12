using System.IO;

namespace HeadsetManager.Services;

public sealed class HeadsetStatus
{
    public bool Linked { get; init; }
    public int Battery { get; init; }
    public byte Flags { get; init; }
    public int Rssi { get; init; }
    public int LossPermille { get; init; }
    public int RttMs { get; init; }
    public int FrameMs { get; init; }
    public int JitterFrames { get; init; }
    public int Channel { get; init; }
    public int TxPowerQdbm { get; init; }
    public int SpeakerDb { get; init; }
    public int MicDb { get; init; }
    public int SidetoneDb { get; init; }
    public int MicBoostDb { get; init; }
    public int Profile { get; init; }
    public int ApplyId { get; init; }
    public byte[] PeerMac { get; init; } = new byte[6];
    public string Version { get; init; } = "";

    public bool MicMuted => (Flags & 0x01) != 0;
    public bool ToneOn => (Flags & 0x02) != 0;

    public static HeadsetStatus FromReport(byte[] buffer)
    {
        if (buffer.Length < 33 || buffer[0] != 1)
        {
            throw new InvalidDataException("Relatório de estado incompleto.");
        }

        var mac = new byte[6];
        Array.Copy(buffer, 19, mac, 0, 6);
        var versionBytes = new byte[8];
        Array.Copy(buffer, 25, versionBytes, 0, 8);
        var version = System.Text.Encoding.ASCII.GetString(versionBytes).Trim('\0', ' ');
        return new HeadsetStatus
        {
            Linked = buffer[1] != 0,
            Battery = buffer[2],
            Flags = buffer[3],
            Rssi = (sbyte)buffer[4],
            LossPermille = BitConverter.ToUInt16(buffer, 5),
            RttMs = BitConverter.ToUInt16(buffer, 7),
            FrameMs = buffer[9],
            JitterFrames = buffer[10],
            Channel = buffer[11],
            TxPowerQdbm = buffer[12],
            SpeakerDb = (sbyte)buffer[13],
            MicDb = (sbyte)buffer[14],
            SidetoneDb = (sbyte)buffer[15],
            MicBoostDb = buffer[16],
            Profile = buffer[17],
            ApplyId = buffer[18],
            PeerMac = mac,
            Version = version
        };
    }
}

public sealed class HeadsetConfig
{
    public int FrameMs { get; set; } = 5;
    public int JitterFrames { get; set; } = 2;
    public int Channel { get; set; } = 6;
    public int TxPowerQdbm { get; set; } = 60;
    public int SpeakerDb { get; set; }
    public int MicDb { get; set; }
    public int SidetoneDb { get; set; } = -60;
    public int MicBoostDb { get; set; }
    public byte Flags { get; set; }
    public byte[] PeerMac { get; set; } = new byte[6];
    public int Profile { get; set; } = LinkMath.ProfileBalance;

    public byte[] ToReport()
    {
        var buffer = new byte[17];
        buffer[0] = 2;
        buffer[1] = (byte)FrameMs;
        buffer[2] = (byte)JitterFrames;
        buffer[3] = (byte)Channel;
        buffer[4] = (byte)TxPowerQdbm;
        buffer[5] = (byte)(sbyte)SpeakerDb;
        buffer[6] = (byte)(sbyte)MicDb;
        buffer[7] = (byte)(sbyte)SidetoneDb;
        buffer[8] = (byte)MicBoostDb;
        buffer[9] = Flags;
        for (var i = 0; i < 6; i++)
        {
            buffer[10 + i] = i < PeerMac.Length ? PeerMac[i] : (byte)0;
        }
        buffer[16] = (byte)Profile;
        return buffer;
    }
}
