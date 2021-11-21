#include <string>
#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <dirent.h>

#include <utils/utils.h>



#include "utils.h"
#include "DigNetMessages.h"

using namespace std;
using namespace utils;

static std::string 
substr(
        const string& str,
        const string& before, 
        const string& after)
{
    const size_t begin = str.find(before.c_str());
    const size_t end = str.find(after.c_str(), begin);
    if (begin != string::npos && end != string::npos){
        return str.substr(begin + before.size(), end - begin - before.size());
    }
    return "";
}


/*****************************************************************************/
void
parse_weather_info(
    const string& page, 
    PACKET_WEATHER_INFORMATION& info)
{
    istringstream ss(page);
    string buf;
    buf.resize(4096);
        // Time data is on line 14
    for(int i=0;i<12;i++){
        ss.getline(&buf[0], 4096);
    }
        // Get time. Time string contains lots of changing information (date, year) but we only care about hh:mm
    string time_str(substr(buf, "Gates<br/>", " <font"));
    // Parse date string into date (struct tm)
    tm time_tmp;
    memset(&time_tmp, 0, sizeof(time_tmp));
    strptime(time_str.c_str(), "%A, %B %dth %Y %H:%M", &time_tmp);
    const time_t time = mktime(&time_tmp);
    info.time=time;
        // Measurements on line 15
    for(int i=0;i<3;i++){
        ss.getline(&buf[0], 4096);
    }
    string wind_speed(     substr(buf, "Wind speed:</td><td nowrap>",      "m/s"));
    string wind_gust(      substr(buf, "Wind gust:</td><td nowrap>",       "m/s"));
    string wind_direction( substr(buf, "Wind direction:</td><td nowrap>",  "deg"));
    string water_level(    substr(buf, "Water level <font size=-2>(“0”=29.50m BK77)</font>:&nbsp;</td><td nowrap>",     "cm"));

    // Use doubles for computations to preserve accuaricy, cast to int when writing to packet
    const uint16_t not_set = 0xffff;
    double speed, gust, direction, level;
        // parse and convert to cm
    if (double_of(wind_speed, speed)){
        speed*=100;
        info.wind_speed = (uint16_t)speed;
    } else {
        info.wind_speed = not_set;
    }
    if (double_of(wind_gust, gust)){
        gust *= 100;
        info.gust_speed = (uint16_t)gust;
    } else {
        info.gust_speed = not_set;
    }
    if (double_of(wind_direction, direction)){
        info.wind_direction = (uint16_t)direction;
    } else {
        info.wind_direction = not_set;
    }
    if (double_of(water_level, level)){
        info.water_level = (uint16_t)level;
    } else {
        info.water_level = not_set;
    }
}

/*****************************************************************************/
int
getLatestPointsFileNum(
    const string& dirName)
{
    int maxNum = -1;
    if (dirName != "") {
        DIR* dir = opendir(dirName.c_str());
        while (dir != 0){
            dirent* file = readdir(dir);
            if (file == 0){
                break;
            }
            char* begin = strstr(file->d_name, "points_");
            if (begin == 0) {
                continue;
            }
            char* end = strstr(file->d_name, ".ipt");
            if (end == 0){
                continue;
            }
            // File name format is always points_XXXXX.ipt so no string manipulation is needed to extract the sequence number
            const string numpart(&file->d_name[7],&file->d_name[12]);
            int num;
            const bool int_ok= int_of(numpart, num);
            if (int_ok){
                maxNum = mymax(maxNum, num);
            }
        }
        closedir(dir);
    }
    return maxNum;
}

void 
tmpname(
    const string&   base,
    string&         fileName)
{
    char* tmpdir = getenv("TEMP");
    string dir;
    if (tmpdir == 0){
        cout<<"Warning: TMP not set, using /tmp"<<endl;
        dir = "/tmp";
    } else {
        dir = tmpdir;
    }
    const int pid = getpid();
    const int uid = getuid();
    timespec curtime;
    clock_gettime(CLOCK_REALTIME, &curtime);
    fileName = ssprintf("%s/%s_%d_%d_%d.tmp", dir.c_str(), base.c_str(), uid, pid, curtime.tv_nsec);
}
