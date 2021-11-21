/* $Header: /cvsroot/Soft/MixGPS/main.cxx,v 1.4 2006/04/29 13:53:30 Administrator Exp $ */
#pragma warning(disable:4786) 


#include <string.h>		// strcasecmp
#include <stdio.h>		// fopen and friends.

#include <utils/NMEA.h>		// LineReader
#include <utils/util.h>
#include <utils/Transformations.h>
#include <utils/thread.h>	// CriticalSection :)
#include <utils/math.h>
#include <utils/TcpPollingBroadcaster.h>
#include <utils/TracerStorage.h>

#include <fx.h>

#include <utils/MixGPS.h>		// GPS Mixer.

#include <fxutils/fxutil.h>

#include <armadillo.h>          // protection system...

using namespace std;    // comfortable...
using namespace utils;
using namespace fxutils;

#define	CONFIG_FILENAME		"MixGPS.ini"
#define	INPUT_POLL_INTERVAL	20
#define	OUTPUT_POLL_INTERVAL	200
#define	DISPLAY_POLL_INTERVAL	30

/********************************************************************************/
/*
 * GPS structures :)
 */
#define	NGPS	2

typedef struct {
	/* GUI widgets. */
	FXComboBox*			port_cb;
	FXTextField*			speed_tf;
	FXTextField*			x_tf;		/* X-coordinate text field. */
	FXTextField*			y_tf;		/* Y-coordinate text field. */
	FXLabel*			packets_lb;
	FXLabel*			longitude_lb;
	FXLabel*			latitude_lb;
	FXLabel*			x_lb;
	FXLabel*			y_lb;
	FXLabel*			dx_lb;
	FXLabel*			dy_lb;

	/* Stuff behind the scenes. */
	utils::LineReader		linereader;
	int				linecount;
	MixGPSXY			relview_xy;	/* Coordinates relative to the viewpoint.xy */
	MixGPSXY			abs_xy;		/* Absolute coordinates in Lambert-EST */
} GPS;

/********************************************************************************/
static void
parse_pair(	utils::FileConfig&	cfg,
		const char*		key,
		const double		def1,
		const double		def2,
		double&			val1,
		double&			val2,
		FXTextField*		w1,
		FXTextField*		w2)
{
	val1 = def1;
	val2 = def2;

	std::vector<std::string>	v;
	std::string			s;

	if (cfg.get_string(key, s)) {
		split(s, ";", v);

		if (v.size()>0)
			val1 = atof(v[0].c_str());

		if (v.size()>1)
			val2 = atof(v[1].c_str());
	}

	char	xbuf[200];
	w1->setText(xsprintf(xbuf, sizeof(xbuf), "%4.2f", val1));
	w2->setText(xsprintf(xbuf, sizeof(xbuf), "%4.2f", val2));
}

/********************************************************************************/
/*
 * Fill the serial port combo box.
 */
template <class SerialInterface>
void
init_serial_combo(	FXApp*				app,		/* FX Application object - if nonzero, display errors. */
			FileConfig&			cfg,		/* Configuration. */
			const char*			keyname,	/* Configuration key name. */
			const std::vector<std::string>&	serial_ports,	/* Available ports. */
			FXComboBox*			widget,		/* Port widget. */
			FXTextField*			speed_tf,	/* Speed widget. */
			FXLabel*			status_lb,	/* Port status. */
			SerialInterface&		linereader,	/* Serial port to be opened. */
			std::string&			errorbuf
			)
{
	/* Get port, speed. */
	std::string	portname("COM1");
	int		speed = 4800;
	{
		std::string	portstr;
		cfg.get_string(keyname, "COM1;4800", portstr);
		std::vector<std::string>	v;
		split(portstr, ";", v);

		if (v.size()>0)
			portname = v[0];
		if (v.size()>1)
			speed = atoi(v[1].c_str());
	}

	/* Initialize widget, speed_tf. */
	int	myindex = -1;
	for (int j=0; j<serial_ports.size(); ++j) {
		widget->appendItem(serial_ports[j].c_str());
		if (strcasecmp(serial_ports[j].c_str(), portname.c_str()) == 0)
			myindex = j;
	}
	if (myindex < 0) {
		myindex = 0;
		widget->prependItem(portname.c_str());
	}
	widget->setCurrentItem(myindex);
	widget->setNumVisible(widget->getNumItems());

	char	xbuf[200];
	speed_tf->setText(xsprintf(xbuf, sizeof(xbuf), "%d", speed));

	/* Open port :) */
	try {
		linereader.open(portname, speed);
		status_lb->setText("0");
	} catch (const std::exception& e) {
		char		xbuf[2048];
		sprintf(xbuf, "Serial port error: %s. ", e.what());
		errorbuf += xbuf;
		if (app != 0)
			FXMessageBox::error(app, MBOX_OK, "MixGPS error", "Serial port error: %s\n", e.what());
		status_lb->setText("closed");
	}
}

/********************************************************************************/
/********************************************************************************/
/*
 * Main window class.
 */
class MainWindow : public FXMainWindow {
  FXDECLARE(MainWindow)
private:
	FileConfig	cfg_;
	GPS		gps_[NGPS];	/* Three GPS-s :) */
	double		ship_width_;
	double		ship_length_;
	MixGPSXY	viewpoint_;

	MixGPS		mixer_;		/* GPS Mixer. */

	FXTextField*	ship_width_tf_;
	FXTextField*	ship_length_tf_;
	FXTextField*	center_x_tf_;
	FXTextField*	center_y_tf_;

	/* Log File. */
	std::string	logfile_name_;
	FILE*		logfile_;
	FXTextField*	logfile_tf_;
	FXLabel*	logfile_lines_lb_;
	int		logfile_lines_;

	/* GPS output. */
	FXComboBox*	gpsout_cb_;
	FXTextField*	gpsout_speed_tf_;
	FXLabel*	gpsout_lines_lb_;
	SerialPort	gpsout_serialport_;
	int		gpsout_lines_;

	/* GPS2 output. */
	FXComboBox*	gpsout2_cb_;
	FXTextField*	gpsout2_speed_tf_;
	FXLabel*	gpsout2_lines_lb_;
	SerialPort	gpsout2_serialport_;

	/* Compass output. */
	FXComboBox*	compassout_cb_;
	FXTextField*	compassout_speed_tf_;
	FXLabel*	compassout_lines_lb_;
	SerialPort	compassout_serialport_;
	int		compassout_lines_;

    /* TCP output. */
	TracerWriter            tcpout_writer_;
	TcpPollingBroadcaster   tcpout_bbc_;
	ts_buffer_t             tcpout_buf_;
    FXLabel*                tcpout_port_lb_;
    FXLabel*                tcpout_status_lb_;

	/* Distances. */
	FXLabel*	distance_gps12_;
	FXLabel*	distance_gps13_;
	FXLabel*	distance_gps23_;

	/* Graphical display canvas. */
	FXCanvas*			display_canvas_;
	MixGPSXY			gps_origin_;
	double				compass_direction_;
	double				gps_direction_;
	std::vector<MixGPSXY>		gps_pos_;
	std::vector<MixGPSXY>		gps_pos2_;
	std::vector<MixGPS::GPSInfo>	gpsinfo_;

	/* Initialization errors. */
	std::string	init_error_buf_;
	MixGPS		mixgps_;
protected:
			MainWindow() : cfg_(CONFIG_FILENAME) {}

public:
	typedef	FXMainWindow	inherited;
	enum {
		ID_COMBO = inherited::ID_LAST,
		ID_TEXTFIELD,
		ID_SERIALPORT_CONFIG,
		ID_SAVE_PREFERENCES,
		ID_INPUT_POLL,
		ID_OUTPUT_POLL,
		ID_DISPLAY_CANVAS,
		ID_LAST
	};

			MainWindow(	FXApp*	app);
	virtual		~MainWindow();

	virtual void	create();

	long		onSerialPortConfig(FXObject*, FXSelector, void*);
	long		onComboBoxChange(FXObject*, FXSelector, void*);
	long		onSavePreferences(FXObject*, FXSelector, void*);
	long		onInputPoll(FXObject*, FXSelector, void*);
	long		onOutputPoll(FXObject*, FXSelector, void*);
	long		onDisplayPoll(FXObject*, FXSelector, void*);
	long		onDisplayPaint(FXObject*, FXSelector, void*);
	long		onDisplayConfigure(FXObject*, FXSelector, void*);

}; // class MainWindow

FXDEFMAP(MainWindow) MainWindowMap[]={
  //________Message_Type_____________________ID____________Message_Handler_______
  FXMAPFUNC(SEL_COMMAND,	MainWindow::ID_SAVE_PREFERENCES,	MainWindow::onSavePreferences),
  FXMAPFUNC(SEL_COMMAND,	MainWindow::ID_SAVE_PREFERENCES,	MainWindow::onComboBoxChange),
  FXMAPFUNC(SEL_TIMEOUT,	MainWindow::ID_INPUT_POLL,		MainWindow::onInputPoll),
  FXMAPFUNC(SEL_TIMEOUT,	MainWindow::ID_OUTPUT_POLL,		MainWindow::onOutputPoll),
  FXMAPFUNC(SEL_TIMEOUT,	MainWindow::ID_DISPLAY_CANVAS,		MainWindow::onDisplayPoll),
  FXMAPFUNC(SEL_PAINT,		MainWindow::ID_DISPLAY_CANVAS,		MainWindow::onDisplayPaint),
  FXMAPFUNC(SEL_CONFIGURE,	MainWindow::ID_DISPLAY_CANVAS,		MainWindow::onDisplayConfigure),
}; 

FXIMPLEMENT(MainWindow,MainWindow::inherited,MainWindowMap,ARRAYNUMBER(MainWindowMap))

/********************************************************************************/
MainWindow::MainWindow(	FXApp*	app)
:	inherited(app, "MixGPS", 0, 0, DECOR_ALL),
	cfg_(CONFIG_FILENAME)
{
	char	xbuf[200];

	/* Load configuration. */
	try {
		cfg_.load();
	} catch (...) {
	}
	cfg_.set_section("MixGPS");

	/* Set up Geodetic Transformation. */
	{
		TRANSFORM	transform;
		std::string	zone;
		set_transform(cfg_, "", transform, zone);
	}

	/* Search for serial ports. */
	std::vector<std::string>	serial_ports;
	utils::list_serial_ports(serial_ports);

	/* GUI widgets. */
	const int		frameopts = LAYOUT_SIDE_TOP|LAYOUT_FILL_X|LAYOUT_FILL_Y;
	FXVerticalFrame*	mainframe = new FXVerticalFrame(this, frameopts);
	FXHorizontalFrame*	interface_frame = new FXHorizontalFrame(mainframe, frameopts|PACK_UNIFORM_WIDTH);
	FXVerticalFrame*	numeric_frame = new FXVerticalFrame(interface_frame, frameopts|PACK_UNIFORM_WIDTH);
	display_canvas_ = new FXCanvas(interface_frame, this, ID_DISPLAY_CANVAS, frameopts);

	/* MixGPS initialization data. */
	std::vector<MixGPSXY>	mixgps(NGPS);

	/* Create and initialize GPS GUI widgets */
	{
		FXGroupBox*		gpsgroup = new FXGroupBox(numeric_frame, "Input", frameopts|FRAME_RIDGE);
		FXMatrix*		gpsframe = new FXMatrix(gpsgroup, 12, MATRIX_BY_COLUMNS);
		new FXLabel(gpsframe, "");	// 1
		new FXLabel(gpsframe, "Port");	// 2
		new FXLabel(gpsframe, "Speed");	// 3
		new FXLabel(gpsframe, "X");	// 4
		new FXLabel(gpsframe, "Y");	// 5
		new FXLabel(gpsframe, "#");	// 6
		new FXLabel(gpsframe, "Lon.");	// 7
		new FXLabel(gpsframe, "Lat.");	// 8
		new FXLabel(gpsframe, "X");	// 9
		new FXLabel(gpsframe, "Y");	// 10
		new FXLabel(gpsframe, "DX");	// 11
		new FXLabel(gpsframe, "DY");	// 12

		for (int i=0; i<NGPS; ++i) {
			char	gpsname[256];
			sprintf(gpsname, "GPS%d", i+1);
			GPS&		gps = gps_[i];
			MixGPSXY&	gpsxy = mixgps[i];


			/* Parse the configuration. */
			cfg_.set_section(gpsname);
			{
				std::string	s;
				cfg_.get_string("Coord", s);
				std::vector<std::string>	v;
				split(s, ";", v);
	
				gpsxy.x = v.size()>0 ? atof(v[0].c_str()) : 0.0;
				gpsxy.y = v.size()>1 ? atof(v[1].c_str()) : 0.0;
			}

			/* Create widgets. */
			new FXLabel(gpsframe, gpsname);					// 1
			gps.port_cb = new FXComboBox(gpsframe, 5, 0, 0, COMBOBOX_STATIC);// 2
			gps.speed_tf = new FXTextField(gpsframe, 5, this, ID_TEXTFIELD);// 3
			gps.x_tf = new FXTextField(gpsframe, 5, this, ID_TEXTFIELD);	// 4
			gps.y_tf = new FXTextField(gpsframe, 5, this, ID_TEXTFIELD);	// 5
			gps.packets_lb = new FXLabel(gpsframe, "");			// 6
			gps.longitude_lb = new FXLabel(gpsframe, "");			// 7
			gps.latitude_lb = new FXLabel(gpsframe, "");			// 8
			gps.x_lb = new FXLabel(gpsframe, "");				// 9
			gps.y_lb = new FXLabel(gpsframe, "");				// 10
			gps.dx_lb = new FXLabel(gpsframe, "");				// 11
			gps.dy_lb = new FXLabel(gpsframe, "");				// 12

			/* Update widgets. */
			init_serial_combo<utils::LineReader>(
						0, cfg_, "Port", serial_ports,
						gps.port_cb, gps.speed_tf, gps.packets_lb, 
						gps.linereader,
						init_error_buf_);
			gps.linecount = 0;
			gps.x_tf->setText(xsprintf(xbuf, sizeof(xbuf), "%5.3f", gpsxy.x));
			gps.y_tf->setText(xsprintf(xbuf, sizeof(xbuf), "%5.3f", gpsxy.y));
		}

	}

	FXHorizontalFrame*	ship_distances_frame = new FXHorizontalFrame(numeric_frame, frameopts, 0,0,0,0, 0,0,0,0, 0,0);
	/* Create and initialize ship and viewpoint parameters. */
	{
		cfg_.set_section("MixGPS");

		FXGroupBox*	shipgroup = new FXGroupBox(ship_distances_frame, "Ship geometry", frameopts|FRAME_RIDGE);
		FXMatrix*	shipframe = new FXMatrix(shipgroup, 3, MATRIX_BY_COLUMNS);

		new FXLabel(shipframe, "");
		new FXLabel(shipframe, "X/length");
		new FXLabel(shipframe, "Y/width");

		const int	ncols = 5;
		new FXLabel(shipframe, "Ship size");
		ship_length_tf_	= new FXTextField(shipframe, ncols);
		ship_width_tf_	= new FXTextField(shipframe, ncols);
		new FXLabel(shipframe, "Viewpoint");
		center_x_tf_	= new FXTextField(shipframe, ncols);
		center_y_tf_	= new FXTextField(shipframe, ncols);

		parse_pair(cfg_, "Ship", 12, 6, ship_length_, ship_width_, ship_length_tf_, ship_width_tf_);
		parse_pair(cfg_, "ViewPoint", 6, 3, viewpoint_.x, viewpoint_.y, center_x_tf_, center_y_tf_);
	}

	/* Create distances :) */
	{
		FXGroupBox*	distancesgroup = new FXGroupBox(ship_distances_frame, "Distances", frameopts|FRAME_RIDGE);
		FXMatrix*	distancesframe = new FXMatrix(distancesgroup, 3, MATRIX_BY_ROWS);

		new FXLabel(distancesframe, "GPS1 - GPS2");
		new FXLabel(distancesframe, "GPS1 - GPS3");
		new FXLabel(distancesframe, "GPS2 - GPS3");

		distance_gps12_ = new FXLabel(distancesframe, "(not yet)");
		distance_gps13_ = new FXLabel(distancesframe, "(not yet)");
		distance_gps23_ = new FXLabel(distancesframe, "(not yet)");
	}

	mixgps_.open(viewpoint_, mixgps);

	/* Open log file. */
	{
		cfg_.set_section("MixGPS");

		logfile_name_ = "MixGPS-log.txt";
		cfg_.get_string("LogFile", logfile_name_);
		logfile_ = fopen(logfile_name_.c_str(), "a");
	}

	/* Create and initialize output widgets. */
	{
		cfg_.set_section("OUT");

		FXGroupBox*	outputgroup = new FXGroupBox(numeric_frame, "Output", frameopts|FRAME_RIDGE);
		FXMatrix*	outputframe = new FXMatrix(outputgroup, 4, MATRIX_BY_COLUMNS);


		/* Create widgets. */
		const int	ncols = 5;

		new FXLabel(outputframe, "Type");
		new FXLabel(outputframe, "Port/File");
		new FXLabel(outputframe, "Speed");
		new FXLabel(outputframe, "#");

		new FXLabel(outputframe, "Log file");
		logfile_tf_ = new FXTextField(outputframe, 20);
		new FXLabel(outputframe, "");
		logfile_lines_lb_ = new FXLabel(outputframe, "0");

		new FXLabel(outputframe, "GPS");
		gpsout_cb_ = new FXComboBox(outputframe, 5, 0, 0, COMBOBOX_STATIC);
		gpsout_speed_tf_ = new FXTextField(outputframe, ncols);
		gpsout_lines_lb_ = new FXLabel(outputframe, "0");
		gpsout_lines_ = 0;

		new FXLabel(outputframe, "GPS2");
		gpsout2_cb_ = new FXComboBox(outputframe, 5, 0, 0, COMBOBOX_STATIC);
		gpsout2_speed_tf_ = new FXTextField(outputframe, ncols);
		gpsout2_lines_lb_ = new FXLabel(outputframe, "0");

		new FXLabel(outputframe, "Compass");
		compassout_cb_ = new FXComboBox(outputframe, 5, 0, 0, COMBOBOX_STATIC);
		compassout_speed_tf_ = new FXTextField(outputframe, ncols);
		compassout_lines_lb_ = new FXLabel(outputframe, "0");
		compassout_lines_ = 0;

		new FXLabel(outputframe, "TCP");
		tcpout_port_lb_ = new FXLabel(outputframe, "");
        new FXLabel(outputframe, "");
		tcpout_status_lb_ = new FXLabel(outputframe, "");

		/* Initialize widgets. */
		logfile_tf_->setText(logfile_name_.c_str());
		logfile_lines_lb_->setText(logfile_==0 ? "closed" : "0");
		logfile_lines_ = 0;

		init_serial_combo<utils::SerialPort>(
						0, cfg_, "GPSPort", serial_ports,
						gpsout_cb_, gpsout_speed_tf_, gpsout_lines_lb_, 
						gpsout_serialport_,
						init_error_buf_);
		init_serial_combo<utils::SerialPort>(
						0, cfg_, "GPSPort2", serial_ports,
						gpsout2_cb_, gpsout2_speed_tf_, gpsout2_lines_lb_, 
						gpsout2_serialport_,
						init_error_buf_);
		init_serial_combo<utils::SerialPort>(
						0, cfg_, "CompassPort", serial_ports,
						compassout_cb_, compassout_speed_tf_, compassout_lines_lb_, 
						compassout_serialport_,
						init_error_buf_);


        // Open TCP port, if any.
        {
            int tcp_port = 0;
            if (cfg_.get_int("Listen", tcp_port)) {
                FXString    sport;
                sport.format("%d", tcp_port);
                tcpout_port_lb_->setText(sport);

                try {
                    tcpout_bbc_.open(tcp_port, "");
                    tcpout_status_lb_->setText("Opened.");
                } catch (const exception& e) {
                    tcpout_status_lb_->setText("Not configured.");
                    init_error_buf_ += "\n";
                    init_error_buf_ += e.what();
                }
            } else {
                tcpout_status_lb_->setText("Not configured.");
            }
        }
	}
	
	new FXHorizontalSeparator(mainframe, SEPARATOR_RIDGE|LAYOUT_LEFT|LAYOUT_FILL_X);
	/* Button frame. */
	{
		FXHorizontalFrame*	buttonframe = new FXHorizontalFrame(mainframe, frameopts|PACK_UNIFORM_WIDTH);
		new FXButton(buttonframe, "Save settings.", 0, this, ID_SAVE_PREFERENCES);
		new FXButton(buttonframe, "Quit", 0, getApp(), FXApp::ID_QUIT);
	}

	/* Start input/output pollers. */
	getApp()->addTimeout(this, ID_INPUT_POLL, INPUT_POLL_INTERVAL);
	getApp()->addTimeout(this, ID_OUTPUT_POLL, OUTPUT_POLL_INTERVAL);
	getApp()->addTimeout(this, ID_DISPLAY_CANVAS, DISPLAY_POLL_INTERVAL);
}

/********************************************************************************/
MainWindow::~MainWindow()
{
}

/********************************************************************************/
void
MainWindow::create()
{
	inherited::create();
	show(PLACEMENT_SCREEN);

	if (init_error_buf_.size() > 0)
		FXMessageBox::error(getApp(), MBOX_OK, "MixGPS initialization error", "%s", init_error_buf_.c_str());
}

/********************************************************************************/
long
MainWindow::onComboBoxChange(FXObject*, FXSelector, void*)
{
	return 1;
}

/********************************************************************************/
long
MainWindow::onSerialPortConfig(FXObject*, FXSelector, void*)
{
	return 1;
}

/********************************************************************************/
long
MainWindow::onSavePreferences(FXObject*, FXSelector, void*)
{
	try {
		cfg_.store();
	} catch (const std::exception& e) {
		FXMessageBox::error(getApp(), MBOX_OK, "MixGPS", "Error saving preferences: %s", e.what());
	} catch (...) {
		FXMessageBox::error(getApp(), MBOX_OK, "MixGPS", "Error saving preferences: unknown :(");
	}
	return 1;
}

/********************************************************************************/
long
MainWindow::onInputPoll(FXObject*, FXSelector, void*)
{
	utils::my_time	mt;
	std::string	line;
	char		xbuf[1024];
	int		last_logfile_lines = logfile_lines_;
	my_time		computer_time;
	bool		any_packets = false;
	int		i;

	try {
		GPGGA		gga;
		/* Get packets. */
		for (i=0; i<NGPS; ++i) {
			GPS&		gps = gps_[i];
			const int	last_linecount = gps.linecount;
			get_my_time(computer_time);
	
			while (gps.linereader.pop_line(mt, line) && parse_gga(line, computer_time, gga)) {
				mixgps_.handle(gga, i);
	
				if (logfile_ != 0) {
					fprintf(logfile_, "%02d%02d%02d.%03d %d %s\n",
						mt.hour, mt.minute, mt.second, mt.millisecond,
						i+1,
						line.c_str());
					++logfile_lines_;
				}
				/* Update gps.x, gps.y, gga.longitude, gga.altitude. */
				double	foo;
				local_of_gps(gga.latitude, gga.longitude, gga.altitude, gps.abs_xy.x, gps.abs_xy.y, foo);

				gps.latitude_lb->setText(xsprintf(xbuf, sizeof(xbuf), "%10.7f", gga.latitude));
				gps.longitude_lb->setText(xsprintf(xbuf, sizeof(xbuf), "%10.7f", gga.longitude));
				gps.x_lb->setText(xsprintf(xbuf, sizeof(xbuf), "%4.2f", gps.abs_xy.x));
				gps.y_lb->setText(xsprintf(xbuf, sizeof(xbuf), "%4.2f", gps.abs_xy.y));
				++gps.linecount;
				any_packets = true;
			}

			if (last_linecount < gps.linecount)
				gps.packets_lb->setText(xsprintf(xbuf, sizeof(xbuf), "%d", gps.linecount));
		}

		/* Update DX, DY, if needed. */
		if (any_packets) {
			double		center_x = 0.0;
			double		center_y = 0.0;

			for (i=0; i<NGPS; ++i) {
				center_x += gps_[i].abs_xy.x;
				center_y += gps_[i].abs_xy.y;
			}

			center_x /= NGPS;
			center_y /= NGPS;

			for (i=0; i<NGPS; ++i) {
				GPS&		gps = gps_[i];
				gps.dx_lb->setText(xsprintf(xbuf, sizeof(xbuf), "%4.2f", gps.abs_xy.x - center_x));
				gps.dy_lb->setText(xsprintf(xbuf, sizeof(xbuf), "%4.2f", gps.abs_xy.y - center_y));
			}
		}

		if (last_logfile_lines < logfile_lines_)
			logfile_lines_lb_->setText(xsprintf(xbuf, sizeof(xbuf), "%d", logfile_lines_));
	} catch (const std::exception&) {
		// FXMessageBox::error(getApp(), MBOX_OK, "MixGPS Input Error", "%s", e.what());
	} catch (...) {
		// FXMessageBox::error(getApp(), MBOX_OK, "MixGPS Input Error", "Unknown Error :(");
	}

	/* Start us again. */
	getApp()->addTimeout(this, ID_INPUT_POLL, INPUT_POLL_INTERVAL);
	return 1;
}

/********************************************************************************/
long
MainWindow::onOutputPoll(FXObject*, FXSelector, void*)
{
	GPGGA		gga;
	my_time		computer_time;
	double		compass_direction = 0;
	double		gps_direction = 0;
	char		xbuf[200];
	double		speed = 0;
	double		gps12 = 0;
	double		gps13 = 0;
	double		gps23 = 0;

	try {
		get_my_time(computer_time);
		std::string	gps_s;
		std::string	gps_s2;
		if (mixgps_.calculate(computer_time, gga, compass_direction, gps_direction, speed, gps12, gps13, gps23)) {
			/* $GPGGA */
			gps_s = string_of_gga(gga);
			gps_s += "\r\n";
            // OUT: serial port
			if (gpsout_serialport_.ok()) {
				gpsout_serialport_.write(gps_s.c_str(), gps_s.size());
				++gpsout_lines_;
			}

            // OUT: TCP GPS POSITION
            {
                // Construct packet.
                TS_GpsPosition  ts_gps_position;
                ts_gps_position.rtime       = gga.gps_time;
                ts_gps_position.gpstime     = gga.gps_time;
                ts_gps_position.latitude    = gga.latitude;
                ts_gps_position.longitude   = gga.longitude;
                ts_gps_position.altitude    = gga.altitude;
                ts_gps_position.pdop        = gga.pdop;
                ts_gps_position.nr_of_satellites    = gga.nr_of_satellites;
                ts_gps_position.signal_status       = gga.quality;

                // Send packet.
                tcpout_buf_.resize(0);
                tcpout_writer_.writeGpsPosition(ts_gps_position, tcpout_buf_);
	            tcpout_bbc_.write(reinterpret_cast<const char*>(&tcpout_buf_[0]), tcpout_buf_.size());
            }

		/* $GPVTG:
		 * 1) Track, degrees
		 * 2) T = True
		 * 3) Track, degrees
		 * 4) M = Magnetic
		 * 5) Speed, knots,
		 * 6) N = Knots
		 * 7) Speed, kilometers per hour
		 * 8) K = Kilometers per hour
		 * 9) Checksum
		 */
			sprintf(xbuf, "$GPVTG,%3.1f,T,%3.1f,M,%3.1f,N,%3.1f,K*10\r\n",
				gps_direction, gps_direction,
				speed / 1.852, speed);
			gps_s2 = xbuf;
			if (gpsout_serialport_.ok()) {
				gpsout_serialport_.write(xbuf, strlen(xbuf));
				++gpsout_lines_;
			}
            // OUT: TCP GPS SPEED
            {
                // Construct packet.
                TS_GpsSpeed ts_gps_speed;
                ts_gps_speed.rtime  = gga.gps_time;
                ts_gps_speed.speed  = gps_direction;
                ts_gps_speed.track  = speed;

                // Send packet.
                tcpout_buf_.resize(0);
                tcpout_writer_.writeGpsSpeed(ts_gps_speed, tcpout_buf_);
	            tcpout_bbc_.write(reinterpret_cast<const char*>(&tcpout_buf_[0]), tcpout_buf_.size());
            }

			/* COMPASS */
			char		compassbuf[200];
			sprintf(compassbuf, "$COMPA,%3.1f,0,0*00\r\n", compass_direction);
			if (compassout_serialport_.ok()) {
				compassout_serialport_.write(compassbuf, strlen(compassbuf));
				++compassout_lines_;
			}
            // OUT: TCP COMPASS
            {
                // Construct packet.
                TS_Compass  ts_compass;
                ts_compass.rtime        = gga.gps_time;
                ts_compass.direction    = compass_direction;

                // Send packet.
                tcpout_buf_.resize(0);
                tcpout_writer_.writeCompass(ts_compass, tcpout_buf_);
	            tcpout_bbc_.write(reinterpret_cast<const char*>(&tcpout_buf_[0]), tcpout_buf_.size());
            }

			/* Distances. */
			distance_gps12_->setText(xsprintf(xbuf, sizeof(xbuf), "%3.1f", gps12));
			distance_gps13_->setText(xsprintf(xbuf, sizeof(xbuf), "%3.1f", gps13));
			distance_gps23_->setText(xsprintf(xbuf, sizeof(xbuf), "%3.1f", gps23));

			/* Update counters. */
			gpsout_lines_lb_->setText(xsprintf(xbuf, sizeof(xbuf), "%d", gpsout_lines_));
			gpsout2_lines_lb_->setText(xsprintf(xbuf, sizeof(xbuf), "%d", gpsout_lines_));
			compassout_lines_lb_->setText(xsprintf(xbuf, sizeof(xbuf), "%d", compassout_lines_));
		}
		if (gpsout2_serialport_.ok()) {
			gpsout2_serialport_.write(gps_s.c_str(), gps_s.size());
			gpsout2_serialport_.write(gps_s2.c_str(), gps_s2.size());
		} 
	} catch (const std::exception&) {
		// pass
		// FXMessageBox::error(getApp(), MBOX_OK, "MixGPS Output Error", "%s", e.what());
	} catch (...) {
		// pass
		// FXMessageBox::error(getApp(), MBOX_OK, "MixGPS Output Error", "Unknown Error :(");
	}

    // Flush tcp broadcast.
    if (tcpout_bbc_.opened()) {
        NANOBEGIN;

		std::string	sbuf;
        int         sid;
		for (;;) {
			sbuf.resize(0);
			const int	this_round = tcpout_bbc_.read(sbuf, sid);
            if (this_round < 0) {
				break;
            }
        }

        NANOEND;
    }

    /* Start us again. */
	getApp()->addTimeout(this, ID_OUTPUT_POLL, OUTPUT_POLL_INTERVAL);
	return 1;
}

/********************************************************************************/
long
MainWindow::onDisplayPoll(FXObject* sender, FXSelector sel, void* data)
{
	my_time		computer_time;
	double		speed = 0;
	double		altitude = 0;

	try {
		get_my_time(computer_time);
		if (mixgps_.calculate(computer_time, gps_origin_, compass_direction_, gps_direction_, speed, altitude, gps_pos_, gps_pos2_)) {
			mixgps_.get_internals(gpsinfo_);
			onDisplayPaint(this, sel, data);
		}
	} catch (const std::exception&) {
		// pass
		// FXMessageBox::error(getApp(), MBOX_OK, "MixGPS Output Error", "%s", e.what());
	} catch (...) {
		// pass
		// FXMessageBox::error(getApp(), MBOX_OK, "MixGPS Output Error", "Unknown Error :(");
	}

	/* Start us again. */
	getApp()->addTimeout(this, ID_DISPLAY_CANVAS, DISPLAY_POLL_INTERVAL);
	return 1;
}

/********************************************************************************/
#define	NCOLOR	3
static FXColor
gps_colors[NCOLOR] = { FXRGB(255,0,0), FXRGB(0,255,0), FXRGB(0,0,255) };
static FXColor
current_colors[NCOLOR] = { FXRGB(255,0,0), FXRGB(0,255,0), FXRGB(0,0,255) };

/********************************************************************************/
static void
draw_posdot(	FXDC&		dc,
		const double	gps_x,
		const double	gps_y,
		const double	gps_center_x,
		const double	gps_center_y,
		const double	zoomf,
		const int	w2,
		const int	h2,
		int&		x,
		int&		y)
{
	x = w2 + (gps_y - gps_center_y) * zoomf;
	y = h2 - (gps_x - gps_center_x) * zoomf;

	dc.drawEllipse(x-2,y-2,4,4);
}

/********************************************************************************/
static void
draw_centerdot(	FXDC&		dc,
		const double	gps_x,
		const double	gps_y,
		const double	gps_center_x,
		const double	gps_center_y,
		const double	zoomf,
		const int	w2,
		const int	h2,
		int&		x,
		int&		y)
{
	x = w2 + (gps_y - gps_center_y) * zoomf;
	y = h2 - (gps_x - gps_center_x) * zoomf;

	dc.drawRectangle(x-3,y-3,6,6);
}

/********************************************************************************/
static void
my_draw_arrow(	FXDC&		dc,
		const double	gps_x,
		const double	gps_y,
		const double	angle,	// degrees
		const double	length,
		const double	gps_center_x,
		const double	gps_center_y,
		const double	zoomf,
		const int	w2,
		const int	h2)
{
	const int	x1 = w2 + (gps_y - gps_center_y) * zoomf;
	const int	y1 = h2 - (gps_x - gps_center_x) * zoomf;
	const double	r = zoomf * length;
	const int	x2 = x1 + r * sin(deg2rad(angle));
	const int	y2 = y1 - r * cos(deg2rad(angle));
	dc.drawLine(x1,y1,x2,y2);
}

/********************************************************************************/
long
MainWindow::onDisplayPaint(FXObject*, FXSelector, void*)
{
	const int	w = display_canvas_->getWidth();
	const int	h = display_canvas_->getHeight();
	const int	w2 = w / 2;
	const int	h2 = h / 2;
	const double	zoomf = 20;
	int		i,j;

	FXImage	picture(getApp(), NULL, IMAGE_SHMI|IMAGE_SHMP, w, h);
	picture.create();

	do { // this scope is for execution of FXDCWindow's destructor.
		FXDCWindow		dc(&picture);

		// 1. Clear display.
		dc.setForeground(~display_canvas_->getBackColor());
		dc.fillRectangle(0, 0, w, h);

		double		center_x = 0;
		double		center_y = 0;
		int		last_x,last_y;	// to draw lines between data points.
		int		x,y;

		// Calculate center X,Y.
		for (i=0; i<gps_pos_.size(); ++i) {
			center_x += gps_pos_[i].x;
			center_y += gps_pos_[i].y;
		}
		center_x /= gps_pos_.size();
		center_y /= gps_pos_.size();

		// Draw GPS and estimated data.
		for (i=0; i<gpsinfo_.size(); ++i) {
			// 2. Draw GPS data.
			dc.setForeground(i<NCOLOR ? gps_colors[i] : gps_colors[1]);
			const MixGPS::GPSInfo&	gpsi = gpsinfo_[i];

			for (j=0; j<gpsi.packets.size(); ++j) {
				draw_posdot(dc, gpsi.packets[j].x, gpsi.packets[j].y, center_x, center_y, zoomf, w2,h2, x,y);
				if (j>0)
					dc.drawLine(x,y,last_x,last_y);
				last_x = x;
				last_y = y;
			}

			// 3. Draw estimated data.
			dc.setForeground(i<NCOLOR ? current_colors[i] : current_colors[1]);
			draw_centerdot(dc, gps_pos_[i].x, gps_pos_[i].y, center_x, center_y, zoomf, w2, h2, x,y);
			dc.drawLine(x,y,last_x,last_y);
		}

		// Draw origin :)
		dc.setForeground(FXRGB(150,150,150));
		draw_centerdot(dc, gps_origin_.x, gps_origin_.y, center_x, center_y, zoomf, w2,h2,x,y);

		my_draw_arrow(dc, gps_origin_.x, gps_origin_.y, compass_direction_, 5, center_x, center_y, zoomf, w2,h2);
	} while (0);

	FXDCWindow	dc_window(display_canvas_);
	dc_window.drawImage(&picture, 0, 0);

	return 1;
}

/********************************************************************************/
long
MainWindow::onDisplayConfigure(FXObject*, FXSelector, void*)
{
	return 1;
}

/********************************************************************************/
/********************************************************************************/
int
main(	int	argc,
	char**	argv)
{
	FXApp		app("MixGPS", "EE");

	app.init(argc, argv);

	MainWindow	mw(&app);
	app.create();

	return app.run();
}
