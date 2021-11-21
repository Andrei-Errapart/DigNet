/* GPS Viewer main file. */

#include <utils/util.h>
#include "MainWindow.h"
#include "src/config.h"
#include "src/version.h"

#if !defined(GPSVIEWER_X1) || !defined(GPSVIEWER_Y1) || !defined(GPSVIEWER_X2) || !defined(GPSVIEWER_Y2)
#error Missing limits in config.h.
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
        TRACE_SETFILE("GpsViewer-log.txt");
    }
    else
    {
        TRACE_SETFILE("disabled");
    }

    TRACE_ENTER("main");

    FXApp    application("Scribble","FoxTest");
    application.init(argc,argv);
    new MainWindow(&application, BUILD_NAME);
    application.create();
    return application.run();
}
