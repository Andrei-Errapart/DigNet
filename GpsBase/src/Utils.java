// vim: shiftwidth=4
// vim: ts=4
package com.errapartengineering.DigNet;

import java.lang.Integer;

import java.util.Vector;
import java.util.Calendar;
import java.util.Date;

import java.io.IOException;
import java.io.DataOutputStream;
import java.io.DataInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ByteArrayInputStream;

import com.nokia.m2m.orb.idl.terminal.ET;
import com.nokia.m2m.orb.idl.terminal.ETHelper;

import javax.microedition.rms.RecordStore;
import javax.microedition.rms.RecordStoreException;
import javax.microedition.rms.RecordStoreNotOpenException;

import com.nokia.m2m.imp.iocontrol.IOControl;

/**
Some utility functions.
 */
public abstract class Utils {
	/** Sleep for a given amount of milliseconds. */
	public final static void sleep(	int	milliseconds)
	{
		try {
			Thread.sleep(milliseconds);
		} catch (InterruptedException ignore) {
		}
	}

	/** Join the thread until it dies. */
	public final static void join(Thread thread)
	{
		while (thread.isAlive()) {
			try {
				thread.join();
			} catch (InterruptedException e) {
				// pass.
			}
		}
	}

    /** Convert (signed) byte to (unsigned) integer. */
    public final static int uint_of_byte(byte b)
    {
        return b >= 0
            ? b
            : 256 + b;
    }

	/** Split the string with given separator.
	
	There will always be at least one member in the returned array.
	*/
	public final static String[] split(String s, char sep)
	{
		// Temporary.
		Vector r = new Vector();

		// Do the splitting.
		int so_far = 0;
		int spos = -1;
		do {
			spos = s.indexOf(sep, so_far);
			if (spos>=0) {
				r.addElement(s.substring(so_far, spos));
				so_far = spos + 1;
			}
		} while (spos>=0);
		r.addElement(s.substring(so_far, s.length()));

		// Convert
		String[] sr = new String[r.size()];
		for (int i=0; i<sr.length; ++i) {
			sr[i] = (String)r.elementAt(i);
		}
		return sr;
	}

	/** Join the string array with given separator.
	 */
	public final static String join(String[] array, String sep)
	{
		StringBuffer	sb = new StringBuffer();
		if (array.length>0) {
			sb.append(array[0]);
		}
		for (int i=1; i<array.length; ++i) {
			sb.append(sep);
			sb.append(array[i]);
		}
		return sb.toString();
	}

	/** Convert signed byte to unsigned 16-bit char. */
	public final static char char_of_byte(byte b)
	{
		return (char)(b>=0
			? (int)b
			: (256 + b));
	}

	private static char[] hexchar = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

	/** Convert integer in range 0..255 into hexadecimal representation with possible leading zero. */
	public final static String hex_of_uint8(	int	i)
	{
		char[] chars = { hexchar[(i >>> 4) & 0x0F], hexchar[i & 0x0F] };
		return new String(chars);
	}

	/** Convert byte (changed to be unsigned integer) in range 0..255 into hexadecimal representation with possible leading zero. */
	public final static String hex_of_byte(	byte	b)
	{
		int i = uint_of_byte(b);
		char[] chars = { hexchar[(i >>> 4) & 0x0F], hexchar[i & 0x0F] };
		return new String(chars);
	}

	/** Convert integer in range 0..65535 into hexadecimal representation with possible leading zeroes. */
	public final static String hex_of_uint16(	int	i)
	{
		char[] chars = {
			hexchar[(i >>> 12) & 0x0F],
			hexchar[(i >>> 8) & 0x0F],
			hexchar[(i >>> 4) & 0x0F],
			hexchar[i & 0x0F] };
		return new String(chars);
	}

    /** Set IO output state.
     * 
     * N12i specific.
     */
    public final static void setIO(	IOControl	ioc, int port, boolean state)
	{
		try {
			ioc.setDigitalOutputPin(port, state);
		} catch (IOException ex) {
			// pass.
		}
	}

    /** Convert Calendar representation of time to month in range 1..12. */
    private final static int monthTo1to12(int calendar_month)
    {
        switch (calendar_month)
        {
            case Calendar.JANUARY: return 1;
            case Calendar.FEBRUARY: return 2;
            case Calendar.MARCH: return 3;
            case Calendar.APRIL: return 4;
            case Calendar.MAY: return 5;
            case Calendar.JUNE: return 6;
            case Calendar.JULY: return 7;
            case Calendar.AUGUST: return 8;
            case Calendar.SEPTEMBER: return 9;
            case Calendar.OCTOBER: return 10;
            case Calendar.NOVEMBER: return 11;
            case Calendar.DECEMBER: return 12;
        }
        return 1;
    }

    /** String representation of number with given number of places. Zeros are stuffed as required. */
    public final static String stringOfInt(int i, int nr_of_places, char prefix)
    {
        String sign = i >= 0 ? "" : "-";
        String si = Integer.toString(i >= 0 ? i : -i);
        int nmissing = nr_of_places - sign.length() - si.length();
        if (nmissing <= 0)
        {
            return sign + si;
        }
        StringBuffer sb = new StringBuffer();
        sb.append(sign);
        for (int j = 0; j < nmissing; ++j)
        {
            sb.append(prefix);
        }
        sb.append(si);
        return sb.toString();
    }

    /** Date in a form: "yyyy-MM-dd HH:mm:ss". */
	public final static String formatDate(Date dt)
	{
		Calendar cal = Calendar.getInstance();
		cal.setTime(dt);
		int year	= cal.get(Calendar.YEAR);
		int month	= monthTo1to12(cal.get(Calendar.MONTH));
		int	day		= cal.get(Calendar.DAY_OF_MONTH);
		int hour	= cal.get(Calendar.HOUR_OF_DAY);
		int minute	= cal.get(Calendar.MINUTE);
		int	second	= cal.get(Calendar.SECOND);
		return
			stringOfInt(year, 4, '0') + "-" +
			stringOfInt(month, 2, '0') + "-" +
			stringOfInt(day, 2, '0') + " " +
			stringOfInt(hour, 2, '0') + ":" +
			stringOfInt(minute, 2, '0') + ":" +
			stringOfInt(second, 2, '0') +
			" (" + dt.getTime()+")";

	}

	/** Date in a form: "yyyy-MM-dd HH:mm:ss". */
	public final static String formatDate(long ms)
	{
		return formatDate(new Date(ms));
	}

    /**
     * Query GSM/GPRS phone IMEI.
     * 
     * N12i specific.
     */
    public final static String queryIMEI()
    {
        // Query IMEI.
        org.omg.CORBA.ORB orb = org.omg.CORBA.ORB.init(null, null);
        // Resolve the Device object.
        org.omg.CORBA.Object ref = orb.string_to_object("corbaloc::127.0.0.1:19740/ORB/OA/IDL:ET:1.0");
        ET et = ETHelper.narrow(ref);
        String imei = et.IMEI();

        // Close the ORB.
        orb.destroy();

        return imei;
    }
} // class Utils

