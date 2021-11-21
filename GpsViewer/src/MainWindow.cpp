#include <winsock2.h>    // this has to be first, otherwise windows.h will complain :(

#include <utils/math.h>
#include <utils/util.h>
#include <utils/myrtcm.h>
#include <utils/ugl.h>
#include <utils/mystring.h>

#include <utils/NMEA.h>
#include <utils/TcpPollingReader.h>
#include <fxutils/PointStorageViewer.h>
#include <utils/Serial.h> // AsyncIO
#include <utils/rtcm.h>
#include <utils/CMR.h>
#include <utils/MixGPS.h>
#include <utils/Transformations.h>

#include <fx.h>
#include <fxutils/fxutil.h>
#include <fxutils/MapAreaGL.h>
#include <fxutils/SerialLink.h>

#include <memory>   // std::auto_ptr
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <time.h>

#include "Ship.h"
#include "DigNetMessages.h"

#include "MainWindow.h"

#include "resources.h"  // resources::zoom_in, resources::zoom_out
#include "hydroicon.h"

#include "config.h"
#include "version.h"
#include "AutoPointfileLoad.h"

static const double    limit_x1 = GPSVIEWER_X1;
static const double    limit_x2 = GPSVIEWER_X2;
static const double    limit_y1 = GPSVIEWER_Y1;
static const double    limit_y2 = GPSVIEWER_Y2;

using namespace utils;
using namespace fxutils;


/// Serial poll timer, milliseconds.
#define     TIMEOUT_SERIAL_POLL         100
/// Time to reconnect, ms.
#define     COUNTDOWN_SERIAL_READER     (15000 / TIMEOUT_SERIAL_POLL)
/// Time to reconnect after error, ms
#define     COUNTDOWN_SERIAL_READER_ERROR_RECONNECT     (5000 / TIMEOUT_SERIAL_POLL)
/// Time betwen pings sent, ms.
#define     COUNTDOWN_TCPSEND_PING      (5000 / TIMEOUT_SERIAL_POLL)
/// Voltage is sent once per 5 minutes. If no message is recieved for 12 minutes show a warning
#define     COUNTDOWN_MODEMBOX_VOLTAGE  (12*60)

#define    FILENAME_LOG             "log.txt"
#define    FILENAME_AXIS1           "axis.txt"
#define    FILENAME_AXIS2           "axis2.txt"
#define    FILENAME_PROJECT         "Project.dxf"
#define    FILENAME_CONFIGURATION   "GpsServer.ini"

static const char*  FILENAME_WORKAREALOG  = "WorkAreaLog.ini";

#define    EVENTNAME_AUTH        "client"

char * GPS_STATUS_STR[] ={
    "No fix",
    "Fix",
    "Differential"
};
const unsigned int    color_black    = 0x00000000;

static const FXColor            POINTS_COLOR =    FXRGB(255,255,255);
static const unsigned int       POINTS_FONTSIZE = 12;
static const FXColor            BACKGROUNDS[] = {
    FXRGB(0xFF, 0xFF, 0xFF),
    FXRGB(0x4f, 0x4f, 0x4f),
    FXRGB(0, 0, 0)
};

static const unsigned int   BUTTON_FIXHEIGHT    = 120;

//=====================================================================================//
/// Set font and return argument.
template <class X>
X*
apply_font(     X*    obj,
                FXFont*    f)
{
    obj->setFont(f);
    return obj;
}

//=====================================================================================//
/// Ilmakaare nimi (N, NNE jne.) kraadide põhjale põhjasuunast.
static const char*
name_of_direction(    const double    dir)
{
    static const char*    names[17] = {
        "N",        // 0
        "NNE",      // 1
        "NE",       // 2
        "NEE",      // 3
        "E",        // 4
        "SEE",      // 5
        "SE",       // 6
        "SSE",      // 7
        "S",        // 8
        "SSW",      // 9
        "SW",       // 10
        "SWW",      // 11
        "W",        // 12
        "NWW",      // 13
        "NW",       // 14
        "NNW",      // 15
        "N"         // 16
    };

    double    mydir = dir;
    while (mydir < 0)
        mydir += 360.0;

    const int    idx = round(mydir / 22.5);
    if (idx<0 || idx>=17) {
        return names[0];
    }
    return names[idx];
}

//=====================================================================================//
/// Draw a circle.
/// And a little dot inside (glSetPointSize).
void
set_circle(    ugl::VPointArray&    vpoints,
        const double        x,
        const double        y,
        const double        radius,
        const double        r,
        const double        g,
        const double        b)
{
    unsigned int    i;

    vpoints.clear();
    for (i=0; i<=10; ++i) {
        const double    dx = radius * cos(deg2rad(i*36));
        const double    dy = radius * sin(deg2rad(i*36));
        vpoints.append(y+dy, x+dx, 0, r, g, b);
    }
    vpoints.insert_break();

    const double    radius2 = 0.25;
    for (i=0; i<=10; ++i) {
        const double    dx = radius2 * cos(deg2rad(i*36));
        const double    dy = radius2 * sin(deg2rad(i*36));
        vpoints.append(y+dy, x+dx, 0, r, g, b);
    }
}


//=====================================================================================//
/// Read axis file.
static void
read_axis_file(    Axis&            axis,
           const std::string&    filename)
{
    try {
        std::vector<std::string>    lines;
        std::vector<std::string>    v;
        hxio::read_lines(lines, filename);

        for (unsigned int i=0; i<lines.size(); ++i) {
            split(lines[i], ",", v);
            double        x = 0;
            double        y = 0;
            if (v.size()>=3 && double_of(v[1], x) && double_of(v[2], y)) {
                // Yeah, append axis element.
                AxisPoint    apt;
                apt.name = v[0];
                apt.x = x;
                apt.y = y;
                axis.points.push_back(apt);
                axis.points_gl.append(y, x, 0,
                        1.0, 0, 0.0);
            }
        }
    } catch (const std::exception&) {
        // Pass.
    }
}



//=====================================================================================//
// Update axis info on the screen.
static void
update_axis_info(    std::vector<AxisPoint>&    axis_points,
            double&             best_distance_so_far,    // update only if \c best_distance_so_far<0 OR \c distance_this_round<\cbest_distance_so_far
            const bool          final_call,        // is it final call? is it OK to clear labels?
            const double        x,
            const double        y,
            FXLabel*            name_lbl,
            FXLabel*            progress_lbl,
            FXLabel*            distance_lbl,
            FXLabel*            deviation_lbl,
            ugl::VPointArray&   nearest_point,
            bool                hack_piirisaare
            )
{
    int    best_idx    = -1;
    double    best_distance    = -1;
    double    best_xx        = 0;
    double    best_yy        = 0;
    for (unsigned int i=1; i<axis_points.size(); ++i) {
        const AxisPoint&    pt1 = axis_points[i-1];
        const AxisPoint&    pt2 = axis_points[i];
        double            xx = 0;
        double            yy = 0;
        if (point_line_intersect(pt1.x, pt1.y, pt2.x, pt2.y, x, y, xx, yy)) {
            const double    line_distance = distance(x, y, xx, yy);
            if (line_distance<best_distance || best_idx<0) {
                best_idx = i-1;
                best_distance = line_distance;
                best_xx = xx;
                best_yy = yy;
            }
        }
    }

    if (best_distance>=0 && (best_distance<best_distance_so_far || best_distance_so_far<0)) {
        best_distance_so_far = best_distance;

        char            xbuf[1024];

        const AxisPoint&    apt = axis_points[best_idx];
        double            progress = distance(apt.x, apt.y, best_xx, best_yy);

        for (int i=0; i<best_idx; ++i) {
            const AxisPoint&    pt1 = axis_points[i];
            const AxisPoint&    pt2 = axis_points[i+1];
            progress += distance(pt1.x, pt1.y, pt2.x, pt2.y);
        }

        name_lbl->setText(fxstring_of_string(apt.name));
        if (hack_piirisaare)
        {
            progress += 50000;
            progress = progress / 100;
            progress_lbl->setText(xsprintf(xbuf, sizeof(xbuf), "%5.1f m", progress));
            distance_lbl->setText(xsprintf(xbuf, sizeof(xbuf), "%5.1f m", best_distance));
        } else {
            progress_lbl->setText(xsprintf(xbuf, sizeof(xbuf), "%3.1f m", progress));
            distance_lbl->setText(xsprintf(xbuf, sizeof(xbuf), "%3.1f m", best_distance));
        }
        const double    angle = rad2deg(atan2(y-best_yy, x-best_xx));
        const char*    dir_name = name_of_direction(angle);
        deviation_lbl->setText(dir_name);
        set_circle(nearest_point, best_xx, best_yy, 0.5, 0.0, 0.0, 1.0);
    }

    if (final_call && best_distance_so_far<0) {
        name_lbl->setText("None");
        progress_lbl->setText("None");
        distance_lbl->setText("None");
        deviation_lbl->setText("None");
#if (0)
        nearest_point.clear();
#endif
    }
}

//=====================================================================================//
template <class FXType>
void
SetSizeToZero(
    FXType* w)
{
    w->setLayoutHints(w->getLayoutHints() | LAYOUT_FIX_HEIGHT|LAYOUT_FIX_WIDTH);
    w->setHeight(0);
    w->setWidth(0);
    w->disable();
}

//=====================================================================================//
/// Append rectangular marker image.

static void
append_marker(    ugl::VPointArray&    vpoints,
        const double        x,
        const double        y)
{
    const double    dx = 0.25;
    const double    dy = 0.25;
    vpoints.append(y-dy, x-dx, 0,    0.0, 1.0, 0.0);
    vpoints.append(y-dy, x+dx, 0,    0.0, 1.0, 0.0);
    vpoints.append(y+dy, x+dx, 0,    0.0, 1.0, 0.0);
    vpoints.append(y+dy, x-dx, 0,    0.0, 1.0, 0.0);
    vpoints.append(y-dy, x-dx, 0,    0.0, 1.0, 0.0);
    vpoints.insert_break();
}

//=====================================================================================//
// Message Map for the Main Window class
FXDEFMAP(MainWindow) MainWindowMap[]={
    //________Message_Type_____________________ID____________Message_Handler_______
    FXMAPFUNC(SEL_TIMEOUT,    MainWindow::ID_SERIAL_POLL,            MainWindow::onSerialPoll),
    FXMAPFUNC(SEL_TIMEOUT,    MainWindow::ID_TIMER_1HZ,            MainWindow::onTimer1Hz),
    
    FXMAPFUNC(SEL_PAINT,    MainWindow::ID_MAP_AREA,            MainWindow::onPaintMapArea),
    FXMAPFUNC(SEL_UPDATE,    MainWindow::ID_MAP_AREA,            MainWindow::onUpdMapArea),
    FXMAPFUNC(SEL_PAINT,    MainWindow::ID_WORK_AREA,            MainWindow::onPaintWorkArea),
    FXMAPFUNC(SEL_UPDATE,    MainWindow::ID_WORK_AREA,            MainWindow::onUpdWorkArea),

    FXMAPFUNC(SEL_COMMAND,    MainWindow::ID_TOGGLE_BACKGROUND,    MainWindow::onCmdToggleBackground),

    FXMAPFUNC(SEL_KEYRELEASE,0,                                    MainWindow::onKeyRelease),

    FXMAPFUNC(SEL_COMMAND,    MainWindow::ID_TOGGLE_BARGE,        MainWindow::onCmdToggleBarge),
    FXMAPFUNC(SEL_COMMAND,    MainWindow::ID_ZOOM_IN,                MainWindow::onCmdZoomIn),
    FXMAPFUNC(SEL_COMMAND,    MainWindow::ID_ZOOM_OUT,            MainWindow::onCmdZoomOut),
    FXMAPFUNC(SEL_COMMAND,    MainWindow::ID_FOLLOW_NEXT_SHIP,    MainWindow::onCmdFollowNextShip),
    FXMAPFUNC(SEL_COMMAND,    MainWindow::ID_EXIT,                MainWindow::onCmdExit),
    FXMAPFUNC(SEL_COMMAND,    MainWindow::ID_SAVE_POSITION,       MainWindow::onCmdSavePosition),
    FXMAPFUNC(SEL_COMMAND,    MainWindow::ID_TOGGLE_VIEWMODE,     MainWindow::onCmdToggleViewmode),
    FXMAPFUNC(SEL_COMMAND,    MainWindow::ID_TOGGLE_WORKMARK,     MainWindow::onCmdToggleWorkmark),
    FXMAPFUNC(SEL_COMMAND,    MainWindow::ID_TOGGLE_WORKLOGMODE,      MainWindow::onCmdToggleWorklogmode),
    FXMAPFUNC(SEL_COMMAND,    MainWindow::ID_TOGGLE_WORK_AREA,    MainWindow::onCmdToggleWorkArea),
    FXMAPFUNC(SEL_LEFTBUTTONPRESS,      MainWindow::ID_MAP_AREA,    MainWindow::onLeftButtonPressMaparea),
    FXMAPFUNC(SEL_LEFTBUTTONRELEASE,    MainWindow::ID_MAP_AREA,    MainWindow::onLeftButtonReleaseMaparea),
    FXMAPFUNC(SEL_MOTION,                MainWindow::ID_MAP_AREA,    MainWindow::onMotionMaparea),
};

//=====================================================================================//
// Macro for the ScribbleApp class hierarchy implementation
FXIMPLEMENT(MainWindow,FXMainWindow,MainWindowMap,ARRAYNUMBER(MainWindowMap))

//=====================================================================================//
// Construct a MainWindow
MainWindow::MainWindow(FXApp *a, const FXString& title, const bool log)
:    FXMainWindow(a, title, new FXGIFIcon(a,hydroicon), new FXGIFIcon(a,hydroicon), DECOR_ALL, 0,0, 790, 420)
    ,cfg_("GpsViewer.ini")
	,modembox_disabled_(false)
    ,timer_10s_(0)
    ,timer_60s_(0)
    ,timer_3600s_(0)
    ,time_since_startup_(0)
    ,markers_(GL_LINE_STRIP)
    ,log_file_(0)
    ,shutdown_counter_(-1)
    ,server_link_laststatus_(TcpPollingReader::STATUS_DISCONNECTED)
    ,our_ship_(0)
    ,followed_ship_(0)
    ,viewmode_(VIEWMODE_NORMAL)
    ,worklogmode_(WORKLOGMODE_MARK)
    /* === BUTTONS FRAME === */
    ,frame_buttons_(0)
    ,frame_buttons_mini_(0)
    ,button_toggle_barge_(0)
    ,button_save_position_(0)
    ,button_worklog_toggle_view_(0)
    ,button_next_ship_(0)
    ,workarea_(FILENAME_WORKAREALOG)
    ,workmark_current_(WORKMARK_EMPTY)
    ,workmark_update_index_(0)
    ,background_index_(0)
    ,map_rotation_angle_(0)
    ,workarea_packets_(0)
    ,show_work_area_(true),
    loaded_pointfile_number_(-1)
{
    const int        opt_right    = JUSTIFY_RIGHT|LAYOUT_FILL_X|LAYOUT_SIDE_LEFT;
    const int        opt_left    = JUSTIFY_LEFT|LAYOUT_FILL_X|LAYOUT_SIDE_RIGHT;
    const unsigned int    button_opts    = FRAME_THICK|FRAME_RAISED|LAYOUT_TOP|LAYOUT_LEFT|LAYOUT_FILL_X;
    const unsigned int    pl = 10;
    const unsigned int    pr = 10;
    const unsigned int    pt = 5;
    const unsigned int    pb = 5;

    // Set the the window title
    build_name_ = ssprintf("%s build %d version %s", BUILD_NAME, BUILD_NUMBER, BUILD_VERSION);
    SetTitle();
    TRACE_PRINT("info", ("Version info: name: %s build: %d version: %s date: %s", BUILD_NAME, BUILD_NUMBER, BUILD_VERSION, BUILD_DATE));

    // Create log file, and-or append.
    if (log) {
        log_file_ = fopen(FILENAME_LOG, "a");
        if (log_file_ == 0) {
            TRACE_PRINT("error", ("Log file could not be opened."));
            FXMessageBox::error(this, MBOX_OK, "GPS Viewer error.", "Log file could not be opened.");
            onCmdExit(0, 0, 0);
        }
    }

    ship_id.ship_number=255;
    strcpy(ship_id.ship_name, "Peeter");

    // Load initialization file.
    try {
        cfg_.load();
    } catch (const std::exception&) {
        // pass
    }

    cfg_.set_section("GPSViewer");
    // modembox and stuff.
    cfg_.get_string("ModemBox",  "", modembox_portname_);
    cfg_.get_string("GpsServer", "", server_ip_port_);

    // map_area_options_
    cfg_.get_uint("PointsColor", color_black, map_area_options_.pointColor);
    cfg_.get_double("MapRotationAngle", 0, map_rotation_angle_);
    map_area_options_.RotationAngle = map_rotation_angle_;


    std::string name;
    int number;
    double g1dx, g1dy, g2dx, g2dy;
    if (Ship::LoadMyShip(cfg_, number, name, g1dx, g1dy, g2dx, g2dy))
    {
        our_ship_= new Ship(number);
        our_ship_->Name=name;
        our_ship_->voltage_received_countdown = COUNTDOWN_MODEMBOX_VOLTAGE;
        // modembox minimum voltage
        cfg_.get_double("ModemboxMinimumVoltage", 12, our_ship_->modembox_minimum_voltage);

        ships_.push_back(our_ship_);
        followed_ship_=our_ship_;
        strcpy(ship_id.ship_name,name.c_str());
        ship_id.ship_number=number;
        our_ship_->Has_Valid_Name = true;

        MixGPSXY                mixgps_viewpoint;
        std::vector<MixGPSXY>    mixgps_gps(2);                            
        mixgps_gps[0].x = g1dx;
        mixgps_gps[0].y = g1dy;
        mixgps_gps[1].x = g2dx;
        mixgps_gps[1].y = g2dy;
        mixgps_viewpoint.x = 0;
        mixgps_viewpoint.y = 0;
        mixgps_ = std::auto_ptr<MixGPS>(new MixGPS(mixgps_viewpoint, mixgps_gps));        
        TRACE_PRINT("info", ("Loaded %d:%s from .ini", number, name.c_str()));
    } else {
        TRACE_PRINT("info", ("MyShip not found in .ini!"));
    }

    // Big fat font.
    {
        FXFontDesc    fontdesc;
        getApp()->getNormalFont()->getFontDesc(fontdesc);
        fontdesc.size=150;
        myfont_ = new FXFont(getApp(),fontdesc);
    }

    FXHorizontalFrame*    contents = new FXHorizontalFrame(this,LAYOUT_SIDE_TOP|LAYOUT_FILL_X|LAYOUT_FILL_Y,0,0,0,0, 0,0,0,0);

    // LEFT pane or the map area.
    {

        map_area_ = new MapAreaGL(
            contents, LAYOUT_FILL_X|LAYOUT_FILL_Y|LAYOUT_TOP|LAYOUT_LEFT, &map_area_options_,
            0,0,0,0, 0,0,0,0, 0,0);
        map_area_->setBoat(false);
        map_area_->setBackgroundColor(BACKGROUNDS[background_index_]);
        // map_area_options_
        ps_viewer_ = std::auto_ptr<PointStorageViewer>(new PointStorageViewer(map_area_, 1));
        dynamic_points_ = std::auto_ptr<PointStorageViewer>(new PointStorageViewer(map_area_, 1));
        map_area_->mihkels.push_back(ps_viewer_->mihkel());
        map_area_->mihkels.push_back(dynamic_points_->mihkel());
        if (followed_ship_ != 0) {
            map_area_->setFollowingMode(true);
        }
    }

    // Switcher panel first! :)
    frame_switcher_ = new FXSwitcher(contents, LAYOUT_FILL_Y|LAYOUT_TOP|LAYOUT_LEFT, 0,0,0,0, 0,0,0,0);

    // VIEWMODE_NORMAL: RIGHT pane for the buttons.
    {
        FXVerticalFrame*    frame_control_panel = new FXVerticalFrame(
            frame_switcher_,LAYOUT_FILL_Y|LAYOUT_TOP|LAYOUT_LEFT, 0,0,0,0,1,1,1,1, 1,1);
        frame_buttons_ = new FXVerticalFrame(
            frame_control_panel, LAYOUT_FILL_X|LAYOUT_TOP|LAYOUT_LEFT, 0,0,0,0, 0,0,0,0, 0,0);

        new FXHorizontalSeparator(frame_control_panel, SEPARATOR_GROOVE|LAYOUT_FILL_X);

        // Info labels.
        {
            FXMatrix*    mtx = new FXMatrix(frame_control_panel, 2, LAYOUT_SIDE_TOP|LAYOUT_FILL_X|LAYOUT_TOP|MATRIX_BY_COLUMNS);

            xcoordinate_text_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "X-coord.:", 0, opt_left), myfont_);
            xcoordinate_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);

            ycoordinate_text_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "Y-coord.:", 0, opt_left), myfont_);
            ycoordinate_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);

            apply_font<FXLabel>(new FXLabel(mtx, "GPS status:", 0, opt_left), myfont_);
            gps_status_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);

            apply_font<FXLabel>(new FXLabel(mtx, "UTC time:", 0, opt_left), myfont_);
            gpstime_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);

            // new FXHorizontalSeparator(button_frame, SEPARATOR_GROOVE|LAYOUT_FILL_X);

            axis_name_text_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "Axis name:", 0, opt_left), myfont_);
            axis_name_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);

            apply_font<FXLabel>(new FXLabel(mtx, "Progress:", 0, opt_left), myfont_);
            axis_progress_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);

            apply_font<FXLabel>(new FXLabel(mtx, "Distance:", 0, opt_left), myfont_);
            axis_distance_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);

            apply_font<FXLabel>(new FXLabel(mtx, "Away to:", 0, opt_left), myfont_);
            axis_direction_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);

            speed_txt_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "Speed:", 0, opt_left), myfont_);
            speed_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);
            
            wind_speed_text_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "Wind speed:", 0, opt_left), myfont_);
            wind_speed_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);

            gust_speed_text_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "Gust speed:", 0, opt_left), myfont_);
            gust_speed_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);

            wind_direction_text_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "Wind direction:", 0, opt_left), myfont_);
            wind_direction_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);

            water_level_text_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "Water level:", 0, opt_left), myfont_);
            water_level_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);
        }

        new FXHorizontalSeparator(frame_control_panel, SEPARATOR_GROOVE|LAYOUT_FILL_X);

        // Zoom-in/Zoom-out buttons.
        {
            FXVerticalFrame* vf1 = new FXVerticalFrame(frame_control_panel,LAYOUT_FILL_Y|LAYOUT_FILL_X, 0,0,0,0, 0,0,0,0, DEFAULT_SPACING, 0);
            apply_font<FXButton>(new FXButton(
                vf1, "Toggle Work A&rea", NULL,
                this, ID_TOGGLE_WORK_AREA, button_opts|LAYOUT_FIX_HEIGHT|LAYOUT_FILL_ROW|LAYOUT_FILL_X|LAYOUT_FILL_COLUMN, 0,0,0,BUTTON_FIXHEIGHT/3, 10,10,5,5), myfont_);
            FXHorizontalFrame*    hf1 = new FXHorizontalFrame(vf1,
                LAYOUT_FILL_X, 0,0,0,0, 0,0,0,0, DEFAULT_SPACING,0);
            new FXButton(hf1, "\tZoom In",
                new FXGIFIcon(getApp(), resources::zoom_in), this, ID_ZOOM_IN, button_opts, 0,0,0,0, pl,pr,pt,pb);
            new FXButton(hf1, "\tZoom Out",
                new FXGIFIcon(getApp(), resources::zoom_out), this, ID_ZOOM_OUT, button_opts, 0,0,0,0, pl,pr,pt,pb);
        }
    }

    // VIEWMODE_WORKLOG: RIGHT pane for the buttons.
    {
        const unsigned int  button_opts = FRAME_THICK|FRAME_RAISED|LAYOUT_FILL_X|LAYOUT_TOP|LAYOUT_LEFT;

        FXVerticalFrame* frame_buttons = new FXVerticalFrame(
            frame_switcher_, LAYOUT_FILL_X|LAYOUT_TOP|LAYOUT_LEFT, 0,0,0,0, 0,0,0,0);

        {
            FXHorizontalFrame*    hf1 = new FXHorizontalFrame(frame_buttons,
                LAYOUT_FILL_X|PACK_UNIFORM_WIDTH, 0,0,0,0, 0,0,0,0, DEFAULT_SPACING,0);

            {
                button_worklogmode_switcher_ =
                    new FXSwitcher(hf1,
                        LAYOUT_FILL_X|LAYOUT_TOP|LAYOUT_LEFT|LAYOUT_FIX_HEIGHT,
                        0,0,0,BUTTON_FIXHEIGHT, 0,0,0,0);

                apply_font<FXButton>(new FXButton(
                    button_worklogmode_switcher_, "", new FXGIFIcon(a, resources::icon_Pointer),
                    this, ID_TOGGLE_WORKLOGMODE,
                    button_opts|LAYOUT_FIX_HEIGHT|ICON_BEFORE_TEXT,
                    0,0,0,BUTTON_FIXHEIGHT, 10,10,5,5), myfont_);

                apply_font<FXButton>(new FXButton(
                    button_worklogmode_switcher_, "", new FXGIFIcon(a, resources::icon_Hand),
                    this, ID_TOGGLE_WORKLOGMODE,
                    button_opts|LAYOUT_FIX_HEIGHT|ICON_BEFORE_TEXT,
                    0,0,0,BUTTON_FIXHEIGHT, 10,10,5,5), myfont_);
            }

            apply_font<FXButton>(new FXButton(
                hf1, "Toggle\n&Worklog", NULL,
                this, ID_TOGGLE_VIEWMODE, button_opts|LAYOUT_FIX_HEIGHT, 0,0,0,BUTTON_FIXHEIGHT, 10,10,5,5), myfont_);
        }

        // Button to toggle current mark type.
        {
            // this is complicated.
            button_worklog_switcher_ =
                new FXSwitcher(frame_buttons,
                    LAYOUT_FILL_X|LAYOUT_TOP|LAYOUT_LEFT|LAYOUT_FIX_HEIGHT,
                    0,0,0,BUTTON_FIXHEIGHT, 0,0,0,0);
            // workmark_icon_problem.gif
            const unsigned char* button_icons[WORKMARK_SIZE] = {
                resources::workmark_icon_none,
                resources::workmark_icon_dredged,
                resources::workmark_icon_ok,
                resources::workmark_icon_problem
            };
            const char* button_texts[WORKMARK_SIZE] = {
                "Clear",
                "Dredged",
                "OK",
                "Problem!"
            };
            for (unsigned int i=0; i<WORKMARK_SIZE; ++i) {
                button_worklog_togglemark_[i] = apply_font<FXButton>(new FXButton(
                    button_worklog_switcher_, button_texts[i], new FXGIFIcon(a, button_icons[i]),
                    this, ID_TOGGLE_WORKMARK,
                    button_opts|LAYOUT_FIX_HEIGHT|ICON_BEFORE_TEXT,
                    0,0,0,BUTTON_FIXHEIGHT, 10,10,5,5), myfont_);
            }
        }

        // Zoom-in/Zoom-out buttons.
        {
            FXHorizontalFrame*    hf1 = new FXHorizontalFrame(frame_buttons,
                LAYOUT_FILL_X, 0,0,0,0, 0,0,0,0, DEFAULT_SPACING,0);
            new FXButton(hf1, "\tZoom In",
                new FXGIFIcon(getApp(), resources::zoom_in), this, ID_ZOOM_IN, button_opts, 0,0,0,0, pl,pr,pt,pb);
            new FXButton(hf1, "\tZoom Out",
                new FXGIFIcon(getApp(), resources::zoom_out), this, ID_ZOOM_OUT, button_opts, 0,0,0,0, pl,pr,pt,pb);
        }

        {
            FXMatrix*    mtx = new FXMatrix(frame_buttons, 2, LAYOUT_SIDE_TOP|LAYOUT_FILL_X|LAYOUT_TOP|MATRIX_BY_COLUMNS);

            // workarea_gpstime_lbl_
            apply_font<FXLabel>(new FXLabel(mtx, "Time:", 0, opt_left), myfont_);
            workarea_gpstime_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);

            // workarea_packets_lbl_
            apply_font<FXLabel>(new FXLabel(mtx, "Mark packets:", 0, opt_left), myfont_);
            workarea_packets_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);
        }
    }

    // VIEWMODE_VOLTAGES: RIGHT pane for the buttons.
    {
        FXVerticalFrame*    frame_control_panel = new FXVerticalFrame(
            frame_switcher_,LAYOUT_FILL_Y|LAYOUT_TOP|LAYOUT_LEFT, 0,0,0,0,1,1,1,1, 1,1);
        voltage_frame_buttons_ = new FXVerticalFrame(
            frame_control_panel, LAYOUT_FILL_X|LAYOUT_TOP|LAYOUT_LEFT, 0,0,0,0, 0,0,0,0, 0,0);

        new FXHorizontalSeparator(frame_control_panel, SEPARATOR_GROOVE|LAYOUT_FILL_X);

        // Info labels.
        {
            voltages_matrix_ = new FXMatrix(frame_control_panel, 2, LAYOUT_SIDE_TOP|LAYOUT_FILL_X|LAYOUT_TOP|MATRIX_BY_COLUMNS);
            apply_font<FXLabel>(new FXLabel(voltages_matrix_, "Speed:", 0, opt_left), myfont_);
            demo_speed_lbl_ = apply_font<FXLabel>(new FXLabel(voltages_matrix_, "no data", 0, opt_right), myfont_);

            // Labels for voltages are created as data comes from server

        }

        new FXHorizontalSeparator(frame_control_panel, SEPARATOR_GROOVE|LAYOUT_FILL_X);

        // Zoom-in/Zoom-out buttons.
        {
            FXVerticalFrame* vf1 = new FXVerticalFrame(frame_control_panel,LAYOUT_FILL_Y|LAYOUT_FILL_X, 0,0,0,0, 0,0,0,0, DEFAULT_SPACING, 0);
            apply_font<FXButton>(new FXButton(
                vf1, "Toggle Work A&rea", NULL,
                this, ID_TOGGLE_WORK_AREA, button_opts|LAYOUT_FIX_HEIGHT|LAYOUT_FILL_ROW|LAYOUT_FILL_X|LAYOUT_FILL_COLUMN, 0,0,0,BUTTON_FIXHEIGHT/3, 10,10,5,5), myfont_);
            FXHorizontalFrame*    hf1 = new FXHorizontalFrame(vf1,
                LAYOUT_FILL_X, 0,0,0,0, 0,0,0,0, DEFAULT_SPACING,0);
            new FXButton(hf1, "\tZoom In",
                new FXGIFIcon(getApp(), resources::zoom_in), this, ID_ZOOM_IN, button_opts, 0,0,0,0, pl,pr,pt,pb);
            new FXButton(hf1, "\tZoom Out",
                new FXGIFIcon(getApp(), resources::zoom_out), this, ID_ZOOM_OUT, button_opts, 0,0,0,0, pl,pr,pt,pb);
        }
    }

    // Accelerators
    getAccelTable()->addAccel(parseAccel("Alt-F4"), getApp(), MKUINT(FXApp::ID_QUIT, SEL_COMMAND));
    getAccelTable()->addAccel(parseAccel("F5"), this, MKUINT(ID_TOGGLE_BACKGROUND, SEL_COMMAND));
    getAccelTable()->addAccel(parseAccel("A"), this, MKUINT(ID_ZOOM_IN,SEL_COMMAND));
    getAccelTable()->addAccel(parseAccel("Z"), this, MKUINT(ID_ZOOM_OUT,SEL_COMMAND));
    getAccelTable()->addAccel(parseAccel("S"), this, MKUINT(ID_SAVE_POSITION,SEL_COMMAND));
    getAccelTable()->addAccel(parseAccel("T"), this, MKUINT(ID_TOGGLE_BARGE,SEL_COMMAND));
    getAccelTable()->addAccel(parseAccel("N"), this, MKUINT(ID_FOLLOW_NEXT_SHIP,SEL_COMMAND));
    getAccelTable()->addAccel(parseAccel("R"), this, MKUINT(ID_TOGGLE_WORK_AREA,SEL_COMMAND));

    // But what about our FRIENDS????
    {
        // Volume information...
        char    name_buffer[1024] = { 0 };
        char    filesystemname_buffer[1024] = { 0 };
        DWORD    serial_number = 0;
        DWORD    max_component_length = 0;
        DWORD    filesystem_flags = 0;
        const BOOL r_volume = GetVolumeInformation(0,
            name_buffer, sizeof(name_buffer),
            &serial_number, &max_component_length,
            &filesystem_flags, filesystemname_buffer, sizeof(filesystemname_buffer));
#if (0)
        char    xbuf[1024];
        sprintf(xbuf, "client name=GpsViewer Firmware_Version=0.0 Firmware_Date=\"2007-09-02 00:00:00\" USER=%08x BuildInfo=\"BuildInfo Comes Here\"", serial_number);
        tcp_greeting_ = xbuf;
#endif
    }

    setupControlPanel();
    // Follow own ship
    onCmdFollowNextShip(0, 0, 0);
}


//=====================================================================================//
MainWindow::~MainWindow()
{
    for (unsigned int i=0; i<ships_.size(); i++)
    {
        delete ships_[i];
    }
    delete myfont_;
}

//=====================================================================================//
/// Create and initialize GUI widgets.
///
/// Read data files, etc.
void MainWindow::create()
{
    // Create the windows
    inherited::create();

    myfont_->create();

    // Make the main window appear
    show(PLACEMENT_SCREEN);


    // Initiate connection.
    try {
        if (IsModemboxSpecified()) {
            TRACE_PRINT("info", ("Connection: Modembox at port %s", modembox_portname_.c_str()));
            if (toupper(modembox_portname_[0])=='C') {
                // serial port specified.
                modembox_io_ = std::auto_ptr<SerialLink>(
                    new SerialLink(
                        getApp(),
                        modembox_portname_,
                        5000, 60000,
						AsyncIO::ReadCallback([this](const void* data, const int count) { this->onSerialInput((const char*)data, count); })));
            } else {
                modembox_link_.open(modembox_portname_);
            }
            TRACE_PRINT("info", ("Connection to Modembox successful."));
        } else {
            // Start connecting to the server....
            if (server_ip_port_.length()>0) {
                TRACE_PRINT("info", ("Connection: Server at %s", server_ip_port_.c_str()));
                server_link_.open(server_ip_port_);
            }
        }
    } catch (const std::exception& e) {
        FXMessageBox::error(this, MBOX_OK, "GpsViewer Error", "Connection error: %s", e.what());
    }

    read_axis_file(axis1_, FILENAME_AXIS1);
    read_axis_file(axis2_, FILENAME_AXIS2);

    // Read markers file.
    try {
        std::vector<std::string>    lines;
        std::vector<std::string>    v;
        hxio::read_lines(lines, "markers.txt");

        for (int i=lines.size()-1; i>=0; --i) {
            split(lines[i], ",", v);
            double        x = 0;
            double        y = 0;
            if (v.size()>=2 && double_of(v[0], x) && double_of(v[1], y)) {
                // Yeah, append marker.
                append_marker(markers_, x, y);
                break;
            }
        }
    } catch (const std::exception&) {
        // Pass.
    }

    // Read DXF
    try {
        map_area_->readDxfFile(FILENAME_PROJECT);
    } catch (const std::exception&) {
        // Pass.
    }

    try {
        ps_viewer_->load_indexed_points("points.ipt");
    } catch (const std::exception&) {
        // Pass.
    }
    try {
        if (!pointfile_downloader_.GetNewFile(loaded_pointfile_name_)){
            loaded_pointfile_name_="";
        }
        if (loaded_pointfile_name_ != ""){
            load_indexed_points(dynamic_points_, loaded_pointfile_name_);
            loaded_pointfile_number_ = get_pointfile_number(loaded_pointfile_name_);
        }
    } catch (const std::exception& e) {
        TRACE_PRINT("error", ("Unable to loat pointfile %s: %s", loaded_pointfile_name_.c_str(), e.what()));
        loaded_pointfile_name_ = "";
    }
    SetTitle();

    
    // ID_MAP_AREA mihkel.
    {
        MapAreaGL::Mihkel    mihkel = {ID_MAP_AREA, this};
        map_area_->mihkels.push_back(mihkel);
    }
    // ID_WORK_AREA mihkel.
    {
        MapAreaGL::Mihkel    mihkel = {ID_WORK_AREA, this};
        map_area_->mihkelsBefore.push_back(mihkel);
    }

    getApp()->addTimeout(this, ID_SERIAL_POLL, 100);
    getApp()->addTimeout(this, ID_TIMER_1HZ);
    
#if defined(STARTUP_NUMBER_ZOOMOUTS)
    for (int i=0; i<STARTUP_NUMBER_ZOOMOUTS; ++i) {
        map_area_->onZoomOut(0, 0, 0);
    }
#endif

    {
        Point    pt;
        pt.x=(GPSVIEWER_X2+GPSVIEWER_X1)/2;
        pt.y=(GPSVIEWER_Y1+GPSVIEWER_Y2)/2;
        pt.z = 0;
        pt.color = FXRGB(0,0,0);
        pt.direction = 0;
        pt.id = 1;
        pt.signal_ok = true;
        pt.time = my_time_of_now();
#if (0)
        map_area_->addPoint(pt);
#else
        map_area_->addPoint(pt, false, 0);
#endif
    }
    maximize();
}

//=====================================================================================//
void
MainWindow::resize(int w, int h)
{
    inherited::resize(w, h);
    updateShipZooms();
    map_area_->update();
}

//=====================================================================================//
int
MainWindow::IsWarningRequired()
{
    int warnings=0;
    for (std::vector<Ship*>::const_iterator it = ships_.begin();it != ships_.end(); ++it) {
        Ship* s = *it;
        if (s->voltage_received_countdown<=0)
        {
            warnings++;
        }
        if (s->modembox_last_voltage.IsValid() && s->modembox_last_voltage() < s->modembox_minimum_voltage)
        {
            warnings++;
        }
    }
    return warnings;
}

//=====================================================================================//
void 
MainWindow::UpdateVoltages(const Ship& ship)
{
    std::map<int, std::pair<FXLabel*, FXLabel*> >::iterator it = voltage_value_lbl_.find(ship.Number);
    if (it == voltage_value_lbl_.end()){
        const int        opt_right    = JUSTIFY_RIGHT|LAYOUT_FILL_X|LAYOUT_SIDE_LEFT;
        const int        opt_left    = JUSTIFY_LEFT|LAYOUT_FILL_X|LAYOUT_SIDE_RIGHT;

        std::pair<FXLabel*, FXLabel*> el;
        el.first  = apply_font<FXLabel>(new FXLabel(voltages_matrix_, ship.Name.c_str(), 0, opt_left), myfont_);
        el.second = apply_font<FXLabel>(new FXLabel(voltages_matrix_, "", 0, opt_right), myfont_);
        voltage_value_lbl_.insert(make_pair(ship.Number, el));
        it = voltage_value_lbl_.find(ship.Number);
        el.first->create();
        el.second->create();
    }
    char buf[32];
    it->second.second->setText(xsprintf(buf, sizeof(buf), "%2.1fV", ship.modembox_last_voltage));
}

//=====================================================================================//
void
MainWindow::UpdateSpeed()
{
    if (followed_ship_ != 0){
        char buf[16];
        speed_lbl_->setText(xsprintf(buf, sizeof(buf), "%2.1fkn", followed_ship_->GetSpeed()));
        demo_speed_lbl_->setText(buf);
    }
}

//=====================================================================================//
void
MainWindow::SetTitle()
{
    std::string s(build_name_);
    if (loaded_pointfile_number_ != -1){
        s.append(ssprintf(" Pointfile %d", loaded_pointfile_number_));
    }
    const std::string pointfile_info = pointfile_downloader_.Title();
    s.append(pointfile_info);

    // Set weather info
    if (last_weather_read_time_.IsValid()) {
        const uint32_t seconds = float_of_time(my_time_of_now());
        uint32_t timediff = seconds - last_weather_read_time_();
        s.append(ssprintf(" Weather %d minutes old", timediff/60));
    }
    if (followed_ship_!=0){
        s.append(ssprintf(". Following %s", followed_ship_->Name.c_str()));
    }
    setTitle(s.c_str());    
}

//=====================================================================================//
void
MainWindow::load_indexed_points(
	std::auto_ptr<fxutils::PointStorageViewer>&	ps_viewer,
	const std::string&							filename
)
{
	const int	poison_magic = 0x43A8016F;
	int			magic;
	try {
		hxio::IO	f;
		f.open(filename, "rb");
		f.read(&magic, sizeof(magic));
		if (poison_magic == magic) {
			modembox_disabled_ = true;
			return;
		}
		f.close();
	} catch (const std::exception&) {
		// pass.
		return;
	}
	ps_viewer->load_indexed_points(filename);
	modembox_disabled_ = false;
}

//=====================================================================================//
long
MainWindow::onNetworkDataRead(FXObject*, FXSelector, void*)
{
    std::string buffer;
    server_link_.read(buffer);
    if (buffer.length()>0) {
        dataParse(server_cmr_decoder_, buffer.c_str(), buffer.length(), true);
    }
    getApp()->addTimeout(this, ID_SERIAL_POLL, TIMEOUT_SERIAL_POLL);
    return 1;
}

//=====================================================================================//
void
MainWindow::WriteDignetMessage(
    const std::string&              msg
)
{
    TRACE_PRINT("info", ("Sending to server: %s", msg.c_str()));
    WritePacket(msg.c_str(), msg.length(), CMR::DIGNET);
}

//=====================================================================================//
void
MainWindow::WritePacket(
    const void*                     data,
    const int                       size,
    const int                       msg_type
)
{
    TRACE_PRINT("info", ("Sending to server packet type 0x%0x, size %d", msg_type, size));
    std::vector<unsigned char>  buffer;
    CMR::encode(buffer, msg_type, data, size);
    if (IsModemboxSpecified()){
        // Check if connection exists and show warning if it is not?
        if (modembox_io_.get()!=0) {
            modembox_io_->write(buffer);
        }
        if (modembox_link_.opened()) {
            modembox_link_.write(buffer);
        }
    } else {
        server_link_.write(buffer);
    }
}

//=====================================================================================//
bool
MainWindow::IsModemboxSpecified() const
{
    return modembox_portname_.size()>0;
}

//=====================================================================================//
void 
MainWindow::dataParse(
    utils::CMRDecoder&  cmrdecoder,
    const char*         data,
    const unsigned int  size,
    const bool          accept_self
)
{
    try {

    TRACE_ENTER("dataParse");
    std::vector<unsigned char> dat((unsigned char*)data, (unsigned char*)data+size);
    bool    any_packets = false;

    cmrdecoder.feed(dat);
    CMR cmr;
    while(cmrdecoder.pop(cmr)){
        std::string type=cmr.TypeName();
        std::string value=cmr.ToString();
        TRACE_PRINT("info", ("Got CMR packet of type %s, %d bytes: %s", type.c_str(), cmr.data.size(), value.c_str()));
        
        // If receiving forwarded packet then this will change to the number of the ship that forwarded it
        int forwarded_from = our_ship_ != 0 ? our_ship_->Number : -1;
        // Decode forwarded packets in-place
        if (cmr.type == PACKET_FORWARD::CMRTYPE) {
            TRACE_PRINT("Info", ("Got forwarded message"));
            PACKET_FORWARD* p = reinterpret_cast<PACKET_FORWARD*>(&cmr.data[0]);
            cmr.type = p->original_type;
            forwarded_from = p->ship_id;
            // Get payload size
            int packet_length = cmr.data.size() - sizeof(PACKET_FORWARD) +1;
            cmr.data.assign(&p->original_data_block[0], &p->original_data_block[0]+packet_length);
        }
        switch (cmr.type){
        case CMR::PING:
            {
                WritePacket(0, 0, CMR::PING);
            }
            break;
        case CMR::SERIAL:
			if (modembox_disabled_) {
				TRACE_PRINT("Info", ("Got serial, forced to ignore."));
			} else {
				TRACE_PRINT("Info", ("Got serial"));
				if (cmr.data.size()>0 && cmr.data[0]<2) {
					const unsigned int    port_no = cmr.data[0];
					// GPS client is in first byte and encoded as a number starting from 0
					std::string tmp(cmr.data.begin()+1, cmr.data.end());
					utils::LineDecoder&    ld = gps_decoder_[port_no];
					ld.feed(tmp);
				}
			}
            break;
        case CMRTYPE_SHIP_POSITION:
			if (modembox_disabled_) {
				TRACE_PRINT("Info", ("Got ship position, forced to ignore."));
			} else {
                if (cmr.data.size() < sizeof(PACKET_SHIP_POSITION)) {
                    TRACE_PRINT("info", ("Wrong length of Ship position packet! Expected %d, got %d", sizeof(PACKET_SHIP_POSITION), cmr.data.size()));
                    break;
                }
                const PACKET_SHIP_POSITION&        other_pos = *reinterpret_cast<PACKET_SHIP_POSITION*>(&cmr.data[0]);
                TRACE_PRINT("Info", ("Got ship #%d position, flags: %d", other_pos.ship_number, (unsigned char)other_pos.flags));

                if (accept_self || other_pos.ship_number!=ship_id.ship_number) {
                    Ship* other_ship = findShip(other_pos.ship_number);
                    if (!other_ship) {
                        other_ship = new Ship(other_pos.ship_number);
                        other_ship->Number = other_pos.ship_number;
                        other_ship->voltage_received_countdown = COUNTDOWN_MODEMBOX_VOLTAGE;
                        other_ship->modembox_minimum_voltage = 12;
                        if (!other_ship->Has_Valid_Name)
                        {
                            PACKET_REQUEST_FROM_SHIP req;
                            req.ship_number=other_pos.ship_number;
                            req.flags=REQUEST_SHIP_INFO;
                            WritePacket(&req, sizeof(req), CMRTYPE_REQUEST_FROM_SHIP);
                            TRACE_PRINT("info", ("Requesting information on ship # %d form server", req.ship_number));
                            // FIXME: remove after server update
                            req.flags=REQUEST_SHIP_ID;
                            WritePacket(&req, sizeof(req), CMRTYPE_REQUEST_FROM_SHIP);
                            TRACE_PRINT("info", ("Requesting ID of ship #%d form server", req.ship_number));
                        }
                        ships_.push_back(other_ship);
                        updateShipZooms();
                    }

                    // Check if is a shadow packet, if not then use it as regular ship position
                    if (other_pos.flags==SHIP_POSITION_SHADOW)
                    {
                        other_ship->SetShadowPosition(other_pos.gps1_x, other_pos.gps1_y, other_pos.heading);
                        other_ship->SaveShadow();
                    }
                    else
                    {
                        other_ship->SetPosition(other_pos.gps1_x, other_pos.gps1_y, other_pos.heading);
                        // Check if following any ships, if mouse hasn't been dragged and if current packet comes from followed ship
                        if (map_area_->getFollowingMode() &&  followed_ship_!=0 && followed_ship_->Number == other_pos.ship_number)
                        {
                            focusShip();
                        }
                    }
                    any_packets=true;
                }
            }
            break;
        case CMRTYPE_GPS_POSITION:
			if (modembox_disabled_) {
				TRACE_PRINT("Info", ("Got gps position, forced to ignore."));
			} else {
                TRACE_PRINT("info", ("Got GPS!"));
                if (cmr.data.size() < sizeof(PACKET_GPS_POSITION)) {
                    TRACE_PRINT("info", ("Wrong length of GPS packet! Expected %d, got %d", sizeof(PACKET_GPS_POSITION), cmr.data.size()));
                    break;
                }
                PACKET_GPS_POSITION& gps= *reinterpret_cast<PACKET_GPS_POSITION*>(&cmr.data[0]);
                Ship*   ship = findShip(gps.ship_number);
                if (ship!=0) {
                    GpsPosition pos;
                    my_time_of_now(pos.Time);
                    pos.X = gps.x;
                    pos.Y = gps.y;
                    pos.Z = 0;
                    ship->SetGpsPosition(gps.gps_number, pos);
                    any_packets=true;
                    UpdateSpeed();
                }
            }    // case CMRTYPE_GPS_POSITION
            break;
        case CMR::RTCM:
            TRACE_PRINT("info", ("RTCM"));
            break;
        case CMRTYPE_SHIP_ID:
            {
                TRACE_PRINT("info", ("CMRTYPE_SHIP_ID"));
                if (cmr.data.size() < sizeof(PACKET_SHIP_ID)) {
                    TRACE_PRINT("info", ("Wrong length of ship ID packet! Expected %d, got %d", sizeof(PACKET_SHIP_ID), cmr.data.size()));
                    break;
                }
                PACKET_SHIP_ID* id = reinterpret_cast<PACKET_SHIP_ID*>(&cmr.data[0]);
                TRACE_PRINT("info", ("Ship ID request answer received: %d:%s", id->ship_number, id->ship_name));
                Ship* s = findShip(id->ship_number);
                if (s == 0)
                {
                    s = new Ship(id->ship_number);
                    s->Name=id->ship_name;
                    s->voltage_received_countdown = COUNTDOWN_MODEMBOX_VOLTAGE;
                    s->modembox_minimum_voltage = 12;
                    ships_.push_back(s);
                }
                else
                {
                    s->Name=std::string(id->ship_name);
                }
                s->Has_Valid_Name=true;
            } 
            break;
        case CMRTYPE_GPS_OFFSETS:
            {
                TRACE_PRINT("info", ("CMRTYPE_GPS_OFFSETS: Shouldn't be here!"));
                // FIXME: Temporarly put back for testing with old server
                //break;
                // FIXME: remove soon
                
                if (cmr.data.size() < sizeof(PACKET_GPS_OFFSETS)) {
                    TRACE_PRINT("info", ("Wrong length of GPS Offsets packet! Expected %d, got %d", sizeof(PACKET_GPS_OFFSETS), cmr.data.size()));
                    break;
                }
                const PACKET_GPS_OFFSETS*    off = reinterpret_cast<const PACKET_GPS_OFFSETS*>(&cmr.data[0]);
                TRACE_PRINT("info", ("GPS offset request ansvered: %d, %6f, %6f, %6f, %6f",
                    off->ship_number, off->gps1_dx, off->gps1_dy, off->gps2_dx, off->gps2_dy));
                Ship* s = findShip(off->ship_number);
                if (s!=0)
                {
                    if (s == our_ship_) {
                        MixGPSXY                mixgps_viewpoint;
                        std::vector<MixGPSXY>    mixgps_gps(2);
                        bool                    should_change = true;
                        if (mixgps_.get() != 0) {
                            mixgps_->get_config(mixgps_viewpoint, mixgps_gps);
                            should_change =
                                   fabs(mixgps_gps[0].x - off->gps1_dx) > 0.01
                                || fabs(mixgps_gps[0].y - off->gps1_dy) > 0.01
                                || fabs(mixgps_gps[1].x - off->gps2_dx) > 0.01
                                || fabs(mixgps_gps[1].y - off->gps2_dy) > 0.01;
                        }
                        if (should_change) {
                            // default mixgps settings.
                            mixgps_gps[0].x = off->gps1_dx;
                            mixgps_gps[0].y = off->gps1_dy;
                            mixgps_gps[1].x = off->gps2_dx;
                            mixgps_gps[1].y = off->gps2_dy;
                            mixgps_viewpoint.x = 0;
                            mixgps_viewpoint.y = 0;
                            mixgps_ = std::auto_ptr<MixGPS>(new MixGPS(mixgps_viewpoint, mixgps_gps));
                        }
                    }
                }
            }
            break;
        case PACKET_SHIP_INFO::CMRTYPE:
            {
                TRACE_PRINT("info", ("PACKET_SHIP_INFO"));
                PACKET_SHIP_INFO* info = reinterpret_cast<PACKET_SHIP_INFO*>(&cmr.data[0]);
                const size_t packetSize = sizeof(PACKET_SHIP_INFO) - 1 + info->ship_name_length + info->imei_length;
                TRACE_PRINT("info", ("Size: %d name length: %d", packetSize, info->ship_name_length));
                if (cmr.data.size() < packetSize) {
                    TRACE_PRINT("info", ("Wrong length of ship information packet! Expected %d, got %d", packetSize, cmr.data.size()));
                    break;
                }
                std::vector<MixGPSXY>    tmp_gps(2);
                // Convert from cm to m
                tmp_gps[0].x = info->gps1_dx/100.0;
                tmp_gps[0].y = info->gps1_dy/100.0;
                tmp_gps[1].x = info->gps2_dx/100.0;
                tmp_gps[1].y = info->gps2_dy/100.0;

                std::string ship_name;
                ship_name.resize(info->ship_name_length);
                memcpy(&ship_name[0], info->ship_name_imei, ship_name.size());
                TRACE_PRINT("info", ("Ship ID request answer received: %d:%s", info->ship_number, ship_name.c_str()));
                TRACE_PRINT("info", ("GPS offsets: %6f, %6f, %6f, %6f",
                    tmp_gps[0].x, tmp_gps[0].y, tmp_gps[1].x, tmp_gps[1].y));
                Ship* s = findShip(info->ship_number);
                if (s == 0)
                {
                    s = new Ship(info->ship_number);
                    s->Name=ship_name;
                    s->voltage_received_countdown = COUNTDOWN_MODEMBOX_VOLTAGE;
                    s->modembox_minimum_voltage = 12;
                    ships_.push_back(s);
                }
                else
                {
                    s->Name=std::string(ship_name);
                }
                s->Has_Valid_Name=true;

                if (s == our_ship_) {
                    MixGPSXY                mixgps_viewpoint;
                    std::vector<MixGPSXY>    mixgps_gps(2);
                    bool                    should_change = true;
                    if (mixgps_.get() != 0) {
                        mixgps_->get_config(mixgps_viewpoint, mixgps_gps);
                        should_change =
                               fabs(mixgps_gps[0].x - tmp_gps[0].x) > 0.01
                            || fabs(mixgps_gps[0].y - tmp_gps[0].y) > 0.01
                            || fabs(mixgps_gps[1].x - tmp_gps[1].x) > 0.01
                            || fabs(mixgps_gps[1].y - tmp_gps[1].y) > 0.01;
                    }
                    if (should_change) {
                        // default mixgps settings.
                        mixgps_gps[0].x = tmp_gps[0].x;
                        mixgps_gps[0].y = tmp_gps[0].y;
                        mixgps_gps[1].x = tmp_gps[1].x;
                        mixgps_gps[1].y = tmp_gps[1].y;
                        mixgps_viewpoint.x = 0;
                        mixgps_viewpoint.y = 0;
                        mixgps_ = std::auto_ptr<MixGPS>(new MixGPS(mixgps_viewpoint, mixgps_gps));
                    }
                }
            }
            break;
        case CMR::DIGNET:
			if (modembox_disabled_) {
				TRACE_PRINT("Info", ("Got DigNet message, forced to ignore."));
			} else {
                TRACE_PRINT("info", ("Got Dignet message"));
                std::string name;
                std::map<std::string, std::string> params;
                ParseDigNetMessage(name, params, cmr);

                // Find the ship that set the message (could be forwarded from others)
                Ship* s = findShip(forwarded_from);
                if (s == 0 && forwarded_from != -1 && forwarded_from != 100){
                    // Should only happen when modembox connects to server after GpsViewer
                    TRACE_PRINT("info", ("Got dignet message from yet unknown ship: %d. Requesting information", forwarded_from));
                    // Request startup info. Not pretty but should happen EXTREMELY rarely (when modembox/server restarts and no information about the modembox hasn't yet been sent) 
                    //  so it shouldn't be too bad. Alternative would be to request information about that specific ship.
                    /*
                    std::string query(DIGNETMESSAGE_QUERY);
                    query.append(std::string(" ") + DIGNETMESSAGE_STARTUPINFO);
                    WriteDignetMessage(query);
                    */
                    // FIXME: temporarly here until newer server is started
                    PACKET_REQUEST_FROM_SHIP req;
                    req.ship_number=forwarded_from;
                    req.flags=REQUEST_SHIP_INFO;
                    WritePacket(&req, sizeof(req), CMRTYPE_REQUEST_FROM_SHIP);
                    TRACE_PRINT("info", ("Requesting information on ship # %d form server", req.ship_number));
                    req.flags=REQUEST_SHIP_ID;
                    WritePacket(&req, sizeof(req), CMRTYPE_REQUEST_FROM_SHIP);
                    TRACE_PRINT("info", ("Requesting ID of ship #%d form server", req.ship_number));
                    break;
                }
                if (name == DIGNETMESSAGE_MODEMBOX) {
                    double voltage = -1;
                    if (GetArg(params, "supplyvoltage", voltage)) {
                        s->voltage_received_countdown = COUNTDOWN_MODEMBOX_VOLTAGE;
                        s->modembox_last_voltage = voltage;
                        s->last_read_time = my_time_of_now();
                        UpdateVoltages(*s);
                        TRACE_PRINT("info", ("Modembox voltage of '%s' recieved: %3.1f", s->Name.c_str(), voltage));
                    }
                } else if (name==DIGNETMESSAGE_WORKAREA) {
                    WorkArea    wa;
                    if (GetArg(params, wa) && workarea_.SetWorkArea(wa)) {
                        // yes.
                        map_area_->update();
                    }
                } else if (name == DIGNETMESSAGE_VOLTAGE) {
                    int ship_id;
                    Ship* otherShip = 0;
                    if (GetArg(params, "ship", ship_id)) {
                        otherShip = findShip(ship_id);
                        if (otherShip == 0){
                            const std::string msg = cmr.ToString();
                            TRACE_PRINT("info", ("Got DigNet startup message but no such ship is found with given ID. Message: %s", msg.c_str()));
                            break;
                        }
                    }
                    double voltage;
                    if (GetArg(params, "voltage", voltage)){
                        otherShip->modembox_last_voltage = voltage;
                    }
                    double limit;
                    if (GetArg(params, "limit", limit)){
                        otherShip->modembox_minimum_voltage = limit;
                    }
                    int lastRead;
                    if (GetArg(params, "lastRead", lastRead)){
                        otherShip->voltage_received_countdown = COUNTDOWN_MODEMBOX_VOLTAGE - lastRead;
                    }
                    my_time t;
                    int lastReadHour;
                    if (GetArg(params, "h", lastReadHour)){
                        t.hour = lastReadHour;
                    }
                    int lastReadMinute;
                    if (GetArg(params, "m", lastReadMinute)){
                        t.minute = lastReadMinute;
                    }
                    otherShip->last_read_time = t;
                    UpdateVoltages(*otherShip);
                }
            }
            break;
        case PACKET_WORKLOG_ROWS::CMRTYPE:
			if (modembox_disabled_) {
				TRACE_PRINT("Info", ("Got worklog rows, forced to ignore."));
			} else {
				TRACE_PRINT("Info", ("Got worklog rows"));
				if (cmr.data.size()>=sizeof(PACKET_WORKLOG_ROWS)) {
					const PACKET_WORKLOG_ROWS* Packet = reinterpret_cast<PACKET_WORKLOG_ROWS*>(&cmr.data[0]);
					if (Packet->latest_timestamp>0 && workarea_.HandleRowsPacket(Packet, cmr.data.size())) {
						map_area_->update();
						++workarea_packets_;
						char    xbuf[256];
						workarea_packets_lbl_->setText(xsprintf(xbuf, sizeof(xbuf), "%d", workarea_packets_));
					}
				}
			}
            break;
        case PACKET_POINTSFILE_INFO::CMRTYPE:
            // Falltrhrough, pointfile_downloader handles both packets itself
        case PACKET_POINTSFILE_CHUNK::CMRTYPE:
            {
                pointfile_downloader_.HandlePacket(cmr);
                SetTitle();
                break;
            }
        case PACKET_WEATHER_INFORMATION::CMRTYPE:
			if (modembox_disabled_) {
				TRACE_PRINT("Info", ("Got weather information, forced to ignore."));
			} else {
                if (cmr.data.size()<sizeof(PACKET_WEATHER_INFORMATION)){

                    TRACE_PRINT("info", ("Wrong weather info packet size. Got %d, expected %d", cmr.data.size(), sizeof(PACKET_WEATHER_INFORMATION)));
                    break;
                }
                const PACKET_WEATHER_INFORMATION* weather = reinterpret_cast<PACKET_WEATHER_INFORMATION*>(&cmr.data[0]);
                TRACE_PRINT("info", ("Got weather update at %d", weather->time));
                water_level_    = weather->water_level;
                wind_direction_ = weather->wind_direction;
                wind_speed_     = weather->wind_speed/100.0;
                gust_speed_     = weather->gust_speed/100.0;
                last_weather_read_time_ = weather->time;
                
                const int notSet = 0xffff;
                char    xbuf[512];
                if (weather->water_level != notSet){
                    water_level_lbl_->setText(xsprintf(xbuf, sizeof(xbuf), "%d cm", water_level_));
                }
                if (weather->wind_direction != notSet){
                    wind_direction_lbl_->setText(xsprintf(xbuf, sizeof(xbuf), "%3.2d deg", wind_direction_));
                }
                if (weather->wind_speed!= notSet){
                    wind_speed_lbl_->setText(xsprintf(xbuf, sizeof(xbuf), "%3.1f m/s", wind_speed_));
                }
                if (weather->gust_speed!= notSet){
                    gust_speed_lbl_->setText(xsprintf(xbuf, sizeof(xbuf), "%3.1f m/s", gust_speed_));
                }
                SetTitle();
                break;
            }
        default:
            TRACE_PRINT("info", ("Unknown CMR recieved:"));
            break;
        }
    }

    bool    any_our_packets = false;
    bool    quality_ok = true;

    double    tmp_x = 0;
    double    tmp_y = 0;
    // see what GPS'es said
    for (int i=0; i<2; i++){
        my_time time;
        std::string line;
        while(gps_decoder_[i].pop(time, line)){
            TRACE_PRINT("info", ("GPS %d sent message: '%s'", i, line.c_str()));
            GPGGA    gga_i;
            if (parse_gga(line, time, gga_i)) {
                if (our_ship_ != 0) {
                    GpsPosition gps;
                    my_time_of_now(gps.Time);
                    local_of_gps(gga_i.latitude, gga_i.longitude, gga_i.altitude, gps.X, gps.Y, gps.Z);
                    our_ship_->SetGpsPosition(i, gps);
                    UpdateSpeed();
                }

                if (mixgps_.get() == 0) {
                    if (i==0) {
                        double    foo_z;
                        local_of_gps(gga_i.latitude, gga_i.longitude, gga_i.altitude, tmp_x, tmp_y, foo_z);
                        any_our_packets = true;
                        if (gga_i.quality<1) {
                            quality_ok = false;
                        }
                    }
                } else {
                    any_our_packets = true;
                    mixgps_->feed(gga_i, i);

                    PACKET_GPS_POSITION gpspos;
                    gpspos.ship_number=ship_id.ship_number;
                    gpspos.gps_number=i;
                    double    foo_z;
                    local_of_gps(gga_i.latitude, gga_i.longitude, gga_i.altitude, gpspos.x, gpspos.y, foo_z);
                    char    xbuf[512];
                    if (gga_i.quality>=0 && gga_i.quality<=2)
                    {
                        gps_status_lbl_->setText(xsprintf(xbuf, sizeof(xbuf), "%s", GPS_STATUS_STR[gga_i.quality]));
                    }
                    else 
                    {
                        TRACE_PRINT("info", ("Wrong GPS quality: %s", gga_i.quality));
                        gps_status_lbl_->setText(xsprintf(xbuf, sizeof(xbuf), "%s", GPS_STATUS_STR[0]));
                    }
                    if (gga_i.quality<1) {
                        quality_ok = false;
                    }
                    WritePacket(&gpspos, sizeof(gpspos), CMRTYPE_GPS_POSITION);
                }
            }
        }
    }

    if (any_our_packets && quality_ok) {
        utils::my_time    time_now = my_time_of_now();
        double        compass_direction = 0;
        double        gps_direction = 0;
        double        speed = 0;
        double        gps12 = 0;
        double        gps13 = 0;
        double        gps23 = 0;
        GPGGA         gga;

        if (followed_ship_==0 && our_ship_!=0) {
            followed_ship_ = our_ship_;
            focusShip();
        }

        if (mixgps_.get() == 0) {
            // display temporary position?
        } else {
            if (mixgps_->calculate(time_now, gga, compass_direction, gps_direction, speed, gps12, gps13, gps23)) {
                TRACE_PRINT("info", ("MixGPS, compass dir: %6.3f, gps dir: %6.3f speed: %6.3f gps12: %6.3f gps13: %6.3f, GPS23: %6.3f",
                                    compass_direction, gps_direction, speed, gps12, gps13, gps23));
                Point    pt;
                pt.color = color_black;
                pt.direction = 0;
                pt.id = 0;
                pt.signal_ok = true;
                pt.time = gga.computer_time;
                local_of_gps(gga.latitude, gga.longitude, gga.altitude, pt.x, pt.y, pt.z);

                char    xbuf[512];
                xcoordinate_lbl_->setText(xsprintf(xbuf, sizeof(xbuf), "%4.2f", pt.x));
                ycoordinate_lbl_->setText(xsprintf(xbuf, sizeof(xbuf), "%4.2f", pt.y));

                gpstime_lbl_->setText(xsprintf(xbuf, sizeof(xbuf), "%02d:%02d:%02d.%1d", gga.gps_time.hour, gga.gps_time.minute, gga.gps_time.second, gga.gps_time.millisecond / 100));
                workarea_gpstime_lbl_->setText(xbuf);
                TRACE_PRINT("info", ("GPS time: %s", xbuf));

                if (pt.x>=limit_x1 && pt.x<=limit_x2 && pt.y>=limit_y1 && pt.y<=limit_y2) {
                    if (log_file_ != 0) {
                        SonarIO::write_gps_position(log_file_, pt.time, pt.time, gga.latitude, gga.longitude, gga.altitude, 5, gga.quality, gga.pdop);
                        fflush(log_file_);
                    }
                    //map_area_->addPoint(pt);

                    // Generate our ship.
                    if (our_ship_ != 0) {
                        our_ship_->SetPosition(pt.x, pt.y, compass_direction);
                        if (our_ship_ == followed_ship_ && map_area_->getFollowingMode()) {
                            focusShip();
                        }
                    }

                    // Send our position to others.
                    PACKET_SHIP_POSITION    packet;
                    packet.ship_number = ship_id.ship_number; // FIXME: GpsServer fixes it for us.
                    packet.has_barge = our_ship_==0 ? 0 : (our_ship_->Has_Barge ? 0x01 : 0x00);
                    packet.gps1_x = pt.x;
                    packet.gps1_y = pt.y;
                    packet.heading = compass_direction;
                    packet.flags=0;    // Normal position, no extras
                    WritePacket(&packet, sizeof(packet), CMRTYPE_SHIP_POSITION);

                    // Update our lovely labels.
                    double    best_distance_so_far = -1;
                    TRACE_PRINT("info", ("axis info: %f %f", pt.x, pt.y));
                    // If modembox connection exists then do not use the hack. If it doesn't exist then assume TCP connection and use the hack
                    const bool hack = !IsModemboxSpecified();
                    update_axis_info(axis1_.points, best_distance_so_far, false, pt.x, pt.y, axis_name_lbl_, axis_progress_lbl_, axis_distance_lbl_, axis_direction_lbl_, nearest_point_, hack);
                    update_axis_info(axis2_.points, best_distance_so_far, true, pt.x, pt.y, axis_name_lbl_, axis_progress_lbl_, axis_distance_lbl_, axis_direction_lbl_, nearest_point_, hack);
                } else {
                    TRACE_PRINT("info", ("PT out of range: (%1.3f-%1.3f,%1.3f %1.3f-%1.3f,%1.3f)", pt.x, limit_x1, limit_x2, pt.y, limit_y1, limit_y2));
                }
            }
        }
    }
    if (any_packets || any_our_packets) {
        map_area_->update();
    }
    } catch (const std::exception& e) {
        TRACE_PRINT("error", ("E:%s", e.what()));
    }
}

//=====================================================================================//
Ship*
MainWindow::findShip(
    const int   id
) const
{
    for ( unsigned int i = 0; i<ships_.size(); i++ ) {
        if (ships_[i]->Number== id) {
            return ships_[i];
        }
    }
    return 0;
}

//=====================================================================================//
void
MainWindow::setupControlPanel()
{
    // 1. Constants.
    const unsigned long     rbutton_opts = RADIOBUTTON_NORMAL;
    const unsigned int      button_opts = FRAME_THICK|FRAME_RAISED|LAYOUT_FILL_X|LAYOUT_TOP|LAYOUT_LEFT;
    const bool              is_tugboat = our_ship_!=0 && !our_ship_->IsDredger();
    const bool              is_dredger = our_ship_!=0 && our_ship_->IsDredger();

    // 2. Delete old buttons.
    xdelete(button_toggle_barge_);
    xdelete(button_save_position_);
    xdelete(button_worklog_toggle_view_);
    xdelete(button_next_ship_);
    xdelete(frame_buttons_mini_);

    // 3. Create new buttons.
    if (IsModemboxSpecified()) {
        if (is_tugboat) {
            // TUGBOAT
            button_toggle_barge_ = apply_font<FXButton>(new FXButton(
                frame_buttons_, "&Toggle barge", NULL,
                this, ID_TOGGLE_BARGE, button_opts, 0,0,0,0, 10,10,5,5), myfont_);            
            button_save_position_ = apply_font<FXButton>(new FXButton(
                frame_buttons_, "&Save position", NULL,
                this, ID_SAVE_POSITION, button_opts, 0,0,0,60, 10,10,5,5), myfont_);
        } else {
            // MUST BE DREDGER.
            frame_buttons_mini_ = new FXHorizontalFrame(frame_buttons_, LAYOUT_SIDE_TOP|LAYOUT_FILL_X, 0,0,0,0, 0,0,0,0);
            button_save_position_ =
                apply_font<FXButton>(new FXButton(
                    frame_buttons_mini_, "&Save\nposition", NULL,
                    this, ID_SAVE_POSITION, button_opts|LAYOUT_FIX_HEIGHT, 0,0,0,BUTTON_FIXHEIGHT*2/3, 10,10,5,5), myfont_);
            button_worklog_toggle_view_ =
                apply_font<FXButton>(new FXButton(
                    frame_buttons_mini_, "Toggle\n&Worklog", NULL,
                    this, ID_TOGGLE_VIEWMODE, button_opts|LAYOUT_FIX_HEIGHT, 0,0,0,BUTTON_FIXHEIGHT*2/3, 10,10,5,5), myfont_);
        }
    }

    if (is_tugboat || server_ip_port_.size()>0) {
        // Next ship always present except when SimpleGPS is specified.
        button_next_ship_ = apply_font<FXButton>(
            new FXButton(
                frame_buttons_, "&Next ship", NULL, this, ID_FOLLOW_NEXT_SHIP,
                FRAME_THICK|FRAME_RAISED|LAYOUT_FILL_X|LAYOUT_TOP|LAYOUT_LEFT, 0,0,0,0, 10,10,5,5), myfont_);
    }

    if (is_dredger) {
        // Unset unused widgets.
        SetSizeToZero(xcoordinate_text_lbl_);
        SetSizeToZero(xcoordinate_lbl_);
        SetSizeToZero(ycoordinate_text_lbl_);
        SetSizeToZero(ycoordinate_lbl_);
        SetSizeToZero(axis_name_text_lbl_);
        SetSizeToZero(axis_name_lbl_);
        SetSizeToZero(speed_txt_lbl_);
        SetSizeToZero(speed_lbl_);

        SetSizeToZero(wind_speed_text_lbl_);
        SetSizeToZero(wind_speed_lbl_);
        SetSizeToZero(gust_speed_text_lbl_);
        SetSizeToZero(gust_speed_lbl_);
        SetSizeToZero(wind_direction_text_lbl_);
        SetSizeToZero(wind_direction_lbl_);
    }

    if (is_tugboat){
        SetSizeToZero(water_level_text_lbl_);
        SetSizeToZero(water_level_lbl_);
    }
    // Plain GpsViewer without modembox gets to see other ship voltages, set that view
    if (!IsModemboxSpecified()){
        viewmode_ = VIEWMODE_VOLTAGES;
        frame_switcher_->setCurrent(viewmode_);
        apply_font<FXButton>(
            new FXButton(
                voltage_frame_buttons_, "&Next ship", NULL, this, ID_FOLLOW_NEXT_SHIP,
                FRAME_THICK|FRAME_RAISED|LAYOUT_FILL_X|LAYOUT_TOP|LAYOUT_LEFT, 0,0,0,0, 10,10,5,5), myfont_);

        const int        opt_right    = JUSTIFY_RIGHT|LAYOUT_FILL_X|LAYOUT_SIDE_LEFT;
        const int        opt_left    = JUSTIFY_LEFT|LAYOUT_FILL_X|LAYOUT_SIDE_RIGHT;

        wind_speed_text_lbl_        = apply_font<FXLabel>(new FXLabel(voltages_matrix_, "Wind speed:", 0, opt_left), myfont_);
        wind_speed_lbl_             = apply_font<FXLabel>(new FXLabel(voltages_matrix_, "no data", 0, opt_right), myfont_);
        gust_speed_text_lbl_        = apply_font<FXLabel>(new FXLabel(voltages_matrix_, "Gust speed:", 0, opt_left), myfont_);
        gust_speed_lbl_             = apply_font<FXLabel>(new FXLabel(voltages_matrix_, "no data", 0, opt_right), myfont_);
        wind_direction_text_lbl_    = apply_font<FXLabel>(new FXLabel(voltages_matrix_, "Wind direction:", 0, opt_left), myfont_);
        wind_direction_lbl_         = apply_font<FXLabel>(new FXLabel(voltages_matrix_, "no data", 0, opt_right), myfont_);
        water_level_text_lbl_       = apply_font<FXLabel>(new FXLabel(voltages_matrix_, "Water level:", 0, opt_left), myfont_);
        water_level_lbl_            = apply_font<FXLabel>(new FXLabel(voltages_matrix_, "no data", 0, opt_right), myfont_);
    }
}


//=====================================================================================//
void
MainWindow::onSerialInput(const char* data, const unsigned int size)
{
    modembox_delta_time_ = 0;
    dataParse(modembox_cmr_decoder_, data, size, false);
}

//=====================================================================================//
long
MainWindow::onSerialPoll(FXObject*,FXSelector,void*)
{
    bool            any_packets = false;

    // Check TCP input.
    {
        std::string            tcpbuffer;
        server_link_.read(tcpbuffer);
        if (tcpbuffer.length()>0) {
            TRACE_PRINT("info", ("Received data: %d bytes", tcpbuffer.length()));
            dataParse(server_cmr_decoder_, tcpbuffer.c_str(), tcpbuffer.length(), true);
        }
        // Check if we have been connected since last time...
        TcpPollingReader::STATUS    status = server_link_.status();
        if (status != server_link_laststatus_) {
            switch (status) {
            case TcpPollingReader::STATUS_CONNECTED:
                TRACE_PRINT("info", ("GpsViewer: New link, sending greeting."));
                std::string identity(ssprintf("client name=ModemBox IMEI=12345 Firmware_Version=%d Firmware_Date=\"%s\"", BUILD_NUMBER, BUILD_DATE));
                WriteDignetMessage(identity);
                // Send build number
                PACKET_BUILD_INFO build;
                build.build_number = BUILD_NUMBER;
                WritePacket(&build, sizeof(BUILD_NUMBER), PACKET_BUILD_INFO::CMRTYPE);
                break;
            }
        }
        server_link_laststatus_ = status;
    }

    // Check modemlink TCP input
    {
        std::string            tcpbuffer;
        modembox_link_.read(tcpbuffer);

        modembox_delta_time_ = 0;
        dataParse(modembox_cmr_decoder_, tcpbuffer.c_str(), tcpbuffer.size(), false);
    }

    if (any_packets) {
        map_area_->update();
    }

    getApp()->addTimeout(this, ID_SERIAL_POLL, TIMEOUT_SERIAL_POLL);
    return 1;
}

//=====================================================================================//

long
MainWindow::onTimer1Hz(FXObject*, FXSelector, void*)
{
    time_since_startup_++;
    for (std::vector<Ship*>::iterator it = ships_.begin(); it != ships_.end(); ++it){
        Ship* s = *it;
        s->voltage_received_countdown--;
    }

    // Query for startup information (all other ships)
    // time_since_startup_ == 0 on startup, first time when executing reaches here it has been increased by one thus making it equal to 1
    if (time_since_startup_ == 1){
        std::string query(DIGNETMESSAGE_QUERY);
        query.append(std::string(" ") + DIGNETMESSAGE_STARTUPINFO);
        WriteDignetMessage(query);
    }

    // Check for pointsfile download
    std::string filename;
    // Returns true if download has finished
    if (pointfile_downloader_.PopNewFile(filename) && filename != ""){
        TRACE_PRINT("info", ("Downloading of %s finished, loading it in", filename.c_str()));
        try {
			load_indexed_points(dynamic_points_, filename);
            loaded_pointfile_number_ = get_pointfile_number(filename);
        } catch (const std::exception& e){
            TRACE_PRINT("error", ("Unable to load pointfile %s: %s", filename.c_str(), e.what()));
            FXMessageBox::error(this, MBOX_OK,
                "Unable to read indexed points from file %s.\nReason: %s",
                filename.c_str(),
                e.what());
        }
        SetTitle();
    } else {
        PACKET_POINTSFILE_CHUNK_REQUEST req;
        // Get two chunks in a row
        for (int i=0;i<100;i++){
            if (pointfile_downloader_.GetRequest(req)){
                WritePacket(&req, sizeof(req), PACKET_POINTSFILE_CHUNK_REQUEST::CMRTYPE);
            }
        }
    }
    // 10s timer.
    if (--timer_10s_ <= 0) {
        // TODO: Add status update here?
        timer_10s_=10;
        // Update weather information age
        SetTitle();
        const unsigned int  nrows = 30;
        PACKET_WORKLOG_QUERY_BY_INDICES query;
        query.start_row_index = workmark_update_index_;
        query.nrows = nrows;
        WritePacket(&query, sizeof(query), PACKET_WORKLOG_QUERY_BY_INDICES::CMRTYPE);

        workmark_update_index_ += nrows;
        if (workmark_update_index_ > workarea_.Marks.dim1()) {
            workmark_update_index_ = 0;
        }
        // See if we know all ships
        for (unsigned int i=0; i<ships_.size(); ++i) {
            if (!ships_[i]->Has_Valid_Name){
                std::string query(DIGNETMESSAGE_QUERY);
                query.append(std::string(" ") + DIGNETMESSAGE_STARTUPINFO);
                WriteDignetMessage(query);
            }
        }
    }

    bool    workarea_queried = false;

    // 60s timer.
    if (--timer_60s_ <= 0) {
        timer_60s_=60;
        // Send out shadow position

        if (our_ship_!=0 && our_ship_->Previous_Pos.IsValid()) {
            PACKET_SHIP_POSITION    packet;
            packet.ship_number      = our_ship_->Number;
            packet.has_barge        = our_ship_->Previous_Has_Barge ? 0x01 : 0x00;
            packet.gps1_x           = our_ship_->Previous_Pos().X;
            packet.gps1_y           = our_ship_->Previous_Pos().Y;
            packet.heading          = our_ship_->Previous_Pos().Heading;
            packet.flags            = SHIP_POSITION_SHADOW;
            WritePacket(&packet, sizeof(packet), CMRTYPE_SHIP_POSITION);
        }
#ifdef FORCED_CRASH
        char c;
        std::vector<unsigned char>  packetbuffer;
        CMR::encode(packetbuffer, CMRTYPE_CRASH, &c, sizeof(c));
        try {
            if (IsModemboxSpecified()) {
                if (modembox_io_.get() != 0) {
                    modembox_io_->write(packetbuffer);
                }
                if (modembox_link_.opened()) {
                    modembox_link_.write(packetbuffer);
                }
            } else {
                server_link_.write(packetbuffer);
            }
        } catch (const std::exception&) {
            // pass.
        }
#endif

        // Query workarea, if not yet received.
        if (workarea_.Source != WorkAreaMarking::SOURCE_SERVER) {
            WriteDignetMessage("? WorkArea");
            workarea_queried = true;
        }
        // Send Viewer version information
        PACKET_BUILD_INFO build;
        build.build_number = BUILD_NUMBER;
        WritePacket(&build, sizeof(BUILD_NUMBER), PACKET_BUILD_INFO::CMRTYPE);
    }

    // 3600s timer.
    if (--timer_3600s_ <= 0) {
        timer_3600s_ = 3600;
        if (!workarea_queried) {
            // Refresh the workarea.
            WriteDignetMessage("? WorkArea");
        }
    }

    // Update the UTC time to "no data" if required.
    if (++modembox_delta_time_ > 2) {
        gpstime_lbl_->setText("OFFLINE");
    }

    if (IsWarningRequired()>0) {
        map_area_->update();
    }
    getApp()->addTimeout(this, ID_TIMER_1HZ);
    return 1;
}

//=====================================================================================//
long
MainWindow::onPaintMapArea(FXObject*,FXSelector,void* ptr)
{
    ugl::Rect2*    viewport = reinterpret_cast<ugl::Rect2*>(ptr);

    // AXIS.
    axis1_.points_gl.draw();
    axis2_.points_gl.draw();

    // NEAREST POINT.
    glLineWidth(5.0);
    nearest_point_.draw();
    glLineWidth(1.0);

    // MARKERS
    glLineWidth(5.0);
    markers_.draw();
    glLineWidth(1.0);

    glLineWidth(3.0);
    for (unsigned int i=0; i<ships_.size(); ++i) {
        ships_[i]->Draw(viewmode_ != VIEWMODE_WORKLOG);
    }
    glLineWidth(1.0);

    if (IsWarningRequired()>0) {
        if (warning_font_.get()==0) {
            warning_font_ = std::auto_ptr<ugl::BitmapFont>(new ugl::BitmapFont("Times New Roman", 36));
        }

        const double    offset_x = ugl::get_origin_x();
        const double    offset_y = ugl::get_origin_y();
        const int height = map_area_->getHeight();
        const int width = map_area_->getHeight();
        
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        // Map screen coordinates to (0,0) top left and (width, height) for bottom right
        glOrtho(0, width, height, 0, -100, 100);

        if (time_since_startup_&1) {
            glColor3d(1.0, 0.0, 0.0);
        } else {
            glColor3d(0.0, 1.0, 0.0);
        }


        int yoff=34;

        for (std::vector<Ship*>::const_iterator it = ships_.begin(); it != ships_.end(); ++it) {
            Ship* s = *it;
            if (s->modembox_last_voltage.IsValid() && s->modembox_last_voltage() < s->modembox_minimum_voltage) {
                std::string msg1 = ssprintf("%s: Battery low: %3.1fV", s->Name.c_str(), s->modembox_last_voltage()); 
                warning_font_->draw(msg1, offset_x+2, yoff+offset_y, -1);
                yoff+=36;
            }
            if (s->voltage_received_countdown <= 0) {
                std::string msg;
                if (s->last_read_time.IsValid()){
                    msg = ssprintf("%s: Disconnected %2d:%2d, %3.1fV", s->Name.c_str(), s->last_read_time().hour, s->last_read_time().minute, s->modembox_last_voltage);
                } else {
                    msg = ssprintf("%s: Timeout, no voltage information received", s->Name.c_str());
                }
                warning_font_->draw(msg, offset_x+2, yoff+offset_y, -1);
                yoff+=36;
            }
        }
    }
    return 1;
}

//=====================================================================================//
long
MainWindow::onUpdMapArea(FXObject*,FXSelector,void* ptr)
{
    ugl::Rect2      area;
    bool            area_any = false;

    std::vector<ugl::VPointArray*>    vpoints;
    vpoints.push_back(&markers_);
    vpoints.push_back(&axis1_.points_gl);
    vpoints.push_back(&axis2_.points_gl);
    vpoints.push_back(&nearest_point_);
    vpoints.push_back(&points_);

    for (unsigned int i=0; i<vpoints.size(); ++i) {
        ugl::Rect2    this_round;
        if (vpoints[i]->minmax(this_round)) {
            area = area_any ? ugl::minmax_of_rect2(area, this_round) : this_round;
            area_any = true;
        }
    }

    if (area_any) {
        ugl::Rect2*    r = reinterpret_cast<ugl::Rect2*>(ptr);
        *r = area;
        return 1;
    }
    return 0;
}

//=====================================================================================//
long
MainWindow::onPaintWorkArea(FXObject*,FXSelector,void* ptr)
{
    // Show work area if user has toggled or if user is marking 
    if (show_work_area_ || viewmode_==VIEWMODE_WORKLOG){
        // Work area.
        workarea_.draw(getApp(), map_area_->getBackgroundColor());

        // Black corner.
        ugl::Point2     pt1;
        ugl::Point2     pt2;
        ugl::Point2     pt3;
        ugl::Point2     pt4;
        if (workmark_indices_.IsValid() && workarea_.GetGLCorners(pt1,pt2,pt3,pt4,workmark_indices_())) {
            glColor3d(0, 0, 0);
            glLineWidth(3.0);
            glBegin(GL_LINE_STRIP);
                glVertex3d(pt1.x, pt1.y, 0.0);
                glVertex3d(pt2.x, pt2.y, 0.0);
                glVertex3d(pt3.x, pt3.y, 0.0);
                glVertex3d(pt4.x, pt4.y, 0.0);
                glVertex3d(pt1.x, pt1.y, 0.0);
            glEnd();
        }
    }
    return 1;
}

//=====================================================================================//
long
MainWindow::onUpdWorkArea(FXObject*,FXSelector,void* ptr)
{
    return 0;
}

//=====================================================================================//
long
MainWindow::onCmdToggleBackground(FXObject*,FXSelector sel,void*)
{
    background_index_ = (background_index_ + 1) % ARRAYNUMBER(BACKGROUNDS);
    map_area_->setBackgroundColor(BACKGROUNDS[background_index_]);
    map_area_->update();
    return 1;
}

//=====================================================================================//
long
MainWindow::onKeyRelease(FXObject* sender, FXSelector sel, void* data)
{
    FXEvent*    ev = reinterpret_cast<FXEvent*>(data);
    getAccelTable()->handle(sender, sel, data);
    return 1;
}

//=====================================================================================//
long
MainWindow::onCmdToggleBarge(FXObject*,FXSelector,void*)
{
    if (IsModemboxSpecified() && our_ship_!=0) {
        our_ship_->SetHasBarge(!our_ship_->Has_Barge);
        map_area_->update();
    } else {
        FXMessageBox::error(this, MBOX_OK, "GpsViewer Error", "On ship only");
    }
    return 1;
}


//=====================================================================================//
long
MainWindow::onCmdSavePosition(FXObject*,FXSelector,void*)
{
    if (our_ship_==0) {
        FXMessageBox::error(this, MBOX_OK, "GpsViewer Error", "On ship only");
        return 1;
    }
    try {
        if (our_ship_->Pos.IsValid())
        {
            our_ship_->SetPositionAsShadow();
            our_ship_->SaveShadow();
            
            // Send our shadow position to server
            // Check if connection is actually working?
            PACKET_SHIP_POSITION    packet;
            packet.ship_number      = our_ship_->Number;
            packet.has_barge        = our_ship_->Previous_Has_Barge ? 0x01 : 0x00;
            packet.gps1_x           = our_ship_->Previous_Pos().X;
            packet.gps1_y           = our_ship_->Previous_Pos().Y;
            packet.heading          = our_ship_->Previous_Pos().Heading;
            packet.flags            = SHIP_POSITION_SHADOW;
            WritePacket(&packet, sizeof(packet), CMRTYPE_SHIP_POSITION);
            updateShipZooms();
        } else {
            FXMessageBox::error(this, MBOX_OK, "GpsViewer Error", "Invalid coordinates, can't save position");
        }
    } catch (const std::exception& e) {
        FXMessageBox::error(this, MBOX_OK, "GpsViewer Error", "Error saving position: %s", e.what());
    }
    return 1;
}

//=====================================================================================//
long
MainWindow::onCmdToggleViewmode(FXObject*,FXSelector,void*)
{
    switch (viewmode_) {
    case VIEWMODE_NORMAL:
        viewmode_ = VIEWMODE_WORKLOG;
        map_area_->setRotationAngle(map_rotation_angle_ + workarea_.Angle);
        map_area_->setMouseTarget(this, ID_MAP_AREA);
        {
            MatrixIndices   mi = { 0, 0};
            workmark_indices_ = mi;
        }
        break;
    case VIEWMODE_WORKLOG:
        viewmode_ = VIEWMODE_NORMAL;
        map_area_->setRotationAngle(map_rotation_angle_);
        map_area_->setMouseTarget(0, ID_MAP_AREA);
        workmark_indices_ = utils::Option<MatrixIndices>();
        break;
    }
    frame_switcher_->setCurrent(viewmode_);
    map_area_->update();
    return 1;
}

//=====================================================================================//
long
MainWindow::onCmdToggleWorkmark(FXObject*,FXSelector,void*)
{
    workmark_current_ = static_cast<WORKMARK>((workmark_current_ + 1) % WORKMARK_SIZE);
    button_worklog_switcher_->setCurrent(workmark_current_);
    return 1;
}

//=====================================================================================//
long
MainWindow::onCmdToggleWorklogmode(FXObject*, FXSelector, void*)
{
    switch (worklogmode_) {
    case WORKLOGMODE_MARK:
        button_worklogmode_switcher_->setCurrent(1);
        worklogmode_ = WORKLOGMODE_PAN;
        if (workmark_indices_.IsValid()) {
            workmark_indices_ = utils::Option<MatrixIndices>();
            map_area_->update();
        }
        break;
    case WORKLOGMODE_PAN:
        button_worklogmode_switcher_->setCurrent(0);
        worklogmode_ = WORKLOGMODE_MARK;
        workmark_indices_ = utils::Option<MatrixIndices>();
        break;
    }
    return 1;
}

//=====================================================================================//
long
MainWindow::onLeftButtonPressMaparea(FXObject*,FXSelector,void* ptr)
{
    switch (worklogmode_) {
    case WORKLOGMODE_MARK:
        // FIXME: place current object?
        {
            FXEvent*    ev = reinterpret_cast<FXEvent*>(ptr);
        }
        break;
    case WORKLOGMODE_PAN:
        return 0; // this signals MapArea that it can use the mouse movement event.
    }
    return 1;
}

//=====================================================================================//
long
MainWindow::onLeftButtonReleaseMaparea(FXObject*,FXSelector,void* ptr)
{
    switch (worklogmode_) {
    case WORKLOGMODE_MARK:
        if ((modembox_io_.get()!=0 && modembox_io_->state() == SerialLink::STATE_OPEN) || modembox_link_.opened()) {
			if (modembox_disabled_) {
				TRACE_PRINT("debug", ("Forced to ignore the mark"));
			} else {
				FXEvent*            ev = reinterpret_cast<FXEvent*>(ptr);
				const ugl::Point2   map_xy = map_area_->getMouseXY(ev);
				MatrixIndices       mi;
				if (workarea_.GetIndices(mi, map_xy)) {
					// yes.
					PACKET_WORKLOG_SETMARK Packet;
					Packet.workmark = workmark_current_;
					Packet.row_index = mi.Row;
					Packet.col_index = mi.Column;
					WritePacket(&Packet, sizeof(Packet), PACKET_WORKLOG_SETMARK::CMRTYPE);
				}
			}
        } else {
            // FIXME: shall we display error message?
        }
        break;
    case WORKLOGMODE_PAN:
        return 0; // this signals MapArea that it can use the mouse movement event.
    }
    return 1;
}

//=====================================================================================//
long
MainWindow::onMotionMaparea(FXObject*,FXSelector,void* ptr)
{
    switch (worklogmode_) {
    case WORKLOGMODE_MARK:
        // FIXME: place current object?
        {
            const FXEvent*        ev = reinterpret_cast<FXEvent*>(ptr);
            const ugl::Point2   map_xy = map_area_->getMouseXY(ev);
            MatrixIndices       mi;
            if (workarea_.GetIndices(mi, map_xy)) {
                if (!workmark_indices_.IsValid() ||
                    workmark_indices_().Row!=mi.Row ||
                    workmark_indices_().Column!=mi.Column) {
                    workmark_indices_ = mi;
                    map_area_->update();
                }
            } else {
                if (workmark_indices_.IsValid()) {
                    workmark_indices_ = utils::Option<MatrixIndices>();
                    map_area_->update();
                }
            }
        }
        break;
    case WORKLOGMODE_PAN:
        return 0; // this signals MapArea that it can use the mouse movement event.
    }
    return 1;
}

//=====================================================================================//
void
MainWindow::updateShipZooms()
{
    const double    MAX_VIEW_AREA_HEIGHT = 250; // meters.
    double    x;
    double    y;
    double    zoom;
    map_area_->getXYZoom(x, y, zoom);

    const double    new_factor = zoom>MAX_VIEW_AREA_HEIGHT ? zoom/MAX_VIEW_AREA_HEIGHT : 1.0;

    for (unsigned int i=0; i<ships_.size(); ++i) {
        ships_[i]->SetEnlargeFactor(new_factor);
    }
    map_area_->update();
}

//=====================================================================================//
void
MainWindow::focusShip()
{
    if (followed_ship_!=0 && followed_ship_->Pos.IsValid())
    {
#if (0)
        map_area_->addPoint(fxutils::Point(followed_ship_->Pos().X, followed_ship_->Pos().Y));
#else
        map_area_->addPoint(fxutils::Point(followed_ship_->Pos().X, followed_ship_->Pos().Y), false, 0);
#endif
        map_area_->update();
    }
}
//=====================================================================================//
long
MainWindow::onCmdZoomIn
(FXObject* sender, FXSelector sel, void* ptr)
{
    map_area_->handle(sender, MKUINT(MapAreaGL::ID_ZOOM_IN, SEL_COMMAND), ptr);
    updateShipZooms();
    return 1;
}

//=====================================================================================//
long
MainWindow::onCmdZoomOut
(FXObject* sender, FXSelector sel, void* ptr)
{
    map_area_->handle(sender, MKUINT(MapAreaGL::ID_ZOOM_OUT, SEL_COMMAND), ptr);
    updateShipZooms();
    return 1;
}

//=====================================================================================//
long
MainWindow::onCmdFollowNextShip
(FXObject* sender, FXSelector sel, void* ptr)
{
    if (ships_.size()==0)
    {
        return 1;
    }
    // Find the number for next ship. There are two possible values for the next ship: 
    //        next biggest after currently followed ship or absolutely smallest number. 
    //        As the numbers could be rather arbitary (some ships are not connected, 
    //        sequence is random etc) one has to go through the entire array of ships and 
    //        find the both numbers at once. If one can't find the next biggest then the 
    //        smallest is used.

    // Assumes that has valid ships in ships_ vector
    const unsigned int current=followed_ship_==0 ? 0: followed_ship_->Number;
    Ship* minShip=followed_ship_ == 0 ? ships_[0]: followed_ship_ ;
    Ship* nextShip=minShip;
    // As we use pointers and an extra variable is needed or else the second if wouldn't work.
    unsigned int nextShipNum = INT_MAX;
    for (unsigned int i=0;i<ships_.size();i++)
    {
        const unsigned int shipNum=ships_[i]->Number;
        if (shipNum<minShip->Number)
        {
            minShip=ships_[i];
        } else if ( shipNum>current && shipNum<nextShipNum)
        {
            nextShip=ships_[i];
            nextShipNum=nextShip->Number;
        }
    }
    if (nextShip == followed_ship_)
    {
        followed_ship_ = minShip;
    }
    else 
    {
        followed_ship_ = nextShip;
    }
    SetTitle();
    map_area_->setFollowingMode(true);
    UpdateSpeed();
    focusShip();
    return 1;
}

//=====================================================================================//
long
MainWindow::onCmdToggleWorkArea(FXObject* sender, FXSelector, void* ptr)
{
    show_work_area_=!show_work_area_;
    return 1;
}

//=====================================================================================//
long
MainWindow::onCmdExit(FXObject*,FXSelector,void*)
{
    if (log_file_ != 0) {
        fclose(log_file_);
        log_file_ = 0;
    }
    getApp()->exit();
    return 1;
}
