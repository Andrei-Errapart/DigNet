#include <ctype.h>  // isdigit

#include <utils/Config.h>           // FileConfig
#include <utils/util.h>             // ssprintf and friends.
#include <utils/Transformations.h>  // gps_of_local.
#include <utils/NMEA.h>
#include <utils/math.h>
#include <DigNetMessages.h>

#include <fxutils/fxutil.h>

#include "globals.h"
#include "ModemBoxSimulator.h"
#include "Joystick.h"

static const char*  CONFIG_FILENAME = "ModemBoxSimulator.ini";

using namespace utils;
using namespace fxutils;

// 1 distance slider unit = 10cm.
static const double SLIDER2METERS   = 0.1;

// Speed from knots to m/s
static const double KN2MS = 1.0/1.852;
/* ------------------------------------------------------------- */
enum {
	TIMEOUT_POLL_100HZ = 10,
	TIMEOUT_POLL_10HZ = 100,
	TIMEOUT_POLL_GPS = 1000,

    /** Server connection ping period, milliseconds. */
    SERVER_PING_PERIOD = 10 * 1000,
    /** Server connection timeout, milliseconds. */
    SERVER_PING_TIMEOUT = 6 * SERVER_PING_PERIOD,
    /// Bluetooth timeout, milliseconds.
    BLUETOOTH_TIMEOUT = 120 * 1000,
};

/* ------------------------------------------------------------- */
template <class FXType>
std::string
string_of_fx(   const FXType*   fxt)
{
    return string_of_fxstring(fxt->getText());
}

PacketInfo::PacketInfo(
        const bool              logged,
        const int               code,
        const std::string&      name):
    logged(logged)
    ,code(code)
    ,name(name)
{
}


template <class Key, class Val>
void add_to_map(std::map<Key, Val>& map, Key key, Val val)
{
    std::pair<Key, Val> pair(key, val);
    map.insert(pair);
}

void 
add_packet(
    std::map<int, PacketInfo>&  packets, 
    const bool                  logged, 
    const int                   code, 
    const std::string&          name)
{
    add_to_map<int, PacketInfo>(packets, code, PacketInfo(logged, code, name));
}


void 
init_packets(   std::map<int, PacketInfo>& packets)
{
    add_packet(packets, false,  CMRTYPE_GPS_OFFSETS,        "GPS offsets");
    add_packet(packets, false,  CMRTYPE_GPS_OFFSETS,        "GPS offsets");
    add_packet(packets, false,  CMRTYPE_SHIP_ID,            "Ship ID");
    add_packet(packets, false,  CMRTYPE_GPS_POSITION,       "GPS position");
    add_packet(packets, false,  CMRTYPE_SHIP_POSITION,      "Ship position");
    add_packet(packets, false,  CMRTYPE_REQUEST_FROM_SHIP,  "Request from ship");
    add_packet(packets, false,  CMRTYPE_SHIP_INFO,          "Ship info (old)");
    add_packet(packets, false,  CMR::DIGNET,                "DigNet");
    add_packet(packets, false,  CMR::SERIAL,                "Serial");
    add_packet(packets, false,  CMR::PING,                  "Ping");

    add_packet(packets, false,  PACKET_FORWARD::CMRTYPE,                    "Forward");
    add_packet(packets, false,  PACKET_WORKLOG_ROWS::CMRTYPE,               "Worklog rows");
    add_packet(packets, false,  PACKET_WORKLOG_SETMARK::CMRTYPE,            "Worklog set mark");
    add_packet(packets, false,  PACKET_WORKLOG_QUERY_BY_INDICES::CMRTYPE,   "Worklog query by indices");
    add_packet(packets, false,  PACKET_WORKLOG_QUERY_BY_TIMESTAMP::CMRTYPE, "Worklog query by timestamp");
    add_packet(packets, false,  PACKET_SHIP_INFO::CMRTYPE,                  "Ship info");
    add_packet(packets, false,  PACKET_BUILD_INFO::CMRTYPE,                 "Build info");
    add_packet(packets, false,  PACKET_POINTSFILE_INFO::CMRTYPE,            "Pointfile info");
    add_packet(packets, false,  PACKET_POINTSFILE_CHUNK_REQUEST::CMRTYPE,   "Pointfile chunk request");
    add_packet(packets, false,  PACKET_POINTSFILE_CHUNK::CMRTYPE,           "Pointfile chunk");
    add_packet(packets, false,  PACKET_WEATHER_INFORMATION::CMRTYPE,        "Weather information");
}

/* ------------------------------------------------------------- */
// Message Map for the Main Window class
FXDEFMAP(ModemBoxSimulator) ModemBoxSimulatorMap[]={
	//________Message_Type_____________________ID____________Message_Handler_______
	FXMAPFUNC(SEL_TIMEOUT,  ModemBoxSimulator::ID_POLL_100HZ,       ModemBoxSimulator::onGenerateRandomCmr),
	FXMAPFUNC(SEL_COMMAND,  ModemBoxSimulator::ID_SELECT_PROFILE,	ModemBoxSimulator::onCmdSelectProfile),
	FXMAPFUNC(SEL_COMMAND,  ModemBoxSimulator::ID_SAVE_PROFILE,     ModemBoxSimulator::onCmdSaveProfile),
	FXMAPFUNC(SEL_COMMAND,  ModemBoxSimulator::ID_CLEAR_LOG,        ModemBoxSimulator::onCmdClearLog),
	FXMAPFUNC(SEL_TIMEOUT,  ModemBoxSimulator::ID_POLL_10HZ,        ModemBoxSimulator::onTimerPoll10HZ),
	FXMAPFUNC(SEL_TIMEOUT,  ModemBoxSimulator::ID_POLL_GPS,         ModemBoxSimulator::onTimerPollGps),
	FXMAPFUNC(SEL_CHANGED,  ModemBoxSimulator::ID_GPS_DISTANCE,     ModemBoxSimulator::onChangedGpsDistance),
	FXMAPFUNC(SEL_CHANGED,  ModemBoxSimulator::ID_GPS_HEADING,      ModemBoxSimulator::onChangedGpsHeading),
	FXMAPFUNC(SEL_COMMAND,  ModemBoxSimulator::ID_GPS_HEADING,      ModemBoxSimulator::onChangedGpsHeading),// For direction buttons
    FXMAPFUNC(SEL_COMMAND,  ModemBoxSimulator::ID_RANDOM_CMR,       ModemBoxSimulator::onToggledGenerateRandomCmr),
    FXMAPFUNC(SEL_COMMAND,  ModemBoxSimulator::ID_SEND_CUSTOM_GPS,  ModemBoxSimulator::onSendCustomGpsLine),
    FXMAPFUNC(SEL_CHANGED,  ModemBoxSimulator::ID_MOVEMENT,         ModemBoxSimulator::onChangedSpeed),
    FXMAPFUNC(SEL_COMMAND,  ModemBoxSimulator::ID_MOVEMENT,         ModemBoxSimulator::onChangedSpeed), // For speed buttons
    FXMAPFUNC(SEL_COMMAND,  ModemBoxSimulator::ID_LOGGING_OPTIONS,  ModemBoxSimulator::onCmdShowLoggingOptions),
};

FXIMPLEMENT(ModemBoxSimulator, ModemBoxSimulator::inherited, ModemBoxSimulatorMap,ARRAYNUMBER(ModemBoxSimulatorMap));

/* ------------------------------------------------------------- */


FXDEFMAP(LogOptionsDialog) LogOptionsDialogMap[]={
    FXMAPFUNC(SEL_COMMAND,  LogOptionsDialog::ID_CHECK_PRESSED,  LogOptionsDialog::onChekbuttonPress),
};

FXIMPLEMENT(LogOptionsDialog, FXDialogBox, LogOptionsDialogMap, ARRAYNUMBER(LogOptionsDialogMap));

// Construct logging options dialog
LogOptionsDialog::LogOptionsDialog(FXWindow* owner, std::map<int, PacketInfo>& packets):
    FXDialogBox(owner,"Test of Dialog Box",DECOR_TITLE|DECOR_BORDER)
{

    // Bottom buttons
    buttons=new FXHorizontalFrame(this,LAYOUT_SIDE_BOTTOM|FRAME_NONE|LAYOUT_FILL_X|PACK_UNIFORM_WIDTH,0,0,0,0,40,40,20,20);

    // Separator
    new FXHorizontalSeparator(this,LAYOUT_SIDE_BOTTOM|LAYOUT_FILL_X|SEPARATOR_GROOVE);

    // Contents
    contents=new FXHorizontalFrame(this,LAYOUT_SIDE_TOP|FRAME_NONE|LAYOUT_FILL_X|LAYOUT_FILL_Y|PACK_UNIFORM_WIDTH);
    

    // Buttons
    FXMatrix*   mtx = new FXMatrix(contents, 2, MATRIX_BY_COLUMNS|LAYOUT_FILL_X);
    std::map<int, PacketInfo>::iterator iter;
    for( iter = packets.begin(); iter != packets.end(); iter++ ) {
        FXCheckButton* b =  new FXCheckButton(mtx, iter->second.name.c_str(), this, ID_CHECK_PRESSED);
        add_to_map<FXCheckButton*, int>(buttons_, b, iter->first);
        add_to_map<int, PacketInfo*>(packets_, iter->first, &iter->second);
    }
    // Accept
    new FXButton(buttons,"&Ok",NULL,this,ID_ACCEPT,BUTTON_DEFAULT|BUTTON_INITIAL|FRAME_RAISED|FRAME_THICK|LAYOUT_RIGHT|LAYOUT_CENTER_Y);
}

long
LogOptionsDialog::onChekbuttonPress(FXObject* obj, FXSelector, void* ptr)
{
    FXCheckButton* b = reinterpret_cast<FXCheckButton*>(obj);
    ButtonMap::iterator fxb = buttons_.find(b);
    if (fxb != buttons_.end()){
        PacketMap::iterator packet = packets_.find(fxb->second);
        if (packet != packets_.end()){
            packet->second->logged = b->getCheck() == TRUE;
        }
    }
    return 1;
}

ModemBoxSimulator::ModemBoxSimulator(
    FXApp*  a,
    int     argc,
    char**  argv
)
:   inherited(a, "ModemBox Simulator", NULL, NULL, DECOR_ALL, 0, 0, 800,600)
    ,argc_(argc)
    ,argv_(argv)
    // ModemBox variables.
    ,received_ping_time(-1)
    ,SupplyVoltageTime(-1)
    ,GpsPositionTime(-1)
    // Communication...
    ,commServerStatus_(utils::TcpPollingReader::STATUS_DISCONNECTED)
    ,commBluetoothConnected_(false)
    ,commBluetoothTime_(System::currentTimeMillis())
{
    init_packets(shownPackets_);
    FXVerticalFrame*    fmain = new FXVerticalFrame(this,LAYOUT_SIDE_TOP|LAYOUT_FILL_X|LAYOUT_FILL_Y,0,0,0,0, 0,0,0,0);
	FXHorizontalFrame*	fgroups = new FXHorizontalFrame(fmain,LAYOUT_SIDE_TOP|LAYOUT_FILL_X, 0,0,0,0, 0,0,0,0);

    // Profile
    {
        FXGroupBox* fprofile = new FXGroupBox(fgroups, "Profile", FRAME_RIDGE);
        FXMatrix*   mtx = new FXMatrix(fprofile, 2, MATRIX_BY_COLUMNS);

        // Profile selection.
        new FXLabel(mtx, "Name:");
        lbxProfile_ = new FXListBox(mtx, this, ID_SELECT_PROFILE);

        new FXLabel(mtx, "IMEI:");
        tfIMEI_ = new FXTextField(mtx, 20, 0, 0);

        new FXLabel(mtx, "FW. Version:");
        tfFirmwareVersion_ = new FXTextField(mtx, 20, 0, 0);

        new FXLabel(mtx, "FW. Date:");
        tfFirmwareDate_ = new FXTextField(mtx, 20, 0, 0);

        new FXLabel(mtx, "FW. Info:");
        tfFirmwareBuildInfo_ = new FXTextField(mtx, 20, 0, 0);

        new FXLabel(mtx, "BT Port:");
        tfBluetoothPort_ = new FXTextField(mtx, 20, 0, 0);

        new FXLabel(mtx, "Server:");
        tfServer_ = new FXTextField(mtx, 20, 0, 0);

        for (unsigned int i=0; i<2; ++i) {
            std::string sname;
            ssprintf(sname, "GPS %d:", i+1);
            new FXLabel(mtx, sname.c_str());
            new FXLabel(mtx, "");

            // FXMatrix*   fgps = new FXMatrix(mtx, 2, MATRIX_BY_COLUMNS);
            new FXLabel(mtx, "X:", 0, LAYOUT_RIGHT);
            tfGps_[i].first = new FXTextField(mtx, 15, 0, 0);
            new FXLabel(mtx, "Y:", 0, LAYOUT_RIGHT);
            tfGps_[i].second = new FXTextField(mtx, 15, 0, 0);
        }

        new FXLabel(mtx, "B. Voltage:");
        tfSupplyVoltage_ = new FXTextField(mtx, 20, 0, 0);

        // Button to save it all :P
        new FXButton(fprofile, "Save profile", 0, this, ID_SAVE_PROFILE, LAYOUT_FILL_X|FRAME_RAISED);
    }

    // Traffic statistics
    {
        FXGroupBox* fstats = new FXGroupBox(fgroups, "Traffic statistics", FRAME_RIDGE);
        FXMatrix*   mtx = new FXMatrix(fstats, 10, MATRIX_BY_ROWS);
        new FXLabel(mtx, "Connection:");
        new FXLabel(mtx, "Status:");
        new FXLabel(mtx, "Input pings:");
        new FXLabel(mtx, "Input data packets:");
        new FXLabel(mtx, "Input total bytes:");
        new FXLabel(mtx, "Time of last input:");
        new FXLabel(mtx, "Output pings:");
        new FXLabel(mtx, "Output data packets:");
        new FXLabel(mtx, "Output total bytes:");
        new FXLabel(mtx, "Time of last Output:");

        statsBluetooth_ = std::auto_ptr<TrafficStatistics>(new TrafficStatistics(mtx, "Bluetooth"));
        statsServer_ = std::auto_ptr<TrafficStatistics>(new TrafficStatistics(mtx, "Server"));
    }

    // GPS buttons.
    {
        FXGroupBox* fgps = new FXGroupBox(fgroups, "Misc", FRAME_RIDGE|LAYOUT_FILL_X|LAYOUT_FILL_Y);
        FXMatrix*   mtx = new FXMatrix(fgps, 3, MATRIX_BY_COLUMNS|LAYOUT_FILL_X);

        new FXLabel(mtx, "GPS 2 distance:");
        tfGps2Distance_ = new FXTextField(mtx, 6, 0, 0);
        slGps2Distance_ = new FXSlider(mtx, this, ID_GPS_DISTANCE,
            LAYOUT_TOP|LAYOUT_FIX_WIDTH|SLIDER_HORIZONTAL|LAYOUT_CENTER_Y,0,0,200,0);
        slGps2Distance_->setRange(30, 300);

        new FXLabel(mtx, "GPS 2 heading:");
        tfGps2Heading_ = new FXTextField(mtx, 6, 0, 0);
        diGps2Heading_ = new FXDial(mtx, this, ID_GPS_HEADING, DIAL_CYCLIC|DIAL_HORIZONTAL|LAYOUT_CENTER_Y|LAYOUT_FIX_WIDTH, 0,0,200,0);
        diGps2Heading_->setRange(0, 359);
    
        generateRandomCmr_ = new FXCheckButton(mtx, "Generate random packets", this, ID_RANDOM_CMR);
        new FXLabel(mtx, "");
        new FXLabel(mtx, "");
        
        new FXLabel(mtx, "Custom GPS line:");
        new FXButton(mtx, "Send", 0, this, ID_SEND_CUSTOM_GPS, LAYOUT_FILL_X|FRAME_RAISED);
        gpsLine_ = new FXTextField(mtx, 60, 0, 0);
        new FXLabel(mtx, "From GPS2:");
        gpsLineFromGps1_ = new FXCheckButton(mtx, "", 0, 0);
        new FXLabel(mtx, "");

        // Movement simulation
        cbSpeed_ = new FXCheckButton(mtx, "Simulate movement (kn)", 0, 0);
        tfSpeed_ = new FXTextField(mtx, 6, 0, 0);
        slSpeed_ = new FXSlider(mtx, this, ID_MOVEMENT, LAYOUT_TOP|LAYOUT_FIX_WIDTH|SLIDER_HORIZONTAL|LAYOUT_CENTER_Y, 0, 0, 200,0);
        slSpeed_->setValue(0, TRUE);
        // Speed ranges from -100 .. 100kn
        slSpeed_->setRange(-50, 50);
        FXMatrix*   speed_buttons_mtx = new FXMatrix(fgps, 4, MATRIX_BY_COLUMNS|LAYOUT_FILL_X);
        new FXLabel(speed_buttons_mtx, "Speed:");
        add_to_map<FXButton*, int>(speedButtons_, new FXButton(speed_buttons_mtx , "1kn",   0, this, ID_MOVEMENT, FRAME_RAISED|LAYOUT_FILL), 1);
        add_to_map<FXButton*, int>(speedButtons_, new FXButton(speed_buttons_mtx , "5kn",   0, this, ID_MOVEMENT, FRAME_RAISED|LAYOUT_FILL), 5);
        add_to_map<FXButton*, int>(speedButtons_, new FXButton(speed_buttons_mtx , "10kn",  0, this, ID_MOVEMENT, FRAME_RAISED|LAYOUT_FILL), 10);

        // Direction buttons
        {
            FXMatrix*   dir_buttons_mtx = new FXMatrix(fgps, 3, MATRIX_BY_COLUMNS|LAYOUT_FILL_X);
            const char dirLabels[][3] = {   "NW",   "N ",   "NE",   "W ",   "",     "E ",   "SW",   "S ",   "SE" };
            const int dirVals[] = {         315,    0,      45,     270,    -100,   90,     225,    180,    135 };
            int i=0;
            for (; i<4; i++){
                add_to_map<FXButton*, int>(directionButtons_, new FXButton(dir_buttons_mtx, dirLabels[i], 0, this, ID_GPS_HEADING, FRAME_RAISED|LAYOUT_FILL), dirVals[i]);
            }
            i++;
            new FXLabel(dir_buttons_mtx, "");
            for (; i<9; i++){
                add_to_map<FXButton*, int>(directionButtons_, new FXButton(dir_buttons_mtx, dirLabels[i], 0, this, ID_GPS_HEADING, FRAME_RAISED|LAYOUT_FILL), dirVals[i]);
            }
        }
    }

    // Log box.
    {
        FXGroupBox* flog = new FXGroupBox(fmain, "Log", FRAME_RIDGE|LAYOUT_TOP|LAYOUT_FILL_X|LAYOUT_FILL_Y);
        FXVerticalFrame*    fvert = new FXVerticalFrame(flog,LAYOUT_TOP|LAYOUT_FILL_X|LAYOUT_FILL_Y,0,0,0,0, 0,0,0,0);
        {
	        FXHorizontalFrame*	fopts = new FXHorizontalFrame(fvert,LAYOUT_SIDE_TOP|LAYOUT_FILL_X, 0,0,0,0, 0,0,0,0);
            new FXButton(fopts, "Clear", 0, this, ID_CLEAR_LOG, FRAME_RAISED);
            new FXButton(fopts, "Options", 0, this, ID_LOGGING_OPTIONS, FRAME_RAISED);
            cbLogGpsSerial_ = new FXCheckButton(fopts, "GPS-NMEA");
        }
        tLog_ = new FXText(fvert, 0, 0, TEXT_SHOWACTIVE|TEXT_AUTOSCROLL|LAYOUT_FILL_X|LAYOUT_FILL_Y|FRAME_SUNKEN|FRAME_THICK);
    }
    dlgLogOptions_ = new LogOptionsDialog(this, shownPackets_);

    // Common Accelerators
	getAccelTable()->addAccel(parseAccel("Alt-F4"), getApp(), MKUINT(FXApp::ID_QUIT, SEL_COMMAND));
    joystick_.init((HINSTANCE)(getApp()->getDisplay()));

}

/* ------------------------------------------------------------- */
ModemBoxSimulator::~ModemBoxSimulator()
{
}

/* ------------------------------------------------------------- */
static bool
is_all_digits(  const char* s)
{
    if (s==0 || *s==0) {
        return false;
    }
    bool all_digits = true;
    for (; *s!=0; ++s) {
        if (!isdigit(*s)) {
            all_digits = false;
            break;
        }
    }
    return all_digits;
}

/* ------------------------------------------------------------- */
static bool
get_gps(
    const FileConfig&       cfg,
    const char*             key,
    GPS&                    gps_pos
    )
{
    std::string     s;
    if (cfg.get_string(key, s)) {
        std::vector<std::string>    v;
        split(s, ";", v);
        const bool r = v.size()>=2
             && double_of(v[0], gps_pos.X)
             && double_of(v[1], gps_pos.Y);
        return r;
    }
    return false;
}

/* ------------------------------------------------------------- */
void
ModemBoxSimulator::create()
{
    inherited::create();

    // Profile initialization.
    try {
        // 1. Optional parameter.
        const char*     startup_name =
                            argc_>1
                                ? argv_[1]
                                : 0;
        bool            startup_use_number = is_all_digits(startup_name);
        // 2. startup_number: -1 if use default, number otherwise.
        int             startup_number =
                            startup_use_number
                                ? atoi(startup_name)
                                : -1;

        // 3. Fill Profiles_.
        // 4. Update startup_number.
        {
            PROFILE     profile;
            std::string     section_name;
            FileConfig  cfg(CONFIG_FILENAME);
            cfg.load();

            for (unsigned int number=1; ; ++number) {
                ssprintf(section_name, "Profile %d", number);
                cfg.set_section(section_name);
                if (cfg.get_string("Name", profile.Name)
                    && cfg.get_string("IMEI", profile.IMEI)
                    && cfg.get_string("FirmwareVersion", profile.FirmwareVersion)
                    && cfg.get_string("FirmwareDate", profile.FirmwareDate)
                    && cfg.get_string("FirmwareBuildInfo", profile.FirmwareBuildInfo)
                    && cfg.get_string("BluetoothPort", profile.BluetoothPort)
                    && cfg.get_string("Server", profile.Server)
                    && get_gps(cfg, "GPS1", profile.Gps[0])
                    && get_gps(cfg, "GPS2", profile.Gps[1])
                    && cfg.get_double("SupplyVoltage", profile.SupplyVoltage)
                    ) {
                        // Insert the profile.
                        profile.Number = number;
                        Profiles_.push_back(profile);

                        // Update startup_number, if needed.
                        if (startup_name!=0 && !startup_use_number && profile.Name==startup_name) {
                            startup_number = number;
                        }
                } else {
                    break;
                }
            }
        }

        // 5. Setup the listbox.
        lbxProfile_->setNumVisible(static_cast<int>(Profiles_.size()));
        for (unsigned int i=0; i<Profiles_.size(); ++i) {
            const PROFILE&  profile = Profiles_[i];
            lbxProfile_->appendItem(profile.Name.c_str(), 0, 0);
        }

        // 6. Load given profile.
        if (Profiles_.size()>0) {
            if (startup_name==0) {
                // Use default.
                startup_number = 1;
            } else {
                if (startup_number<1 || startup_number>static_cast<int>(Profiles_.size())) {
                    FXMessageBox::error(this, MBOX_OK, "Error",
                        "Given profile %s '%s' not found.",
                        ( startup_use_number ? "number" : "name"),
                        startup_name);
                    exit(0);
                }
            }
        } else {
            FXMessageBox::error(this, MBOX_OK, "Error", "No valid profiles in '%s'!", CONFIG_FILENAME);
            exit(0);
        }

        LoadProfile(startup_number);
    } catch (const std::exception& e) {
        FXMessageBox::error(this, MBOX_OK, "Error", "%s", e.what());
        exit(0);
    }

    show(PLACEMENT_SCREEN);

    // Start timers.
	getApp()->addTimeout(this, ID_POLL_10HZ, TIMEOUT_POLL_10HZ);
	getApp()->addTimeout(this, ID_POLL_GPS, TIMEOUT_POLL_GPS);
}


/* ------------------------------------------------------------- */
long
ModemBoxSimulator::onCmdSelectProfile(FXObject*, FXSelector, void*)
{
    const int   index = lbxProfile_->getCurrentItem();
    if (index != ProfileIndex_) {
        LoadProfile(index+1);
    }

    return 1;
}

/* ------------------------------------------------------------- */
long
ModemBoxSimulator::onCmdSaveProfile(FXObject*, FXSelector, void*)
{
    const int   profileIndex = lbxProfile_->getCurrentItem();
    PROFILE&  profile = Profiles_[profileIndex];
    try {
        // 1. GUI -> Profiles_[profileIndex].
        profile.IMEI = string_of_fxstring(tfIMEI_->getText());
        profile.FirmwareVersion = string_of_fxstring(tfFirmwareVersion_->getText());
        profile.FirmwareDate = string_of_fxstring(tfFirmwareDate_->getText());
        profile.FirmwareBuildInfo = string_of_fxstring(tfFirmwareBuildInfo_->getText());
        profile.BluetoothPort = string_of_fxstring(tfBluetoothPort_->getText());
        profile.Server = string_of_fxstring(tfServer_->getText());
        for (unsigned int gpsIndex=0; gpsIndex<2; ++gpsIndex) {
            GPS gps;
            if (double_of_textfield(gps.X, tfGps_[gpsIndex].first)
                && double_of_textfield(gps.Y, tfGps_[gpsIndex].second)) {
                profile.Gps[gpsIndex] = gps;
            } else {
                throw std::runtime_error(ssprintf("Check GPS coordinates for invalid input."));
            }
        }
        profile.SupplyVoltage = double_of(string_of_fxstring(tfSupplyVoltage_->getText()));

        // 2. Save configuration file.
        FileConfig  cfg(CONFIG_FILENAME);
        try {
            cfg.load();
        } catch (const std::exception&) {
            // pass.
        }

        cfg.set_section(ssprintf("Profile %d", profile.Number));
        cfg.set_string("Name", profile.Name);
        cfg.set_string("IMEI", profile.IMEI);
        cfg.set_string("FirmwareVersion", profile.FirmwareVersion);
        cfg.set_string("FirmwareDate", profile.FirmwareDate);
        cfg.set_string("FirmwareBuildInfo", profile.FirmwareBuildInfo);
        cfg.set_string("BluetoothPort", profile.BluetoothPort);
        cfg.set_string("Server", profile.Server);
        for (unsigned int gpsIndex=0; gpsIndex<2; ++gpsIndex) {
            const GPS&  gps = profile.Gps[gpsIndex];
            cfg.set_string(
                ssprintf("GPS%d", gpsIndex+1),
                ssprintf("%4.2f;%4.2f", gps.X, gps.Y));
        }
        cfg.set_double("SupplyVoltage", profile.SupplyVoltage);

        cfg.store();

        FXMessageBox::information(this, MBOX_OK, "ModemBoxSimulator", "Profile saved.");
    } catch (const std::exception& e) {
        FXMessageBox::error(this, MBOX_OK, "Error", "%s", e.what());
        exit(0);
    }
    return 1;
}

/* ------------------------------------------------------------- */
long
ModemBoxSimulator::onCmdClearLog(FXObject*, FXSelector, void*)
{
    tLog_->setText("");
    return 1;
}

/* ------------------------------------------------------------- */
long
ModemBoxSimulator::onTimerPoll10HZ(FXObject*, FXSelector, void*)
{
    static int offsetAsk = 0;
    if (++offsetAsk >10){
        offsetAsk=0;
        // Get ship information if not already have had it
        if (!gpsOffset_[0].some()){
            // Query for ship information on startup to get needed GPS offsets. Real Modembox does not do that
            std::string msg = DIGNETMESSAGE_QUERY;
            msg.append(std::string(" ") + DIGNETMESSAGE_STARTUPINFO);
            CMR AuthPacket(CMR::DIGNET, msg);
            WriteServer(AuthPacket);
        }
    }
    if (joystick_.hasJoystick()){
        const joyplug_state& state = joystick_.getState();
        // Put ship into movement simulation mode when pressing button #10
        static bool button10_down=false;
        if (state.button[9]){
            if (!button10_down){
                cbSpeed_->setCheck(!cbSpeed_->getCheck());
            }
            button10_down = true;
        } else {
            button10_down = false;
        }
         
        // If simulating movement, left analog stick.
        if (cbSpeed_->getCheck()) {
            static short joy_x, joy_y;
            // Leave some deadroom
            if (abs(state.joy_axis_x)>10){
                joy_x = state.joy_axis_x;
            } else {
                joy_x = 0;
            }
            if (abs(state.joy_axis_y)>3){
                joy_y = state.joy_axis_y;
            } else {
                joy_y = 0;
            }

            // Joystick reports negative numbers in "forward" direction
            float speed = (joy_y-10)/245.0*10*(-1);

            // "Turbo" when pressing button #7
            if (state.button[6]){
                speed*=5;
            }

            float turn = joy_x/(100.0)+diGps2Heading_->getValue();
            while(turn>360){
                turn-=360;
            }
            while(turn<0){
                turn+=360;
            }

            slSpeed_->setValue(speed, TRUE);
            onChangedSpeed(0, 0, 0);

            diGps2Heading_->setValue(turn, TRUE);
            onChangedGpsHeading(0, 0, 0);
        }
        // Change voltages on keypress (1-4)
        if (state.button[0]){
            fxprintf(tfSupplyVoltage_, "%2.1f", 11.0);
        }
        if (state.button[1]){
            fxprintf(tfSupplyVoltage_, "%2.1f", 15.0);
        }
        if (state.button[2]){
            fxprintf(tfSupplyVoltage_, "%2.1f", 24.0);
        }
        if (state.button[3]){
            fxprintf(tfSupplyVoltage_, "%2.1f", 30.0);
        }
    }
    try {
        // 1. Server link.
        {
            // Data?
            std::string buffer;
            commServer_.read(buffer);
            if (buffer.size()>0) {
                cmrdServer_.feed(
                    reinterpret_cast<const unsigned char*>(buffer.c_str()),
                    static_cast<const unsigned int>(buffer.size()));
                CMR Packet;
                while (cmrdServer_.pop(Packet)) {
                    lpacket("S. IN", Packet);
                    statsServer_->tbInput->registerPacket(Packet);
                    if (Packet.type == CMR::PING) {
                        WriteServer(Packet);
                    }
                    ModemBox_ProcessPacket(Packet);
                }
            }

            // Connection state.
            utils::TcpPollingReader::STATUS new_status = commServer_.status();
            if (commServerStatus_ != new_status) {
                if (new_status == TcpPollingReader::STATUS_CONNECTED) {
                    // Connected.
                    statsServer_->tfStatus->setText("Connected.");
                    received_ping_time = System::currentTimeMillis();
                    std::string msg;
                        msg += "client name=ModemBox";
                        msg += " Firmware_Version=" + string_of_fx(tfFirmwareVersion_);
                        msg += " Firmware_Date=\"" + string_of_fx(tfFirmwareDate_) + "\"";
                        msg += " IMEI=" + string_of_fx(tfIMEI_);
                        msg += " BuildInfo=" + string_of_fx(tfFirmwareBuildInfo_);
                    CMR AuthPacket(CMR::DIGNET, msg);
                    WriteServer(AuthPacket);
                    ModemBox_ProcessConnect();
                    ModemBox_ProcessIdle();
                } else if (commServerStatus_ == TcpPollingReader::STATUS_CONNECTED) {
                    // Disconnected.
                    statsServer_->tfStatus->setText("Closed.");
                }
                commServerStatus_ = new_status;
            }
        }

        ModemBox_ProcessIdle();
    } catch (const std::exception& e) {
        lprintf("SERVER POLL ERROR", "%s", e.what());
    }

    // 2. Serial input queue.
    try {
        if (!serialInputFifo_.empty()) {
            IOPanel_ToggleSerialTraffic();

            std::vector<unsigned char>  buffer;
            while (!serialInputFifo_.empty()) {
                buffer.push_back(serialInputFifo_.front());
                serialInputFifo_.pop_front();
            }
            ModemBox_ProcessSerial(&buffer[0], static_cast<const unsigned int>(buffer.size()));
        }

        ModemBox_ProcessIdle();
    } catch (const std::exception& e) {
        lprintf("SERIAL POLL ERROR", "%s", e.what());
    }

    // 2. Bluetooth connection state.
    try {
        if (commBluetoothTime_>0 && commBluetoothTime_ + BLUETOOTH_TIMEOUT<System::currentTimeMillis()) {
            if (IsBluetoothConnected()) {
                lprintf("BLUETOOTH", "CLOSING");
                commBluetooth_ = std::auto_ptr<AsyncIO>();
            } else {
                ReconnectBluetooth();
            }
        }
        if (commBluetoothConnected_ != IsBluetoothConnected()) {
            commBluetoothConnected_ = IsBluetoothConnected();
            statsBluetooth_->tfStatus->setText(commBluetoothConnected_ ? "Connected" : "Closed");
        }
    } catch (const std::exception& e) {
        lprintf("BLUETOOTH POLL ERROR", "%s", e.what());
    }

	getApp()->addTimeout(this, ID_POLL_10HZ, TIMEOUT_POLL_10HZ);
    return 1;
}

/* ------------------------------------------------------------- */
static void
move_ship(
    FXTextField*    gps1_x,
    FXTextField*    gps1_y,
    FXTextField*    gps2_x,
    FXTextField*    gps2_y,
    FXSlider*       slSpeed,
    FXDial*         diHeading,
    const double    dt)
{
    try {
        const double    distance = KN2MS * slSpeed->getValue() * dt;
        const double    heading = deg2rad(diHeading->getValue());
        double          x1 = double_of_textfield(gps1_x);
        double          y1 = double_of_textfield(gps1_y);
        double          x2 = double_of_textfield(gps2_x);
        double          y2 = double_of_textfield(gps2_y);
        x1 = x1 + distance*cos(heading);
        y1 = y1 + distance*sin(heading);
        x2 = x2 + distance*cos(heading);
        y2 = y2 + distance*sin(heading);
        fxprintf(gps1_x, "%4.2f", x1);
        fxprintf(gps1_y, "%4.2f", y1);
        fxprintf(gps2_x, "%4.2f", x2);
        fxprintf(gps2_y, "%4.2f", y2);
    } catch (const std::exception&) {
    }
}


/* ------------------------------------------------------------- */
long
ModemBoxSimulator::onTimerPollGps(FXObject*, FXSelector, void*)
{
    // Find time delta
    my_time gpsTime(my_time_of_now());
    static double lastTime = float_of_time(gpsTime);
    const double gpsTimed = float_of_time(gpsTime);
    const double dt = gpsTimed - lastTime;
    lastTime = gpsTimed;

    if (cbSpeed_->getCheck()){
        move_ship(
            tfGps_[0].first, tfGps_[0].second,
            tfGps_[1].first, tfGps_[1].second,
            slSpeed_, diGps2Heading_, dt);
    }

    const bool  switch_gps = (rand() % 10) >= 5;
    // Poll GPS.
    for (unsigned int loopIndex=0; loopIndex<2; ++loopIndex) {
        const unsigned int gpsIndex = switch_gps ? (1-loopIndex): loopIndex;
        try {
            const double    x = double_of_textfield(tfGps_[gpsIndex].first);
            const double    y = double_of_textfield(tfGps_[gpsIndex].second);
            GPGGA           gpgga;

            // gpgga.
            gpgga.computer_time = gpsTime;
            gpgga.gps_time = gpsTime;
            gps_of_local(x, y, 0.0, gpgga.latitude, gpgga.longitude, gpgga.altitude);
            gpgga.north = true;
            gpgga.east = true;
            gpgga.quality = GPSQUALITY_DIFFERENTIAL;
            gpgga.nr_of_satellites = 10;
            gpgga.pdop = 5.0;

            // gpgga_line.
            std::string gpgga_line(string_of_gga(gpgga, true, 8));
            if (cbLogGpsSerial_->getCheck() != FALSE) {
                lprintf("GPS", "%d : %s", gpsIndex, gpgga_line.c_str());
            }
            gpgga_line += "\r\n";

            // CMR data block and output buffer.
            std::vector<unsigned char>  data_block;
            data_block.push_back(gpsIndex);
            for (unsigned int i=0; i<gpgga_line.size(); ++i) {
                data_block.push_back(gpgga_line[i]);
            }

            // Send to bluetooth.
            CMR Packet(CMR::SERIAL, data_block);
            WriteBluetooth(Packet);

            // Send to imaginary serial input.
            std::vector<unsigned char>  buffer;
            Packet.emit(buffer);
            for (unsigned int i=0; i<buffer.size(); ++i) {
                serialInputFifo_.push_back(buffer[i]);
            }
        } catch (const std::exception&) {
            // pass.
        }
    }

	getApp()->addTimeout(this, ID_POLL_GPS, TIMEOUT_POLL_GPS);
    return 1;
}
/* ------------------------------------------------------------- */
static void
gps2_by_headingdistance(
    FXTextField*    gps1_x,
    FXTextField*    gps1_y,
    FXTextField*    gps2_x,
    FXTextField*    gps2_y,
    FXSlider*       slDistance,
    FXDial*         diHeading,
    double          dirFix)
{
    try {
        const double    distance = SLIDER2METERS * slDistance->getValue();
        const double    heading = diHeading->getValue()+dirFix;
        const double    x1 = double_of_textfield(gps1_x);
        const double    y1 = double_of_textfield(gps1_y);
        const double    x2 = x1 + distance*cos(deg2rad(heading));
        const double    y2 = y1 + distance*sin(deg2rad(heading));
        fxprintf(gps2_x, "%4.2f", x2);
        fxprintf(gps2_y, "%4.2f", y2);
    } catch (const std::exception&) {
    }
}

/* ------------------------------------------------------------- */
long
ModemBoxSimulator::onChangedGpsDistance(FXObject*, FXSelector, void*)
{
    const double distance = SLIDER2METERS * slGps2Distance_->getValue();
    fxprintf(tfGps2Distance_, "%4.1f", distance);
    gps2_by_headingdistance(
        tfGps_[0].first, tfGps_[0].second,
        tfGps_[1].first, tfGps_[1].second,
        slGps2Distance_, diGps2Heading_, dirFix_);
    return 1;
}

/* ------------------------------------------------------------- */
long
ModemBoxSimulator::onChangedGpsHeading(FXObject* obj, FXSelector, void*)
{
    FXButton* b = reinterpret_cast<FXButton*>(obj);
    FXButtonIntMap::iterator fxb = directionButtons_.find(b);
    if (fxb != directionButtons_.end()){
        diGps2Heading_->setValue(fxb->second);
    }
    const double heading = diGps2Heading_->getValue();
    fxprintf(tfGps2Heading_, "%4.1f", heading); 
    gps2_by_headingdistance(
        tfGps_[0].first, tfGps_[0].second,
        tfGps_[1].first, tfGps_[1].second,
        slGps2Distance_, diGps2Heading_, dirFix_);
    return 1;
}

/* ------------------------------------------------------------- */
long
ModemBoxSimulator::onToggledGenerateRandomCmr(FXObject*, FXSelector, void* ptr)
{
    if (ptr)
    {
        getApp()->addTimeout(this, ID_POLL_100HZ, TIMEOUT_POLL_100HZ);
    }
    else 
    {
        getApp()->removeTimeout(this, ID_POLL_100HZ);
    }
    return 1;
}

/* ------------------------------------------------------------- */
long
ModemBoxSimulator::onGenerateRandomCmr(FXObject*, FXSelector, void*)
{
    const unsigned int packet_len = rand() % CMR::MAX_DATABLOCK_LENGTH;
    std::vector<unsigned char>  data_block;
    for (unsigned int i=0; i<packet_len; ++i) {
        data_block.push_back(rand()&0xff);
    }
    int type = rand()%10+0xbc00;
    CMR packet(type, data_block);
    if (IsServerConnected()) {
        WriteServer(packet);
        if (type==0xbc00){
            commServer_.write(data_block);
        }
    }
    if (IsBluetoothConnected()) {
        WriteBluetooth(packet);
        if (type==0xbc00){
            commServer_.write(data_block);
        }
    }
    if (generateRandomCmr_->getCheck()==TRUE)
    {
        getApp()->addTimeout(this, ID_POLL_100HZ, TIMEOUT_POLL_100HZ/5);
    }
    return 1;
}

/* ------------------------------------------------------------- */
long
ModemBoxSimulator::onSendCustomGpsLine(FXObject*, FXSelector, void*)
{
    std::vector<unsigned char>  data_block;
    FXString s(gpsLine_->getText());
    int gps1 = gpsLineFromGps1_->getCheck();
    std::string str;
    str = '\0'+gps1;
    str.append(s.text());
    data_block.assign(str.begin(), str.end());

    CMR packet(CMR::SERIAL, data_block);
    if (IsServerConnected()) {
        WriteServer(packet);
    }
    if (IsBluetoothConnected()) {
        WriteBluetooth(packet);
    }
    return 1;
}

/* ------------------------------------------------------------- */
long
ModemBoxSimulator::onChangedSpeed(FXObject* obj, FXSelector, void*)
{
    FXButton* b = reinterpret_cast<FXButton*>(obj);
    FXButtonIntMap::iterator fxb = speedButtons_.find(b);
    if (fxb != speedButtons_.end()){
        slSpeed_->setValue(fxb->second);
    }

    const double speed = slSpeed_->getValue();
    fxprintf(tfSpeed_, "%4.1f", speed);
    return 1;
}

/* ------------------------------------------------------------- */
long
ModemBoxSimulator::onCmdShowLoggingOptions(FXObject*, FXSelector, void*)
{
    dlgLogOptions_->show(PLACEMENT_OWNER);
    return 1;
}

/* ------------------------------------------------------------- */
void
ModemBoxSimulator::LoadProfile(
        const unsigned int  ProfileNumber)
{
    const unsigned int profileIndex = ProfileNumber-1;
    const PROFILE&  profile = Profiles_[profileIndex];

    // Update GUI.
    lbxProfile_->setCurrentItem(profileIndex);
    tfIMEI_->setText(profile.IMEI.c_str());
    tfFirmwareVersion_->setText(profile.FirmwareVersion.c_str());
    tfFirmwareDate_->setText(profile.FirmwareDate.c_str());
    tfFirmwareBuildInfo_->setText(profile.FirmwareBuildInfo.c_str());
    tfBluetoothPort_->setText(profile.BluetoothPort.c_str());
    tfServer_->setText(profile.Server.c_str());
    for (unsigned int i=0; i<2; ++i) {
        const GPS&  gps = profile.Gps[i];
        fxprintf(tfGps_[i].first, "%4.2f", gps.X); 
        fxprintf(tfGps_[i].second, "%4.2f", gps.Y); 
    }
    fxprintf(tfSupplyVoltage_, "%3.1f", profile.SupplyVoltage);

    // Update current profile index.
    ProfileIndex_ = profileIndex;

    // Connect devices.
    commServer_.close();
    commServer_.open(profile.Server);

    ReconnectBluetooth();
    gpsOffset_[0] = utils::Option<GPS>();
    gpsOffset_[1] = utils::Option<GPS>();
}

/* ------------------------------------------------------------- */
void
ModemBoxSimulator::ReconnectBluetooth()
{
    const PROFILE&  profile = Profiles_[ProfileIndex_];
    if (commBluetooth_.get() != 0) {
        lprintf("BLUETOOTH", "CLOSING.");
        commBluetooth_ = std::auto_ptr<AsyncIO>();
    }
    try {
        lprintf("BLUETOOTH", "OPENING %s", profile.BluetoothPort.c_str());
        commBluetooth_ = std::auto_ptr<AsyncIO>(new AsyncIO(
            open_serial(profile.BluetoothPort,
				9600, IOFLAGS_OVERLAPPED),
				profile.BluetoothPort,
				makeFunctor((AsyncIO::ReadCallback*)0,
				*this, &ModemBoxSimulator::onBluetoothInput),
				512));
        commBluetoothTime_ = System::currentTimeMillis();
    } catch (const std::exception& e) {
        lprintf("BT OPEN", "Error: %s", e.what());
    }
    commBluetoothTime_ = System::currentTimeMillis();
}

/* ------------------------------------------------------------- */
void
ModemBoxSimulator::onBluetoothInput(char* data, unsigned int len)
{
    // 1. Feed the CMR decoder.
    cmrdBluetooth_.feed(reinterpret_cast<const unsigned char*>(data), len);

    // 2. Handle packets.
    CMR                         packet;
    std::vector<unsigned char>  buffer;
    while (cmrdBluetooth_.pop(packet)) {
        statsBluetooth_->tbInput->registerPacket(packet);
        lpacket("BT IN", packet);

        buffer.resize(0);
        packet.emit(buffer);

        for (unsigned int i=0; i<buffer.size(); ++i) {
            serialInputFifo_.push_back(buffer[i]);
        }
    }

    commBluetoothTime_ = System::currentTimeMillis();
}

/* ------------------------------------------------------------- */
bool
ModemBoxSimulator::IsBluetoothConnected() const
{
    const bool r = commBluetooth_.get() != 0;
    return r;
}

/* ------------------------------------------------------------- */
bool
ModemBoxSimulator::IsServerConnected() const
{
    const bool r = commServer_.status() == TcpPollingReader::STATUS_CONNECTED;
    return r;
}

/* ------------------------------------------------------------- */
void
ModemBoxSimulator::lprintf(
        const char* prefix,
        const char* fmt, ...
        )
{
    std::string  buffer;

    // Add time.
    my_time mt(my_time_of_now());
    ssprintf(buffer, "%02d:%02d:%02d.%d", mt.hour, mt.minute, mt.second, (mt.millisecond + 50)/ 100);

    buffer += " : ";
    buffer += prefix;
    buffer += " : ";

    // Format arguments.
    std::string args;
    va_list ap;
    va_start(ap, fmt);
    vssprintf(args, fmt, ap);
    va_end(ap);
    buffer += args;

    buffer += "\n";

    // Log the string.
    tLog_->appendText(buffer.c_str());
}

/* ------------------------------------------------------------- */
void
ModemBoxSimulator::lpacket(
    const char* prefix,
    const utils::CMR&   Packet
)
{
    bool display = true;
    std::map<int, PacketInfo>::const_iterator p = shownPackets_.find(Packet.type);
    if (p != shownPackets_.end()){
        display = p->second.logged;
    } else {
        TRACE_PRINT("warning", ("Unknown packet found: %d", Packet.type))
    }
    if (display) {
        const std::string   spacket(Packet.ToString());
        lprintf(prefix, "%s", spacket.c_str());
    }
}

/* ------------------------------------------------------------- */
void
ModemBoxSimulator::WriteBluetooth(
    const utils::CMR&   packet
)
{
    lpacket("BT OUT", packet);
    statsBluetooth_->tbOutput->registerPacket(packet);
    if (IsBluetoothConnected()) {
        std::vector<unsigned char>  buffer;
        packet.emit(buffer);
        commBluetooth_->write(buffer);
    }
}

/* ------------------------------------------------------------- */
void
ModemBoxSimulator::WriteServer(
    const utils::CMR&   packet
)
{
    lpacket("SERVER OUT", packet);
    statsServer_->tbOutput->registerPacket(packet);
    if (IsServerConnected()) {
        std::vector<unsigned char>  buffer;
        packet.emit(buffer);
        commServer_.write(buffer);
    }
}

    // ---------------- ModemBox functions 1:1 ------------------------
/// GPS coordinate sending period, milliseconds.
static long GPS_SEND_PERIOD = 10*1000;
/// GPS coordinate max. age, milliseconds
long GPS_MAX_AGE = 3 * 1000;
/// Supply voltage sending period, milliseconds.
long SUPPLY_SEND_PERIOD = 5 * 60 * 1000;

/*--------------------------------------------------------------------*/
/** Send supply voltage to the clients.
 */
void
ModemBoxSimulator::SendSupplyVoltage()
{
    const std::string s_voltage = string_of_fxstring(tfSupplyVoltage_->getText());

    CMR packet(CMR::DIGNET, ssprintf("modembox supplyvoltage=%s", s_voltage.c_str()));

    SupplyVoltageTime = System::currentTimeMillis();

    WriteServer(packet);
    WriteBluetooth(packet);
}

/* ------------------------------------------------------------- */
void
ModemBoxSimulator::IOPanel_ToggleRtcmTraffic()
{
}

/* ------------------------------------------------------------- */
void
ModemBoxSimulator::IOPanel_TogglePacketTraffic()
{
}

/* ------------------------------------------------------------- */
void
ModemBoxSimulator::IOPanel_ToggleSerialTraffic()
{
}

/* ------------------------------------------------------------- */
void
ModemBoxSimulator::ModemBox_ProcessIdle()
{
    long now = System::currentTimeMillis();

    if (SupplyVoltageTime < 0 || SupplyVoltageTime + SUPPLY_SEND_PERIOD < now)
    {
        SendSupplyVoltage();
    }
}

/* ------------------------------------------------------------- */
void
ModemBoxSimulator::ModemBox_ProcessConnect()
{
    SendSupplyVoltage();
}

/* ------------------------------------------------------------- */
void
ModemBoxSimulator::ModemBox_ProcessPacket(
        const utils::CMR&   Packet
    )
{
    if (Packet.type == PACKET_SHIP_INFO::CMRTYPE){
        const PACKET_SHIP_INFO* ship = reinterpret_cast<const PACKET_SHIP_INFO*>(&Packet.data[0]);
        std::string ship_name;
        ship_name.resize(ship->ship_name_length);
        memcpy(&ship_name[0], ship->ship_name_imei, ship_name.size());
        if (ship_name==Profiles_[ProfileIndex_].Name){
            GPS g1, g2;
            g1.X=ship->gps1_dx/100.0;
            g1.Y=ship->gps1_dy/100.0;
            g2.X=ship->gps2_dx/100.0;
            g2.Y=ship->gps2_dy/100.0;
            gpsOffset_[0] = g1;
            gpsOffset_[1] = g2;
            //GPS fix for direction
            const double a = gpsOffset_[1]().Y-gpsOffset_[0]().Y;
            const double b = gpsOffset_[1]().X-gpsOffset_[0]().X;
            dirFix_ = (rad2deg(atan2(a, b)));//+182;
            const double distance = utils::distance(g1.X, g1.Y, g2.X, g2.Y);
            fxprintf(tfGps2Distance_, "%.1f", distance);
            slGps2Distance_->setValue(distance/SLIDER2METERS, TRUE);
        }
    }
    if (IsBluetoothConnected()) {
        WriteBluetooth(Packet);
        if (Packet.type == CMR::RTCM)
        {
            IOPanel_ToggleRtcmTraffic();
        }
    }
}

/* ------------------------------------------------------------- */
void
ModemBoxSimulator::ModemBox_ProcessSerial(
    const unsigned char*    SerialData,
    const unsigned int      SerialLength
)
{
    serial_decoder.feed(SerialData, SerialLength);
    long now = System::currentTimeMillis();
    CMR Packet;
    while (serial_decoder.pop(Packet)) {
	    IOPanel_TogglePacketTraffic();
        // Watch the serial packets.
        if (Packet.type == CMR::SERIAL) {
            if (Packet.data.size() > 1 && Packet.data[0] >= 0 && Packet.data[0] <= 1)
            {
                int index = Packet.data[0];
                GpsLines[index].Feed(Packet);
            }
        } else if (IsServerConnected()) {
            WriteServer(Packet);
        }
    }

    // Check if we need to send data.
    if (GpsPositionTime < 0 || GpsPositionTime + GPS_SEND_PERIOD < now)
    {
        GpsPositionTime = now;
        for (int gpsIndex = 0; gpsIndex<2; ++gpsIndex)
        {
            GpsLine& gline = GpsLines[gpsIndex];
            if (gline.Line.size()>0 && gline.LineTime + GPS_MAX_AGE >= now)
            {
                if (IsServerConnected()) {
                    std::vector<unsigned char>  data_block;
                    data_block.push_back(gpsIndex);
                    for (unsigned int i=0; i<gline.Line.size(); ++i) {
                        data_block.push_back(gline.Line[i]);
                    }
                    CMR gps_packet(CMR::SERIAL, data_block);
                    WriteServer(gps_packet);
                }
            }
        }
    }
}

