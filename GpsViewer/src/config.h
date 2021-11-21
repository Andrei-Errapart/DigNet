#ifndef GpsViewer_config_h_
#define GpsViewer_config_h_

// Tööala piiramise (koordinaatsüsteem: Lambert-Est).
// Kui neid pole defineeritud, siis programm ei kompileeru.
#if (0)
// omedu special
#define    GPSVIEWER_X1 6467000
#define    GPSVIEWER_Y1  655000
#define    GPSVIEWER_X2 6485000
#define    GPSVIEWER_Y2  712000
#else
#define JARVE
#ifdef JARVE
#define    GPSVIEWER_X1 (6468000)
#define    GPSVIEWER_Y1 (696200 )
#define    GPSVIEWER_X2 (6478200)
#define    GPSVIEWER_Y2 (706900 )
#else
#define    GPSVIEWER_X1 (6470000)
#define    GPSVIEWER_Y1 (650000 )
#define    GPSVIEWER_X2 (6480000)
#define    GPSVIEWER_Y2 (710000 )
#endif
#endif

#define    DISABLE_SHUTDOWN

#define    STARTUP_NUMBER_ZOOMOUTS    0

#endif // GpsViewer_config_h_ 
