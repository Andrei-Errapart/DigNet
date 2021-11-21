// vim: shiftwidth=4
// vim: ts=4

#include <stdexcept>
#include <vector>
#include <string>

#include <iostream> // cout, ofstream

#include <utils/util.h>
#include <utils/hxio.h>
#include <utils/math.h>

#include <mysql.h>

#include <unistd.h>
#include <assert.h>

#include "DigNetDB.h"

using namespace std;
using namespace utils;

/*****************************************************************************/
static int
do_query(
        void*           mysql,
        const string&   query)
{
    //cout<<"Executing query: \""<<query<<"\""<<endl;
    const int   r = mysql_real_query(reinterpret_cast<MYSQL*>(mysql), query.c_str(), query.size());
    // cout << "MySQL result:" << r << endl << flush;
    return r;
}

/*****************************************************************************/
/**
Query just 1 row from the database. FIXME: error handling.
*/
static bool
query1row(
        void*           mysql,
        const string&   query,
        vector<string>& row)
{
    //cout<<"Executing query: \""<<query<<"\""<<endl;
    bool    r = false;
    MYSQL*  sql = reinterpret_cast<MYSQL*>(mysql);
    if (do_query(mysql, query) == 0) {
        MYSQL_RES*  result = mysql_store_result(sql);
        if (result == 0) {
            throw runtime_error(ssprintf("query1row: Mysql misbehaved on query %s", query.c_str()));
        } else {
            MYSQL_ROW   mysql_row = mysql_fetch_row(result);
            if (mysql_row != 0) {
                const int   ncols = mysql_num_fields(result);
                row.resize(ncols);
                for (unsigned int i = 0; i < row.size(); ++i) {
                    row[i] = mysql_row[i];
                }
                r = true;
            }
            mysql_free_result(result);
        }
    } else {
        // bad error.
        throw runtime_error(ssprintf("query1row: Invalid query \"%s\" or smth.", query.c_str()));
    }
    // unsuccessful.
    return r;
}

/*****************************************************************************/
/** Query a table from the MySQL.

Throws exceptions on errors.
*/
static int
querytable(
        void*                                   mysql,
        const std::string&                      query,
        std::vector<std::vector<std::string> >& results)
{
    MYSQL*  sql = reinterpret_cast<MYSQL*>(mysql);
    // Execute the query.
    if (do_query(mysql, query) == 0) {
        results.resize(0);
        MYSQL_RES*  result = mysql_store_result(sql);
        if (result == 0) {
            // There were no results.
            return mysql_affected_rows(sql);
        } else {
            // Fetch the results.
            const int   ncols = mysql_num_fields(result);
            MYSQL_ROW   mysql_row;
            while ((mysql_row = mysql_fetch_row(result)) != 0) {
                results.resize(results.size() + 1);
                vector<string>& row = results[results.size()-1];
                row.resize(ncols);
                for (unsigned int i = 0; i < row.size(); ++i) {
                    const char* cell = mysql_row[i];
                    row[i] = cell == 0 ? "" : cell;
                }
            }
            mysql_free_result(result);
            return 0;
        }
    } else {
        // bad error.
        throw runtime_error(ssprintf("query1table: Invalid query \"%s\" or smth.", query.c_str()));
    }
    assert(0);
    return -1;
}
/*****************************************************************************/
/** Execute an insert query.

  \return ID of the inserted row.

  Throws exceptions on errors.
*/
static int
execute_insert(
        void*           mysql,
        const string&   query)
{
    MYSQL*  sql = reinterpret_cast<MYSQL*>(mysql);

    // Execute the query.
    if (do_query(mysql, query) == 0) {
        // Get the ID.
        return mysql_insert_id(sql);
    } else {
        // bad error.
        throw runtime_error(ssprintf("execute_insert: Invalid query \"%s\" or smth.", query.c_str()));
    }
}

/*****************************************************************************/
typedef enum {
    UPDATE_NULL_THROW,
    UPDATE_NULL_OK
} UPDATE_FLAGS;

/**
Update just 1 row.
*/
static bool
update1row(
        void*               mysql,
        const string&       query,
        const UPDATE_FLAGS  flags = UPDATE_NULL_THROW)
{
    //cout<<"update1row query: \""<<query<<"\""<<endl;
    MYSQL*  sql = reinterpret_cast<MYSQL*>(mysql);
    if (do_query(mysql, query) == 0) {
        const int   nrows = mysql_affected_rows(sql);
        if (flags == UPDATE_NULL_THROW ? (nrows != 1) : (nrows != 1 && nrows != 0)) {
            throw runtime_error(ssprintf("update1row: Updated %d rows instead of just 1, query: %s", nrows, query.c_str()));
        }
        return true;
    } else {
        throw runtime_error(ssprintf("update1row: Invalid query \"%s\"or smth.", query.c_str()));
    }
    return false;
}

/*****************************************************************************/
static string
escape_string(
        void*           mysql,
        const string&   s)
{
    MYSQL*  sql = reinterpret_cast<MYSQL*>(mysql);
    string  buffer;

    buffer.resize(2*s.size() + 1);
    const unsigned long l = mysql_real_escape_string(sql, &buffer[0], &s[0], s.size());
    buffer.resize(l);

    return buffer;
}

/*****************************************************************************/
bool
DigNetDB::refreshArea()
{
    WorkArea tmp;
    bool  r = false;

    string query("SELECT "
                 "Change_Time, "
                 "Start_X, "
                 "Start_Y, "
                 "End_X, "
                 "End_Y, "
                 "Step_Forward, "
                 "Pos_Sideways "
                 "FROM WorkArea "
                 "LIMIT 1");

    try {
        vector<string> result;
        vector<string> vsideways;
        if (query1row(mysql_, query, result)) {
            tmp.Change_Time = my_time_of_mysql(result[0]);
            r =
                double_of(result[1], tmp.Start_X) &&
                double_of(result[2], tmp.Start_Y) &&
                double_of(result[3], tmp.End_X) &&
                double_of(result[4], tmp.End_Y) &&
                double_of(result[5], tmp.Step_Forward);
            split(result[6], ";",  vsideways);
            for (unsigned int i = 0; i < vsideways.size(); ++i) {
                double f = 0;
                r = r && double_of(vsideways[i], f);
                tmp.Pos_Sideways.push_back(f);
            }
            if (r) {
                Area = tmp;
            }
        }
    } catch (const exception& e) {
        cout << "Error querying WorkArea : %s" << e.what() << endl;
    }

    return r;
}

/*****************************************************************************/
DigNetDB::DigNetDB(
        const std::string&  database,
        const std::string&  username,
        const std::string&  password
) :
        mysql_(0)
{
    mysql_ = mysql_init(0);
    MYSQL* h = reinterpret_cast<MYSQL*>(mysql_);

    if (mysql_ == 0) {
        // mysql_library_end();
        throw runtime_error("MySQL library error.");
    }

    if (mysql_real_connect(h, "localhost", username.c_str(), password.c_str(), database.c_str(), 0, 0, 0) == 0) {
        string s(ssprintf(
                     "Unable to connect MySQL server, tried %s:%s@%s. Error was: %s",
                     username.c_str(), password.c_str(), database.c_str(), mysql_error(h)
                 ));
        mysql_close(h);
        mysql_ = 0;
        // mysql_library_end();
        throw runtime_error(s.c_str());
    }
}

/*****************************************************************************/
DigNetDB::~DigNetDB()
{
    if (mysql_ != 0) {
        mysql_close(reinterpret_cast<MYSQL*>(mysql_));
        mysql_ = 0;
        // mysql_library_end();
    }
}

/*****************************************************************************/
void
DigNetDB::updateContactTime(
        const int shipId)
{
    std::string query;
    query = ssprintf("update Ships set Contact_Time = now() where id = %d", shipId);
    update1row(mysql_, query, UPDATE_NULL_OK);
}

/*****************************************************************************/
void
DigNetDB::updateGps(
        const int shipId,
        const int gpsNum,
        const double x,
        const double y)
{
    std::string query;
    query = ssprintf("update Ships set GPS%d_Time = now(), GPS%d_X = %f, GPS%d_Y = %f where id = %d", gpsNum, gpsNum, x, gpsNum, y, shipId);
    //cout<<"Updating GPS: \""<<query<<"\""<<endl;
    update1row(mysql_, query, UPDATE_NULL_OK);
}


/*****************************************************************************/
void
DigNetDB::updateShipHeading(
        const int shipId,
        const double heading)
{
    std::string query = ssprintf("update Ships set Heading_Time = now(), Heading = %f where id = %d", heading, shipId);
    update1row(mysql_, query, UPDATE_NULL_OK);
}

/*****************************************************************************/
void
DigNetDB::addShipPosition(
        const int           shipId,
        const POSITION_TYPE type,
        const double        x,
        const double        y,
        const double        z,
        const double        heading,
        const utils::Option<double>    speed)
{
    std::string insertQuery(ssprintf("insert into ShipPosition set Create_Time = now(), ShipId = %d, type = '%c', x = %f, y = %f, heading = %f", shipId, type, x, y, heading));
    if (speed.IsValid()){
        std::string speedStr(ssprintf(", speed = %f", speed()));
        insertQuery.append(speedStr);
    }
    if (type != POSITION_TYPE_VIEWER){
        std::string zStr(ssprintf(", z = %f", z));
        insertQuery.append(zStr);
    }
    try {
        execute_insert(mysql_, insertQuery);
    } catch (const runtime_error& e){
        cout<<"Error adding ship position: "<<e.what()<<endl;
    }

}

/*****************************************************************************/
void
DigNetDB::addGpsPosition(
        const int       shipId,
        const int       type,
        const double    x,
        const double    y,
        const double    z,
        const int       status)
{
    std::string insertQuery = ssprintf("insert into ShipPosition set Create_Time = now(), ShipId = %d, type = '%c', x = %f, y = %f, z = %f, GpsStatus = %d",
                                        shipId, '1'+type, x, y, z, status);
    try {
        execute_insert(mysql_, insertQuery);
    } catch (const runtime_error& e){
        cout<<"Error adding GPS position: "<<e.what()<<endl;
    }
}

/*****************************************************************************/

void
DigNetDB::saveShadow(
        const int       shipId,
        const double    x,
        const double    y,
        const double    dir)
{
    std::string query(ssprintf("update Ships set Shadow_X = %f, Shadow_Y= %f, Shadow_Dir = %f where id = %d", x, y, dir, shipId));
    update1row(mysql_, query, UPDATE_NULL_OK);
}

bool
DigNetDB::getShadow(
        const int   shipId,
        double&     x,
        double&     y,
        double&     dir)
{
    std::string query(ssprintf("select Shadow_X, Shadow_Y, Shadow_Dir from Ships where id = %d", shipId));
    vector<string> result;
    if (!query1row(mysql_, query, result)) {
        return false;
    }
    const double sx = double_of(result[0]);
    if (sx < 0){
        return false;
    }
    x = sx;
    y = double_of(result[1]);
    dir = double_of(result[0]);
    return true;
}

/*****************************************************************************/

bool
DigNetDB::getShipPosition(
        const int           shipId,
        double&             x,
        double&             y,
        double&             heading)
{
    string query = ssprintf("select Gps1_X, Gps1_y, Heading from Ships where ID = %d", shipId);
    vector<string> result;
    if (!query1row(mysql_, query, result)) {
        return false;
    }
    x = double_of(result[0]);
    y = double_of(result[1]);
    heading = double_of(result[2]);
    return true;
}

/*****************************************************************************/
bool
DigNetDB::getShipVoltageInfo(
        const int           shipId,
        double&             lastReading,
        double&             minimum,
        int&                secondsFromLastRead,
        my_time&        timeLastRead)
{
    bool r = false;
    string query = ssprintf(
		"SELECT "
			"now() - v.Reading_Time, "
			"v.Reading_Time, "
			"v.SupplyVoltage, "
			"s.SupplyVoltageLimit "
		"FROM "
			"SupplyVoltages AS v, Ships AS s "
		"WHERE "
			"s.id = %d AND v.ShipId = s.id "
		"ORDER BY v.Reading_Time DESC LIMIT 1", shipId);
    try {
        vector<string> result;
        if (query1row(mysql_, query, result)) {
            secondsFromLastRead = int_of(result[0]);
            timeLastRead = my_time_of_mysql(result[1]);
            lastReading = double_of(result[2]);
            minimum = double_of(result[3]);
            r = true;
        }
    } catch (const exception& e) {
        cout << "Error reading last voltage: %s" << e.what() << endl;
    }
    return r;
}
        
/*****************************************************************************/
bool
DigNetDB::registerShip(
        int&            shipId,
        int&            groupId,
        string&         name,
        double&         gps1_dx,
        double&         gps1_dy,
        double&         gps2_dx,
        double&         gps2_dy,
        double&         supplyVoltageLimit,
        const string&   imei,
        const string&   firmwareName,
        const string&   firmwareVersion,
        const string&   firmwareBuildDate,
        const string&   phone
)
{
    try {
        string firmwareNameBuf(escape_string(mysql_, firmwareName));
        string imeiBuf(escape_string(mysql_, imei));
        string firmwareVersionBuf(escape_string(mysql_, firmwareVersion));
        string firmwareDateBuf(escape_string(mysql_, firmwareBuildDate));
        // FIXME: Get this from somewhere
        string phoneNumberBuf(escape_string(mysql_, "12345678"));
        string nameBuf(escape_string(mysql_, name));
        string query = ssprintf("select id, GroupId, name, IMEI, Firmware_Name, Firmware_Version, Firmware_Build_date, Gps1_dx, Gps1_dy, Gps2_dx, Gps2_dy, SupplyVoltageLimit  from Ships where IMEI = '%s'", imeiBuf.c_str());
        vector<string> result;
        if (!query1row(mysql_, query, result)) {
            // Ship doesn't exist, abort
            return false;
#if (0)
            string insertQuery = ssprintf("insert into Ships SET SimNumber='', ShipType = '', IMEI = '%s', Firmware_Name = '%s', Firmware_Version = '%s', Firmware_Build_date = '%s', Phone_no = '%s', Name = '%s', Contact_Time = now()",
                                          imeiBuf.c_str(),
                                          firmwareNameBuf.c_str(),
                                          firmwareVersionBuf.c_str(),
                                          firmwareDateBuf.c_str(),
                                          phoneNumberBuf.c_str(),
                                          nameBuf.c_str());
            execute_insert(mysql_, insertQuery);
            // Re-fetch to get ID
            query1row(mysql_, query, result);
#endif
        }
        shipId = int_of(result[0]);
        groupId = int_of(result[1]);
        name = result[2];
        gps1_dx = double_of(result[7]);
        gps1_dy = double_of(result[8]);
        gps2_dx = double_of(result[9]);
        gps2_dy = double_of(result[10]);
        supplyVoltageLimit = double_of(result[11]);
        // If firmwarersion differs then update
        if (result[5].compare(firmwareVersion) != 0) {
            query = ssprintf("update Ships set Firmware_Name = '%s', Firmware_Version = '%s', Firmware_Build_date = '%s' where id = %d",
                             firmwareNameBuf.c_str(), firmwareVersionBuf.c_str(), firmwareDateBuf.c_str(), shipId);
            update1row(mysql_, query);
        }
        // Ship exists in DB
    } catch (const exception& e) {
        cout << "Can't register ship: " << e.what() << endl;
        return false;
    }
    return true;
}

/*****************************************************************************/
bool
DigNetDB::getShipGpsOffsets(
        const int   shipId,
        double&     gps1_dx,
        double&     gps1_dy,
        double&     gps2_dx,
        double&     gps2_dy
)
{
    bool r = false;
    string query = ssprintf("select Gps1_dx, Gps1_dy, Gps2_dx, Gps2_dy from Ships where id = %d", shipId);
    try {
        vector<string> result;
        if (query1row(mysql_, query, result)) {
            gps1_dx = double_of(result[0]);
            gps1_dy = double_of(result[1]);
            gps2_dx = double_of(result[2]);
            gps2_dy = double_of(result[3]);
            r = true;
        }
    } catch (const exception& e) {
        cout << "Error reading offsets: %s" << e.what() << endl;
    }
    return r;
}

/*****************************************************************************/
bool
DigNetDB::shipExists(
        const int shipId)
{
    bool r = false;
    string query(ssprintf("select id from Ships where id = %d", shipId));
    try {
        vector<string> result;
        if (query1row(mysql_, query, result)) {
            r = true;
        }
    } catch (const exception& e) {
        cout << "Error checking for ship existance: %s" << e.what() << endl;
    }
    return r;
}

/*****************************************************************************/
bool
DigNetDB::queryWorkArea(
       WorkArea& workArea)
{
    if (refreshArea()) {
        workArea = Area();
        return true;
    } else {
        if (Area.IsValid()) {
            cout << "queryWorkArea: query failed, responding with old WorkArea." << endl;
            return true;
        } else {
            return false;
        }
    }
}

/*****************************************************************************/
void
DigNetDB::logSupplyVoltage(
        const int    shipId,
        const double voltage)
{
    std::string insertQuery;
    ssprintf(insertQuery,
             "INSERT INTO SupplyVoltages SET ShipID = %d, Reading_Time = now(), SupplyVoltage = %f",
             shipId, voltage);
    execute_insert(mysql_, insertQuery);
}

/*****************************************************************************/
bool
DigNetDB::executeWorklogQueryByIndices(
        const PACKET_WORKLOG_QUERY_BY_INDICES&  queryPacket,
        std::vector<unsigned char>&             responseBuffer,
        PACKET_WORKLOG_ROWS*&                   responsePacket
)
{
    const unsigned int BITS_IN_MARK = 2;
    refreshArea();
    if (Area.IsValid() && queryPacket.nrows > 0) {
        const WorkArea wa = Area();
        const double        length = sqrt(sqr(wa.Start_X - wa.End_X) + sqr(wa.Start_Y - wa.End_Y));
        const int  nrows = utils::round(length / wa.Step_Forward);
        const int  ncols = static_cast<int>(wa.Pos_Sideways.size()) - 1;

        if (nrows > 0 && ncols > 0) {
            try {
                // 1. Fetch the current worklog.
                TNT::Array2D<unsigned int>    ids(nrows, ncols);
                TNT::Array2D<WORKMARK>     worklog(nrows, ncols);
                {
                    for (int i = 0; i < nrows; ++i) {
                      for (int j = 0; j < ncols; ++j) {
                            ids[i][j] = 0;
                            worklog[i][j] = WORKMARK_EMPTY;
                        }
                    }
                    std::vector<std::vector<std::string> > results;
                    querytable(
                        mysql_,
                        "SELECT id, Forward_Index,Sideways_Index,Mark FROM WorkLog ORDER BY id ASC",
                        results);
                    for (unsigned int i = 0; i < results.size(); ++i) {
                        const std::vector<std::string>& row = results[i];
                        int row_index = 0;
                        int col_index = 0;
                        int mark = 0;
                        int id = 0;
                        if (int_of(row[0], id), int_of(row[1], row_index) && int_of(row[2], col_index) && int_of(row[3], mark)) {
                            if (row_index >= 0 && row_index < nrows && col_index >= 0 && col_index < ncols && mark >= 0 && mark < WORKMARK_SIZE) {
                                worklog[row_index][col_index] = static_cast<WORKMARK>(mark);
                                ids[row_index][col_index] = id;
                            }
                        }
                    }
                }

                // 2. Fill responseBuffer & responsePacket.
                const int nbits = queryPacket.nrows * ncols * BITS_IN_MARK;
                const int nbytes = (nbits + 7) / 8;
                responseBuffer.resize(sizeof(PACKET_WORKLOG_ROWS) + nbytes - 1);
                for (unsigned int i = 0; i < responseBuffer.size(); ++i) {
                    responseBuffer[i] = 0;
                }
                responsePacket = reinterpret_cast<PACKET_WORKLOG_ROWS*>(&responseBuffer[0]);

                unsigned int max_id = 0;
                unsigned int total_bit_index = 0;
                for (unsigned int i = 0; i < queryPacket.nrows; ++i) {
                    const int row_index = queryPacket.start_row_index + i;
                    if (row_index >= nrows) {
                        // skip bad rows.
                        continue;
                    }
                    for (int col_index = 0; col_index < ncols; ++col_index) {
                        const unsigned int byte_index = total_bit_index / 8;
                        const unsigned int bit_shift = (8 - BITS_IN_MARK) - (total_bit_index & 0x07);
                        const unsigned int mark = worklog[row_index][col_index];
                        uint8_t&   b = responsePacket->marks[byte_index];

                        b |= mark << bit_shift;

                        total_bit_index += BITS_IN_MARK;
                        max_id = mymax<unsigned int>(ids[row_index][col_index], max_id);
                    }
                }
                responsePacket->latest_timestamp = max_id;
                responsePacket->start_row = queryPacket.start_row_index;

                return true;
            } catch (const std::exception& e) {
                cout << "DigNetDB::executeWorklogQueryByIndices: error: << " << e.what() << endl;
            }
        }
    } else {
        cout << "executeWorklogQueryByIndices: unable to continue without working area." << endl;
        return false;
    }
    return false;
}

/*****************************************************************************/
bool
DigNetDB::executeWorklogSetmark(
        const PACKET_WORKLOG_SETMARK&   queryPacket,
        const unsigned int              shipId,
        std::vector<unsigned char>&     responseBuffer,
        PACKET_WORKLOG_ROWS*&           responsePacket
)
{
    try {
        // 1. Insert the packet.
        execute_insert(
            mysql_,
            ssprintf("INSERT INTO WorkLog "
                     "(Change_Time, Ship_Id, Forward_Index, Sideways_Index, Mark) "
                     "VALUES (now(), %d, %d, %d, %d)",
                     shipId, (int)queryPacket.row_index, (int)queryPacket.col_index, (int)queryPacket.workmark));

        // 2. Return response.
        PACKET_WORKLOG_QUERY_BY_INDICES qpacket;
        qpacket.start_row_index = queryPacket.row_index;
        qpacket.nrows = 1;
        const bool r = executeWorklogQueryByIndices(qpacket, responseBuffer, responsePacket);
        return r;
    } catch (const std::exception& e) {
        cout << "DigNetDB::executeWorklogSetmark: error: << " << e.what() << endl;
    }
    return false;
}

