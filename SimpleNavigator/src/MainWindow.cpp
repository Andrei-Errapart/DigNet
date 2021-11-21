#include <winsock2.h>    // this has to be first, otherwise windows.h will complain :(

#include <utils/math.h>
#include <utils/util.h>
#include <utils/myrtcm.h>
#include <utils/ugl.h>

#include <utils/NMEA.h>
#include <fxutils/PointStorageViewer.h>
#include <utils/Serial.h> // AsyncIO
#include <utils/Transformations.h>
#include <utils/mystring.h>

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

#include "MainWindow.h"

#include "resources.h"  // resources::zoom_in, resources::zoom_out
#include "hydroicon.h"

#include "config.h"
#include "version.h"

#if (0)
static const double    limit_x1 = GPSVIEWER_X1;
static const double    limit_x2 = GPSVIEWER_X2;
static const double    limit_y1 = GPSVIEWER_Y1;
static const double    limit_y2 = GPSVIEWER_Y2;
#endif

using namespace utils;
using namespace fxutils;


/// Serial poll timer, milliseconds.
#define     TIMEOUT_SERIAL_POLL         100
/// Time to reconnect, ms.
#define     COUNTDOWN_SERIAL_READER     (15000 / TIMEOUT_SERIAL_POLL)
/// Time to reconnect after error, ms
#define     COUNTDOWN_SERIAL_READER_ERROR_RECONNECT     (5000 / TIMEOUT_SERIAL_POLL)

#define    FILENAME_LOG             "log.txt"
#define    FILENAME_AXIS1           "axis.txt"
#define    FILENAME_AXIS2           "axis2.txt"
#define    FILENAME_PROJECT         "Project.dxf"
#define    FILENAME_CONFIGURATION   "GpsServer.ini"

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
    FXMAPFUNC(SEL_TIMEOUT,    MainWindow::ID_TIMER_1HZ,            MainWindow::onTimer1Hz),
    
    FXMAPFUNC(SEL_PAINT,    MainWindow::ID_MAP_AREA,            MainWindow::onPaintMapArea),
    FXMAPFUNC(SEL_UPDATE,    MainWindow::ID_MAP_AREA,            MainWindow::onUpdMapArea),

    FXMAPFUNC(SEL_COMMAND,    MainWindow::ID_TOGGLE_BACKGROUND,    MainWindow::onCmdToggleBackground),

    FXMAPFUNC(SEL_KEYRELEASE,0,                                    MainWindow::onKeyRelease),

    FXMAPFUNC(SEL_COMMAND,    MainWindow::ID_ZOOM_IN,                MainWindow::onCmdZoomIn),
    FXMAPFUNC(SEL_COMMAND,    MainWindow::ID_ZOOM_OUT,            MainWindow::onCmdZoomOut),
    FXMAPFUNC(SEL_COMMAND,    MainWindow::ID_FOLLOW_NEXT_SHIP,    MainWindow::onCmdFollowNextShip),
    FXMAPFUNC(SEL_COMMAND,    MainWindow::ID_EXIT,                MainWindow::onCmdExit),
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
    ,cfg_("SimpleNavigator.ini")
	,max_track_length_(-1)
	,track_sparse_factor_(1)
	,track_sparse_counter_(0)
	,simple_gps_transform_(utils::TRANSFORM_LAMBERT_EST)
    ,timer_10s_(0)
    ,timer_60s_(0)
    ,timer_3600s_(0)
    ,time_since_startup_(0)
    ,markers_(GL_LINE_STRIP)
    ,log_file_(0)
    ,shutdown_counter_(-1)
    ,our_ship_(0)
    ,followed_ship_(0)
    /* === BUTTONS FRAME === */
    ,frame_buttons_(0)
    ,background_index_(0)
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

    // Load initialization file.
    try {
        cfg_.load();
    } catch (const std::exception&) {
        // pass
    }

    cfg_.set_section("SimpleNavigator");
    // modembox and stuff.
    cfg_.get_string("SimpleGPS", "", simple_gps_);

    // map_area_options_
    cfg_.get_uint("PointsColor", color_black, map_area_options_.pointColor);

	std::string transform;
	cfg_.get_string("Transform", "Lambert-Est", transform);
	if (strcasecmp(transform.c_str(), "UTM")==0)
	{
		simple_gps_transform_ = utils::TRANSFORM_UTM;
	}

	std::string strack;
	cfg_.get_string("Track", "-1", strack);
	if (strack=="0" || strcasecmp(strack.c_str(), "off")==0 || strack.size()==0)
	{
		max_track_length_ = 0;
	}
	else
	{
		std::vector<std::string>	v;
		utils::split(strack, ",", v);
		switch (v.size())
		{
		case 1:
			max_track_length_ = int_of(v[0]);
			break;
		case 2:
			track_sparse_factor_ = int_of(v[0]);
			max_track_length_ = int_of(v[1]);
			break;
		}
	}

    std::string name;

	cfg_.set_section("MyShip");
	
	std::string ship_name;
	if(cfg_.get_string("Name",	"",	name))	{
        our_ship_= new Ship;
        our_ship_->Name=name;
        followed_ship_=our_ship_;
        TRACE_PRINT("info", ("Loaded %s from .ini", name.c_str()));
	} else {
		TRACE_PRINT("warning", ("Couldn't load ship name from .ini"));
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
        map_area_->mihkels.push_back(ps_viewer_->mihkel());
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
            axis_name_text_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "Axis name:", 0, opt_left), myfont_);
            axis_name_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);

            apply_font<FXLabel>(new FXLabel(mtx, "Progress:", 0, opt_left), myfont_);
            axis_progress_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);

            apply_font<FXLabel>(new FXLabel(mtx, "Distance:", 0, opt_left), myfont_);
            axis_distance_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);

            apply_font<FXLabel>(new FXLabel(mtx, "Away to:", 0, opt_left), myfont_);
            axis_direction_lbl_ = apply_font<FXLabel>(new FXLabel(mtx, "no data", 0, opt_right), myfont_);
        }

        new FXHorizontalSeparator(frame_control_panel, SEPARATOR_GROOVE|LAYOUT_FILL_X);

        // Zoom-in/Zoom-out buttons.
        {
            FXVerticalFrame* vf1 = new FXVerticalFrame(frame_control_panel,LAYOUT_FILL_Y|LAYOUT_FILL_X, 0,0,0,0, 0,0,0,0, DEFAULT_SPACING, 0);
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
    getAccelTable()->addAccel(parseAccel("N"), this, MKUINT(ID_FOLLOW_NEXT_SHIP,SEL_COMMAND));

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
    }

    setupControlPanel();
    // Follow own ship
    onCmdFollowNextShip(0, 0, 0);
}


//=====================================================================================//
MainWindow::~MainWindow()
{
    delete our_ship_;
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
        if (simple_gps_.size()!=0) {
            TRACE_PRINT("info", ("Connection: Simple GPS at %s", simple_gps_.c_str()));
// utils::AsyncIO::ReadCallback([this](const void* data, const int count) { this->magnetometer_callback((const char*)data, count); })));
            simple_gps_io_ = std::auto_ptr<SerialLink>(new SerialLink(getApp(), simple_gps_, 5000, 60000,
				utils::AsyncIO::ReadCallback([this](const void* data, const int count) { this->onSimpleGpsInput((const char*)data, count); })));
            map_area_->setBoat(false);
            TRACE_PRINT("info", ("Connection to Simple GPS successful."));
        }
    } catch (const std::exception& e) {
        FXMessageBox::error(this, MBOX_OK, "SimpleNavigator Error", "Connection error: %s", e.what());
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
    SetTitle();

    
    // ID_MAP_AREA mihkel.
    {
        MapAreaGL::Mihkel    mihkel = {ID_MAP_AREA, this};
        map_area_->mihkels.push_back(mihkel);
    }

    getApp()->addTimeout(this, ID_SERIAL_POLL, 100);
    getApp()->addTimeout(this, ID_TIMER_1HZ);
    
#if defined(STARTUP_NUMBER_ZOOMOUTS)
    for (int i=0; i<STARTUP_NUMBER_ZOOMOUTS; ++i) {
        map_area_->onZoomOut(0, 0, 0);
    }
#endif

    if (false) {
        Point    pt;
        pt.x=(GPSVIEWER_X2+GPSVIEWER_X1)/2;
        pt.y=(GPSVIEWER_Y1+GPSVIEWER_Y2)/2;
        pt.z = 0;
        pt.color = FXRGB(0,0,0);
        pt.direction = 0;
        pt.id = 1;
        pt.signal_ok = true;
        pt.time = my_time_of_now();
        map_area_->addPoint(pt, true, -1);
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
void
MainWindow::SetTitle()
{
    std::string s(build_name_);

    if (followed_ship_!=0){
        s.append(ssprintf(". Following %s", followed_ship_->Name.c_str()));
    }
    setTitle(s.c_str());    
}

//=====================================================================================//
void
MainWindow::setupControlPanel()
{
}

//=====================================================================================//
void
MainWindow::onSimpleGpsInput(const char* data, unsigned int len)
{
#if (1)
    std::string buffer(data, data+len);
    simple_gps_decoder_.feed(buffer);

    my_time time;
    std::string line;
    bool    any_packets = false;
	std::string	foo_zone;
    while(simple_gps_decoder_.pop(time, line)){
        GPGGA    gga;
        GPVTG    vtg;
        if (parse_gga(line, time, gga, true)){
            Point    pt;
            pt.x=pt.y=pt.z=0.0f;
            pt.z = 22;
            bool local=local_of_gps(simple_gps_transform_, gga.latitude, gga.longitude, gga.altitude, "", pt.x, pt.y, pt.z, foo_zone);
			if (++track_sparse_counter_ >= track_sparse_factor_)
			{
				map_area_->addPoint(pt, true, max_track_length_);
				track_sparse_counter_ = 0;
			}
			else
			{
				map_area_->addPoint(pt, false, max_track_length_);
			}
            our_ship_->SetPosition(pt.x, pt.y, 22);

            any_packets = true;
            // If modembox connection exists then do not use the hack. If it doesn't exist then assume TCP connection and use the hack
            const bool hack = false;
            double    best_distance_so_far = -1;
            update_axis_info(axis1_.points, best_distance_so_far, false, pt.x, pt.y, axis_name_lbl_, axis_progress_lbl_, axis_distance_lbl_, axis_direction_lbl_, nearest_point_, hack);
            update_axis_info(axis2_.points, best_distance_so_far, true, pt.x, pt.y, axis_name_lbl_, axis_progress_lbl_, axis_distance_lbl_, axis_direction_lbl_, nearest_point_, hack);
        } else if (parse_vtg(line, time, vtg, true)){
            map_area_->setBoatDirection(vtg.true_track);
            any_packets = true;
        }
    }
    if (any_packets){
        map_area_->update();
    }
#else
    bool    any_packets = false;
    static int  count = 0;

            Point    pt;
            pt.x=pt.y=pt.z=0.0f;
            pt.x = 0.5 * (6487300 + 6485200);
            pt.y = 0.5 * (481415 + 480760) + count;
            ++count;
            bool local = true;
            map_area_->addPoint(pt, true);
            our_ship_->SetPosition(pt.x, pt.y, 0.0);

            any_packets = true;
            // If modembox connection exists then do not use the hack. If it doesn't exist then assume TCP connection and use the hack
            const bool hack = false;
            double    best_distance_so_far = -1;
            update_axis_info(axis1_.points, best_distance_so_far, false, pt.x, pt.y, axis_name_lbl_, axis_progress_lbl_, axis_distance_lbl_, axis_direction_lbl_, nearest_point_, hack);
            update_axis_info(axis2_.points, best_distance_so_far, true, pt.x, pt.y, axis_name_lbl_, axis_progress_lbl_, axis_distance_lbl_, axis_direction_lbl_, nearest_point_, hack);

    if (any_packets){
        map_area_->update();
    }
#endif
}
//=====================================================================================//

long
MainWindow::onTimer1Hz(FXObject*, FXSelector, void*)
{
    time_since_startup_++;

    // 10s timer.
    if (--timer_10s_ <= 0) {
        // TODO: Add status update here?
        timer_10s_=10;
        SetTitle();
    }

    // 60s timer.
    if (--timer_60s_ <= 0) {
        timer_60s_=60;
    }

    // 3600s timer.
    if (--timer_3600s_ <= 0) {
        timer_3600s_ = 3600;
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

    our_ship_->Draw(true);
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
MainWindow::onLeftButtonPressMaparea(FXObject*,FXSelector,void* ptr)
{
    return 0; // this signals MapArea that it can use the mouse movement event.
}

//=====================================================================================//
long
MainWindow::onLeftButtonReleaseMaparea(FXObject*,FXSelector,void* ptr)
{
    return 0; // this signals MapArea that it can use the mouse movement event.
}

//=====================================================================================//
long
MainWindow::onMotionMaparea(FXObject*,FXSelector,void* ptr)
{
    return 0; // this signals MapArea that it can use the mouse movement event.
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

    our_ship_->SetEnlargeFactor(new_factor);
    map_area_->update();
}

//=====================================================================================//
void
MainWindow::focusShip()
{
    if (followed_ship_!=0 && followed_ship_->Pos.IsValid())
    {
        map_area_->addPoint(fxutils::Point(followed_ship_->Pos().X, followed_ship_->Pos().Y), false, -1);
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
    followed_ship_ = our_ship_;
    SetTitle();
    map_area_->setFollowingMode(true);
    focusShip();
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
