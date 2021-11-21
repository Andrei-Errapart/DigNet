#include "ModemBoxSimulator.h"

#include <stdlib.h>

#include <utils/util.h>

/* ------------------------------------------------------------- */
int
main(   int     argc,
        char**  argv)
{
    TRACE_SETFILE("ModemBoxSimulator-log.txt");
	TRACE_ENTER("main");

    srand(0);

	FXApp	application("ModemBoxSimulator","EE");
	application.init(argc,argv);
	new ModemBoxSimulator(&application, argc, argv);
	application.create();
	return application.run();

    return 0;
}
