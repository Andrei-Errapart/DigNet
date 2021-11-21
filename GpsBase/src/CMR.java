// vim: shiftwidth=4
// vim: ts=4
package com.errapartengineering.DigNet;
import java.io.OutputStream;


/// Compact measurement record packet.
public final class CMR {
	public final int type;
	public final byte[] data;

	public final static int	TYPE_PING	= 0x0000;
    /** Encapsulates RTCM stream from the GPS base station.
     */
    public final static int TYPE_RTCM    = 0xBC05;
    /** Data read from serial port. First byte in the data block
     * encodes port number.
     */
    public final static int TYPE_SERIAL  = 0xBC06;
    /** Dignet package. Value is the the same as TYPE_LAMPNET.
     */
    public final static int TYPE_DIGNET  = 0xBC07;
    public final static int TYPE_LAMPNET = 0xBC07;
    public final static int TYPE_LAMPNET_MULTIPACKET_BEGIN = 0xBC08;
    public final static int TYPE_LAMPNET_MULTIPACKET_PAYLOAD = 0xBC09;
    public final static int TYPE_LAMPNET_MULTIPACKET_END = 0xBC0A;

    /** Maximum data block size.
     */
    public final static int MAX_DATABLOCK_LENGTH = 0xFF - 6;
    /** Maximum packet length. */
    public final static int MAX_PACKET_LENGTH = 0xFF;

	/** Maximum payload length for emitting multi-packets instead. */
	public final static int MULTIPACKET_PAYLOAD_THRESHOLD = 0xFF-6;

	/// Construct new CMR.
	public CMR(int type, byte[] data)
	{
		this.type = type;
		this.data = data;
	}

    /// Construct new CMR.
    public CMR(int type, byte[] data, int size)
    {
        this.type = type;
        this.data = new byte[size];
        for (int i = 0; i < size; ++i)
        {
            this.data[i] = data[i];
        }
    }

    /// Construct new CMR.
    public CMR(int type, String data)
	{
		this.type = type;
		this.data = data.getBytes();
	}

	/// Static version of encode.
	public final static void encode(OutputStream os, int type, byte[] data, int data_length) throws java.io.IOException
	{
        if (type == TYPE_PING)
        {
			os.write(0x00);
		} else {
            if (type == TYPE_LAMPNET && data_length > MULTIPACKET_PAYLOAD_THRESHOLD)
            {
				byte[]	mpacketbuffer = new byte[MULTIPACKET_PAYLOAD_THRESHOLD];
                int npackets = (data_length + MULTIPACKET_PAYLOAD_THRESHOLD - 1) / MULTIPACKET_PAYLOAD_THRESHOLD;
                int last_packet_size = data_length % MULTIPACKET_PAYLOAD_THRESHOLD;
				if (last_packet_size == 0) {
					last_packet_size = MULTIPACKET_PAYLOAD_THRESHOLD;
				}

				for (int packetIndex=0; packetIndex<npackets; ++packetIndex) {
					int offset = packetIndex*MULTIPACKET_PAYLOAD_THRESHOLD;
					if (packetIndex+1<npackets) {
						for (int dataIndex=0; dataIndex<MULTIPACKET_PAYLOAD_THRESHOLD; ++dataIndex) {
							mpacketbuffer[dataIndex] = data[offset + dataIndex];
						}
                        encode(os, packetIndex == 0 ? TYPE_LAMPNET_MULTIPACKET_BEGIN : TYPE_LAMPNET_MULTIPACKET_PAYLOAD, mpacketbuffer);
					} else {
						int	this_round = packetIndex+1<npackets ? MULTIPACKET_PAYLOAD_THRESHOLD : last_packet_size;
						if (this_round != mpacketbuffer.length) {
							mpacketbuffer = new byte[this_round];
						}
						for (int dataIndex=0; dataIndex<this_round; ++dataIndex) {
							mpacketbuffer[dataIndex] = data[offset + dataIndex];
						}
                        encode(os, TYPE_LAMPNET_MULTIPACKET_END, mpacketbuffer);
					}
				}
			} else {
                byte[] buffer = new byte[data_length+6];
				buffer[0] = 0x02; // header
				buffer[1] = (byte)((type & 0xFF00) >> 8);
				buffer[2] = (byte)( type & 0x00FF);
                buffer[3] = (byte)(data_length);

				byte checksum = (byte)(buffer[1] + buffer[2] + buffer[3]);
				int i;
                for (i = 0; i < data_length; ++i)
                {
					buffer[i + 4] = data[i];
					checksum = (byte)(checksum + data[i]);
				}
                buffer[data_length + 4] = checksum;
                buffer[data_length + 5] = 0x03; // trailer
				os.write(buffer, 0, buffer.length);
				os.flush();
			}
		}
	}

	/// Static version of encode.
    public final static void encode(OutputStream os, int type, byte[] data) throws java.io.IOException
    {
        encode(os, type, data, data.length);
    }

	/// Static version of emit.
	public final static void encode(OutputStream os, int type, String data) throws java.io.IOException
	{
        byte[] bdata = data.getBytes();
		encode(os, type, bdata, bdata.length);
	}

	/// Emit serialized data to outputstream.
    public final void encode(OutputStream os) throws java.io.IOException
	{
        encode(os, type, data, data.length);
	}

    /// Emit data from serial line to outputstream.
    public final static void encodeSerial(OutputStream os, int serialPortNo, String serialData) throws java.io.IOException
    {
        byte[] serial_block = serialData.getBytes();
        byte[] data_block = new byte[1+serialData.length()];

        data_block[0] = (byte)serialPortNo;
        for (int i = 0; i < serialData.length(); ++i)
        {
            data_block[i + 1] = serial_block[i];
        }

        encode(os, TYPE_SERIAL, data_block);
    }

    public final String toString()
	{
		switch (type) {
        case TYPE_PING:
			return "PING";
        case TYPE_LAMPNET:
			{
				StringBuffer	sb = new StringBuffer();

				for (int i=0; i<data.length; ++i) {
					// TODO: detect ASCII chars.
					sb.append((char)data[i]);
				}

				return sb.toString();
			}
		default:
			{
				StringBuffer	sb = new StringBuffer();

				sb.append("[" + type + " : ");

				for (int i=0; i<data.length; ++i) {
					if (i>0) {
						sb.append(' ');
					}
					sb.append(data[i]);
				}

				sb.append(']');
				return sb.toString();
			}
		}
	}
} // class CMR

