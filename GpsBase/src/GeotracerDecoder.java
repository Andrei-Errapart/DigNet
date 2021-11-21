package com.errapartengineering.DigNet;

import java.util.Vector;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/// <summary>
/// Assemble binary Geotracer packets from the stream.
/// </summary>
public final class GeotracerDecoder
{
    /// <summary>
    /// Input data buffer.
    /// </summary>
    private byte[] buffer_ = new byte[100];
    private int buffer_size_ = 0;

    /// <summary>
    /// Output queue.
    /// </summary>
    private final Vector packet_queue_ = new Vector();

    /// <summary>
    /// Maximum packet size (i.e. line length), bytes.
    /// </summary>
    public final static int MAX_PACKET_SIZE = 4096;

    private final static String CHARS_FIRST = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    private final static String CHARS_OTHERS = "_0123456789";

    /** Append one byte of data to the back of the buffer.
     */
    private final void buffer_push_back(byte d)
    {
        if (buffer_size_ >= buffer_.length)
        {
            int newsize = buffer_size_ * 2 + 10;
            byte[] newbuffer = new byte[newsize];

            for (int i = 0; i < buffer_size_; ++i)
            {
                newbuffer[i] = buffer_[i];
            }
            buffer_ = newbuffer;
        }
        buffer_[buffer_size_] = d;
        buffer_size_ = buffer_size_ + 1;
    }


    /// <summary>
    /// Feed some data in hope to complete some packets.
    /// </summary>
    /// <param name="data">Data bytes.</param>
    /// <param name="length">Data length.</param>
    public final void Feed(byte[] data, int length)
    {
        int i;
        for (int dataIndex = 0; dataIndex < length; ++dataIndex)
        {
            buffer_push_back(data[dataIndex]);
        }
        int n = buffer_size_;
        // String sbuffer = Encoding.ASCII.GetString(buffer_.ToArray(), 0, n);

        int so_far = 0;
        int consumed_count = 0;
        // Minimum command length:
        // addr + '=' + '\r' = 3 bytes.
        while (so_far+3<n)
        {
            int skipped_start = so_far;
            // Find first character...
            while (so_far < n && CHARS_FIRST.indexOf(Utils.char_of_byte(buffer_[so_far])) < 0)
            {
                ++so_far;
            }
            if (so_far >= n)
            {
                break;
            }
            int startpos = so_far;

            // Find command end position.
            int cmd_endpos = startpos + 1;  // first character after command.
            while (cmd_endpos < n && (CHARS_FIRST.indexOf(Utils.char_of_byte(buffer_[cmd_endpos])) >= 0 || CHARS_OTHERS.indexOf(Utils.char_of_byte(buffer_[cmd_endpos])) >= 0))
            {
                ++cmd_endpos;
            }
            if (cmd_endpos >= n)
            {
                break;
            }

            // Command is finished :)
            String cmd = null;
            {
                StringBuffer sb = new StringBuffer();
                for (i = startpos; i < cmd_endpos; ++i)
                {
                    sb.append(Utils.char_of_byte(buffer_[i]));
                }
                // sbuffer.Substring(startpos, cmd_endpos - startpos);
                cmd = sb.toString();
            }

            // Find end of command and payload.
            int endpos = cmd_endpos + 1;
            if (cmd == GeotracerPacket.AFILEDATA)
            {
                if (startpos + 13 >= n)
                {
                    break;
                }
                int datalength = 256 * buffer_[startpos + 11] + buffer_[startpos + 12];
                // total length = "AFILEDATA_" + space + length  + data + checksum + LF.
                int total_length = buffer_[startpos + 13] == 0x0D
                    ? 10 + 1 + 2 + 1
                    : 10 + 1 + 2 + datalength + 2;
                endpos = startpos + total_length;
            }
            else if (cmd == GeotracerPacket.AWRITE)
            {
                //      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14
                //L8:	41 57 52 49 54 45 20 00 00 03 E7 20 03 65 34 00 00 00 00 00 00 00 00 00 
                //      A  W  R  I  T  E            999      869 
                if (startpos + 14 >= n)
                {
                    break;
                }
                int datalength = 256 * buffer_[startpos + 12] + buffer_[startpos + 13];
                // total length = "AWRITE" + space + handle:4 + space + length:2  + data + checksum + LF.
                int total_length = 6 + 1 + 4 + 1 + 2 + datalength + 2;
                endpos = startpos + total_length;
            }
            else if (cmd == GeotracerPacket.CU)
            {
                int nsats = buffer_[startpos + 2];
                int total_length = 31 + nsats * 6 + 1;
                endpos = startpos + total_length;
                --cmd_endpos;
            }
            else
            {
                while (endpos < n && buffer_[endpos] != GeotracerPacket.PACKET_END)
                {
                    ++endpos;
                }
                ++endpos;
            }
            if (endpos > n)
            {
                break;
            }

            try
            {
                if (skipped_start < startpos)
                {
                    int nskipped = startpos - skipped_start;
                    byte[] skipped = new byte[nskipped];
                    for (i = 0; i < nskipped; ++i)
                    {
                        skipped[i] = buffer_[skipped_start + i];
                    }
                    packet_queue_.addElement(new GeotracerPacket("SKIPPED", skipped));
                }
                int npayload = endpos - cmd_endpos - 1;
                byte[] payload = null;
                if (npayload > 0)
                {
                    payload = new byte[npayload];
                    for (i = 0; i < npayload; ++i)
                    {
                        payload[i] = buffer_[cmd_endpos + i + 1];
                    }
                }
                GeotracerPacket packet = new GeotracerPacket(cmd, payload);
                packet_queue_.addElement(packet);
            }
            catch (Exception ex)
            {
                // Pass.
            }
            so_far = endpos;
            consumed_count = endpos;
        }

        // Ditch the front.
        if (consumed_count > 0)
        {
            int remaining = buffer_size_ - consumed_count;
            for (i = 0; i < remaining; ++i)
            {
                buffer_[i] = buffer_[i + consumed_count];
            }
            buffer_size_ = remaining;
        }
    }

    /// <summary>
    /// Feed some data in hope to complete some packets.
    /// </summary>
    /// <param name="data">Data bytes.</param>
    public final void Feed(byte[] data)
    {
        Feed(data, data.length);
    }

    /// <summary>
    /// Pop packet off the queue.
    /// </summary>
    /// <returns>New GeotracerPacket, or null if there is none.</returns>
    public final GeotracerPacket Pop()
    {
        if (packet_queue_.isEmpty())
        {
            return null;
        }
        else
        {
            GeotracerPacket packet = (GeotracerPacket)packet_queue_.firstElement();
            packet_queue_.removeElementAt(0);
            return packet;
        }
    }
}
