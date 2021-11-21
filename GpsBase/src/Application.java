// vim: shiftwidth=4
// vim: ts=4
package com.errapartengineering.DigNet;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import javax.microedition.io.Connector;
import javax.microedition.io.StreamConnection;

import com.nokia.m2m.imp.watchdog.WatchdogTimer;
import javax.microedition.midlet.MIDletStateChangeException;

/**
 * Application  - base class.
 */
public class Application {
    /*--------------------------------------------------------------------*/
    /** Device IMEI, initialized with default value. */
	public String				Imei = "123456789";

    /*--------------------------------------------------------------------*/
    /** Default baudrate. This can be changed in constructor. */
    public int            		SerialPortSpeed = 9600;
	/** Input stream. */
	public InputStream			SerialIn = null;
	/** Output stream. */
	public OutputStream			SerialOut = null;
	/** connection handle. */
	public StreamConnection		SerialConnection = null;

    /*--------------------------------------------------------------------*/
	/** Watchdog. */
	public WatchdogTimer		Watchdog;

    /*--------------------------------------------------------------------*/
    /** Application Startup.
     */
    public void ProcessStartup() throws MIDletStateChangeException
    {
    }

    /*--------------------------------------------------------------------*/
    /** Idle processing.
     */
    public void ProcessIdle(OutputStream ServerOut) throws IOException, StopException
    {
    }

    /** Fresh connection to the server. */
	public void ProcessConnect(
            OutputStream ServerOut) throws IOException, StopException
	{
	}

    /** Process data exchange. ServerOut and both data lengths might be zero. */
	public void ProcessPacket(
			OutputStream    ServerOut,
            CMR             Packet
        ) throws IOException, StopException
	{
	}

    /** Process serial data read from the port. The ServerOut might be zero.
     */
    public void ProcessSerial(
            OutputStream    ServerOut,
            byte[]          SerialData,
            int             SerialLength
        ) throws IOException
    {
    }

    /** Ping timeout. Disconnect follows.
     */
    public void ProcessTimeout(
            OutputStream    ServerOut,
            long            LastTimeReceived
        ) throws IOException
    {
        long current_time = System.currentTimeMillis();
        long dt = current_time - LastTimeReceived;
        SendMessage(ServerOut, "TIMEOUT dt=" + dt +
                " last time:" + Utils.formatDate(LastTimeReceived) +
                " current time:" + Utils.formatDate(current_time));
    }

	/** Disconnect from the server. */
	public void ProcessDisconnect()
	{
	}

    /*--------------------------------------------------------------------*/
    /** Helper function: Send message to the server. */
    public void SendMessage(
        OutputStream ServerOut,
        String message
        ) throws IOException
    {
        if (ServerOut != null)
        {
            CMR.encode(ServerOut, CMR.TYPE_DIGNET, message);
        }
    }

}

