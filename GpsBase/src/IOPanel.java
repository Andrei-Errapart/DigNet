// vim: shiftwidth=4 
// vim: ts=4 

package com.errapartengineering.DigNet;

import com.nokia.m2m.imp.iocontrol.IOControl;
import java.io.IOException;

// IO panel.
public final class IOPanel {
    /// Toggle pin.
    private final static void TogglePin(int pin)
    {
        try
        {
            IOControl ioc = IOControl.getInstance();
            boolean bit = ioc.getDigitalOutputPin(pin);
            ioc.setDigitalOutputPin(pin, !bit);
        }
        catch (IOException ex)
        {
            // pass.
        }
    }

    /// Set pin to given value.
    private final static void SetPin(int pin, boolean state)
    {
        try
        {
            IOControl.getInstance().setDigitalOutputPin(pin, state);
        }
        catch (IOException ex)
        {
            // pass.
        }
    }

    /// Get analog voltage input.
    private final static int GetAnalogVoltage(int pin, int fpMultiplier)
    {
        int adc_reading = 0;
        try
        {
            adc_reading = IOControl.getInstance().getAnalogInputPin(pin);
        }
        catch (IOException ex)
        {
            // pass.
        }
        // Resolution: 10 bits, reading is in range 0..1023.
        // Input voltage is in the range 0..10V.
        // int f_factor = oMathFP.div(oMathFP.mul(fpMultiplier, oMathFP.toFP(10)), oMathFP.toFP(1023));
        // for some reason dividor 244 works fine.
        int r = oMathFP.mul(oMathFP.div(fpMultiplier, oMathFP.toFP(244)), oMathFP.toFP(adc_reading));
        return r;
    }

    public final static void SetConnected(boolean connected)
    {
        SetPin(2, connected);
    }

    public final static void ToggleTcpTraffic()
    {
        TogglePin(3);
    }

    public final static void ToggleSerialTraffic()
    {
        TogglePin(4);
    }

    public final static void Power(boolean state)
    {
        SetPin(5, !state);
    }

    public final static void TogglePacketTraffic()
    {
        TogglePin(6);
    }

    public final static void ModeRtcm(boolean state)
    {
        SetPin(7, state);
    }

    public final static void ModeFactoryReset(boolean state)
    {
        SetPin(8, state);
    }

    public final static void ToggleRtcmTraffic()
    {
        TogglePin(9);
    }

	/// Supply voltage.
	/// Format: Fixpoint.
    public final static int SupplyVoltage()
    {
        return GetAnalogVoltage(1, oMathFP.toFP(2));
    }

	/// Format: Fixpoint.
    public final static int GpsPowerVoltage()
    {
        return GetAnalogVoltage(2, oMathFP.toFP(2));
    }

	/// Format: Fixpoint.
    public final static int LogicSupplyVoltage()
    {
        return GetAnalogVoltage(3, oMathFP.toFP(1));
    }

    /// Modembox supply voltage, in the range 0..40V, format fixed point.
    /// The external division circuit consists of 1500+470 ohm voltage divisor.
    /// Format: Fixpoint.
    public final static int ModemBoxSupplyVoltage()
    {
        return GetAnalogVoltage(1, oMathFP.div(oMathFP.toFP(150+47), oMathFP.toFP(47)));
    }
} // class Build
