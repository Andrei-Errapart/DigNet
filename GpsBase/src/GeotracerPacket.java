package com.errapartengineering.DigNet;

/** Geotracer binary packet representation. */
public final class GeotracerPacket
{
    // public const byte PACKET_START = 0x41;
    public final static byte PACKET_END = 0x0A;

    public final static String AWRITE = "AWRITE";
    public final static String AFILEDATA = "AFILEDATA_";
    public final static String CU = "CU";
    public final static String AVERSION = "AVERSION__";

    public final String Type;
    public final byte[] Payload;

    public GeotracerPacket(String type, byte[] payload)
    {
        Type = type;
        Payload = payload;
    }

    private final int NSkipAndText(StringBuffer sb)
    {
        sb.append(Type);
        int nskip = 0;
        if (Type == AFILEDATA)
        {
            // int datalength = 256 * buffer_[startpos + 11] + buffer_[startpos + 12];
            // total length = "AFILEDATA_" + space + length  + data + checksum + LF.
            // int total_length = 10 + 1 + 2 + datalength:2 + 2;
            int datalength = 256 * Payload[0] + Payload[1];
            nskip = 4;
            sb.append(" L=" + datalength);
        }
        else if (Type == AWRITE)
        {
            // int datalength = 256 * buffer_[startpos + 12] + buffer_[startpos + 13];
            // total length = "AWRITE" + space + handle:4 + length:2  + data + checksum + LF.
            // int total_length = 6 + 1 + 4 + 1 + 2 + datalength + 2;
            int handle = 16777216 * Payload[0] + 65536 * Payload[1] + 256 * Payload[2] + Payload[3];
            int datalength = 256 * Payload[5] + Payload[6];
            nskip = 7;
            sb.append(" L="+datalength+" H="+handle);
        }
        return nskip;
    }

    public final String toString()
    {
        int i;
        if (Payload == null || Payload.length == 0)
        {
            return Type;
        }
        StringBuffer sb = new StringBuffer();
        int nskip = NSkipAndText(sb); // number of payload bytes to skip;
        for (i=0; i<Payload.length; ++i)
        {
            byte b = Payload[i];
            if (nskip <= 0)
            {
                sb.append(" " + Utils.hex_of_byte(b));
            }
            else
            {
                --nskip;
            }
        }
        return sb.toString();
    }
} // class GeotracerPacket
