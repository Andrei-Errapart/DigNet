/* GPS Viewer main file. */


// #include <winsock2.h>	// this has to be first, otherwise windows.h will complain :(

#include <utils/util.h>
#include <utils/mystring.h>
#include "MainWindow.h"
#include "src/config.h"

#include "src/version.h"

#if (0)
#if !defined(GPSVIEWER_X1) || !defined(GPSVIEWER_Y1) || !defined(GPSVIEWER_X2) || !defined(GPSVIEWER_Y2)
#error Missing limits in config.h.
#endif
#endif

//=====================================================================================//
//=====================================================================================//
// Here we begin
int
main(int argc,char *argv[])
{
    bool log=false;
    for (int i=1;i<argc;i++)
    {
        if (strcmp(argv[i], "--log")==0){
            log = true;
        }
    }
    if (log)
    {
        char xbuf[100];
        utils::xsprintf(xbuf, sizeof(xbuf), "%s-log.txt", BUILD_NAME);
        TRACE_SETFILE(xbuf);
    }
    else
    {
	    TRACE_SETFILE("disabled");
    }

	TRACE_ENTER("main");

	FXApp	application("Scribble","FoxTest");
	application.init(argc,argv);
	new MainWindow(&application, BUILD_NAME);
	application.create();
	return application.run();
}
