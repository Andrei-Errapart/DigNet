/*
vim: shiftwidth=4
vim: ts=4
*/

#ifdef WIN32
#pragma warning(disable:4786)
#include <winsock2.h>   // struct timeval.
#endif

#include <vector>   // std::vector
#include <string>   // std::string
#include <list>     // std::list
#include <map>      // std::map
#include <numeric>  // std::numeric_limits
#include <fstream>

#include <stdio.h>  // printf, sprintf
#include <stdarg.h> // varargs.

#include <event.h>  // libevent.

#include <utils/Config.h>       // FileConfig
#include <utils/mysockets.h>        // open_server_socket
#include <utils/util.h>         // SimpleConfig, Error.
#include <utils/math.h>

#include "GpsServer.h"
#include "DigNetMessages.h"
#include "DigNetDB.h"
#include "utils.h"
#include "SendEmail.h"

#include <utils/CMR.h>
#include <utils/Transformations.h>


using namespace utils;
using namespace std;

// If defined log all incoming packages
#define LOG_PACKETS

static const char*  FILENAME_CONFIGURATION  = "GpsServer.ini";


static const char*  AUTHENTICATION_GPSBASE      = "GpsBase";
//static const char*    AUTHENTICATION_GPSVIEWER    = "GpsViewer";
static const char*  AUTHENTICATION_MODEMBOX     = "ModemBox";
static const char*  AUTHENTICATION_GPSADMIN     = "GpsAdministrator";

std::string GpsServer::rtcm_source_;

//const LOG_LEVEL MAX_LOGGED_LEVEL=LOG_LEVEL_3;
const LOG_LEVEL MAX_LOGGED_LEVEL = LOG_LEVEL_DEBUG;
//const LOG_LEVEL MAX_LOGGED_LEVEL=LOG_LEVEL_ESSENTIAL;



string 
clientmodeName(
        CLIENTMODE mode)
{
    switch (mode){
    case CLIENTMODE_UNIDENTIFIED: 
        return "unidentified"; 
    case CLIENTMODE_GPSBASE:
        return "GPS base";
    case CLIENTMODE_GPSADMIN:
        return "GPS admin";
    case CLIENTMODE_OSKANDO:
        return "Oskando";
    case CLIENTMODE_MODEMBOX:
        return "Modembox";
    case CLIENTMODE_GPSVIEWER:
        return "GPS viewer";
    case CLIENTMODE_RTCMLISTENER:
        return "RTCM listener";
    default:
        return ssprintf("Unknown client type: %d", mode);
    }
}


TCP_CLIENT::TCP_CLIENT(
    int socket,
    GpsServer* server,
    CLIENTMODE mode,
    const string& ip) :
        fd(socket),
        server(server),
        mode(mode),
        ip_address(ip),
        idle_ticks(0),
        voltage_changed(false),
        group_id(0),
        gpsviewer_build_number(BUILD_NO_NUMBER),
        // FIXME: until no sure way is found to check if GPSViewer is attached assume it is
        has_gpsviewer (true)
{
}


/*****************************************************************************/
std::string
TCP_CLIENT::reportMinuteStatistics(){
    stringstream ss;
    ss.precision(1);    // floating point shows 1 place after .
    ss<<fixed;          // don't use scientific format for FP
    ss<<"\t(ID: 0x"<<hex<<fd<<dec<<") Idle "<<idle_ticks;
    ss<<" Last minute packets received/sent: "<<packets_received_.countAtInterval(60)<<"/"<<packets_sent_.countAtInterval(60);
    uint64_t bRec = bytes_received_.countAtInterval(60);
    uint64_t bSen = bytes_sent_.countAtInterval(60);
    ss<<" Bytes received/sent: "<<bRec<<"/"<<bSen<<" "<<bRec/60.0<<"/"<<bSen/60.0<<" per second";
    return ss.str();
}

/*****************************************************************************/
void 
TCP_CLIENT::setVoltage(
    const double voltage)
{
    const bool was_below_limit = modembox_voltage_limit > last_modembox_voltage;
    const bool is_below_limit  = modembox_voltage_limit > voltage;
    // voltage changed from below-limit to over-limit or vice versa
    voltage_changed = was_below_limit != is_below_limit;
    voltage_reading_time = my_time_of_now();
    last_modembox_voltage = voltage;
}

/*****************************************************************************/
void
getVoltageString(
    const TCP_CLIENT& client,
    std::string& subject,
    std::string& message)
{
    const char* stateStr[] = {"went over limit", "went under limit", "is steady"};
    const char* state;
    if (client.voltage_changed){
        if (client.last_modembox_voltage > client.modembox_voltage_limit){
            state = stateStr[0];
        } else {
            state = stateStr[1];
        }
    } else {
        state = stateStr[2];
    }

    subject = ssprintf("[DigNet] %s: voltage %s",
                client.ship_id->ship_name, 
                state);
    my_time now;
    my_time_of_now(now);
    const double time = float_of_time(now) - float_of_time(client.voltage_reading_time());
    const int seconds = (int)time%60;
    const int minutes = (int)time/60.0;
    message = ssprintf("%s: voltage %s. Limit %3.1f, last reading %3.1f. Last reading done %d:%d minutes ago.", 
                client.ship_id->ship_name,
                state,
                client.modembox_voltage_limit,
                client.last_modembox_voltage,
                minutes, seconds);
}

/*****************************************************************************/
bool
get_ship_info(
    const TCP_CLIENT*       client,
    DigNetDB&               database,
    vector<unsigned char>&  buffer)   // PACKET_SHIP_INFO packet gets written here, buffer is resized to be as big as needed
{
    // Check if ID exists and client is with modembox
    if (client->mode == CLIENTMODE_MODEMBOX) {
        const PACKET_SHIP_ID& id = *client->ship_id;
        double x1, y1, x2, y2;
        if (database.getShipGpsOffsets(id.ship_number, x1, y1, x2, y2)) {
            PACKET_SHIP_INFO info;
            info.ship_number = id.ship_number;
            // From meters to cm
            info.gps1_dx = (int16_t)(x1 * 100.0);
            info.gps1_dy = (int16_t)(y1 * 100.0);
            info.gps2_dx = (int16_t)(x2 * 100.0);
            info.gps2_dy = (int16_t)(y2 * 100.0);

            info.ship_name_length = strlen(id.ship_name);
            info.imei_length = client->imei.size();

            // Get the total needed space for the packet
            const size_t size = sizeof(PACKET_SHIP_INFO) - 1 + strlen(id.ship_name) + client->imei.size();
            // Get the byte where to start copying ship name
            const size_t name_start = &info.ship_name_imei[0] - (uint8_t*) & info;
            const size_t imei_start = name_start + strlen(id.ship_name);

            buffer.resize(size);
            // Copy the information to buffer
            memcpy(&buffer[0], &info, sizeof(PACKET_SHIP_INFO));
            memcpy(&buffer[name_start], &id.ship_name[0], strlen(id.ship_name));
            memcpy(&buffer[imei_start], &client->imei[0], client->imei.size());
            return true;
        }
    }
    return false;
}

/*****************************************************************************/
void
GpsServer::log(
    LOG_LEVEL           level,
    const TCP_CLIENT*   client,
    const char*         fmt,
    va_list             arguments)
{
    if (level >= MAX_LOGGED_LEVEL) {
        my_time rtime;
        my_time_of_now(rtime);
        printf("%02d%02d%02d: ", rtime.hour, rtime.minute, rtime.second);

        if (client != 0 && client->ship_id.get() != 0) {
            printf("'#%d:%s': ", client->ship_id->ship_number, client->ship_id->ship_name);
        }

        vfprintf(stdout, fmt, arguments);
        printf("\n");
        fflush(stdout);
    }
}

/*****************************************************************************/
void
GpsServer::log(
    LOG_LEVEL           level,
    const TCP_CLIENT*   client,
    const char*         fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log(level, client, fmt, ap);
    va_end(ap);
}

/*****************************************************************************/
void
GpsServer::log(
    const TCP_CLIENT*   client,
    const char*         fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log(LOG_LEVEL_DEBUG, client, fmt, ap);
    va_end(ap);
}

/*****************************************************************************/
void
GpsServer::log(
    const char*        fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log(LOG_LEVEL_DEBUG, 0, fmt, ap);
    va_end(ap);
}

/*****************************************************************************/
void
GpsServer::send_raw_data(
    TCP_CLIENT*                         tcp_client,
    std::vector<unsigned char>&         data)
{
    bufferevent_write(tcp_client->event, &data[0], data.size());
    ++tcp_client->server->packets_sent_;
    tcp_client->server->bytes_sent_+=data.size();
    ++tcp_client->packets_sent_;
    tcp_client->bytes_sent_+=data.size();
}

/*****************************************************************************/
void
GpsServer::send_event(
    TCP_CLIENT*             tcp_client,
    const std::string&      event,
    int                     type)
{
    send_packet(tcp_client, &event[0], event.length(), type);
}

/*****************************************************************************/
void
GpsServer::send_event(
    TCP_CLIENT*                         tcp_client,
    const std::vector<unsigned char>&   event,
    int                                 type)
{
    send_packet(tcp_client, &event[0], event.size(), type);
}

/*****************************************************************************/
void
GpsServer::send_packet(
    TCP_CLIENT*         tcp_client,
    const void*         event,
    int                 size,
    int                 type)
{
    // log(LOG_LEVEL_3, tcp_client, "Sending CMR type 0x%0x, size: %d", type, size);
    std::vector<unsigned char> buf;
    CMR::encode(buf, type, event, size);
    send_raw_data(tcp_client, buf);
}

/*****************************************************************************/
void
GpsServer::send_buffer_to_all(
    std::list<TCP_CLIENT*>&         tcp_clients,
    void*                           buffer,
    int                             size,
    unsigned int                    mask,
    int                             type
)
{
    std::vector<unsigned char> buf;
	// ModemBox is broken: it can't accept packets larger than 126 bytes.
	const char*	buffer_ptr = reinterpret_cast<const char*>(buffer);
	const int	max_size = 120;
	int			todo = size;
	while (todo > 0) {
		int this_round = todo>max_size ? max_size : todo;
		CMR::encode(buf, type, buffer_ptr, this_round);

		buffer_ptr += this_round;
		todo -= this_round;
	}
    
    for (std::list<TCP_CLIENT*>::const_iterator it = tcp_clients.begin(); it != tcp_clients.end(); ++it) {
        TCP_CLIENT* tcp_client = *it;
        if (tcp_client->mode & mask) {
            tcp_client->server->send_raw_data(tcp_client, buf);
        }
    }
}

/*****************************************************************************/
void
GpsServer::send_buffer_to_all(
    std::list<TCP_CLIENT*>&         tcp_clients,
    std::vector<unsigned char>&     buffer,
    unsigned int                    mask,
    int                             type
)
{
    send_buffer_to_all(tcp_clients, &buffer[0], buffer.size(), mask, type);
    return;
}
/*****************************************************************************/
void
GpsServer::forward_packet_to_all(
    std::list<TCP_CLIENT*>&         clients,
    const TCP_CLIENT*               sender,
    const utils::CMR&               packet
)
{

    std::vector<unsigned char>  msgbuf(sizeof(PACKET_FORWARD) - 1 + packet.data.size());
    PACKET_FORWARD*             msg = (PACKET_FORWARD*)(&msgbuf[0]);
    msg->ship_id = (sender->ship_id.get() == 0) ? 0 : sender->ship_id->ship_number;
    msg->original_type = packet.type;
    memcpy(&msg->original_data_block[0], &packet.data[0], packet.data.size());
    std::vector<unsigned char> buf;
    CMR::encode(buf, PACKET_FORWARD::CMRTYPE, msg, msgbuf.size());

    for (std::list<TCP_CLIENT*>::const_iterator it = clients.begin(); it != clients.end(); ++it) {
        TCP_CLIENT*   client = *it;
        if (client->group_id == sender->group_id && client != sender) {
            client->server->send_raw_data(client, buf);
        }
    }
}

/*****************************************************************************/
void
GpsServer::send_startup_info(
    TCP_CLIENT*               client)
{
    // send pointfile information ASAP
    check_pointfile();
    for (std::list<TCP_CLIENT*>::const_iterator it = tcp_clients_.begin(); it != tcp_clients_.end(); ++it) {
        TCP_CLIENT* other = *it;
        if (other->group_id == client->group_id && other->mode != CLIENTMODE_GPSVIEWER && other->ship_id.get() != 0) {
            const int id = other->ship_id->ship_number;
            log(client, "Sending information about %s", other->ship_id->ship_name);
            // Ship info
            std::vector<unsigned char> buf;
            if (get_ship_info(other, *database_, buf)) {
                send_event(client, buf, PACKET_SHIP_INFO::CMRTYPE);
            } else {
                log("Unable to get ship #%d information from DB", id);
            }

            // Ship position
            PACKET_SHIP_POSITION pos;
            pos.ship_number = id;
            pos.has_barge = false;
            pos.flags = 0;
            if (database_->getShipPosition(pos.ship_number, pos.gps1_x, pos.gps1_y, pos.heading)) {
                send_packet(client, &pos, sizeof(PACKET_SHIP_POSITION), CMRTYPE_SHIP_POSITION);
            } else {
                log("Unable to get ship #%d position from DB", id);
            }

            // Modembox voltage, minimum safe value and time
            double last, minimum;
            int secs;
            my_time timeLastRead;
            if (database_->getShipVoltageInfo(id, last, minimum, secs, timeLastRead)) {
                std::string voltage = ssprintf("%s ship=%d voltage=%3.1f limit=%3.1f lastRead=%d h=%d m=%d", DIGNETMESSAGE_VOLTAGE, id, last, minimum, secs, timeLastRead.hour, timeLastRead.minute);
                send_event(client, voltage, CMR::DIGNET);
            } else {
                log("Unable to get ship #%d voltages from DB", id);
            }
            // Send weather information
            send_packet(client, &weather_, sizeof(weather_), PACKET_WEATHER_INFORMATION::CMRTYPE);
            // Send shadow to viewers
            if (client->mode == CLIENTMODE_GPSVIEWER){
                PACKET_SHIP_POSITION pos;
                memset(&pos, 0, sizeof(pos));
                if (database_->getShadow(id, pos.gps1_x, pos.gps1_y, pos.heading)){
                    pos.ship_number = id;
                    pos.has_barge = 1;
                    pos.flags = SHIP_POSITION_SHADOW;
                    send_packet(client, &pos, sizeof(pos), CMRTYPE_SHIP_POSITION);
                }
            }
        }
    }
}

/*****************************************************************************/
void
GpsServer::check_pointfile()
{
    PACKET_POINTSFILE_INFO p;
    const int num = getLatestPointsFileNum(points_file_dir_);
    if (num != -1){
        p.number = num;
        std::string filename(ssprintf("%s/points_%05d.ipt", points_file_dir_.c_str(), p.number));
        log("File name: %s", filename.c_str());
        std::ifstream pointfile;
        pointfile.open(filename.c_str(), std::ios::binary);
        pointfile.seekg(0, ios::end);
        p.size = pointfile.tellg();
        vector<unsigned char> buf;
        CMR::encode(buf, PACKET_POINTSFILE_INFO::CMRTYPE, &p, sizeof(p));
        for (std::list<TCP_CLIENT*>::const_iterator it = tcp_clients_.begin(); it != tcp_clients_.end(); ++it) {
            TCP_CLIENT* tcp_client = *it;
            send_raw_data(tcp_client, buf);
        }
    } else {
        log("Couldn't find pointfile");
    }
}

/*****************************************************************************/
bool
GpsServer::get_chunck(
    uint16_t                    file_no,
    uint16_t                    chunk_no,
    PACKET_POINTSFILE_CHUNK&    packet)
{
    const std::string filename(ssprintf("%s/points_%05d.ipt", points_file_dir_.c_str(), file_no));
    // log("File path: %s path length: %d", filename.c_str(), filename.length());
    std::ifstream in(filename.c_str(), ios::binary);
    if (!in.is_open()){
        log("Unable to open pointfile '%s'", filename.c_str());
        return false;
    }
    in.seekg(chunk_no*CHUNK_SIZE, ios::beg);
    in.read(reinterpret_cast<char*>(&packet.data[0]), CHUNK_SIZE);
    packet.file_number = file_no;
    packet.chunk_number = chunk_no;
    return true;
}

/*****************************************************************************/
TCP_CLIENT*
GpsServer::find_client(
    int ship_no)
{
    for (std::list<TCP_CLIENT*>::const_iterator it = tcp_clients_.begin(); it != tcp_clients_.end(); ++it) {
        TCP_CLIENT* tcp_client = *it;
        if (tcp_client->ship_id.get() != 0 && tcp_client->ship_id->ship_number == ship_no) {
            return tcp_client;
        }
    }
    return 0;
}


/*****************************************************************************/
void
GpsServer::kill_tcp_client(
    TCP_CLIENT*&        tcp_client,
    const char*        reason)
{
    log(tcp_client, "Shutting down client 0x%0x, name %s. Reason: %s", tcp_client->fd, tcp_client->name.c_str(), reason);
    // Shutdown events.
    bufferevent_disable(tcp_client->event, EV_READ | EV_WRITE);
    bufferevent_free(tcp_client->event);
    tcp_client->event = 0;

    // Close socket.
    close_socket(tcp_client->fd);
    tcp_client->fd = 0;

    // Remove client from the chain...
    tcp_clients_.remove(tcp_client);
// XXX With more than 1 GpsBase/GpsAdmin this won't work. Must have separate lists for them. Currently it assumes that two admins and GpsBases won't connect at once.
    switch (tcp_client->mode) {
        case CLIENTMODE_GPSBASE:
            tcp_client->server->gps_base_ = 0;
            log(tcp_client, "Client was GpsBase");
            break;
        case CLIENTMODE_GPSADMIN:
            tcp_client->server->gps_admin_ = 0;
            log(tcp_client, "Client was GpsAdmin");
            break;
        default:
            // pass
            break;
    }
    xdelete(tcp_client);
}

/*****************************************************************************/
void
GpsServer::modembox_handle_packet(
    TCP_CLIENT* tcp_client,
    CMR&  Packet
)
{
    // 1. Log (if not ping).
    if (Packet.type != utils::CMR::PING) {
        std::string msg(Packet.data.begin(), Packet.data.end());
        //log ( tcp_client, "ModemBox sent something: %s", msg.c_str() );
    }

    // 2. Update contact time.
    database_->updateContactTime(tcp_client->ship_id->ship_number);

    // 3. Handle packet.
    switch (Packet.type) {
        case CMR::SERIAL: {
            //log ( tcp_client, "Parsing serial" );
            if (Packet.data.begin() == Packet.data.end() || Packet.data.size() == 0) {
                log(LOG_LEVEL_3, tcp_client, "Null-length serial packet");
                break;
            }
            const int gps_no = Packet.data[0];
            if (gps_no < 0 || gps_no > 1) {
                log(LOG_LEVEL_ESSENTIAL, tcp_client, "Wrong GPS number: %d", gps_no);
                break;
            }
            std::string tmp(Packet.data.begin() + 1, Packet.data.end());
            my_time time = my_time_of_now();
            log(LOG_LEVEL_ESSENTIAL, tcp_client,
					"GPS %d: %s",
					gps_no, tmp.c_str());
            tcp_client->nmea_decoder[gps_no].feed(tmp);
            std::string line;

            //log ( tcp_client, "GPS %d sent message: '%s'", gps_no, line.c_str() );
            GPGGA   gga_i;
            if (parse_gga(tmp, time, gga_i)) {
                my_time nw = my_time_of_now();
                //log(LOG_LEVEL_3, tcp_client, "GPS%d time: %d:%d:%d", gps_no, gga_i.gps_time.minute, gga_i.gps_time.second, gga_i.gps_time.millisecond);
                //log(LOG_LEVEL_3, tcp_client, "time now:   %d:%d:%d", nw.minute, nw.second, nw.millisecond);
                if (tcp_client->mixgps.get() != 0) {
                    tcp_client->mixgps->feed(gga_i, gps_no);
                }
                double pos_x, pos_y, pos_z;
                pos_x = pos_y = pos_z = 0.0;
                local_of_gps(gga_i.latitude, gga_i.longitude, gga_i.altitude, pos_x, pos_y, pos_z);
                database_->updateGps(tcp_client->ship_id->ship_number, gps_no + 1, pos_x, pos_y);
                // Add new GPS position to DB. Second parameter (type) must be the same as
                database_->addGpsPosition(tcp_client->ship_id->ship_number, gps_no, pos_x, pos_y, pos_z, gga_i.quality);

                if (gps_no != 1) {
                    //log ( tcp_client, "GPS %d coordinates: %3.1f %3.1f %3.1f Quality: %d Sattelites: %d pdop: %3.1f", gps_no, pos_x, pos_y, pos_z, gga.quality, gga.nr_of_satellites, gga.pdop );
                }

                tcp_client->gps_coordinates[gps_no].x = pos_x;
                tcp_client->gps_coordinates[gps_no].y = pos_y;

                std::vector<unsigned char> v;
                my_time time = my_time_of_now();
                double compass_direction, gps_direction, speed, gps12, gps13, gps23;
                GPGGA gga;
                if (gps_no == 1
                        && tcp_client->mixgps.get() != 0
                        && tcp_client->mixgps->calculate(time, gga, compass_direction, gps_direction, speed, gps12, gps13, gps23)) {
                    log(LOG_LEVEL_3, tcp_client, "MixGPS got heading :%3.3f", compass_direction);
                    union Packet{
                        PACKET_SHIP_POSITION pos;
                        char dat[sizeof(PACKET_SHIP_POSITION)];
                    };
                    if (compass_direction == std::numeric_limits<double>::quiet_NaN() ||
                            compass_direction == std::numeric_limits<double>::signaling_NaN()) {
                        log(LOG_LEVEL_3, tcp_client, "Compass direction is NaN: %d", compass_direction);
                        break;
                    }
                    database_->addShipPosition(tcp_client->ship_id->ship_number, POSITION_TYPE_MIXGPS, pos_x, pos_y, pos_z, compass_direction, speed);

                    Packet packet;
                    packet.pos.ship_number = tcp_client->ship_id->ship_number;
                    packet.pos.has_barge = 0;
                    //local_of_gps ( gga.latitude, gga.longitude, gga.altitude, packet.pos.gps1_x, packet.pos.gps1_y, z );
                    double  ship_z;
                    local_of_gps(gga.latitude, gga.longitude, gga.altitude,  packet.pos.gps1_x, packet.pos.gps1_y, ship_z);
                    packet.pos.heading = compass_direction;
                    packet.pos.flags = 0;
                    std::vector<unsigned char> v;
                    v.assign(&packet.dat[0], &packet.dat[0] + sizeof(packet));
                    // FIXME send only to needed clients (clients with GpsViewer)
                    for (std::list<TCP_CLIENT*>::const_iterator it = tcp_clients_.begin(); it != tcp_clients_.end(); ++it) {
                        TCP_CLIENT*   client = *it;
                        // Send to all other clients belonging to same group, don't send back to original GPS info sender
                        if (client->group_id == tcp_client->group_id && client->ship_id.get() != 0 && client->ship_id->ship_number != packet.pos.ship_number) {
                            //log(tcp_client, "Sending coords to %s", name.c_str());
                            send_event(client, v, CMRTYPE_SHIP_POSITION);
                        }
                    }
                }

                PACKET_GPS_POSITION gpspos;
                gpspos.ship_number = tcp_client->ship_id->ship_number;
                gpspos.gps_number = gps_no;
                gpspos.x = tcp_client->gps_coordinates[gps_no].x;
                gpspos.y = tcp_client->gps_coordinates[gps_no].y;
                union GpsPacket{
                    PACKET_GPS_POSITION  pos;
                    char dat[sizeof(PACKET_GPS_POSITION)];
                };
                GpsPacket gp;
                gp.pos = gpspos;
                v.assign(gp.dat, gp.dat + sizeof(GpsPacket));
                for (std::list<TCP_CLIENT*>::const_iterator it = tcp_clients_.begin(); it != tcp_clients_.end(); ++it) {
                    TCP_CLIENT*   client = *it;
                    if (client->group_id == tcp_client->group_id) {
                        //log(tcp_client, "Sending GPS %d coords to %s", gps_no, client->identity["Boat name"].c_str());
                        send_event(client,  v, CMRTYPE_GPS_POSITION);
                    }
                }
            }
            break;
        }
        case CMRTYPE_SHIP_POSITION: {
            if (Packet.data.size() != sizeof(PACKET_SHIP_POSITION)) {
                log("Wrong length ship position packet: %d vs %d. Ignoring", Packet.data.size(), sizeof(PACKET_SHIP_POSITION));
                break;
            }
            PACKET_SHIP_POSITION* pos = reinterpret_cast<PACKET_SHIP_POSITION*>(&Packet.data[0]);
            if (tcp_client->ship_id.get() != 0) {
                if (isnan(pos->heading)) {
                    log(LOG_LEVEL_3, tcp_client, "Ship heading is NaN: %d", pos->heading);
                    break;
                }
                log(LOG_LEVEL_3, tcp_client, "Got ship %d position %6.3f %6.3f, heading %6.3f flags: %d", pos->ship_number, pos->gps1_x, pos->gps1_y, pos->heading, (unsigned char)pos->flags&SHIP_POSITION_SHADOW);
                pos->ship_number = tcp_client->ship_id->ship_number;
                database_->updateShipHeading(tcp_client->ship_id->ship_number, pos->heading);
                // As Z coordinate isn't sent by gpsviewer we replace it with dummy 0.0. In saving function it gets ignored as position type shows it doesn't have that coordinate
                database_->addShipPosition(pos->ship_number, POSITION_TYPE_VIEWER, pos->gps1_x, pos->gps1_y, 0.0, pos->heading);
                if (pos->flags & SHIP_POSITION_SHADOW){
                    database_->saveShadow(pos->ship_number, pos->gps1_x, pos->gps1_y, pos->heading);
                }

            } else {
                log(LOG_LEVEL_3, tcp_client, "Got ship %d position but current client has no ship_id", pos->ship_number);
            }
            forward_packet_to_all(tcp_clients_, tcp_client, Packet);
            break;
        }
        case CMR::DIGNET: {
            string              msg_name;
            map<string, string> msg_args;
            string              data_block(Packet.data.begin(), Packet.data.end());
            try {
                ParseDigNetMessage(msg_name, msg_args, data_block);
            } catch (Error &e) {
                log(LOG_LEVEL_ESSENTIAL, tcp_client, "Error parsing DigNet message: '%s'", e.what());
                break;
            }
            if (msg_name == DIGNETMESSAGE_QUERY) {
                for (map<string, string>::const_iterator it = msg_args.begin(); it != msg_args.end(); ++it) {
                    const std::string&  key = it->first;
                    if (key == DIGNETMESSAGE_WORKAREA) {
                        WorkArea    wa;
                        if (database_->queryWorkArea(wa)) {
                            std::string msg;
                            msg += DIGNETMESSAGE_WORKAREA;
                            msg += " ";
                            // 1. Change time.
                            msg += " Change_Time=\"";
                            msg += wa.Change_Time.ToString();
                            msg += "\"";
                            // 2. Start.
                            msg += ssprintf(" Start=%4.2f;%4.2f", wa.Start_X, wa.Start_Y);
                            // 3. End.
                            msg += ssprintf(" End=%4.2f;%4.2f", wa.End_X, wa.End_Y);
                            // 4. Step_Forward
                            msg += ssprintf(" Step_Forward=%4.2f", wa.Step_Forward);
                            // 5. Pos_Sideways
                            msg += " Pos_Sideways=\"";
                            for (unsigned int i = 0; i < wa.Pos_Sideways.size(); ++i) {
                                if (i > 0) {
                                    msg += ';';
                                }
                                msg += ssprintf("%4.2f", wa.Pos_Sideways[i]);
                            }
                            msg += "\"";
                            log(LOG_LEVEL_ESSENTIAL, tcp_client, "DIGNET workarea: %s", msg.c_str());
                            send_event(tcp_client, msg, CMR::DIGNET);
                        } else {
                            send_event(tcp_client, "WorkArea error=\"not found.\"", CMR::DIGNET);
                        }
                    } else if (key == DIGNETMESSAGE_STARTUPINFO) {
                        // Client requested startup information
                        send_startup_info(tcp_client);
                    } else {
                        log(LOG_LEVEL_ESSENTIAL, tcp_client, "DIGNET unknown key : %s", key.c_str());
                    }
                }
            } else if (msg_name == DIGNETMESSAGE_MODEMBOX) {
                log(LOG_LEVEL_ESSENTIAL, tcp_client, "DIGNET modembox parameters: %s", data_block.c_str());
                map<string, string>::const_iterator msg = msg_args.find("supplyvoltage");
                if (msg != msg_args.end() && tcp_client->ship_id.get() != 0) {
                    const double voltage = double_of(msg->second);
                    tcp_client->setVoltage(voltage);
                    database_->logSupplyVoltage(tcp_client->ship_id->ship_number, voltage);
                    if (tcp_client->voltage_changed){
                        std::string subject, message;
                        getVoltageString(*tcp_client, subject, message);
                        mailer_.send(voltage_emails_, subject, message);
                    }
                }
            } else {
                log(LOG_LEVEL_ESSENTIAL, tcp_client, "DIGNET unknown: '%s'", data_block.c_str());
            }
            // Forward the message to others
            forward_packet_to_all(tcp_clients_, tcp_client, Packet);
            break;
        }
        case CMRTYPE_REQUEST_FROM_SHIP:
            if (Packet.data.size() >= sizeof(PACKET_REQUEST_FROM_SHIP)) {
                const PACKET_REQUEST_FROM_SHIP* req = reinterpret_cast<const PACKET_REQUEST_FROM_SHIP*>(&Packet.data[0]);
                // Flags are in first byte
                switch (req->flags) {
                    case REQUEST_SHIP_INFO: {
                        vector<unsigned char> info;
                        if (get_ship_info(tcp_client, *database_, info)) {
                            send_event(tcp_client, info, PACKET_SHIP_INFO::CMRTYPE);
                        } else {
                            log(tcp_client, "Requested ship #%d info but couldn't find it", tcp_client->ship_id.get() != 0 ? tcp_client->ship_id->ship_number : -1);
                        }
                        break;
                    }
                    case REQUEST_GPS_OFFSETS: {
                        PACKET_GPS_OFFSETS off;
                        if (database_->getShipGpsOffsets(req->ship_number, off.gps1_dx, off.gps1_dy, off.gps2_dx, off.gps2_dy)) {
                            off.ship_number = req->ship_number;
                            // Send requested ship offsets
                            send_packet(tcp_client, (void*)&off, sizeof(PACKET_GPS_OFFSETS), CMRTYPE_GPS_OFFSETS);
                        }
                        log(LOG_LEVEL_ESSENTIAL, tcp_client, "Ansvering to GPS offset request, ship: %d", req->ship_number);
                        break;
                    }
                    case REQUEST_SHIP_ID: {
                        TCP_CLIENT* client = find_client(req->ship_number);
                        if (client != 0 && client->ship_id.get() != 0) {
                            send_packet(tcp_client, client->ship_id.get(), sizeof(PACKET_SHIP_ID), CMRTYPE_SHIP_ID);
                        }
                        log(LOG_LEVEL_ESSENTIAL, tcp_client, "Ansvering to ship ID request, ship: %d", req->ship_number);
                        break;
                    }
                    default:
                        log(LOG_LEVEL_ESSENTIAL, tcp_client, "Unknown request from client: %d", Packet.data[1]);
                        break;
                }
            } else {
                log("Wrong length request packet: %d vs %d. Ignoring", Packet.data.size(), sizeof(PACKET_REQUEST_FROM_SHIP));
            }
            break;
        case PACKET_WORKLOG_SETMARK::CMRTYPE:
            if (Packet.data.size() >= sizeof(PACKET_WORKLOG_SETMARK)) {
                const PACKET_WORKLOG_SETMARK*   query = reinterpret_cast<const PACKET_WORKLOG_SETMARK*>(&Packet.data[0]);
                log("PACKET_WORKLOG_SETMARK: mark=%d at %d, %d", query->workmark, query->row_index, query->col_index);
                if (tcp_client->mode == CLIENTMODE_MODEMBOX) {
                    std::vector<unsigned char>      responseBuffer;
                    PACKET_WORKLOG_ROWS*            responsePacket;

                    if (database_->executeWorklogSetmark(*query, tcp_client->ship_id->ship_number, responseBuffer, responsePacket)) {
                        // yes :)
                        send_buffer_to_all(
                            tcp_clients_,
                            responseBuffer,
                            CLIENTMODE_MODEMBOX | CLIENTMODE_GPSVIEWER,
                            PACKET_WORKLOG_ROWS::CMRTYPE);
                    } else {
                        log("setmark insert database failed.");
                    }
                } else {
                    log("setmark : wrong or missing ship id.");
                }
            } else {
                log("Wrong length PACKET_WORKLOG_SETMARK: %d bytes, should be %d. Ignoring",
                    Packet.data.size(), sizeof(PACKET_WORKLOG_SETMARK));
            }
            break;
        case PACKET_WORKLOG_QUERY_BY_INDICES::CMRTYPE:
            if (Packet.data.size() >= sizeof(PACKET_WORKLOG_QUERY_BY_INDICES)) {
                const PACKET_WORKLOG_QUERY_BY_INDICES*  query = reinterpret_cast<const PACKET_WORKLOG_QUERY_BY_INDICES*>(&Packet.data[0]);
                log("PACKET_WORKLOG_QUERY_BY_INDICES: start_row %d, total %d rows",
                    (int)query->start_row_index, (int)query->nrows);

                std::vector<unsigned char>      responseBuffer;
                PACKET_WORKLOG_ROWS*            responsePacket;

                if (database_->executeWorklogQueryByIndices(*query, responseBuffer, responsePacket)) {
                    send_event(tcp_client, responseBuffer, PACKET_WORKLOG_ROWS::CMRTYPE);
                } else {
                    log("setmark query database failed.");
                }
            } else {
                log("Wrong length PACKET_WORKLOG_QUERY_BY_INDICES : %d bytes, should be %d. Ignoring",
                    Packet.data.size(), sizeof(PACKET_WORKLOG_QUERY_BY_INDICES));
            }
            break;
        case PACKET_BUILD_INFO::CMRTYPE:
            if (Packet.data.size() >= sizeof(PACKET_BUILD_INFO)) {
                const PACKET_BUILD_INFO*    build = reinterpret_cast<const PACKET_BUILD_INFO*>(&Packet.data[0]);
                log("PACKET_BUILD_INFO: build %d", build->build_number);
                tcp_client->gpsviewer_build_number = build->build_number;
                tcp_client->has_gpsviewer = true;
            } else {
                log("Wrong length PACKET_WORKLOG_SETMARK: %d bytes, should be %d. Ignoring",
                    Packet.data.size(), sizeof(PACKET_WORKLOG_SETMARK));
            }
            break;
        case PACKET_POINTSFILE_CHUNK_REQUEST::CMRTYPE:
            {
                PACKET_POINTSFILE_CHUNK_REQUEST* req = reinterpret_cast<PACKET_POINTSFILE_CHUNK_REQUEST*>(&Packet.data[0]);
                PACKET_POINTSFILE_CHUNK chunk;
                if (get_chunck(req->file_number, req->chunk_number, chunk)){
                    log(LOG_LEVEL_ESSENTIAL, tcp_client,
							"sending file #%d chunk #%d",
							req->file_number, req->chunk_number);
                    send_packet(tcp_client, &chunk, sizeof(chunk), PACKET_POINTSFILE_CHUNK::CMRTYPE);
                } else {
                    log(LOG_LEVEL_ESSENTIAL, tcp_client,
							"error, requested #%d chunk #%d, but not found.",
							tcp_client->name.c_str(), req->file_number, req->chunk_number);
				}
            }
            break;
        default:
            log(LOG_LEVEL_3, tcp_client, "Got some unknown message: 0x%0x", Packet.type);
    }
}

/*****************************************************************************/
void
GpsServer::timer_handler(
    int             fd,
    short           event,
    void*           _self)
{
    GpsServer*      server = reinterpret_cast<GpsServer*>(_self);
    std::list<TCP_CLIENT*>  clients_to_kill;
    server->timer_ticks_++;
    // 1. Report clients...
    if (++(server->timer_count10_) >= 10) {
        server->timer_count10_ = 0;
        // Check for weather updates
        if (server->weather_info_downloader_.get()!=0){
            std::string weather;
            if (server->weather_info_downloader_->popContents(weather)){
                parse_weather_info(weather, server->weather_);
                log(0, "Weather: Wind: %d Gust: %d Dir: %d water: %d time: %d", server->weather_.wind_speed, server->weather_.gust_speed, server->weather_.wind_direction, server->weather_.water_level, server->weather_.time);
                server->send_buffer_to_all(server->tcp_clients_, &server->weather_, sizeof(server->weather_), CLIENTMODE_MODEMBOX|CLIENTMODE_GPSVIEWER, PACKET_WEATHER_INFORMATION::CMRTYPE);
            }
        }
        const uint64_t packetsSent = server->packets_sent_.countAtInterval(10);
        const uint64_t packetsRecieved = server->packets_received_.countAtInterval(10);

        const uint64_t bytesRecieved = server->bytes_received_.countAtInterval(10);
        const uint64_t bytesSent = server->bytes_sent_.countAtInterval(10);
        log(LOG_LEVEL_3, 0, "10s timer. Packets sent/received last 10s: %lld/%lld, bytes sent/received: %lld/%lld", packetsSent, packetsRecieved, bytesSent, bytesRecieved);

    }
    if (++ (server->timer_count60_) >= 60) {
        server->timer_count60_ = 0;
        server->check_pointfile();
        // Send out voltage information 10 minutes after startup.
        // FIXME Time should be configurable?
        static bool startup_mail_sent = false;
        if (!startup_mail_sent && server->timer_ticks_ >= 13*60){
            startup_mail_sent = true;
            std::string subject("[DigNet] Server startup voltage information");
            std::stringstream message;
            for (std::list<TCP_CLIENT*>::const_iterator it = server->tcp_clients_.begin(); it != server->tcp_clients_.end(); ++it) {
                TCP_CLIENT* tcp_client = *it;
                std::string subj, msg;
                getVoltageString(*tcp_client, subj, msg);
                message<<msg<<std::endl;
            }
            server->mailer_.send(server->voltage_emails_, subject, message.str());
        }

        
        for (std::list<TCP_CLIENT*>::const_iterator it = server->tcp_clients_.begin(); it != server->tcp_clients_.end(); ++it) {
            TCP_CLIENT* tcp_client = *it;
            // FIXME Find a better way to not send ID to gpsviewers
            if (tcp_client->ship_id.get() != 0 && tcp_client->imei != "12345") {
                log(tcp_client, "Sending ID");
                server->send_packet(tcp_client, tcp_client->ship_id.get(), sizeof(PACKET_SHIP_ID), CMRTYPE_SHIP_ID);
                //server->send_packet ( tcp_client, tcp_client->ship_info.get(), sizeof(PACKET_SHIP_INFO), CMRTYPE_SHIP_INFO );
            }
        }

        const uint64_t bytesSen = server->bytes_sent_.countAtInterval(60);
        const uint64_t bytesRec = server->bytes_received_.countAtInterval(60);
        const float bpsSen = bytesSen/60.0;
        const float bpsRec = bytesRec/60.0;
        log(LOG_LEVEL_ESSENTIAL, 0, "1-minute timer. Uptime %d, total of %d clients. Packets sent/received: %lld/%lld. Bytes: %lld/%lld per second: %3.1f/%3.1f ", server->timer_ticks_, server->tcp_clients_.size(), server->packets_sent_.countAtInterval(60), server->packets_received_.countAtInterval(60), bytesSen, bytesRec, bpsSen, bpsRec);
        // Send everyones GPS offsets to everyone else
        // FIXME: Send only if changed?
        for (std::list<TCP_CLIENT*>::const_iterator it = server->tcp_clients_.begin(); it != server->tcp_clients_.end(); ++it) {
            TCP_CLIENT* tcp_client = *it;
            string s = tcp_client->reportMinuteStatistics();
            log(LOG_LEVEL_ESSENTIAL, tcp_client, s.c_str());
        }
        // Handle RTCM source
        if (rtcm_source_.size() > 0) {
            const bool  is_connected = server->rtcm_source_socket_ != (int) INVALID_SOCKET;
            const int   delta_bytes = server->rtcm_source_bytes_read_ - server->rtcm_source_bytes_read_last_;
            log(0, "RTCM source: %s, %d bytes last minute, %d bytes total.",
                is_connected ? "connected" : "disconnected",
                delta_bytes,
                server->rtcm_source_bytes_read_
               );

            server->rtcm_source_bytes_read_last_ = server->rtcm_source_bytes_read_;
            if (is_connected) {
                if (delta_bytes == 0) {
                    server->rtcm_source_reconnect("No data.");
                }
            } else {
                server->rtcm_source_reconnect("Disconnection.");
            }
        }
        // daily voltage mail
        const my_time now = my_time_of_now();
        if (now.hour == server->daily_report_time_.hour && now.minute == server->daily_report_time_.minute){
            std::string subject(ssprintf("[DigNet] Daily voltage information for %d/%d", now.month, now.day));
            std::stringstream message;
            for (std::list<TCP_CLIENT*>::const_iterator it = server->tcp_clients_.begin(); it != server->tcp_clients_.end(); ++it) {
                TCP_CLIENT* tcp_client = *it;
                if (tcp_client->mode == CLIENTMODE_MODEMBOX){
                    std::string subj, msg;
                    getVoltageString(*tcp_client, subj, msg);
                    message<<msg<<std::endl;
                }
            }
            server->mailer_.send(server->voltage_emails_, subject, message.str());
        }
    }

    if (++ (server->timer_count3600_) >= 3600) {
        server->timer_count3600_ = 0;
        log(LOG_LEVEL_ESSENTIAL, 0, "1 hour timer. Total packets sent/received: %lld/%lld", server->packets_sent_.countAtInterval(3600), server->packets_received_.countAtInterval(3600));
        // Gather all offsets into (offset, groupID) pairs, ignore GpsViewers
        std::vector<std::pair<PACKET_GPS_OFFSETS, int> > offsets;
        // For newer clients send SHIP_INFO instead of GPS_OFFSETS
        std::vector<std::pair<vector<unsigned char>, int > > infos;
        for (std::list<TCP_CLIENT*>::const_iterator it = server->tcp_clients_.begin(); it != server->tcp_clients_.end(); ++it) {
            TCP_CLIENT* tcp_client = *it;
            if (tcp_client->ship_id.get() != 0 && tcp_client->imei != "12345") {
                std::pair<PACKET_GPS_OFFSETS, int> off;
                if (server->database_->getShipGpsOffsets(tcp_client->ship_id->ship_number, off.first.gps1_dx, off.first.gps1_dy, off.first.gps2_dx, off.first.gps2_dy)) {
                    off.first.ship_number = tcp_client->ship_id->ship_number;
                    off.second = tcp_client->group_id;
                    offsets.push_back(off);
                }
                std::pair<vector<unsigned char>, int > info;
                if (get_ship_info(tcp_client, *server->database_, info.first)) {
                    info.second = tcp_client->group_id;
                    infos.push_back(info);
                }
            }
        }
        // Send offsets to their respective groups
        for (std::list<TCP_CLIENT*>::const_iterator it = server->tcp_clients_.begin(); it != server->tcp_clients_.end(); ++it) {
            TCP_CLIENT* tcp_client = *it;
            // Send any data only if client has GPSViewer attached
            if (tcp_client->has_gpsviewer) {
                // Send SHIP_INFO to newer builds, GPS_OFFSETS to older ones
                if (tcp_client->gpsviewer_build_number <= BUILD_ACCEPTS_SHIP_INFO) {
                    for (unsigned int i = 0;i < infos.size();i++) {
                        if (offsets[i].second == tcp_client->group_id) {
                            server->send_packet(tcp_client, &infos[i].first[0], infos[i].first.size(), PACKET_SHIP_INFO::CMRTYPE);
                        }
                    }
                } else {
                    for (unsigned int i = 0;i < offsets.size();i++) {
                        if (offsets[i].second == tcp_client->group_id) {
                            server->send_packet(tcp_client, &offsets[i].first, sizeof(PACKET_GPS_OFFSETS), CMRTYPE_GPS_OFFSETS);
                        }
                    }
                }
            }
        }
    }
    // 2. Ping clients, if possible.
    if (++ (server->timer_countping_) >= PING_TICKS) {
        server->timer_countping_ = 0;
        log(LOG_LEVEL_ESSENTIAL, 0, "Ping time.");
        send_buffer_to_all(server->tcp_clients_, 0, 0, CLIENTMODE_PINGABLE, utils::CMR::PING);
    }

    // 3. Kill idle clients.
    {
        // Find idle clients....
        for (std::list<TCP_CLIENT*>::const_iterator it = server->tcp_clients_.begin(); it != server->tcp_clients_.end(); ++it)
        {
            TCP_CLIENT* tcp_client = *it;
            ++tcp_client->idle_ticks;
            if (tcp_client->idle_ticks >= MAX_IDLE_TICKS) {
                clients_to_kill.push_front(tcp_client);
            }
        }

        for (std::list<TCP_CLIENT*>::iterator it = clients_to_kill.begin(); it != clients_to_kill.end(); ++it)
        {
            TCP_CLIENT *c = *it;
            log(c, "Killing client 0x%0x, idle ticks %d", c->fd, c->idle_ticks);
            server->kill_tcp_client(*it, "No activity.");
        }
    }

    // 4. Fire again.
    evtimer_add(&server->timer_event_, &server->timer_period_);
}

/*****************************************************************************/
void
GpsServer::tcp_client_read_handler(
    struct bufferevent*     event,
    void*                   _tcp_client)
{
    TCP_CLIENT* tcp_client = reinterpret_cast<TCP_CLIENT*>(_tcp_client);
    GpsServer*  server = tcp_client->server;
    std::string&    buffer = server->tcp_read_buffer_;


    // Reset idle activity counter.
    tcp_client->idle_ticks = 0;

    // Read from buffer.
    int so_far = 0;

    for (;;) {
        if (((int)(buffer.size())) == so_far)
            buffer.resize(so_far <= 0 ? 1024 : 2*so_far);
        int this_round = bufferevent_read(event, &buffer[so_far], buffer.size() - so_far);
        if (this_round <= 0)
            break;
        so_far += this_round;
    }

    if (so_far == 0) {
        server->kill_tcp_client(tcp_client, "TCP End-of-stream.");
        return;
    }
    std::vector<unsigned char> buf(buffer.begin(), buffer.begin() + so_far);
#ifdef LOG_PACKETS
    {
        // Log all packets
        FILE* f = fopen("packets.raw", "ab");
        if (f != 0) {
            fwrite(&buf[0], sizeof(unsigned char), buf.size(), f);
            fclose(f);
        }
    }
#endif
    server->bytes_received_ += buf.size();
    tcp_client->bytes_received_ += buf.size();

    //log ( tcp_client, "Parsing buffer of %d bytes coming from 0x%0x: %s", buf.size(), tcp_client->fd, tcp_client->name.c_str() );
    //OsKando
    if ((tcp_client->mode == CLIENTMODE_OSKANDO) || (tcp_client->mode == CLIENTMODE_UNIDENTIFIED && buf[0] == 'P')) {
        //log("Oskando sent a message: %s", std::string(buf.begin(), buf.end()).c_str());
        tcp_client->mode = CLIENTMODE_OSKANDO;
        tcp_client->name = "Oskando MK3";
        return;
    }

    tcp_client->cmr_decoder.feed(buf);
    utils::CMR cmr;
    buffer.resize(0);

    std::vector<std::string>    args;
    std::vector<unsigned char>  write_buffer;
    bool                kill_client = false;

    // Handle packets...
    while (tcp_client->cmr_decoder.pop(cmr)) {
        ++server->packets_received_;
        ++tcp_client->packets_received_;
#ifdef LOG_PACKETS
        {
            // Log last packet
            FILE *f = fopen("lastpacket.raw", "wb");
            if (f != 0) {
                fwrite(&cmr.data[0], sizeof(unsigned char), cmr.data.size(), f);
                fclose(f);
            }
        }
#endif
        std::string s(cmr.ToString());
        if (cmr.type == utils::CMR::PING) {
            // Ping packets are considered "processed" when they are received
            continue;
        } else {
            //std::string type_name=cmr.TypeName();
            //log ( tcp_client, "Got CMR packet type 0x%0x:%s from 0x%0x:%s, size %d", cmr.type, type_name.c_str(), tcp_client->fd, tcp_client->name.c_str(), cmr.data.size() );
        }
        if (cmr.type != utils::CMR::PING && (cmr.data.begin() == cmr.data.end() || cmr.data.size() == 0)) {
            log(LOG_LEVEL_3, tcp_client, "Null-length packet, skipping");
            continue;
        }

#ifdef FORCED_CRASH
        if (cmr.type == CMRTYPE_CRASH) {
            log(LOG_LEVEL_ESSENTIAL, tcp_client, "Got crash message");
            char* c = 0;
            c[100] = 0;
        }
#endif
        switch (tcp_client->mode) {
            case CLIENTMODE_UNIDENTIFIED: {
                log(tcp_client, "unidentified client");

                string              msg_name;
                map<string, string>  msg_args;
                string              data_block(cmr.data.begin(), cmr.data.end());
                try {
                    ParseDigNetMessage(msg_name, msg_args, data_block);
                } catch (Error &e) {
                    log(LOG_LEVEL_ESSENTIAL, tcp_client, "Error parsing DigNet message: '%s'", e.what());
                    break;
                }
                if (msg_name == DIGNETMESSAGE_IDENTIFY) {
                    // If can't authenticate kill klient
                    kill_client = !tcp_client->server->authenticate(tcp_client, msg_args, data_block);
                } else {
                    std::string type_name(cmr.TypeName());
                    std::string value(cmr.ToString());
                    log(LOG_LEVEL_ESSENTIAL, tcp_client,
                        "Unknown packet from 0x%0x:%s, type: %s:%s",
                        tcp_client->fd, tcp_client->name.c_str(), type_name.c_str(), value.c_str()
                       );
                }
            }
            break;
            case CLIENTMODE_GPSBASE:
                if (cmr.type != utils::CMR::PING) {
                    std::string value = cmr.ToString();
                    //log ( tcp_client, "GPSbase sent something: %s", value.c_str() );
                }
                switch (cmr.type) {
                    case (utils::CMR::LAMPNET) :
                            // send Lampnet to gpsadmin
                            if (tcp_client->server->gps_admin_ != 0) {
                                tcp_client->server->send_event(tcp_client->server->gps_admin_, cmr.data);
                            }
                        break;
                        //Send raw RTCM, no need to encode into CMR
                    case (utils::CMR::RTCM) :
                            for (std::list<TCP_CLIENT*>::const_iterator it = tcp_client->server->rtcm_listening_clients_.begin();it != tcp_client->server->rtcm_listening_clients_.end();it++) {
                                TCP_CLIENT* rtcm_client = *it;
                                std::string value = cmr.ToString();
                                //log ( tcp_client, "Sending %s to client %d", value.c_str(), rtcm_client->name.c_str() );
                                rtcm_client->server->send_raw_data(rtcm_client, cmr.data);
                                server->bytes_sent_ += cmr.data.size();
                            }
                        break;
                    default:
                        break;
                }
                break;
            case CLIENTMODE_GPSADMIN:
                if (cmr.type != utils::CMR::PING) {
                    std::string value = cmr.ToString();
                    //log ( tcp_client, "GpsAdmin sent something: %s", value.c_str() );
                }
                switch (cmr.type) {
                    case (utils::CMR::LAMPNET) :
                            // send Lampnet to gpsadmin
                            if (tcp_client->server->gps_base_ != 0) {
                                tcp_client->server->send_event(tcp_client->server->gps_base_, cmr.data);
                            } else {
                                std::string s = "Admin tries to talk with base but base does not exist!";
                                log(tcp_client, "%s", s.c_str());
                                tcp_client->server->send_event(tcp_client, s + "\n");
                            }
                        break;
                    default:
                        break;
                }
                break;
            case CLIENTMODE_GPSVIEWER:
                // Fallthrough because modembox_handle_packet also handles GpsViewer packets
            case CLIENTMODE_MODEMBOX:
                server->modembox_handle_packet(tcp_client, cmr);
                break;
            default: {
                std::string packet = cmr.ToString();
                log(LOG_LEVEL_3, tcp_client, "WARNING! Unknow client 0x%0x:%s, mode %d sent something: %s", tcp_client->fd, tcp_client->name.c_str(), tcp_client->mode, packet.c_str());
                //assert(false);
            }
        }
    }

    if (kill_client) {
        server->kill_tcp_client(tcp_client, "Rejected.");
    }
}

/*****************************************************************************/
bool
GpsServer::authenticate(
    TCP_CLIENT*                                 client,
    const std::map<std::string, std::string>&   credentials,
    const std::string&                          queryline
)
{
    bool success = false;
    log(client, "Authenticating someone");
    try {
        std::map<std::string, std::string>::const_iterator iter;
        for (iter = credentials.begin(); iter != credentials.end(); iter++) {
            std::string arg = iter->first;
            std::string val = iter->second;
            if (arg == "name") {
                client->name = val;
            } else if (arg == "Firmware_Version") {
                client->firmware_version = val;
            } else if (arg == "Firmware_Date") {
                client->firmware_date = val;
            } else if (arg == "IMEI") {
                client->imei = val;
            } else if (arg == "BuildInfo") {
                client->build_info = val;
            } else if (arg == "USER") {
                client->build_info = val;
            } else {
                log(client,  "Unknown authentication parameter: %s=%s", arg.c_str(), val.c_str());
            }
        }
        int         ship_id = -1;
        std::string name;
        double      gps1_dx = -1;
        double      gps1_dy = -1;
        double      gps2_dx = -1;
        double      gps2_dy = -1;
        double      supply_voltage_limit = -1;

        const bool exists =
            database_->registerShip(
                ship_id, client->group_id, name,
                gps1_dx, gps1_dy, gps2_dx, gps2_dy, supply_voltage_limit,
                client->imei, client->firmware_name, client->firmware_version, client->firmware_date, client->phone_no);
        if (!exists) {
            log(LOG_LEVEL_ESSENTIAL, client, "Client with unknown IMEI '%s' tried to connect, refusing", client->imei.c_str());
            return false;
        }

        PACKET_SHIP_ID* id = new PACKET_SHIP_ID;
        strncpy(id->ship_name, name.c_str(), sizeof(id->ship_name));
        id->ship_number = ship_id;
        id->flags = 0;
        client->ship_id = std::auto_ptr<PACKET_SHIP_ID>(id);
        if (client->imei != "12345") {
            client->modembox_voltage_limit = supply_voltage_limit;
            client->last_modembox_voltage = supply_voltage_limit;
            client->mixgps_viewpoint.x = 0;
            client->mixgps_viewpoint.y = 0;
            std::vector<utils::MixGPSXY>    gps_offset(2);
            gps_offset[0].x = gps1_dx;
            gps_offset[0].y = gps1_dy;
            gps_offset[1].x = gps2_dx;
            gps_offset[1].y = gps2_dy;
            client->mixgps = std::auto_ptr<MixGPS>(new MixGPS(client->mixgps_viewpoint, gps_offset));
            if (client->ship_id.get() != 0) {
                log(client, "Sending ID");
                // While sending set ID flags to 1
                id->flags = 1;
                send_packet(client, client->ship_id.get(), sizeof(PACKET_SHIP_ID), CMRTYPE_SHIP_ID);
                id->flags = 0;
                //send_packet ( client, client->ship_info.get(), sizeof(PACKET_SHIP_INFO), CMRTYPE_SHIP_INFO );
            }
        }
    } catch (Error e) {
        log(client, "problem parsing identification: %s", e.what());
    }

    log(client, "Name: 0x%0x:%s", client->fd, client->name.c_str());
    if (client->name == AUTHENTICATION_GPSBASE) {
        client->mode = CLIENTMODE_GPSBASE;
    } else if (client->name == AUTHENTICATION_MODEMBOX) {
        // Plain GpsViewers always have this IMEI
        if (client->imei == "12345") {
            client->mode = CLIENTMODE_GPSVIEWER;
            client->has_gpsviewer = true;
            static int gpsviewer_counter = 1;
            char buf[32];
            snprintf(buf, 32, "GpsViewer #%d", gpsviewer_counter);
            strncpy(client->ship_id->ship_name, buf, 32);
            if (client->firmware_version!=""){
                client->gpsviewer_build_number = int_of(client->firmware_version);
            }
        } else {
            client->mode = CLIENTMODE_MODEMBOX;
        }
    } else if (client->name == AUTHENTICATION_GPSADMIN) {
        client->mode = CLIENTMODE_GPSADMIN;
    } else {
        log(client, "Unknown name: %s", client->name.c_str());
    }

    // do the needful.
    switch (client->mode) {
        case CLIENTMODE_UNIDENTIFIED:
            log(LOG_LEVEL_ESSENTIAL, client,
                "Authentication failed for: 0x%0x:%s: %s",
                client->fd, client->name.c_str(), queryline.c_str());
            break;
        case CLIENTMODE_GPSBASE:
            if (client->server->gps_base_ == 0) {
                log(LOG_LEVEL_ESSENTIAL, client, "GpsBase 0x%0x:%s connected.", client->fd, client->name.c_str());
                gps_base_ = client;
                success = true;
            } else {
                std::string s("New GpsBase tries to connect but GpsBase is already connected, rejecting.");
                log(LOG_LEVEL_ESSENTIAL, client, s.c_str());
                send_event(client, s + "\n");
                success = false;
            }
            break;
        case CLIENTMODE_GPSADMIN:
            if (gps_admin_ == 0) {
                log(LOG_LEVEL_ESSENTIAL, client, "GpsAdmin 0x%0x:%s connected.", client->fd, client->name.c_str());
                gps_admin_ = client;
                success = true;
            } else {
                std::string s("New GpsAdmin tries to connect but GpsAdmin is already connected, rejecting.");
                log(LOG_LEVEL_ESSENTIAL, client, s.c_str());
                send_event(client, s + "\n");
                success = false;
            }
            break;
        case CLIENTMODE_GPSVIEWER:
            log(LOG_LEVEL_ESSENTIAL, client, "GpsViewer 0x%0x:%s: has connected, belongs to group %d", client->fd, client->name.c_str(), client->group_id);
            success = true;
            break;
        case CLIENTMODE_MODEMBOX:
            log(LOG_LEVEL_ESSENTIAL, client, "ModemBox 0x%0x:%s: has connected, belongs to group %d", client->fd, client->name.c_str(), client->group_id);
            success = true;
            break;
        default:
            log(LOG_LEVEL_ESSENTIAL, client, "WARNING! Client 0x%0x in unknown state: %d", client->fd, client->mode);
            break;
    }
    // Check if IP maches
    if (filter_ips_ && success){
      // Allow all modemboxes and newer GpsViewers (newer than build 62), block others
      if (client->mode != CLIENTMODE_MODEMBOX && client->mode != CLIENTMODE_GPSVIEWER || (client->mode == CLIENTMODE_GPSVIEWER && client->gpsviewer_build_number < BUILD_BLOCK_OLDER_THAN)){
            bool allowed = false;
            for (unsigned int i = 0; i < allowed_ips_.size(); i++){
                if (client->ip_address == allowed_ips_[i]){
                    allowed = true;
                    break;
                }
            }
            if (!allowed){
                log(LOG_LEVEL_ESSENTIAL, client, "IP did not match whitelist: %s. Rejecting", client->ip_address.c_str());
                success = false;
            }
        }
    }
    return success;
}


/*****************************************************************************/
void
GpsServer::tcp_client_write_handler(
    struct bufferevent*     event,
    void*                   _tcp_client)
{
    TCP_CLIENT* tcp_client = reinterpret_cast<TCP_CLIENT*>(_tcp_client);
    GpsServer*  server = tcp_client->server;
    // printf("TCP client write event.\n");
    // Pass.

    (void) server;
}

/*****************************************************************************/
void
GpsServer::tcp_client_error_handler(
    struct bufferevent*     event,
    short                   what,
    void*                   _tcp_client)
{
    TCP_CLIENT* tcp_client = reinterpret_cast<TCP_CLIENT*>(_tcp_client);
    GpsServer*  server = tcp_client->server;

    server->kill_tcp_client(tcp_client, "TCP read error.");
}


/*****************************************************************************/
void
GpsServer::listening_socket_handler(
    int             fd,
    short           event,
    void*           _self)
{
    GpsServer*  server = reinterpret_cast<GpsServer*>(_self);

    struct sockaddr_in  addr;
#ifdef WIN32
    int         addrlen = sizeof(addr);
#else
    socklen_t       addrlen = sizeof(addr);
#endif
    int         sd = accept(fd, reinterpret_cast<struct sockaddr*>(&addr), &addrlen);
    if (sd == -1) {
        log(0, "Error: Failed to get client.\n");
    } else {
        const string address(inet_ntoa(addr.sin_addr));
        log(0, "New client from %s: 0x%0x!", address.c_str(), sd);
        TCP_CLIENT*     tcp_client = new TCP_CLIENT(sd, server, CLIENTMODE_UNIDENTIFIED, address);
        tcp_client->event = bufferevent_new(sd,
                                            tcp_client_read_handler,
                                            tcp_client_write_handler,
                                            tcp_client_error_handler,
                                            tcp_client);
        server->tcp_clients_.push_front(tcp_client);
        bufferevent_enable(tcp_client->event, EV_READ | EV_WRITE);
    }
}

/*****************************************************************************/
void
GpsServer::rtcm_socket_handler(
    int            fd,
    short          event,
    void*          _self)
{
    GpsServer*  server = reinterpret_cast<GpsServer*>(_self);

    struct sockaddr_in  addr;
#ifdef WIN32
    int         addrlen = sizeof(addr);
#else
    socklen_t       addrlen = sizeof(addr);
#endif
    int         sd = accept(fd, reinterpret_cast<struct sockaddr*>(&addr), &addrlen);
    if (sd == -1) {
        log(0, "Error: Failed to get client.\n");
    } else {
        const string address(inet_ntoa(addr.sin_addr));
        log(0, "New RTCM listening client from %s: 0x%0x!", address.c_str(), sd);
        TCP_CLIENT*     tcp_client = new TCP_CLIENT(sd, server, CLIENTMODE_RTCMLISTENER, address);
        tcp_client->event = bufferevent_new(sd,
                                            0,
                                            rtcm_write_handler,
                                            rtcm_error_handler,
                                            tcp_client);
        server->rtcm_listening_clients_.push_front(tcp_client);
        bufferevent_enable(tcp_client->event, EV_WRITE);
        log(0, "Total listeners: %d", server->rtcm_listening_clients_.size());
    }
}


/*****************************************************************************/
void
GpsServer::rtcm_write_handler(
    struct bufferevent*     event,
    void*                   _tcp_client)
{
    log(0, "writing RTCM");
    TCP_CLIENT* tcp_client = reinterpret_cast<TCP_CLIENT*>(_tcp_client);
    GpsServer*  server = tcp_client->server;
    // Pass.
    (void) server;
}

/*****************************************************************************/
void
GpsServer::rtcm_error_handler(
    struct bufferevent*     event,
    short                   what,
    void*                   _tcp_client)
{
    TCP_CLIENT* tcp_client = reinterpret_cast<TCP_CLIENT*>(_tcp_client);
    GpsServer*  server = tcp_client->server;

    server->kill_rtcm_client(tcp_client, "TCP read error.");

}

/*****************************************************************************/
void
GpsServer::kill_rtcm_client(
    TCP_CLIENT*&    tcp_client,
    const char*     reason)
{
    log(tcp_client, "Shutting down RTCM reading client 0x%0x, name %s. Reason: %s", tcp_client->fd, tcp_client->name.c_str(), reason);
    // Shutdown events.
    bufferevent_disable(tcp_client->event, EV_WRITE);
    bufferevent_free(tcp_client->event);
    tcp_client->event = 0;

    // Close socket.
    close_socket(tcp_client->fd);
    tcp_client->fd = 0;

    // Remove client from the chain...
    rtcm_listening_clients_.remove(tcp_client);
    xdelete(tcp_client);
}

/*****************************************************************************/
GpsServer::GpsServer()
        :
        evb_(0),
        listening_socket_(INVALID_SOCKET),
        timer_count10_(0),
        timer_count60_(0),
        timer_count3600_(0),
        timer_countping_(0),

        rtcm_output_port_(INVALID_SOCKET),
        gps_base_(0),
        gps_admin_(0),
        rtcm_source_socket_(INVALID_SOCKET),
        rtcm_source_bytes_read_(0),
        rtcm_source_bytes_read_last_(0),
        rtcm_source_event_(0),
        database_(0)
{
    weather_.water_level = 0xffff;
    weather_.wind_direction = 0xffff;
    weather_.wind_speed = 0xffff;
    weather_.gust_speed = 0xffff;
    weather_.time = 0xffff;

    try {
        load_winsock();
    } catch (const std::exception& e) {
        log(0, "Error loading WinSock (%s), aborting.", e.what());
        throw;
    }
    timer_ticks_ = 0;
    log("GpsServer class created");
}

/*****************************************************************************/
void
GpsServer::rtcm_source_read_handler(
    struct bufferevent*     event,
    void*                   gps_server)
{
    GpsServer*          server = reinterpret_cast<GpsServer*>(gps_server);
    std::vector<unsigned char>& buffer = server->rtcm_source_read_buffer_;

    // Read from buffer.
    unsigned int    so_far = 0;
    for (;;)
    {
        if (buffer.size() == so_far) {
            buffer.resize(so_far <= 0 ? 1024 : 2*so_far);
        }
        int this_round = bufferevent_read(event, &buffer[so_far], buffer.size() - so_far);
        if (this_round <= 0)
            break;
        so_far += this_round;
    }
    buffer.resize(so_far);
    server->rtcm_source_bytes_read_ += so_far;

    // Detect EOF.
    if (so_far == 0)
    {
        server->rtcm_source_close("Connection closed.");
    } else
    {
        // Forward good stuff.
        // Not needed for GpsViewers? Might be with simple GPS
        server->send_buffer_to_all(server->tcp_clients_, buffer, CLIENTMODE_MODEMBOX, utils::CMR::RTCM);
        // RTCM listeners
        server->send_buffer_to_all(server->rtcm_listening_clients_, buffer, CLIENTMODE_RTCMLISTENER, utils::CMR::RTCM);
    }

}

/*****************************************************************************/
void
GpsServer::rtcm_source_error_handler(
    struct bufferevent*     event,
    short                   what,
    void*                   gps_server)
{
    GpsServer*  server = reinterpret_cast<GpsServer*>(gps_server);
    server->rtcm_source_close("Read error.");

}

/*****************************************************************************/
void
GpsServer::rtcm_source_close(
    const std::string&  reason)
{
    log(0, "rtcm_source_close reason: %s", reason.c_str());
    if (rtcm_source_socket_ != (int) INVALID_SOCKET) {
        // Free buffer.
        bufferevent_disable(rtcm_source_event_, EV_READ | EV_WRITE);
        bufferevent_free(rtcm_source_event_);
        rtcm_source_event_ = 0;

        // Close socket.
        close_socket(rtcm_source_socket_);
        rtcm_source_socket_ = INVALID_SOCKET;
    }

}

/*****************************************************************************/
void
GpsServer::rtcm_source_reconnect(
    const std::string&  reason)
{
    log(0, "rtcm_source_reconnect reason: %s", reason.c_str());
    if (rtcm_source_socket_ != (int) INVALID_SOCKET) {
        rtcm_source_close(reason);
    }
    try {
        rtcm_source_socket_ = connect_server_socket(rtcm_source_);
        rtcm_source_event_ = bufferevent_new(
                                 rtcm_source_socket_,
                                 rtcm_source_read_handler,
                                 0,
                                 rtcm_source_error_handler,
                                 this
                             );

        log(0, "connected to RTCM source at %s", rtcm_source_.c_str());
        rtcm_source_bytes_read_ = 0;
        rtcm_source_bytes_read_last_ = 0;

        // Enable events.
        bufferevent_enable(rtcm_source_event_, EV_READ);
    } catch (const std::exception& e) {
        log(0, "connect %s, error: %s", rtcm_source_.c_str(), e.what());
    }

}

/*****************************************************************************/
// FIXME Lots of cleanup still missing
GpsServer::~GpsServer()
{
    unload_winsock();
}

/*****************************************************************************/
void
GpsServer::run()
{
    log("Starting to run GpsServer");
    // Initalize \c libevent.
    evb_ = reinterpret_cast<struct event_base*>(event_init());

    // Read configuration file.
    utils::FileConfig cfg(FILENAME_CONFIGURATION);
    cfg.load();


    cfg.set_section("Database");
    {
        std::string base, username, passwd;

        if (!cfg.get_string("Database", "", base)) {
            log(LOG_LEVEL_ESSENTIAL, 0, "No database in cfg!");
            return;
        }

        if (!cfg.get_string("Username", "", username)) {
            log(LOG_LEVEL_ESSENTIAL, 0, "No database username in cfg!");
            return;
        }

        if (!cfg.get_string("Password", "", passwd)) {
            log(LOG_LEVEL_ESSENTIAL, 0, "No database password in cfg!");
            return;
        }

        try {
            database_ = std::auto_ptr<DigNetDB>(new DigNetDB(base, username, passwd));
        } catch (const std::exception& e) {
            log(LOG_LEVEL_ESSENTIAL, 0, "Can't open connection to DB: %s", e.what());
            return;
        }

    }

    cfg.set_section("GpsServer");
    {
        cfg.get_int("ListeningPort", 5001, listening_port_);
        cfg.get_int("RtcmOutputPort", 5002, rtcm_output_port_);
        if (cfg.get_string("RtcmSource", rtcm_source_)) {
            log(0, "Rtcm source set to %s", rtcm_source_.c_str());
        } else {
            log(0, "No Rtcm source set");
        }
        if (!cfg.get_bool("FilterIps", false, filter_ips_)){
            log(LOG_LEVEL_ESSENTIAL, 0, "FilterIPs not found in config, defaulting to not filter");
        } else {
            log(LOG_LEVEL_ESSENTIAL, 0, "IP filtering %s", filter_ips_? "on":"off");
        }
        string ips;
        cfg.get_string("AllowedIps", "", ips);
        split(ips, " ", allowed_ips_);
        log(LOG_LEVEL_ESSENTIAL, 0, "Allowing connections from %s", ips.c_str());

        std::string url;
        cfg.get_string("WeatherDownloadUrl", "", url);
        if (url!="") {
            log(0, "Downloading weather information from %s in 60s intervals", url.c_str());
            weather_info_downloader_=std::auto_ptr<FileDownloader>(new FileDownloader(url, 60));
            weather_info_downloader_->create();
        } else {
            log(0, "No weather information URL defined");
        }
        
        cfg.get_string("VoltageEmailSender", "dignet@dignet.ee", sender_email_);
        string emails;
        cfg.get_string("VoltageEmails", emails);
        split(emails, ",", voltage_emails_);
        log(0, "Sending voltage emails to %s with sender address %s", emails.c_str(), sender_email_.c_str());
        string report_time;
        cfg.get_string("VoltageDailyReportTime", report_time);
        memset(&daily_report_time_, 0, sizeof(daily_report_time_));
        vector<string> time;
        split(report_time, ":", time);
        if (time.size()==2){
            daily_report_time_.hour = int_of(time[0]);
            daily_report_time_.minute = int_of(time[1]);
            log(0, "Sending daily voltage information emails at %d:%d", daily_report_time_.hour, daily_report_time_.minute);
        } else {
            log(0, "Warning: couldn't parse daily voltage report time! %s", report_time.c_str());
        }
    }
    cfg.set_section("PointsFile");
    {
        if (cfg.get_string("NewPointsDirectory", "", points_file_dir_)){
            log(LOG_LEVEL_ESSENTIAL, 0, "Points file directory is %s", points_file_dir_.c_str());
        } else {
            log(LOG_LEVEL_ESSENTIAL, 0, "WARNING! Points file directory not set!");
        }
    }
    // Open listening socket.
    listening_socket_ = open_server_socket(listening_port_);
    log(0, "Running on port %d", listening_port_);
    event_set(&listening_socket_event_, listening_socket_, EV_READ | EV_PERSIST, listening_socket_handler,  this);
    event_add(&listening_socket_event_, 0);

    // Open listening socket to wait for clients who want to read RTCM.

    rtcm_socket_ = open_server_socket(rtcm_output_port_);
    log(0, "Writing RTCM to port %d", rtcm_output_port_);
    event_set(&rtcm_socket_event_, rtcm_socket_, EV_READ | EV_PERSIST, rtcm_socket_handler,  this);
    event_add(&rtcm_socket_event_, 0);

    if (rtcm_source_.size() > 0) {
        rtcm_source_reconnect("Boot.");
    }

    // Timer setup.
    event_set(&timer_event_, 0, EV_TIMEOUT | EV_PERSIST, timer_handler, this);
    timer_period_.tv_sec = 1;
    timer_period_.tv_usec = 0;
    timer_count60_ = 0;
    timer_countping_ = 0;
    evtimer_add(&timer_event_, &timer_period_);

    event_dispatch();
}
