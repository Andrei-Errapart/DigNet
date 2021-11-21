
#include "GpsLine.h"
#include "globals.h"

using namespace utils;

/*--------------------------------------------------------------------*/
GpsLine::GpsLine()
:   prefix("$GPGGA")
    ,LineTime(-1)
{
}

/*--------------------------------------------------------------------*/
void
GpsLine::Feed(  const CMR& Packet)
{
    // skip first byte.
    for (unsigned int i = 1; i < Packet.data.size(); ++i ) {
        // End of line?
        const unsigned char b = Packet.data[i];
        if (b == 0x0D) {
            const std::string&  line = sb;
            if (line.size()>=prefix.size() && line.substr(0, prefix.size())==prefix) {
                Line = line;
                LineTime = System::currentTimeMillis();
            }
            sb.resize(0);
        } else {
            if (b != 0x0A) {
                sb += b;
            }
        }
    }
    if (sb.size() + 1 >= CMR::MAX_DATABLOCK_LENGTH) {
        sb.resize(0);
    }
}
