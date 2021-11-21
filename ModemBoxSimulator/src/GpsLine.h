#ifndef GpsLine_h_
#define GpsLine_h_

#include <string>
#include <utils/CMR.h>

class GpsLine {
public:
    long        LineTime;
    std::string Line;

    GpsLine();

    void
    Feed(
        const utils::CMR&   packet
    );
private:
    std::string prefix;
    std::string sb;
}; // class GpsLine

#endif /* GpsLine_h_ */