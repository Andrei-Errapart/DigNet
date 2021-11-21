/**
vim: ts=4
vim: shiftwidth=4
*/
#ifndef Ship_h_
#define Ship_h_

#include <string>    // std::string

#include <fx.h>

#include <utils/ugl.h>
#include <utils/Config.h>
#include <utils/util.h>

#include "DigNetMessages.h"
#include "globals.h"

/** Ship (i.e. ModemBox/GpsViewer on the wild)
    Handles drawing and saving itself and shadow to file aswell
Note: only std::vector<Ship*> allowed.
*/
class Ship {
public:
    // No outside modifying.
    unsigned int            Number;         ///< Ship number.
    std::string             Name;           ///< Ship name.
    bool                    Has_Valid_Name; ///< Ship has real name, GPS offsets and other needed data
    bool                    Has_Barge;      ///< Do we have barge connected?
    double                  Speed_DX;       ///< Ship speed in X direction
    double                  Speed_DY;       ///< Ship speed in Y direction
    utils::Option<ShipPosition> Pos;        ///< Position from the last SetPosition.
    utils::Option<GpsPosition>  GPS[2];     ///< GPS positions
    double                  EnlargeFactor;  ///< Number of times ship is drawn bigger than normal.

    // Modembox voltage information
    double                          modembox_minimum_voltage;      ///< When reported voltage drops below this level a message is shown
    utils::Option<double>           modembox_last_voltage;         ///< Last voltage recieved from modembox
    int                             voltage_received_countdown;    ///< If reaches zero a warning is displayed about not recieving voltage information for too long
    utils::Option<utils::my_time>   last_read_time;                ///< Time when voltage was last recorded

    // Previous position of the ship.
    utils::Option<ShipPosition> Previous_Pos;
    bool                        Previous_Has_Barge;


    /** Constructor. */
    Ship(
        const int number
        );

    /** Draw our ship (OpenGL context required). 
        @param show_shadow draw ship's shadow if true
      */
    void
    Draw(
        const bool  show_shadow
    );

    /** Update ship position.
        The value is cached.
        @param x ship X coordinate, meters, Lambert-Est
        @param y ship Y coordinate, meters, Lambert-Est
        @param heading ship heading (in degrees, clockwise, zero at North)
      */
    void
    SetPosition(
        const double    x,
        const double    y,
        const double    heading);

    /** Update GPS position.
        @param gpsIndex GPS number (0 or 1)
        @param gpsPosition gps position, @see GpsPosition in globals.h
      */
    void
    SetGpsPosition(
        const unsigned int  gpsIndex,
        const GpsPosition&  gpsPosition);

    /** Update ship shadow position.
        The value is cached.
        @param x shadow X coordinate, meters, Lambert-Est
        @param y shadow Y coordinate, meters, Lambert-Est
        @param heading shadow heading (in degrees, clockwise, zero at North)
     */
    void
    SetShadowPosition(
        const double    x,
        const double    y,
        const double    heading);

    /** Set barge. The given value is cached.
        @param value true If ship has a barge
     */
    void
    SetHasBarge(
        const bool value
    );

    /** Set enlargement factor. 
        @param NewEnlargeFactor enlargement factor, multiplier (1 == no enlargement, >1 enlarge)
      */
    void
    SetEnlargeFactor(
        const double    NewEnlargeFactor
    );

    /** Create a shadow of the ship and send to server*/
    void
    SetPositionAsShadow();

    /** Saves current shadow to file */
    void
    SaveShadow();

    /** Returns the speed of the ship in knots */
    double
    GetSpeed();

    /** Save all ship positions to file
        @param ship own ship
        @param other_ships other ships
      */
    static void
    SaveShipsToFile(
        Ship& ship, 
        std::vector<Ship*>& other_ships
    );

    /** Loads own ship from file
        @param cfg FileConfig to load from
        @param name ship name to load
        @param gps1_dx GPS 1 x offset from ship 0,0 coordinates
        @param gps1_dy GPS 1 y offset from ship 0,0 coordinates
        @param gps2_dx GPS 2 x offset from ship 0,0 coordinates
        @param gps2_dy GPS 2 y offset from ship 0,0 coordinates
      */
    static bool
    LoadMyShip(
        utils::FileConfig   cfg,
        int&                number,
        std::string&        name,
        double&             gps1_dx,
        double&             gps1_dy,
        double&             gps2_dx,
        double&             gps2_dy
    );

    /** Loads ship information from .ini (name). Returns true on success
      */
    bool
    LoadInfo();

    /** Returns true if ship is dredger. 
        Warning! Currently hardocded with ship ID's
    */
    inline bool
    IsDredger() const 
    {
        // FIXME: this is a hack to distinguish between dredgers and tugboats.
        return Number==2 || Number==3;
    }

private:
    /// Loads shadow position from file
    void 
    LoadPosition();

    bool                    Drawing_Dirty_;    ///< Update coordinates before drawing?
    FXColor                 Color_;
};

#endif /* Ship_h_ */
