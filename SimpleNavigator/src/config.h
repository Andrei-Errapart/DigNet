#ifndef GpsViewer_config_h_
#define GpsViewer_config_h_

// Tööala piiramise (koordinaatsüsteem: Lambert-Est).
// Kui neid pole defineeritud, siis programm ei kompileeru.
#if (1)
/*
ülemine parempoolne nurk
y:  481415
x: 6487300

Alumine vasakpoolne nurk:
y:  480760
x: 6485200
*/
// Paatsalu
// omedu special
#define	GPSVIEWER_X1 (6487300 - 500)
#define	GPSVIEWER_Y1  (480760 - 500)
#define	GPSVIEWER_X2 (6485200 + 500)
#define	GPSVIEWER_Y2  (481415 + 500)
#endif

#define	DISABLE_SHUTDOWN

#define	STARTUP_NUMBER_ZOOMOUTS	0

#endif // GpsViewer_config_h_ 
