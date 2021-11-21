// vim: shiftwidth=4
// vim: ts=4
package com.errapartengineering.DigNet;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import java.util.Vector;
import java.util.Date;

import com.nokia.m2m.imp.watchdog.WatchdogTimer;

import javax.microedition.io.StreamConnection;
import javax.microedition.midlet.MIDletStateChangeException;

/**
 * GpsBase
 */
public final class ApplicationModemBox extends Application {
    /// GPS coordinate sending period, milliseconds.
    private final static long GPS_SEND_PERIOD = 10*1000;
    /// GPS coordinate max. age, milliseconds
    private final static long GPS_MAX_AGE = 3 * 1000;

    /// Supply voltage sending period, milliseconds.
    private final static long SUPPLY_SEND_PERIOD = 5 * 60 * 1000;

    private CMRDecoder serial_decoder = new CMRDecoder();

    /*--------------------------------------------------------------------*/
    /// Accumulate GPS lines with the given prefix.
    private final static class GpsLine
    {
        public long LineTime = -1;
        public String Line = null;

        private String prefix = "$GPGGA";
        private StringBuffer sb = new StringBuffer();

        public GpsLine(String prefix)
        {
            this.prefix = prefix;
        }

        public final void Feed(CMR Packet)
        {
            // skip first byte.
            for (int i = 1; i < Packet.data.length; ++i )
            {
                // End of line?
                byte b = Packet.data[i];
                if (b == 0x0D)
                {
                    String line = sb.toString();
                    if (line.startsWith(prefix))
                    {
                        Line = line;
                        LineTime = System.currentTimeMillis();
                    }
                    sb.setLength(0);
                }
                else
                {
                    if (b != 0x0A)
                    {
                        sb.append(Utils.char_of_byte(b));
                    }
                }
            }
            if (sb.length() + 1 >= CMR.MAX_DATABLOCK_LENGTH)
            {
                sb.setLength(0);
            }
        }
    }

    /*--------------------------------------------------------------------*/
    public void ProcessStartup() throws MIDletStateChangeException
    {
        if (Watchdog == null)
        {
            Watchdog = new WatchdogTimer();
            Watchdog.setTimeout(15 * 60);	// 15 minutes.
        }
        SerialPortSpeed = 57600;
    }

    /*--------------------------------------------------------------------*/
    /** Last time supply voltage time was sent.
     */
    private long SupplyVoltageTime = -1;

    /*--------------------------------------------------------------------*/
    /** Send supply voltage to the clients.
     */
    private void SendSupplyVoltage(OutputStream ServerOut) throws IOException
    {
        int fp_voltage = IOPanel.ModemBoxSupplyVoltage();
        String s_voltage = oMathFP.toString(fp_voltage);
        CMR msg = new CMR(CMR.TYPE_DIGNET, "modembox supplyvoltage=" + s_voltage);

        SupplyVoltageTime = System.currentTimeMillis();

        if (ServerOut != null)
        {
            msg.encode(ServerOut);
        }
        if (SerialOut != null)
        {
            msg.encode(SerialOut);
        }
    }

    /*--------------------------------------------------------------------*/
    public void ProcessIdle(OutputStream ServerOut) throws IOException, StopException
    {
        long now = System.currentTimeMillis();

        if (SupplyVoltageTime < 0 || SupplyVoltageTime + SUPPLY_SEND_PERIOD < now)
        {
            SendSupplyVoltage(ServerOut);
        }
    }

    /*--------------------------------------------------------------------*/
    public void ProcessConnect(
            OutputStream ServerOut) throws IOException, StopException
    {
        SendSupplyVoltage(ServerOut);
    }

    /*--------------------------------------------------------------------*/
    public void ProcessPacket(
        OutputStream    ServerOut,
        CMR Packet) throws IOException, StopException
    {
        if (this.SerialOut != null)
        {
            Packet.encode(SerialOut);
			if (Packet.type == CMR.TYPE_RTCM)
			{
				IOPanel.ToggleRtcmTraffic();
			}
        }
    }

    /*--------------------------------------------------------------------*/
    // Last time GPS position was sent.
    private long GpsPositionTime = -1;
    private GpsLine[] GpsLines = null;

    /*--------------------------------------------------------------------*/
    public void ProcessSerial(
            OutputStream ServerOut,
            byte[] SerialData,
            int SerialLength
        ) throws IOException
    {
        serial_decoder.feed(SerialData, SerialLength);
        long now = System.currentTimeMillis();
        for (CMR Packet = serial_decoder.pop(); Packet != null; Packet = serial_decoder.pop())
        {
			IOPanel.TogglePacketTraffic();
            // Watch the serial packets.
            if (Packet.type == CMR.TYPE_SERIAL)
            {
                if (GpsLines == null)
                {
                    GpsLines = new GpsLine[2];
                    GpsLines[0] = new GpsLine("$GPGGA");
                    GpsLines[1] = new GpsLine("$GPGGA");
                }
                if (Packet.data != null && Packet.data.length > 1 && Packet.data[0] >= 0 && Packet.data[0] <= 1)
                {
                    int index = Packet.data[0];
                    GpsLines[index].Feed(Packet);
                }
            }
            else if (ServerOut != null)
            {
                    Packet.encode(ServerOut);
            }
        }

        // Check if we need to send data.
        if (GpsLines!=null && (GpsPositionTime < 0 || GpsPositionTime + GPS_SEND_PERIOD < now))
        {
            GpsPositionTime = now;
            for (int i = 0; i < GpsLines.length; ++i)
            {
                GpsLine gline = GpsLines[i];
                if (gline.Line!=null && gline.LineTime + GPS_MAX_AGE >= now)
                {
                    if (ServerOut != null)
                    {
                        CMR.encodeSerial(ServerOut, i, gline.Line);
                    }
                }
            }
        }
    }
}

