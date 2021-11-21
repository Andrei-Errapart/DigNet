/**
vim: ts=4
vim: shiftwidth=4
*/
/**
\file GpsServer main file.
Compile using __USE_BSD.

See \c gps_server.doc for complete specification.
*/
#include <vector>
#include <string>
#include <list>
#include <map>
#include <memory>       // std::auto_ptr

#include <stdio.h>      // printf, sprintf
#include <stdarg.h>     // varargs.

#include <event.h>      // libevent.

#include <utils/mysockets.h>       // open_server_socket
#include <utils/util.h>            // SimpleConfig, Error.

#include <utils/rtcm.h>
#include <utils/myrtcm.h>
#include <utils/CMR.h>
#include <utils/NMEA.h>
#include <utils/MixGPS.h>

#include "DigNetMessages.h"
#include "DigNetDB.h"
#include "Counter.h"
#include "FileDownloader.h"
#include "SendEmail.h"


/// Timeout, in seconds.
#define    MAX_IDLE_TICKS    60

/// Ping every N ticks.
#define    PING_TICKS    10

/// If defined then clients are not checked against allowed client list read from .ini
#define RANDOM_VIEWERS_CAN_CONNECT

typedef enum {
    LOG_LEVEL_DEBUG,            // All messages
    LOG_LEVEL_3 = 10,           // some are cut
    LOG_LEVEL_ESSENTIAL         // Only most neede information, shown in server log
} LOG_LEVEL;

typedef struct {
    double x, y;
} Point2;

/** A mode client might have. Must be bitfield to be able to filter in send_buffer_to_all.*/
typedef enum {
    CLIENTMODE_UNIDENTIFIED = 01,       ///< Yet to receive identification packet.
    CLIENTMODE_GPSBASE      = 020,      ///< Client is GPS base
    CLIENTMODE_GPSADMIN     = 040,      ///< Client is GPS administrator
    CLIENTMODE_OSKANDO      = 0100,     ///< Client is Oscando MK3
    CLIENTMODE_MODEMBOX     = 0200,     ///< Client is ModemBox
    CLIENTMODE_GPSVIEWER    = 0400,     ///< Client is GpsViewer without modembox
    CLIENTMODE_RTCMLISTENER = 01000,    ///< Client is RTCM listener
    CLIENTMODE_PINGABLE     = CLIENTMODE_GPSBASE + CLIENTMODE_GPSADMIN + CLIENTMODE_MODEMBOX + CLIENTMODE_GPSVIEWER ///< All pingable clients
} CLIENTMODE;
/*****************************************************************************/

typedef enum  {
    BUILD_NO_NUMBER         = 0,
    BUILD_ACCEPTS_SHIP_INFO = 31, ///< First version that started accepting SHIP_INFO packet
    BUILD_BLOCK_OLDER_THAN  = 63  ///< Block all older GpsViewers having smaller this build number (<)
} GPSVIEWER_BUILD;
class GpsServer;

/// Client info.
class TCP_CLIENT {
    public:

        TCP_CLIENT(
            int socket,             ///< Socket descriptor
            GpsServer* server,      ///< pointer to server
            CLIENTMODE mode,        ///< Clientmode @see \c CLIENTMODE
            const std::string& ip); ///< Client IP address

        /** Sets supply voltage and related data (time since last read, must send e-mail, ...)
        @param voltage voltage reading
        */
        void 
        setVoltage(
            const double voltage);

        /** Returns a string meant to be logged once per minute */
        std::string reportMinuteStatistics();

        unsigned int            fd;             ///< Client socket.
        struct bufferevent*     event;          ///< Buffered events.
        GpsServer*              server;         ///< Points back to server.
        CLIENTMODE              mode;           ///< Client mode.
        std::string             ip_address;     ///< IP address of client.
        std::string             name;           ///< Name of the client. Valid only if identified.
        std::string             firmware_version;
        std::string             firmware_date;
        std::string             firmware_name;
        std::string             phone_no;
        std::string             imei;
        std::string             build_info;

        int                     idle_ticks;         ///< Number of idle ticks.
        utils::CMRDecoder       cmr_decoder;        ///< Decodes incoming CMR packets from this client
        utils::LineDecoder      nmea_decoder[2];    ///< Decodes incoming messages from bot GPSes
        Point2                  gps_coordinates[2]; ///< GPS positions
        std::auto_ptr<utils::MixGPS>    mixgps;     ///< MixGPS
        utils::MixGPSXY         mixgps_viewpoint;
        std::auto_ptr<PACKET_SHIP_ID>   ship_id;

        // Voltage information
        double                  last_modembox_voltage;          ///< Last voltage received from client
        double                  modembox_voltage_limit;         ///< Lower limit for voltage
        utils::Option<utils::my_time>   voltage_reading_time;   ///< Time of last voltage reading
        bool                    voltage_changed;                ///< If true then voltage changed from either normal to below-limit or from below-limit to normal

        int                     group_id;                   ///< Group ID of the client, gets read from DB. Clients from different groups do not communicate
        int                     gpsviewer_build_number;     ///< Build number of associated GpsViewer. Defaults to 0 if not set
        bool                    has_gpsviewer;              ///< If true then this client has GpsViewer attached

        Counter                 packets_received_;              ///< Total number of packets received
        Counter                 packets_sent_;                  ///< Total number of packets sent
        Counter                 bytes_received_;                ///< Total number of bytes received from clients
        Counter                 bytes_sent_;                    ///< Total number of bytes sent to clients

}; // struct TCP_CLIENT.

/*****************************************************************************/
/// Server main.
class GpsServer {
    private:
        struct event_base*          evb_;
        unsigned int                listening_socket_;              ///< Listening TCP/IP socket.
        struct event                listening_socket_event_;        ///< TCP port "listen" events.
        struct timeval              timer_period_;                  ///< Timer period, every 5 minutes.
        struct event                timer_event_;                   ///< Timer event.
        unsigned int                timer_count10_;                 ///< Count 0..10. When reaches 0 some a little bit of information is said about current status.
        unsigned int                timer_count60_;                 ///< Count 0..59. When reaches 0 detailed status information is reported
        unsigned int                timer_count3600_;               ///< Count 0..3600. When reaches 0 some less frequently updated information is sent to clients
        unsigned int                timer_countping_;               ///< Count ping ticks.
        unsigned int                timer_ticks_;                   ///< Number of timer ticks since server startup

        Counter                     packets_received_;              ///< Total number of packets received
        Counter                     packets_sent_;                  ///< Total number of packets sent
        Counter                     bytes_received_;                ///< Total number of bytes received from clients
        Counter                     bytes_sent_;                    ///< Total number of bytes sent to clients

        unsigned int                rtcm_socket_;                   ///< RTCM output socket.
        struct event                rtcm_socket_event_;             ///< RTCM socket "listen" events.

        std::list<TCP_CLIENT*>      tcp_clients_;                   ///< List of clients :)

        std::string                 tcp_read_buffer_;               ///< Temporary buffer for TCP reads.
        std::vector<unsigned char>  tcp_write_buffer_;              ///< Global write buffer.

        int                         listening_port_;                ///< TCP listening port. Listens for client connections
        int                         rtcm_output_port_;              ///< raw RTCM messages get written to this port
        std::list<TCP_CLIENT*>      rtcm_listening_clients_;        ///< List of clients who listen to RTCM

        TCP_CLIENT*                 gps_base_;                      ///< Connected GpsBase, used to find it quickly to transfer messages between base and admin. Is also in tcp_clients_ list
        TCP_CLIENT*                 gps_admin_;                     ///< Connected GpsAdmin, used to find it quickly to transfer messages between base and admin. Is also in tcp_clients_ list
        TCP_CLIENT*                 oskando_;                       ///< Oskando server messages
        bool                        filter_ips_;                    ///< If true then allow only listed IPs to connect
        std::vector<std::string>    allowed_ips_;                   ///< List of IP addresses that are allowed to connect

        static std::string          rtcm_source_;                   ///< Raw RTCM comes from this server. Holds ip:port to connect to.
        int                         rtcm_source_socket_;            ///< Connection socket for connecting to rtcm server.
        std::vector<unsigned char>  rtcm_source_read_buffer_;       ///< Temporary buffer for TCP reads.
        unsigned int                rtcm_source_bytes_read_;
        unsigned int                rtcm_source_bytes_read_last_;
        struct bufferevent*         rtcm_source_event_;             ///< Buffered events. Default: 0.

        std::auto_ptr<DigNetDB>     database_;                      ///< Database holding ship information.
        std::auto_ptr<FileDownloader> weather_info_downloader_;     ///< Asynchronusly downloads weather information
        PACKET_WEATHER_INFORMATION  weather_;                       ///< Weather information is saved here and sent to clients as needed. Saving is needed to get the information to clients as soon as possible

        // Voltage reporting via e-mail
        std::vector<std::string>    voltage_emails_;                ///< E-mail addresses where to send voltage information
        std::string                 sender_email_;                  ///< Sender e-mail address
        utils::my_time              daily_report_time_;             ///< Time of day when to send daily voltage reports (only hour and minute set)
        SendEmail                   mailer_;                        ///< Class for sending E-Mail
        

        std::string                 points_file_dir_;               ///< Directory where pointsfile updates are held
    public:
        /*****************************************************************************/
        /// Log a message preceded by time in format "HHMMSS" and sender name, if known.
        static void
        log(
            LOG_LEVEL           priority,
            const TCP_CLIENT*   client,
            const char*         fmt,
            va_list             arguments);

        /// Log a message preceded by time in format "HHMMSS" and sender name, if known.
        static void
        log(
            LOG_LEVEL           priority,
            const TCP_CLIENT*   client,
            const char*         fmt, ...);

        /// Helper Log a message preceded by time in format "HHMMSS" and sender name, if known. Defaults to debug
        static void
        log(
            const TCP_CLIENT*   client,
            const char*         fmt, ...);

        /// Helper Log, doesn't take any additional parameters. Defaults to debug
        static void
        log(
            const char*         fmt, ...);

    private:
// Raw RTCM input. Special handlers that read raw RTCM from remote server and forward it to GpsViewers.
        static void
        rtcm_source_read_handler(
            struct bufferevent* event,
            void*               gps_server);

        static void
        rtcm_source_error_handler(
            struct bufferevent* event,
            short               what,
            void*               gps_server);

        void
        rtcm_source_close(
            const std::string&  reason);

        void
        rtcm_source_reconnect(
            const std::string&  reason);


        /*****************************************************************************/

        /// Send raw data to client
        void
        send_raw_data(
            TCP_CLIENT*                         tcp_client,
            std::vector<unsigned char>&         data);

        /// Send event to given client.
        void
        send_event(
            TCP_CLIENT*         tcp_client,
            const std::string&  event,
            int                 type = utils::CMR::LAMPNET);

        void
        send_event(
            TCP_CLIENT*                         tcp_client,
            const std::vector<unsigned char>&   event,
            int                                 type = utils::CMR::LAMPNET);

        void
        send_packet(
            TCP_CLIENT*         tcp_client,
            const void*         event,
            int                 size,
            int                 type = utils::CMR::LAMPNET);


        /***************************************************************/
        /// Send buffer contents to all TCP/IP clients, filtered by mask.
        static void
        send_buffer_to_all(
            std::list<TCP_CLIENT*>&         tcp_clients,
            std::vector<unsigned char>&     buffer,
            unsigned int                    mask = CLIENTMODE_MODEMBOX | CLIENTMODE_GPSVIEWER,
            int                             type = utils::CMR::LAMPNET
        );

        /// Send buffer contents to all TCP/IP clients, filtered by mask.
        static void
        send_buffer_to_all(
            std::list<TCP_CLIENT*>&         tcp_clients,
            void*                           buffer,
            int                             size,
            unsigned int                    mask = CLIENTMODE_MODEMBOX | CLIENTMODE_GPSVIEWER,
            int                             type = utils::CMR::LAMPNET
        );
        /** Forwards given packet to all clients besides the sender itself */
        static void
        forward_packet_to_all(
            std::list<TCP_CLIENT*>&         clients,    ///< List of clients to recieve
            const TCP_CLIENT*               sender,        ///< Sender ID, won't be sent back to this client
            const utils::CMR&               packet        ///< Packet to be sent
        );

        /*****************************************************************************/
        /** Sends all the information about other ships to given client */
        void
        send_startup_info(
            TCP_CLIENT*                     tcp_client      ///< Client to be recieving the information
        );

        /** Sees if new pointfile is present and sends information about it to clients */
        void
        check_pointfile();

        /** Gets a chunk from a given file. 
            @returns true on success 
        */
        bool
        get_chunck(
            const uint16_t              file_no,    ///< File number
            const uint16_t              chunk_no,   ///< Chunk number
            PACKET_POINTSFILE_CHUNK&    data        ///< Data is inserted here, including file and chunk number
        );

        /// Finds a client with given ship number. Returns 0 if not found
        TCP_CLIENT*
        find_client(
            int     ship_no);

        /***************************************************************/
        /// Shutdown and delete TCP client.
        void
        kill_tcp_client(
            TCP_CLIENT*&    tcp_client,
            const char*     reason
        );

        /*****************************************************************************/
        void
        modembox_handle_packet(
            TCP_CLIENT*         tcp_client,
            utils::CMR&   Packet
        );

        /*****************************************************************************/
        /// Timer passed...
        static void
        timer_handler(
            int             fd,
            short           event,
            void*           _self);

        /***************************************************************/
        static void
        tcp_client_read_handler(
            struct bufferevent*     event,
            void*                   _tcp_client);

        /***************************************************************/
        static void
        tcp_client_write_handler(
            struct bufferevent*     event,
            void*                   _tcp_client);

        /***************************************************************/
        static void
        tcp_client_error_handler(
            struct bufferevent*     event,
            short                   what,
            void*                   _tcp_client);

        /*****************************************************************************/
        bool
        authenticate(
            TCP_CLIENT*                                 client,
            const std::map<std::string, std::string>&   credentials,
            const std::string&                          queryline);

        /// TCP client accepted.

        static void
        listening_socket_handler(
            int         fd,
            short       event,
            void*       _self);

        static void
        rtcm_write_handler(
            struct bufferevent*     event,
            void*                   _tcp_client);

        static void
        rtcm_error_handler(
            struct bufferevent*     event,
            short                   what,
            void*                   _tcp_client);

        static void
        rtcm_socket_handler(
            int                     fd,
            short                   event,
            void*                   _self);

        void
        kill_rtcm_client(
            TCP_CLIENT*&            tcp_client,
            const char*             reason);

    public:
        /*****************************************************************************/
        /// Constructor - initialize to zero or reasonable defaults all stuff.
        GpsServer();

        /*****************************************************************************/
        /// Destructor - release resources.
        ~GpsServer();

        /*****************************************************************************/
        /// Initalize & run server.
        void
        run();
};
