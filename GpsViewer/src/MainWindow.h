#ifndef Main_Window_h_
#define Main_Window_h_


#include <utils/util.h>
#include <utils/myrtcm.h>
#include <utils/ugl.h>

#include <utils/NMEA.h>
#include <utils/TcpPollingReader.h>
#include <fxutils/PointStorageViewer.h>
#include <utils/Serial.h> // AsyncIO
#include <utils/rtcm.h>
#include <utils/CMR.h>
#include <utils/MixGPS.h>

#include <fx.h>
#include <fxutils/fxutil.h>
#include <fxutils/MapAreaGL.h>
#include <fxutils/SerialLink.h>

#include <memory>   // std::auto_ptr
#include <string>
#include <vector>
#include <map>

#include "Ship.h"
#include "DigNetMessages.h"
#include "WorkAreaMarking.h"
#include "AutoPointfileLoad.h"

/// Axis point.
typedef struct {
    std::string name;   ///< Name.
    double      x;      ///< X-coordinate (Lambert-Est)
    double      y;      ///< Y-coordinate (Lambert-Est)
} AxisPoint;

/// Axis...
typedef struct Axis_st {
    std::vector<AxisPoint>  points;     /// Array of \c AxisPoint
    utils::ugl::VPointArray points_gl;  /// Copy of points in OpenGL memory space.
    Axis_st() : points_gl(GL_LINE_STRIP) {}
} Axis;

//=====================================================================================//
/// GpsViewer Main Window.
class MainWindow : public FXMainWindow {
    FXDECLARE(MainWindow)
protected:
            MainWindow() : cfg_("") {}
public:
    typedef FXMainWindow    inherited;

    /** View mode. */
    typedef enum {
        /** Normal mode. */
        VIEWMODE_NORMAL     = 0,
        /** Worklog marking mode. */
        VIEWMODE_WORKLOG    = 1,
        /** Voltage viewing mode for GpsViewer without modembox */
        VIEWMODE_VOLTAGES   = 2
    } VIEWMODE;

    /** Pan/Mark mode. */
    typedef enum {
        WORKLOGMODE_MARK    = 0,
        WORKLOGMODE_PAN     = 1
    } WORKLOGMODE;

    /** Constructor
        @param title title string for application window
      */
    MainWindow(
        FXApp* a, 
        const FXString& title,
        const bool log = false
    );

    // Destructor.
    virtual
    ~MainWindow();

    // Messages for our class
    enum {
        ID_SERIAL_POLL = FXMainWindow::ID_LAST,
        ID_TIMER_1HZ,
        ID_MAP_AREA,
        ID_WORK_AREA,
        ID_TOGGLE_BACKGROUND,

        ID_TOGGLE_BARGE,
        ID_ZOOM_IN,
        ID_ZOOM_OUT,
        ID_SAVE_POSITION,
        ID_TOGGLE_VIEWMODE,
        ID_TOGGLE_WORKMARK,
        ID_TOGGLE_WORKLOGMODE,
        ID_FOLLOW_NEXT_SHIP,
        ID_TOGGLE_WORK_AREA,
        ID_EXIT
    };

    /** Write DigNet message to TCP and serial port. If port is not opened message is not written there.
    */
    void
    WriteDignetMessage(
        const std::string&              msg
    );
    void

    /** Write any data to TCP and serial port. If port is not opened message is not written. 
    */
    WritePacket(
        const void*                     data,
        const int                       size,
        const int                       msg_type
    );

    /** Are we working with ModemBox?
    This is true only iff ModemBox is specified on the initialization file.
    */
    bool
    IsModemboxSpecified() const;

    /** Handles all incoming data. */
    void
    dataParse(
        utils::CMRDecoder&  cmrdecoder,
        const char*         data,
        const unsigned int  size,
        const bool          accept_self
    );

    // Returns given ship, 0 if not found
    Ship*
    findShip(
        const int           id
    ) const;

    /** Setup Control Panel (buttons and widgets on the right) according to:
    1. modembox connection - and our ship name.
    2. server link - if any.
    */
    void
    setupControlPanel();

    // Async read callback
    void        onSerialInput(const char* data, const unsigned int len);

    long        onNetworkDataRead(FXObject*,FXSelector,void*);
    /// Poll GPS devices and TCP/IP connections.
    long        onSerialPoll(FXObject*,FXSelector,void*);
    
    /// Gets executed once per second
    long        onTimer1Hz(FXObject*, FXSelector, void*);

    /// Paint map area.
    long        onPaintMapArea(FXObject*,FXSelector,void*);
    long        onUpdMapArea(FXObject*,FXSelector,void*);
    /// Paint work area.
    long        onPaintWorkArea(FXObject*,FXSelector,void*);
    long        onUpdWorkArea(FXObject*,FXSelector,void*);

    /// Change background according to message selector.
    long        onCmdToggleBackground(FXObject*,FXSelector,void*);

    /// Handle key releases. This is used to implement hotkeys on space and backspace keys.
    long        onKeyRelease(FXObject*,FXSelector,void*);

    /// Toggle barge on or off (only on tugboats).
    long        onCmdToggleBarge(FXObject*,FXSelector,void*);

    void        updateShipZooms();
    void        focusShip();

    /// Zoom in
    long        onCmdZoomIn(FXObject* sender, FXSelector sel, void* ptr);
    /// Zoom out
    long        onCmdZoomOut(FXObject* sender, FXSelector sel, void* ptr);
    /// Leave ship position as shadow
    long        onCmdSavePosition(FXObject* sender, FXSelector sel, void* ptr);
    /// Toggle viewmode - only works on dredgers.
    long        onCmdToggleViewmode(FXObject* sender, FXSelector sel, void* ptr);
    /// Toggle current workmark - only works on dredgers.
    long        onCmdToggleWorkmark(FXObject* sender, FXSelector sel, void* ptr);
    /// Toggle worklogmode - only works on dredgers.
    long        onCmdToggleWorklogmode(FXObject* sender, FXSelector sel, void* ptr);
    /// MapArea: place an object (left mouse button press) or start dragging.
    long        onLeftButtonPressMaparea(FXObject* sender, FXSelector sel, void* ptr);
    /// MapArea: stop draggin when in pan mode.
    long        onLeftButtonReleaseMaparea(FXObject* sender, FXSelector sel, void* ptr);
    /// MapArea: update object coordinates.
    long        onMotionMaparea(FXObject* sender, FXSelector sel, void* ptr);
    /// Start following next ship
    long        onCmdFollowNextShip(FXObject* sender, FXSelector sel, void* ptr);
    /// Toggle showing work area
    long        onCmdToggleWorkArea(FXObject* sender, FXSelector, void* ptr);
    /// Quit the program.
    long        onCmdExit(FXObject*,FXSelector,void*);

    // Initialize
    virtual void    create();

    // Handles window resizing
    virtual void resize(int w, int h);

    // Returns >0 if any warnings are to be displayed
    int         IsWarningRequired();

    /** Updates given ship voltage information on screen */
    void UpdateVoltages(const Ship& ship);

    /** Updates displayed speed information on screen */
    void UpdateSpeed();

    /** Builds and sets window title  */
    void SetTitle();

	/** Load indexed points file.
	Sets modembox_disabled_ to true when the file is "poisoned"

	Note: poisoned file magic is 0x43 A8 01 6F.
	*/
	void load_indexed_points(
		std::auto_ptr<fxutils::PointStorageViewer>&	ps_viewer,
		const std::string&							filename
		);
private:

    /* ======================== MODEMBOX ============================== */
    std::string                         modembox_portname_;
    std::auto_ptr<fxutils::SerialLink>  modembox_io_;
    utils::TcpPollingReader             modembox_link_;
    utils::CMRDecoder                   modembox_cmr_decoder_;
    unsigned int                        modembox_delta_time_;   ///< time since last data, seconds.
    utils::LineDecoder                  gps_decoder_[2];        ///< Decodes data coming from GPS'es
    std::auto_ptr<utils::MixGPS>        mixgps_;
    utils::MixGPSXY                     viewpoint_;
	bool								modembox_disabled_;		///< Ignore modembox input when disabled.

    /* ========================= SERVER =============================== */
    utils::TcpPollingReader             server_link_;
    std::string                         server_ip_port_;
    utils::TcpPollingReader::STATUS     server_link_laststatus_;///< Last known status of TCP link.
    utils::CMRDecoder                   server_cmr_decoder_;

    /* ========================= TIMERS =============================== */
    int                                 timer_10s_;             ///< Counts numbers from 10 ... 0.
    int                                 timer_60s_;             ///< Counts numbers from 60 ... 0.
    int                                 timer_3600s_;           ///< Counts from 3600 ... 0.
    int                                 time_since_startup_;    ///< Increases by one every second

    /* ========================= SHIPS ================================ */
    /// All known ships are here
    std::vector<Ship*>  ships_;
    /// Ship to be followed, pointer to the \c ships_ element
    Ship*               followed_ship_;
    /// Pointer to our own ship, real object will be in the \c ships_ list
    Ship*               our_ship_;
    /// Our ship ID
    PACKET_SHIP_ID      ship_id;

    /* ===================== WORKAREA MARKING ========================= */
    VIEWMODE                        viewmode_;
    WorkAreaMarking                 workarea_;
    FXButton*                       button_worklog_togglemark_[WORKMARK_SIZE];
    FXSwitcher*                     button_worklog_switcher_;
    WORKMARK                        workmark_current_;
    utils::Option<MatrixIndices>    workmark_indices_;
    /** Index into workarea_.MarkSources. */
    unsigned int                    workmark_update_index_;
    FXSwitcher*                     button_worklogmode_switcher_;
    WORKLOGMODE                     worklogmode_;
    double                          map_rotation_angle_;

    FXLabel*                        workarea_gpstime_lbl_;
    FXLabel*                        workarea_packets_lbl_;
    unsigned int                    workarea_packets_;

    fxutils::MapAreaOptions         map_area_options_;
    utils::FileConfig               cfg_;

    /* =========== CONTROL PANEL: BUTTONS FRAME ======================= */
    /** switcher frame. 0=VIEWMODE_NORMAL, 1=VIEWMODE_WORKLOG, 2=VIEWMODE_VOLTAGES. */
    FXSwitcher*             frame_switcher_;

    FXVerticalFrame*        frame_buttons_;
    FXHorizontalFrame*      frame_buttons_mini_;    ///< dredger special: save pos & toggle marking view
    FXButton*               button_toggle_barge_;
    FXButton*               button_save_position_;
    FXButton*               button_worklog_toggle_view_;
    FXButton*               button_next_ship_;

    FXLabel*                label_dig_count_;
    FXLabel*                label_fill_count_;

    FILE*                   log_file_;
    utils::ugl::VPointArray markers_;
    Axis                    axis1_;
    Axis                    axis2_;
    utils::ugl::VPointArray nearest_point_;
    /** If bigger than zero, start counting down on every GPS position.
    When zero, shutdown and exit. */
    int                     shutdown_counter_;

    utils::ugl::VPointArray points_;        ///< Just points on the GL area.

    FXFont*                                     myfont_;
    fxutils::MapAreaGL*                         map_area_;
    std::auto_ptr<fxutils::PointStorageViewer>  ps_viewer_;
    std::auto_ptr<fxutils::PointStorageViewer>  dynamic_points_;    ///< Modified parts of the map are held here
    FXLabel*            xcoordinate_text_lbl_;
    FXLabel*            xcoordinate_lbl_;
    FXLabel*            ycoordinate_text_lbl_;
    FXLabel*            ycoordinate_lbl_;
    FXLabel*            gps_status_lbl_;
    FXLabel*            gpstime_lbl_;
    FXLabel*            axis_name_text_lbl_;
    FXLabel*            axis_name_lbl_;
    FXLabel*            axis_progress_lbl_;
    FXLabel*            axis_distance_lbl_;
    FXLabel*            axis_direction_lbl_;
    FXLabel*            speed_lbl_;
    FXLabel*            speed_txt_lbl_;
    FXLabel*            demo_speed_lbl_;            ///< Demo program uses different layout and labels cannot be added to multiple containers

    // Labels for weather information.
    FXLabel*            wind_speed_text_lbl_;
    FXLabel*            wind_speed_lbl_;
    FXLabel*            gust_speed_text_lbl_;
    FXLabel*            gust_speed_lbl_;
    FXLabel*            wind_direction_text_lbl_;
    FXLabel*            wind_direction_lbl_;
    FXLabel*            water_level_text_lbl_;
    FXLabel*            water_level_lbl_;


    /* ======== Viewer without modembox =========== */
    FXMatrix*           voltages_matrix_;           ///< all voltage texts are inserted here
    FXVerticalFrame*    voltage_frame_buttons_;     ///< Buttons
    std::map<int, std::pair<FXLabel*, FXLabel*> > voltage_value_lbl_;   ///< Voltages as name/value pair

    unsigned int        background_index_;
    bool                show_work_area_;            ///< Show dig marks if true

    /* ============================== WARNING TEXTS ========================= */
    std::auto_ptr<utils::ugl::BitmapFont>   warning_font_;

    // Name of build. Contains project name, build number, build revision and build date
    std::string         build_name_;

    // Currently loaded pointfile information
    std::string         loaded_pointfile_name_;
    int                 loaded_pointfile_number_; ///< File number
    AutoPointfileLoad   pointfile_downloader_;    ///< Class dealing with pointfile downloading

    // Weather information
    
    int16_t                 water_level_;           ///< Water level (cm)
    uint16_t                wind_direction_;        ///< Wind direction (deg, 0 at North, clockwise)
    double                  wind_speed_;            ///< Wind speed (m/s)
    double                  gust_speed_;            ///< Gust speed (m/s)
    utils::Option<uint32_t> last_weather_read_time_;///< Time of last measurement, seconds from epoch
};

#endif // MAIN_WINDOW_H
