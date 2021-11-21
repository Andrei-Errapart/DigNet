// vim: shiftwidth=4
// vim: ts=4
package com.errapartengineering.DigNet;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import java.util.Vector;
import java.util.Date;

import com.nokia.m2m.imp.iocontrol.IOControl;
import com.nokia.m2m.imp.watchdog.WatchdogTimer;

import javax.microedition.io.Connector;
import javax.microedition.io.StreamConnection;
import javax.microedition.midlet.MIDlet;
import javax.microedition.midlet.MIDletStateChangeException;

/**
 * GpsBase
 */
public final class ApplicationGpsBase extends Application
{
    /*--------------------------------------------------------------------*/
    /* MODE. */
    private final static int MODE_RTCM = 0;
    private final static int MODE_COMMAND = 1;
    private final static int MODE_POWEROFF = 2;
    private final static int MODE_RESET_POWEROFF = 3;
    private final static int MODE_RESET_POWERON = 4;
    private int Mode = MODE_RTCM;
    private final static int FLAG_POWER = 0x01;
    private final static int FLAG_RTCM = 0x02;
    private final static int FLAG_RESET = 0x04;
    private final static int[] FlagsByMode = {
        FLAG_POWER | FLAG_RTCM,     // MODE_RTCM
        FLAG_POWER,                 // MODE_COMMAND
        FLAG_RTCM,                  // MODE_POWEROFF
        FLAG_RESET,                 // MODE_RESET_POWEROFF
        FLAG_POWER | FLAG_RESET     // MODE_RESET_POWERON
    };
    private final static String[] NameByMode = {
        "RTCM",
        "COMMAND",
        "POWEROFF",
        "RESET_POWEROFF",
        "RESET_POWERON"
    };

    /*--------------------------------------------------------------------*/
    /* ENVIRONMENT. */
    private final static String NAME_MODE = "mode";
    private final static String NAME_GPSVOLTAGE = "gpspowervoltage";
    private final static String NAME_BATTERYVOLTAGE = "supplyvoltage";
    private final static String NAME_LOGICVOLTAGE = "logicsupplyvoltage";

    /*--------------------------------------------------------------------*/
    private final String GetEnv(String name)
    {
        if (name == NAME_MODE)
        {
            return name + "=" + NameByMode[Mode];
        }
        else if (name == NAME_GPSVOLTAGE)
        {
            return name + "=" + "12.0"; // Double.toString(IOPanel.GpsPowerVoltage());
        }
        else if (name == NAME_BATTERYVOLTAGE)
        {
            return name + "=" + "12.0" ; // Double.toString(IOPanel.SupplyVoltage());
        }
        else if (name == NAME_LOGICVOLTAGE)
        {
            return name + "=" + "5.0"; // Double.toString(IOPanel.LogicSupplyVoltage());
        }
        return null;
    }

    /*--------------------------------------------------------------------*/
    private final void SendEnv(OutputStream ServerOut, String name)  throws IOException
    {
        String s = GetEnv(name);
        SendMessage(ServerOut, s);
    }

    /*--------------------------------------------------------------------*/
    private final void SendEnv(OutputStream ServerOut)  throws IOException
    {
        String message =
            GetEnv(NAME_MODE) + "\n" +
            GetEnv(NAME_GPSVOLTAGE) + "\n" +
            GetEnv(NAME_BATTERYVOLTAGE) + "\n" +
            GetEnv(NAME_LOGICVOLTAGE);
        SendMessage(ServerOut, message);
    }

    /*--------------------------------------------------------------------*/
    /** Set the operating mode of the device. */
    private final void SetMode(
        int NewMode)
    {
        int flags = FlagsByMode[NewMode];
        IOPanel.Power((flags & FLAG_POWER) != 0);
        IOPanel.ModeRtcm((flags & FLAG_RTCM) != 0);
        IOPanel.ModeFactoryReset((flags & FLAG_RESET) != 0);
        Mode = NewMode;
    }

    /*--------------------------------------------------------------------*/
	/**
	 * Start the GpsBase reading thread.
	 */
    public void ProcessStartup() throws MIDletStateChangeException
    {
        SetMode(MODE_RTCM);
        if (Watchdog == null)
        {
            Watchdog = new WatchdogTimer();
            Watchdog.setTimeout(15 * 60);	// 15 minutes.
        }
    }

    /*--------------------------------------------------------------------*/
    /**
     * Write log message to serial port
     * 
     * @param msg
     */
	private final void log(
        OutputStream ServerOut,
        String msg)
    {
        if (BuildInfo.logToServer && ServerOut!=null)
        {
            try
            {
                CMR.encode(ServerOut, CMR.TYPE_DIGNET, "log: " + msg);
            }
            catch (IOException ex)
            {
                // pass.
            }
        }
	}

    /*--------------------------------------------------------------------*/
    private final void log(
        OutputStream ServerOut,
        Exception e)
	{
        if (BuildInfo.logToServer && ServerOut!=null)
        {
            try {
                CMR.encode(ServerOut, CMR.TYPE_DIGNET, "exception: " + e.toString());
            }
            catch (IOException ex)
            {
                // pass.
            }
        }
    }

    /*--------------------------------------------------------------------*/
    public void ProcessConnect(
        OutputStream ServerOut) throws IOException, StopException
    {
        // Environment.
        SendEnv(ServerOut);
    }

    /*--------------------------------------------------------------------*/
    public void ProcessPacket(
            OutputStream ServerOut,
            CMR Packet
        ) throws IOException, StopException
    {
        if (Packet.type == CMR.TYPE_DIGNET)
        {
            // Split it up.
            String[] argv = Utils.split(new String(Packet.data, 0, Packet.data.length), ' ');
            log(ServerOut, "argc: " + argv.length);
            for (int i = 0; i < argv.length; ++i)
            {
                log(ServerOut, "argv[" + i + "]:" + argv[i]);
            }
            // CMR.encode(ServerOut, CMR.TYPE_DIGNET, "Echo: " + cmr.data);
            // SendEnv(ServerOut);
        }
    }

    /*--------------------------------------------------------------------*/
    public void ProcessSerial(
            OutputStream ServerOut,
            byte[] SerialData,
            int SerialLength
        ) throws IOException
    {
        if (Mode == MODE_RTCM)
        {
            CMR.encode(ServerOut, CMR.TYPE_RTCM, SerialData, SerialLength);
        }
    }
}
