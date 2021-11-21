/*
vim: ts=4
vim: shiftwidth=4
*/
#ifndef DigNetMessages_h_
#define DigNetMessages_h_
//
// C++ Interface: DigNetMessages
//
// Description:
//
//
// Author: Kalle Last <kalle@errapartengineering.com>, (C) 2007
//
// Copyright: See COPYING file that comes with this distribution
//
//

#include <map>       // std::map
#include <string>    // std::string.

#include <utils/util.h>         // utils::my_time.
#include <utils/mystdint.h>     // uint8_t and friends.
#include <utils/CMR.h>          // CMR.

#pragma pack(push, 1)

enum {
    /// CMR message type for \c PACKET_SHIP_POSITION
    CMRTYPE_GPS_OFFSETS         = 0xBC01,
    CMRTYPE_SHIP_ID             = 0xBC02,
    CMRTYPE_GPS_POSITION        = 0xBC03,
    CMRTYPE_SHIP_POSITION       = 0xBC04,
    // Some ID's are reserved for CMR internal messages (BC05-BC0F)

    CMRTYPE_REQUEST_FROM_SHIP   = 0xBC10,
    CMRTYPE_SHIP_INFO           = 0xBC12
#ifdef FORCED_CRASH
    ,CMRTYPE_CRASH              = 0xDEAD   // Special packet on what GpsServer must crash
#endif
};

/// For PACKET_SHIP_POSITION bitfield
enum PACKET {
    SHIP_POSITION_SHADOW        =    01
};

/// Request types, enum
enum REQUEST_FLAGS {
    REQUEST_SHIP_ID     =   0x1,
    REQUEST_GPS_OFFSETS =   0x2,
    REQUEST_SHIP_INFO   =   0x3
};

typedef struct {
    /// Ship number in the range 1 ... 255. */
    uint8_t     ship_number;
    /// Does it have barge?
    uint8_t     has_barge;
    /// Ship position, x, in local coordinate system.
    double      gps1_x;
    /// Ship position, y, in local coordinate system.
    double      gps1_y;
    /// Heading, degrees, clockwise from north.
    double      heading;
    /// Bitfield : shadow
    uint8_t     flags;
} PACKET_SHIP_POSITION;

typedef struct {
    uint8_t     ship_number;
    uint8_t     gps_number;
    double      x;
    double      y;
} PACKET_GPS_POSITION;

typedef struct {
    uint8_t     ship_number;
    char        ship_name[32];
    uint8_t     flags;          // first bit is 1 when this is own ID
} PACKET_SHIP_ID;

/// Ship GPS offsets relative to ship 0,0 coordinates
typedef struct {
    int     ship_number;
    double  gps1_dx;
    double  gps1_dy;
    double  gps2_dx;
    double  gps2_dy;
} PACKET_GPS_OFFSETS;

/// Request something from specific ship. Flags show what is requested, usually equal to CMR type
typedef struct {
    uint8_t     flags;
    uint8_t     ship_number;
} PACKET_REQUEST_FROM_SHIP;


/** Sql table: WorkArea. */
typedef struct {
    utils::my_time      Change_Time;
    double              Start_X;
    double              Start_Y;
    double              End_X;
    double              End_Y;
    double              Step_Forward;
    std::vector<double> Pos_Sideways;
} WorkArea;

typedef enum {
    /** No work done here. */
    WORKMARK_EMPTY      = 0,
    /** Dredged here. */
    WORKMARK_DREDGED    = 1,
    /** OK. */
    WORKMARK_OK         = 2,
    /** Problem spot. */
    WORKMARK_PROBLEM    = 3,
    /** Number of workmarks. */
    WORKMARK_SIZE       = 4
} WORKMARK;

/** Package forwarded (proxied) by server from other ships. */
typedef struct {
    enum {
        CMRTYPE    = 0xBC11
    };
    /** ID of original message sender */
    uint8_t         ship_id;
    /**  Original message type. */
    uint16_t        original_type;
    /** Copy of original message data, allocated as big as needed */
    uint8_t         original_data_block[1];
} PACKET_FORWARD;

/** WorkArea row update message from the server.
Sent in response to:
a) "WorkAreaMark mark_no row col" from client.
b) "WorkAreaLog1 timestamp" from client.
*/
typedef struct {
    enum {
        CMRTYPE = 0xBC13
    };
    /** Latest timestamp from the given marks.

    If this is zero, it signals there are no rows newer than in request and
    the field "marks" shall be discarded.
    */
    uint32_t    latest_timestamp;

    /** Start row index. */
    uint16_t    start_row;

    /** Marks encoded row-by-row,
    each mark takes 2 bits,
    each byte encodes 4 marks - MSB first.
    There is no padding between rows. Only fully defined rows are meaningful.
    */
    uint8_t     marks[1];
} PACKET_WORKLOG_ROWS;

/** Query from client to set mark. */
typedef struct {
    enum {
        CMRTYPE = 0xBC14
    };
    /** WORKMARK. */
    uint8_t     workmark;
    /** Row index. */
    uint16_t    row_index;
    /** Column index. */
    uint16_t    col_index;
} PACKET_WORKLOG_SETMARK;

/** Query from client to send given rows */
typedef struct {
    enum {
        CMRTYPE = 0xBC15
    };
    /** Starting row index. */
    uint16_t    start_row_index;
    /** Number of rows. */
    uint8_t     nrows;
} PACKET_WORKLOG_QUERY_BY_INDICES;

/** Query to send fresh rows from the client.
Not in use, reserved.
*/
typedef struct {
    enum {
        CMRTYPE = 0xBC16
    };
    /** Minimum timestamp with which send rows. */
    uint32_t    timestamp;
} PACKET_WORKLOG_QUERY_BY_TIMESTAMP;

/** Ship and GPS information in condensed form
 *  Size of packet may be bigger than sizeof(PACKET_SHIP_INFO) because of variable length name and IMEI
*/
typedef struct {
    enum {
        CMRTYPE = 0xBC17
                  //,MINIMUM_BUILD =  ///< Minimum build number this type of packet gets forwarded
                  //,MAXIMUM_BUILD =  ///< Maximum - " -
    };
    uint8_t     ship_number;
    uint8_t     ship_name_length;
    uint8_t     imei_length;
    /** GPS offsets from ship (0,0) in cm */
    int16_t     gps1_dx;
    int16_t     gps1_dy;
    int16_t     gps2_dx;
    int16_t     gps2_dy;
    uint8_t     ship_name_imei[1];  ///< IMEI and ship name are here concatenated after each other. First comes ship name starting from byte 0 and ending at ship_name_length. IMEI comes second starting from ship_name_length+1 to ship_name_length+imei_length. No \0 bytes are stored.
} PACKET_SHIP_INFO;

/** GpsViewer build number
*/
// perhaps other programs start sending their versions some day too (gpsbase, modembox, ...)? If so then add program ID to this packet
typedef struct {
    enum {
        CMRTYPE = 0xBC18   ///< CMR type
    };
    uint32_t    build_number;    ///< build number
} PACKET_BUILD_INFO;


/// Chunk size in bytes when sending points.ipt to clients
#define     CHUNK_SIZE 120

typedef struct {
    enum {
        CMRTYPE = 0xBC19            ///< CMR type
    };
    uint16_t    number;   ///< latest points file number
    uint32_t    size;     ///< File size in bytes
} PACKET_POINTSFILE_INFO;

typedef struct {
    enum {
        CMRTYPE = 0xBC20            ///< CMR type
    };
    uint16_t    file_number;      ///< File number
    uint16_t    chunk_number;     ///< Chunk number (offset = 64*chunk_number)
} PACKET_POINTSFILE_CHUNK_REQUEST;

typedef struct {
    enum {
        CMRTYPE = 0xBC21            ///< CMR type
    };
    uint16_t    file_number;        ///< File number
    uint16_t    chunk_number;       ///< Chunk number (offset = 64*chunk_number, max file size 4MB)
    uint8_t     data[CHUNK_SIZE];           ///< chunk itself
} PACKET_POINTSFILE_CHUNK;

// Measures are in cm to avoid floating point. 2^16-1 = not set
typedef struct {
    enum {
        CMRTYPE = 0xBC22
    };
    int16_t     water_level;            ///< Water level (cm)
    uint16_t    wind_direction;         ///< Wind direction (deg, 0 at North, clockwise)
    uint16_t    wind_speed;             ///< Wind speed (cm/s)
    uint16_t    gust_speed;             ///< Gust speed (cm/s)
    uint32_t    time;                   ///< Measure time (seconds since epoch)
} PACKET_WEATHER_INFORMATION;


#define    DIGNETMESSAGE_QUERY       "?"
#define    DIGNETMESSAGE_IDENTIFY    "client"
#define    DIGNETMESSAGE_WORKAREA    "WorkArea"
#define    DIGNETMESSAGE_MODEMBOX    "modembox"
#define    DIGNETMESSAGE_STARTUPINFO "StartupInfo"
#define    DIGNETMESSAGE_VOLTAGE     "voltage"

/**
Parses DigNet message into the message name and message arguments into key-value pairs.
Format:
"name key=value key=value ..."
where string is a single string without
space and is ignored. Values and keys can be surrounded with doublequotes
(") to override spaces delimating them.
*/
void
ParseDigNetMessage(
    std::string&                            name,
    std::map<std::string, std::string>&     args,
    const std::string&                      msg
);

/** Same, but takes packet as input. */
void
ParseDigNetMessage(
    std::string&                            name,
    std::map<std::string, std::string>&     args,
    const utils::CMR&                       Packet
);

/** Get argument from the map of args, if any. */
bool
GetArg(
    const std::map<std::string, std::string>&   args,
    const std::string&                          key,
    std::string&                                value
);

/** Get argument from the map of args, if any. */
bool
GetArg(
    const std::map<std::string, std::string>&   args,
    const std::string&                          key,
    double&                                     value
);

/** Get argument from the map of args, if any. */
bool
GetArg(
    const std::map<std::string, std::string>&   args,
    const std::string&                          key,
    int&                                        value
);

/** Get argument from the map of args, if any.
Value is expected to be a list of two floating point numbers.
*/
bool
GetArg(
    const std::map<std::string, std::string>&   args,
    const std::string&                          key,
    double&                                     value1,
    double&                                     value2
);

/** Get argument from the map of args, if any.
Value is expected to be a list of floating point numbers.
*/
bool
GetArg(
    const std::map<std::string, std::string>&   args,
    const std::string&                          key,
    std::vector<double>&                        value
);

/** Get argument from the map of args, if any.
Value is expected to be in a form of my_time::ToString().
*/
bool
GetArg(
    const std::map<std::string, std::string>&   args,
    const std::string&                          key,
    utils::my_time&                             value
);

/** Get argument from the map of args, if any. */
bool
GetArg(
    const std::map<std::string, std::string>&   args,
    WorkArea&                                   workArea
);

#pragma pack(pop)

#endif //  DigNetMessages_h_
