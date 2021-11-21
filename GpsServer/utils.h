#ifndef Utils_H_
#define Utils_H_

#include <string>
#include "DigNetMessages.h"


/** Parses weather information page and fills in packet */
void
parse_weather_info(
    const std::string& page, 
    PACKET_WEATHER_INFORMATION& info);


int
getLatestPointsFileNum(
    const std::string& dirName);

/** Generates temporary file name from process ID, user ID and current time in format $TEMP/base_$UID_$PID_$curtimens.tmp
@param base base of filename
@param fileName string where to write the file name 
*/
void 
tmpname(
    const std::string&  base,
    std::string&        fileName);

#endif
