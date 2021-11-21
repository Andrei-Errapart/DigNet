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
 * Main
 */
public final class Main extends MIDlet implements Runnable {
	private Application application =
                            BuildInfo.application=="GpsBase"
                                ? (Application)(new ApplicationGpsBase())
                                : (Application)(new ApplicationModemBox());

    /*--------------------------------------------------------------------*/
    /** TCP server to connect. */
    private final static String[]       SERVER_ADDRESSES = {
		"194.204.26.104"    // Errapart Engineering OY
	};
    /** TCP: server port. */
    private final static int            SERVER_PORT = 5002;

    /** Server connection ping period, milliseconds. */
    private final static long           SERVER_PING_PERIOD = 10 * 1000;
    /** Server connection timeout, milliseconds. */
    private final static long           SERVER_PING_TIMEOUT = 6 * SERVER_PING_PERIOD;

    /** Serial: Geotracer 3220 default baudrate is 4800. */
    private final static int            SERIAL_PORT_SPEED = 9600;

    /*--------------------------------------------------------------------*/
    /** Worker thread.
     */
    private Thread                      WorkerThread_ = null;
    /** Should worker thread stop?
     */
    private boolean                     WorkerThreadStop_ = false;

    /*--------------------------------------------------------------------*/
	/**
	 * Start the GpsController reading thread.
	 */
	public void startApp() throws MIDletStateChangeException
	{
        application.ProcessStartup();
        // Query IMEI.
        application.Imei = Utils.queryIMEI();

		// Open serial port.
		try {
			application.SerialConnection  = (StreamConnection) Connector.open(
                    "comm:3;baudrate=" + application.SerialPortSpeed, Connector.READ_WRITE);
            application.SerialIn = application.SerialConnection.openInputStream();
            application.SerialOut = application.SerialConnection.openOutputStream();
		} catch (Exception e) {
			// Oops, will not continue.
			throw new MIDletStateChangeException("Error:" + e.toString());
		}
        log("Started.");

        // Start the worker thread.
        WorkerThreadStop_ = false;
        WorkerThread_ = new Thread(this);
        WorkerThread_.start();
    }

	/**********************************************************************/
	/**
	 * Write log message to serial port
	 * 
	 * @param msg
	 */
	private final void log(String msg) {
		// Comment off these rows to remove logging to serial port

		if (BuildInfo.logToSerial) {
			msg = "\r\n[" + msg + "]";
			// can't log to serial port
			if (application.SerialOut != null) {
				try {
                    application.SerialOut.write(msg.getBytes());
				} catch (Exception ignored) {
				}
			}
		}
	}

	/**********************************************************************/
	private final void log(Exception e)
	{
		if (BuildInfo.logToSerial) {
            if (application.SerialOut != null)
            {
				try {
					String msg = "\r\n[Exception: " + e.toString() + "]";
                    application.SerialOut.write(msg.getBytes());
					// e.printStackTrace();
				} catch (Exception ignored) {
					// pass.
				}
			}
		}
	}

	/**********************************************************************/
	private final void stop()
	{
        // Signal stop to this thread.
        WorkerThreadStop_ = true;

        // Wait for this thread to die.
        if (WorkerThread_ != null)
        {
            Utils.join(WorkerThread_);
            WorkerThread_ = null;
        }

        // Close serial port, if left open.
        if (application.SerialConnection != null)
        {
            try
            {
                application.SerialConnection.close();
            }
            catch (Exception e)
            {
            }
        }
        application.SerialConnection = null;
        application.SerialIn = null;
        application.SerialOut = null;

        // Disable watchdog, if any.
        if (application.Watchdog != null)
        {
            application.Watchdog.setTimeout(0);
        }
    }

	/**********************************************************************/
	public void pauseApp()
	{
		stop();
	}

	/**********************************************************************/
	public void destroyApp(boolean arg0) throws MIDletStateChangeException
	{
		stop();
	}

    /**********************************************************************/
    /** Emit message to the server. */
    private final void SendMessage(
        OutputStream server_out,
        String message
        ) throws IOException
    {
        if (server_out != null)
        {
            CMR.encode(server_out, CMR.TYPE_DIGNET, message);
        }
    }

    /**********************************************************************/
    /** Input buffer for ProcessSerialInput and ProcessConnection.
     */
    private final byte[] inbuffer = new byte[512];

    /**********************************************************************/
    /** Process serial input
     */
    private final void ProcessSerialInput(OutputStream ServerOut) throws IOException, StopException
    {
        /** Send RTCM packet, if needed.
         */
        int available = application.SerialIn.available();
        if (available > 0)
        {
            if (available > inbuffer.length)
            {
                available = inbuffer.length;
            }
            int nread = application.SerialIn.read(inbuffer, 0, available);

            if (nread > 0)
            {
                IOPanel.ToggleSerialTraffic();
                application.ProcessSerial(ServerOut, inbuffer, nread);
            }
        }
        application.ProcessIdle(ServerOut);
    }

    /**********************************************************************/
    /** Process the connection to the server. Don't worry about exceptions. */
    private final void ProcessConnection(
        InputStream server_in,
        OutputStream ServerOut
        )
        throws IOException, StopException
    {
        // Greeting packet.
        SendMessage(ServerOut,
            "client name="+BuildInfo.application+
            " Firmware_Version=0.9"+
            " Firmware_Date=\"" + BuildInfo.buildDate + "\""+
            " IMEI=\"" + application.Imei + "\""+
            " BuildInfo=\"" + BuildInfo.compilationArguments + "\"");

        application.ProcessConnect(ServerOut);
        application.ProcessIdle(ServerOut);

        long received_ping_time = System.currentTimeMillis();
        CMRDecoder decoder = new CMRDecoder();
        int available;

        // Loop until timeout
        while (!WorkerThreadStop_ && received_ping_time + SERVER_PING_TIMEOUT > System.currentTimeMillis())
        {
            // Do the TCP stuff.
            available = server_in.available();
            if (available > 0)
            {
                // Read data and feed it.
                if (available > inbuffer.length)
                {
                    available = inbuffer.length;
                }
                int nread = server_in.read(inbuffer, 0, available);
                log("Received: " + nread + " bytes.");
                if (nread > 0)
                {
                    IOPanel.ToggleTcpTraffic();

                    // Timeout records.
                    received_ping_time = System.currentTimeMillis();

                    // Feed the cat.
                    decoder.feed(inbuffer, nread);

                    // Handle packets.
                    for (CMR cmr = decoder.pop(); cmr != null; cmr = decoder.pop())
                    {
                        log(cmr.toString());
                        application.ProcessPacket(ServerOut, cmr);
                        if (cmr.type == CMR.TYPE_PING) {
                            log("PING received, watchdog reset.");
                            CMR.encode(ServerOut, CMR.TYPE_PING, "");
                            if (application.Watchdog != null)
                            {
                                application.Watchdog.resetTimer();
                            }
                        }
                    }
                }
            }

            ProcessSerialInput(ServerOut);
            application.ProcessIdle(ServerOut);
        }

        application.ProcessIdle(ServerOut);
        application.ProcessTimeout(ServerOut, received_ping_time);
    }

    /**********************************************************************/
    /** GpsController main loop.
     */
	public void run()
	{
        // Keep alive connection to the server, watch pings and timeouts.
        int server_index = 0;
        InputStream server_in = null;
        OutputStream server_out = null;
        StreamConnection server_connection = null;
        while (!WorkerThreadStop_)
        {
            try
            {
                // Connect to the server.
                String server_name = SERVER_ADDRESSES[server_index];
                log("Connecting to:" + server_name);
                server_connection = (StreamConnection)Connector.open("socket://" + server_name + ":" + SERVER_PORT);
                server_in = server_connection.openInputStream();
                server_out = server_connection.openOutputStream();

                IOPanel.SetConnected(true);
                ProcessConnection(server_in, server_out);
                application.ProcessDisconnect();
            }
            catch (IOException e)
            {
                log("Connect failed:" + e.toString());
                application.ProcessDisconnect();
                // Uh, connection dropped, oh no.
            }
            catch (Exception e)
            {
                // Exception, bad!
                log(e.toString());
                try
                {
                    SendMessage(server_out, "EXCEPTION " + e.toString());
                }
                catch (IOException e2)
                {
                    // pass.
                }
                application.ProcessDisconnect();
            }
            catch (StopException e)
            {
                try
                {
                    SendMessage(server_out, "STOPPED BY REMOTE CONTROL.");
                }
                catch (IOException e2)
                {
                    // pass.
                }
                application.ProcessDisconnect();
                WorkerThreadStop_ = true;
            }

            // Close all the connections.
            if (server_connection != null)
            {
                try
                {
                    server_connection.close();
                }
                catch (Exception ignored)
                {
                }
            }
            server_connection = null;
            server_in = null;
            server_out = null;

            IOPanel.SetConnected(false);

            // wait 10 secs between attempting to connect to the second server.
            for (int i = 0; i < 10; ++i)
            {
                try
                {
                    application.ProcessIdle(null);
                    for (int j = 0; j < 10; ++j)
                    {
                        ProcessSerialInput(null);
                    }
                }
                catch (IOException ex)
                {
                    // pass.
                }
                catch (StopException e)
                {
                    WorkerThreadStop_ = true;
                }

                Utils.sleep(1000);
                if (WorkerThreadStop_)
                {
                    break;
                }
            }

            server_index = (server_index + 1) % SERVER_ADDRESSES.length;
        } // main loop.
    }
}

