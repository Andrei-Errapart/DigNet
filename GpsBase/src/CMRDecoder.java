// vim: shiftwidth=4
// vim: ts=4
package com.errapartengineering.DigNet;

import java.util.Vector;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;


/**
Assemble CMR records from the data feed.
*/
public final class CMRDecoder {
	private final Vector	cmr_queue_ = new Vector();
	private byte[] 	buffer_ = new byte[100];
	private int		buffer_size_ = 0;

	private final static byte	PACKET_PING		= 0x00;
	private final static byte	PACKET_HEADER	= 0x02;
	private final static byte	PACKET_TRAILER	= 0x03;

    /** Append one byte of data to the back of the buffer.
     */
	private final void buffer_push_back(byte d)
	{
		if (buffer_size_ >= buffer_.length)  {
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


	/** Feed some data in hope to complete some packets.
     */
	public final void feed(	byte[] data, int length)
	{
		int i;

		for (i=0; i<length; ++i) {
			buffer_push_back(data[i]);
		}

		int	so_far;	///< Bytes processed this round.
		do {
			so_far = 0;

			// Scroll forward to the next packet.
			while (buffer_size_>so_far && buffer_[so_far]!=PACKET_HEADER) {
				// Any pings?
				if (buffer_[so_far] == PACKET_PING) {
                    CMR cmr = new CMR(CMR.TYPE_PING, new byte[0]);
					cmr_queue_.addElement(cmr);
				}
				++so_far;
			}

			if (buffer_size_>=6 && buffer_[so_far]==PACKET_HEADER) {
				int	payload_length = Utils.uint_of_byte(buffer_[so_far + 3]);
				int	total_length = payload_length + 6;

				if (so_far + total_length <= buffer_size_) {
					int		cmr_type = Utils.uint_of_byte(buffer_[so_far + 1])*256 + Utils.uint_of_byte(buffer_[so_far + 2]);
					byte[]	cmr_data = new byte[payload_length];
					byte	checksum = (byte)(buffer_[so_far + 1] + buffer_[so_far + 2] + buffer_[so_far + 3]);
					byte	checksum2 = buffer_[so_far + 4 + payload_length];
					int		trailer = buffer_[so_far + 5 + payload_length];

					for (i=0; i<payload_length; ++i) {
						byte c = buffer_[so_far + 4 + i];
						checksum = (byte)(checksum + c);
						cmr_data[i] = c;
					}
					if (checksum == checksum2 && trailer == PACKET_TRAILER) {
						CMR cmr = new CMR(cmr_type, cmr_data);
						cmr_queue_.addElement(cmr);
					}
					so_far += total_length;
				}
			}

			// Ditch the front.
			if (so_far > 0) {
				int	remaining = buffer_size_ - so_far;
				for (i=0; i<remaining; ++i) {
					buffer_[i] = buffer_[i + so_far];
				}
                buffer_size_ = remaining;
			}
		} while (so_far > 0);
	}

	/** Pop a CMR packet off the queue. Return null if queue empty.
     */
	public final CMR pop()
	{
		if (cmr_queue_.isEmpty()) {
			return null;
		} else {
			CMR 	cmr = (CMR) cmr_queue_.firstElement();
			cmr_queue_.removeElementAt(0);
			return cmr;
		}
	}

    /** Reset all internal buffers.
     */
    public final void clear()
    {
		cmr_queue_.removeAllElements();
        buffer_size_ = 0;
    }
} // class CMRAssembler

