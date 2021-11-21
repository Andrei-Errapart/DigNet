// vim: shiftwidth=4
// vim: ts=4
#ifndef DigNetDB_h_
#define DigNetDB_h_

#include <string>  // std::string

#include <utils/util.h>

#include "DigNetMessages.h"


/** Position types when saving coordinates to DB */
typedef enum {
    POSITION_TYPE_VIEWER = 'V',  ///< Sent by GpsViewer
    POSITION_TYPE_MIXGPS = 'M'   ///< Calculated with MixGPS using GPS data
} POSITION_TYPE;

/** GpsServer database. */
class DigNetDB {
    private:
        void* mysql_;                       ///< MySql connection
        utils::Option<WorkArea>  Area;
    private:
        bool
        refreshArea();
    public:
        /** Connects to the localhost MySQL database.
         */
        DigNetDB(
            const std::string& database,
            const std::string& username,
            const std::string& password);

        ~DigNetDB();

        /** Every time ships sends something it will update Contact_Time
         */
        void 
        updateContactTime(
            const int shipId);

        /** Every time ship GPS sends something it will update GPS information
         */
        void 
        updateGps(
            const int       shipId, 
            const int       gpsNum, 
            const double    x,
            const double    y);

        /** Every time ships send heading information it will update it in DB
         */
        void 
        updateShipHeading(
            const int       shipId, 
            const double    heading);

        /** Adds a new ship position of ship to database
        \param[in] shipId ID of ship whose coordinates are saved
        \param[in] type type of coordinates (viewer or mixgps. First doesn't have Z coordinate)
        \param[in] x X coordinate in meters
        \param[in] y Y coordinate in meters
        \param[in] z Z coordinate in meters 0.0 for viewer and isn't saved to DB
        \param[in] heading Ship heading in degrees [0..360[
        \param[in] speed ship speed in km/h. Not set if info comes from GpsViewer
         */
        void
        addShipPosition(
            const int                   shipId,
            const POSITION_TYPE         type,
            const double                x,
            const double                y,
            const double                z,
            const double                heading,
            const utils::Option<double> speed = utils::Option<double>());

        /** Adds a new GPS position to database
        \param[in] shipId ID of ship whose GPS coordinates are saved
        \param[in] type type of packet (GPS1 = 0, GPS2 = 1)
        \param[in] x X coordinate in meters
        \param[in] y Y coordinate in meters
        \param[in] z Z coordinate in meters
        \param[in] status GPS signal status (no fix = 0, fix = 1, differential = 2)
         */
        void
        addGpsPosition(
            const int       shipId,
            const int       type,
            const double    x,
            const double    y,
            const double    z,
            const int       status);

        /** Saves ship shadow position
        \param[in] shipId ID of ship
        \param[in] x shadow X coordinate
        \param[in] y shadow Y coordinate
        \param[in] dir shadow direction in degrees
        */
        void
        saveShadow(
            const int       shipId,
            const double    x,
            const double    y,
            const double    dir);
        
        /** Returns ship shadow information in parameters
        \param[in] shipId ID of ship
        \param[out] x shadow X coordinate
        \param[out] y shadow X coordinate
        \param[out] dir shadow direction
         */
        bool
        getShadow(
            const int   shipId,
            double&     x,
            double&     y,
            double&     dir);
        
        /** Returns last known ship position.
        \param[in] shipId ID of ship
        \param[out] x ship X coordinate
        \param[out] y ship Y coordinate
        \param[out] heading ship heading
        */
        bool
        getShipPosition(
            const int           shipId,
            double&             x,
            double&             y,
            double&             heading);

        /** Returns information about ship voltages
        \param[in] shipId ID of ship
        \param[out] lastReading latest data from DB
        \param[out] minimum minimum safe voltage
        \param[out] secondsFromLastRead time since last information was received
        */
        bool
        getShipVoltageInfo(
            const int           shipId,
            double&             lastReading,
            double&             minimum,
            int&                secondsFromLastRead,
            utils::my_time&     timeLastRead);
        
        /** If there is no ship with given IMEI it returns false. If it exists function returns the ship ID, name and other needed information
         */
        bool 
        registerShip(
            int&                shipId,
            int&                groupId,
            std::string&        name,
            double&             gps1_dx,
            double&             gps1_dy,
            double&             gps2_dx,
            double&             gps2_dy,
            double&             supplyVoltageLimit,
            const std::string&  imei,
            const std::string&  firmwareName,
            const std::string&  firmwareVersion,
            const std::string&  firmwareBuildDate,
            const std::string&  phone
        );
        /** Reads given ship GPS offsets from DB
        */
        bool 
        getShipGpsOffsets(
            const int   shipId,
            double&     gps1_dx,
            double&     gps1_dy,
            double&     gps2_dx,
            double&     gps2_dy
        );
        /** Returns true if there is a ship with given ID registered in DB
        */
        bool 
        shipExists(
            const int shipId);

        /** Query first workarea in the table. */
        bool
        queryWorkArea(
            WorkArea& workArea);

        /** Logs supply voltage sent by modembox.
        \param[in] shipId Ship identifier.
        \param[in] voltage Voltage, volts.
        */
        void
        logSupplyVoltage(
            const int  shipId,
            const double voltage
        );

        /** Query by worklog indices.
        \param[in] queryPacket Query, gives starting row and number of rows.
        \param[out] responseBuffer Buffer for response packet.
        \param[out] responsePacket Pointer to the response packet.
         */
        bool
        executeWorklogQueryByIndices(
            const PACKET_WORKLOG_QUERY_BY_INDICES&  queryPacket,
            std::vector<unsigned char>&             responseBuffer,
            PACKET_WORKLOG_ROWS*&                   responsePacket
        );

        /** Set the worklog mark and prepare a response.
        \param[in] queryPacket Query, gives starting row and number of rows.
        \param[out] responseBuffer Buffer for response packet.
        \param[out] responsePacket Pointer to the response packet.
        */
        bool
        executeWorklogSetmark(
            const PACKET_WORKLOG_SETMARK&   queryPacket,
            const unsigned int              shipId,
            std::vector<unsigned char>&     responseBuffer,
            PACKET_WORKLOG_ROWS*&           responsePacket
        );
}; // class DigNetDB

#endif /* DigNetDB_h_ */

