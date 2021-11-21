#ifndef Main_Window_h_
#define Main_Window_h_


#include <utils/util.h>
#include <utils/myrtcm.h>	
#include <utils/ugl.h>

#include <utils/NMEA.h>
#include <fxutils/PointStorageViewer.h>
#include <utils/Serial.h> // AsyncIO
#include <utils/Transformations.h>

#include <fx.h>
#include <fxutils/fxutil.h>
#include <fxutils/MapAreaGL.h>
#include <fxutils/SerialLink.h>

#include <memory>   // std::auto_ptr
#include <string>
#include <vector>
#include <map>

#include "Ship.h"

/// Axis point.
typedef struct {
	std::string	name;	///< Name.
	double		x;	///< X-coordinate (Lambert-Est)
	double		y;	///< Y-coordinate (Lambert-Est)
} AxisPoint;

/// Axis...
typedef struct Axis_st {
	std::vector<AxisPoint>		points;		/// Array of \c AxisPoint
	utils::ugl::VPointArray		points_gl;	/// Copy of points in OpenGL memory space.
	Axis_st() : points_gl(GL_LINE_STRIP) {}
} Axis;

//=====================================================================================//
/// GpsViewer Main Window.
class MainWindow : public FXMainWindow {
	FXDECLARE(MainWindow)
protected:
			MainWindow() : cfg_("") {}
public:
	typedef FXMainWindow	inherited;

	// Constructor
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
		ID_TOGGLE_BACKGROUND,

		ID_ZOOM_IN,
		ID_ZOOM_OUT,
		ID_SAVE_POSITION,
		ID_FOLLOW_NEXT_SHIP,
		ID_EXIT
	};

    /** Setup Control Panel (buttons and widgets on the right) according to:
    1. modembox connection - and our ship name.
    2. server link - if any.
    3. SimpleGPS connection.
    */
    void
    setupControlPanel();

	/// Poll locally connected GPS devices
	void		onSimpleGpsInput(const char* data, unsigned int len);

	/// Gets executed once per second
	long		onTimer1Hz(FXObject*, FXSelector, void*);

	/// Paint map area.
	long		onPaintMapArea(FXObject*,FXSelector,void*);
	long		onUpdMapArea(FXObject*,FXSelector,void*);

	/// Change background according to message selector.
	long		onCmdToggleBackground(FXObject*,FXSelector,void*);

	/// Handle key releases. This is used to implement hotkeys on space and backspace keys.
	long		onKeyRelease(FXObject*,FXSelector,void*);

	void		updateShipZooms();
	void		focusShip();

	/// Zoom in
	long		onCmdZoomIn(FXObject* sender, FXSelector sel, void* ptr);
	/// Zoom out
	long		onCmdZoomOut(FXObject* sender, FXSelector sel, void* ptr);
    /// MapArea: place an object (left mouse button press) or start dragging.
	long		onLeftButtonPressMaparea(FXObject* sender, FXSelector sel, void* ptr);
    /// MapArea: stop draggin when in pan mode.
	long		onLeftButtonReleaseMaparea(FXObject* sender, FXSelector sel, void* ptr);
    /// MapArea: update object coordinates.
	long		onMotionMaparea(FXObject* sender, FXSelector sel, void* ptr);
	/// Start following next ship
	long		onCmdFollowNextShip(FXObject* sender, FXSelector sel, void* ptr);
	/// Quit the program.
	long		onCmdExit(FXObject*,FXSelector,void*);

	// Initialize
	virtual void	create();

	// Handles window resizing
	virtual void resize(int w, int h);

    /** Builds and sets window title  */
    void SetTitle();
private:
    /* ======================== SIMPLE GPS ============================= */
    std::auto_ptr<fxutils::SerialLink>  simple_gps_io_;
	std::string						    simple_gps_;
	utils::LineDecoder				    simple_gps_decoder_;
	utils::TRANSFORM					simple_gps_transform_;

    /* ========================= TIMERS =============================== */
	int					            timer_10s_;				///< Counts numbers from 10 ... 0.
	int					            timer_60s_;				///< Counts numbers from 60 ... 0.
    int                             timer_3600s_;           ///< Counts from 3600 ... 0.
    int                             time_since_startup_;    ///< Increases by one every second

    /* ========================= SHIPS ================================ */
    /// Ship to be followed, pointer to the \c ships_ element
	Ship*					followed_ship_;
    /// Pointer to our own ship, real object will be in the \c ships_ list
	Ship*					our_ship_;

	fxutils::MapAreaOptions			map_area_options_;
	utils::FileConfig			    cfg_;
	/// Maximum length of the trail.
	int								max_track_length_;
	int								track_sparse_factor_;
	int								track_sparse_counter_;

    /* =========== CONTROL PANEL: BUTTONS FRAME ======================= */
    /** switcher frame. 0=VIEWMODE_NORMAL, 1=VIEWMODE_WORKLOG, 2=VIEWMODE_VOLTAGES. */
    FXSwitcher*         frame_switcher_;

	FXVerticalFrame*	frame_buttons_;

	FXLabel*			label_dig_count_;
	FXLabel*			label_fill_count_;

	FILE*				log_file_;
	utils::ugl::VPointArray	markers_;
	Axis				axis1_;
	Axis				axis2_;
	utils::ugl::VPointArray	nearest_point_;
    /** If bigger than zero, start counting down on every GPS position.
    When zero, shutdown and exit. */
	int					shutdown_counter_;

	utils::ugl::VPointArray                     points_;		///< Just points on the GL area.

	FXFont*				                        myfont_;
	fxutils::MapAreaGL*			                map_area_;
	std::auto_ptr<fxutils::PointStorageViewer>  ps_viewer_;
	FXLabel*			axis_name_text_lbl_;
	FXLabel*			axis_name_lbl_;
	FXLabel*			axis_progress_lbl_;
	FXLabel*			axis_distance_lbl_;
	FXLabel*			axis_direction_lbl_;



    unsigned int        background_index_;

    // Name of build. Contains project name, build number, build revision and build date
    std::string build_name_;
};

#endif // MAIN_WINDOW_H
