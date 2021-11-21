#include "GpsServer.h"

class GpsServer;

/*****************************************************************************/
/// Set up listening port and start running...
int
main(
        int    argc,
        char** argv)
{
    TRACE_SETFILE("GpsServer-log.txt");

    try {
        GpsServer server;
        server.run();
    } catch (const std::exception& e) {
        GpsServer::log(0, "Exception: %s", e.what());
    }
    return 0;
}

