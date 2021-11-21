#ifndef ModemBoxSimulator_h_
#define ModemBoxSimulator_h_

#include <utility>  // std::pair
#include <memory>   // std::auto_ptr
#include <fx.h>

#include <string>   // std::string
#include <vector>   // std::vector
#include <deque>    // std::deque

#include <utils/Serial.h>
#include <utils/ugl.h>
#include <utils/TcpPollingReader.h>

#include "TrafficStatistics.h"
#include "GpsLine.h"
#include "Joystick.h"

/* ------------------------------------------------------------- */
class GPS {
public:
    double          X;
    double          Y;
}; // class GPS

/* ------------------------------------------------------------- */
class PROFILE {
public:
    unsigned int    Number;
    std::string     Name;
    std::string     IMEI;
    std::string     FirmwareVersion;
    std::string     FirmwareDate;
    std::string     FirmwareBuildInfo;
    std::string     BluetoothPort;
    std::string     Server;
    GPS             Gps[2];
    double          SupplyVoltage;
}; // class Profile

/* ------------------------------------------------------------- */
/// Packet info.
class PacketInfo {
public:
    int         code;       //< CMR code of packet
    bool        logged;     //< Packet is shown in log if true
    std::string name;       //< Human-readable name of the packet

    PacketInfo(
        const bool          logged,
        const int           code,
        const std::string&  name);
};

/* ------------------------------------------------------------- */
/// Logging options dialog
class LogOptionsDialog : public FXDialogBox {
    FXDECLARE(LogOptionsDialog)
    typedef std::map<FXCheckButton*, int>   ButtonMap;
    typedef std::map<int, PacketInfo*>      PacketMap;
    PacketMap packets_;
    ButtonMap buttons_;
protected:
    FXHorizontalFrame* contents;
    FXHorizontalFrame* buttons;
private:
    LogOptionsDialog(){}
public:
    enum {
        ID_CHECK_PRESSED = FXDialogBox::ID_LAST,
        ID_LAST
    };
    LogOptionsDialog(FXWindow* owner, std::map<int, PacketInfo>& packets);
    virtual ~LogOptionsDialog(){};

    /** Checkbutton was pressed. */
	long
    onChekbuttonPress(FXObject*, FXSelector, void*);

};

/* ------------------------------------------------------------- */
class ModemBoxSimulator : public FXMainWindow {
    FXDECLARE(ModemBoxSimulator)
protected:
    ModemBoxSimulator() {}
public:
    typedef FXMainWindow    inherited;
    typedef std::map<FXButton*, int> FXButtonIntMap;

    enum {
        ID_SELECT_PROFILE = inherited::ID_LAST,
        ID_SAVE_PROFILE,

        ID_CLEAR_LOG,

        ID_POLL_10HZ,
        ID_POLL_GPS,

        ID_GPS_DISTANCE,
        ID_GPS_HEADING,

        ID_RANDOM_CMR,
        ID_POLL_100HZ,
        ID_SEND_CUSTOM_GPS,
        ID_MOVEMENT,

        ID_LOGGING_OPTIONS,

        ID_LAST
    };

public:
	ModemBoxSimulator(
        FXApp* a,
        int argc,
        char**  argv
    );

	virtual
    ~ModemBoxSimulator();

	virtual void
    create();

    /** Change profile. */
	long
    onCmdSelectProfile(FXObject*, FXSelector, void*);
    /** Save current profile back to the profile file. */
	long
    onCmdSaveProfile(FXObject*, FXSelector, void*);
    /** Clear log. */
	long
    onCmdClearLog(FXObject*, FXSelector, void*);
    /** Poll tcp. */
	long
    onTimerPoll10HZ(FXObject*, FXSelector, void*);
    /** Poll GPS-s. */
	long
    onTimerPollGps(FXObject*, FXSelector, void*);
    /** GPS 2 distance changed. */
	long
    onChangedGpsDistance(FXObject*, FXSelector, void*);
    /** GPS 2 heading changed. */
	long
    onChangedGpsHeading(FXObject*, FXSelector, void*);

    /** Random CMR generation, toggles random CMR generation. */
    long
    onToggledGenerateRandomCmr(FXObject*, FXSelector, void*);

    /** Timer function that generates random CMR. Restarts itself if checkbox hasn't changed */
    long
    onGenerateRandomCmr(FXObject*, FXSelector, void*);

    /** Send custom GPS line */
    long
    onSendCustomGpsLine(FXObject*, FXSelector, void*);

    /** Speed changed. */
	long
    onChangedSpeed(FXObject*, FXSelector, void*);

    /** Logging dialog opened */
    long
    onCmdShowLoggingOptions(FXObject*, FXSelector, void*);


private:
    int                     argc_;  // argc
    char**                  argv_;  // argv

    // ----------- PROFILE SELECTION ---------------------
    FXListBox*              lbxProfile_;    ///< Profile selection listbox.
    std::vector<PROFILE>    Profiles_;      ///< List of available profiles.
    int                     ProfileIndex_;  ///< Current profile index.
    FXTextField*            tfIMEI_;
    FXTextField*            tfFirmwareVersion_;
    FXTextField*            tfFirmwareDate_;
    FXTextField*            tfFirmwareBuildInfo_;
    FXTextField*            tfBluetoothPort_;
    FXTextField*            tfServer_;
    std::pair<FXTextField*, FXTextField*>   tfGps_[2];
    FXTextField*            tfSupplyVoltage_;

    // ----------- TRAFFIC STATISTICS --------------------
    std::auto_ptr<TrafficStatistics>    statsBluetooth_;
    std::auto_ptr<TrafficStatistics>    statsServer_;

    // ------------------- GPS ---------------------------
    FXTextField*                    tfGps2Distance_;
    FXSlider*                       slGps2Distance_;
    FXTextField*                    tfGps2Heading_;
    FXDial*                         diGps2Heading_;
    FXCheckButton*                  gpsLineFromGps1_;   ///< If ticked send as coming from GPS1, else as from GPS2
    FXTextField*                    gpsLine_;           ///< User entered data to be sent as if coming from GPS

    FXCheckButton*                  cbSpeed_;           ///< If ticked simulate ship movement
    FXTextField*                    tfSpeed_;           ///< Speed of ship movement
    FXSlider*                       slSpeed_;

    // ------------------- LOG ---------------------------
    FXText*                         tLog_;
    FXCheckButton*                  cbLogGpsSerial_;
    LogOptionsDialog*               dlgLogOptions_;

    // -------------- COMMUNICATION ----------------------
	std::auto_ptr<utils::AsyncIO>   commBluetooth_;
    utils::CMRDecoder               cmrdBluetooth_;
    bool                            commBluetoothConnected_;
    long                            commBluetoothTime_; // last time of bluetooth input data.
	utils::TcpPollingReader			commServer_;
    utils::CMRDecoder               cmrdServer_;
    utils::TcpPollingReader::STATUS commServerStatus_;
    std::deque<unsigned char>       serialInputFifo_;   // ONLY COMPLETE CMR PACKETS.

    // ------------- MISC --------------------------------
    FXCheckButton*                  generateRandomCmr_;
    FXCheckButton*                  logOutgoingCmr_;
    Joystick                        joystick_;
    utils::Option<GPS>              gpsOffset_[2];  // GPS offsets from ship 0,0
    double                          dirFix_;        // GPS direction to ship direction fix
    std::map<int, PacketInfo>       shownPackets_;  // map of packet CMR types that get shown in log. Key is CMR type.
    FXButtonIntMap                  directionButtons_;  
    FXButtonIntMap                  speedButtons_;

private:
    /** Load given profile.
    */
    void
    LoadProfile(
        const unsigned int  ProfileNumber);

    /** Reconnect bluetooth connection. */
    void
    ReconnectBluetooth();

    /** Insert packets from bluetooth to the serial input queue. */
    void
    onBluetoothInput(char* data, unsigned int len);

    bool
    IsBluetoothConnected() const;

    bool
    IsServerConnected() const;

    /** Write to the log.

    Format:
    HH:MM:SS.sss : prefix : message...
    */
    void
    lprintf(
        const char* prefix,
        const char* fmt, ...
        );

    /** Write packet to the log. */
    void
    lpacket(
        const char* prefix,
        const utils::CMR&   Packet
    );

    /** Write to the Bluetooth port. */
    void
    WriteBluetooth(
        const utils::CMR&   packet
    );

    /** Write to the server. */
    void
    WriteServer(
        const utils::CMR&   packet
    );

    // ---------------- ModemBox fields 1:1    ------------------------
    /** Last time supply voltage time was sent.
     */
    long                received_ping_time;
    long                SupplyVoltageTime;
    long                GpsPositionTime;
    utils::CMRDecoder   serial_decoder;
    GpsLine             GpsLines[2];

    // ---------------- ModemBox functions 1:1 ------------------------
    void
    SendSupplyVoltage();

    void
    IOPanel_ToggleRtcmTraffic();

    void
    IOPanel_TogglePacketTraffic();

    void
    IOPanel_ToggleSerialTraffic();

    void
    ModemBox_ProcessIdle();

    void
    ModemBox_ProcessConnect();

    void
    ModemBox_ProcessPacket(
        const utils::CMR&   packet
    );

    void
    ModemBox_ProcessSerial(
        const unsigned char*    data,
        const unsigned int      length
    );
}; // class ModemBoxSimulator

#endif /* ModemBoxSimulator_h_ */
