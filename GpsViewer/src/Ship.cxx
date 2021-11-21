/**
vim: ts=4
vim: shiftwidth=4
*/

#include <utils/math.h>
#include <windows.h>
#include <gl/gl.h>
#include <map>
#include <string>

#include "Ship.h"

#include <fxutils/ColorMixer.h>
#include <utils/Config.h>
#include <utils/mystring.h>
#include <dl_dxf.h> //dxfColors

#include "DigNetMessages.h"
#include "DxfToLines.h"

static const FXColor        SHIP_COLOR =    FXRGB(255,0,255);
static const FXColor        SHADOW_COLOR =  FXRGB(80,80,80);
static const FXColor        DIGGER_COLOR =  FXRGB(80,80,255);

static const char*  SHIP_POSITION_FILE = "ShadowPositions.ini";

static const unsigned char MAX_SHIPS=10;
#define    NUMBER_OF_ELEMENTS(c_array)    (sizeof(c_array) / sizeof(c_array[0]))

using namespace utils;
using namespace fxutils;

static void
getgraphics(
        const std::string&          plan_filename,
        std::vector<std::vector<LINE_SEGMENT>* >&   graphics
)
{
    static char* filenames[] = {
        "tartu.dxf",
        "tartu_barge.dxf",
        "omedu_barge.dxf",
        "velsa 1.dxf",
        "wille.dxf",
        "navigator.dxf"
    };
    static std::map<const std::string, std::vector<LINE_SEGMENT>* > ship_graphics;
    std::vector<const std::string> files;
    // Find needed filenames. Ugly, but works.
    if (stringcasecmp(plan_filename, "Tartu.dxf")==0) {
        files.push_back(filenames[0]);
        files.push_back(filenames[1]);
    } else if (stringcasecmp(plan_filename, "Omedu.dxf")==0) {
        files.push_back(filenames[0]);
        files.push_back(filenames[2]);
    } else if (stringcasecmp(plan_filename, "Tartu_Barge.dxf")==0) {
        files.push_back(filenames[0]);
        files.push_back(filenames[1]);
    } else if (stringcasecmp(plan_filename, "Omedu_Barge.dxf")==0) {
        files.push_back(filenames[0]);
        files.push_back(filenames[2]);
    } else if (stringcasecmp(plan_filename, "Velsa 1.dxf")==0) {
        files.push_back(filenames[3]);
    } else if (stringcasecmp(plan_filename, "Wille.dxf")==0) {
        files.push_back(filenames[4]);
    } else if (stringcasecmp(plan_filename, "Navigator.dxf")==0) {
        files.push_back(filenames[5]);
    }

    for (unsigned int i=0;i<files.size();i++){
        std::map<const std::string, std::vector<LINE_SEGMENT>* >::const_iterator lines_it = ship_graphics.find(files[i]);
        if (lines_it == ship_graphics.end()){
            TRACE_PRINT("info", ("Loading new ship graphics: %s", files[i].c_str()));
            std::pair<std::string, std::vector<LINE_SEGMENT>* > elem;
            std::vector<LINE_SEGMENT>* line=new std::vector<LINE_SEGMENT>();
            DxfLoad(files[i], *line);
            elem.first = files[i];
            elem.second = line;
            ship_graphics.insert(elem);
            lines_it = ship_graphics.find(files[i]);
        }
        graphics.push_back(lines_it->second);
    }
}

/*****************************************************************************/
static void
draw_vpoints(
        const std::string&  filename,
        const double        enlargement_factor,
        const FXColor       color,
        const double        x,
        const double        y,
        const double        heading,
        const bool          is_shadow = false
)
{
    const double    offset_x = ugl::get_origin_x();
    const double    offset_y = ugl::get_origin_y();
    //TRACE_ENTER("draw_vpoints");

    std::vector<std::vector<LINE_SEGMENT>*> plans;
    getgraphics(filename, plans);
    const TransformMatrix2 trn(deg2rad(heading), enlargement_factor);
    int lastColor=-1;
    double lastWidth = -1.0;
    double alpha = 1.0;
    if (is_shadow){
        glEnable(GL_COLOR_MATERIAL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);     
    }
    for (unsigned int i=0;i<plans.size();++i){
        int mode =  GL_LINE_STRIP;
        if (is_shadow){
            alpha = 0.4;
        }
        glBegin(mode);
        const std::vector<LINE_SEGMENT>& planlines = *plans[i];
        for (unsigned int j=0; j<planlines.size(); ++j) {
            const LINE_SEGMENT pl = planlines[j];
            if (pl.colour != lastColor){
                lastColor = pl.colour;
                glEnd();
                const double* color = dxfColors[lastColor];
                glColor4d(color[0], color[1], color[2], alpha);
                glBegin(mode);
            }
            if (pl.width != lastWidth){
                lastWidth = pl.width;
                glEnd();
                glLineWidth(lastWidth);
                glBegin(mode);
            }

            // x,y on plaanide peal vahetuses.
            const double dx1 = trn.tx(pl.y, pl.x);
            const double dy1 = trn.ty(pl.y, pl.x);
            // geodeetilistes koordinatides x:yles, y:paremale.
            if (pl.line_start && j>0) {
                glEnd();
                if (is_shadow && pl.polygon){
                    mode = GL_POLYGON;
                } else {
                    mode = GL_LINE_STRIP;
                }
                glBegin(mode);
            }
            // vpoints.append(y+dx1, x+dy1, 0.0, color_r, color_g, color_b);
            if (is_shadow){
                glVertex3d(y+dx1-offset_x, x+dy1-offset_y, -1.0);
            } else {
                glVertex3d(y+dx1-offset_x, x+dy1-offset_y, 0.0);
            }
        }
        glEnd();
    }
    if (is_shadow){
        glDisable(GL_COLOR_MATERIAL);
        glDisable(GL_BLEND);
    }
}


/*****************************************************************************/
Ship::Ship(
        const int number
     )
:
    Number(number)
    ,Name("")
    ,Has_Valid_Name(false)
    ,Has_Barge(false)
    ,Drawing_Dirty_(false)
    ,EnlargeFactor(0.95)
    ,Speed_DX(0)
    ,Speed_DY(0)
{
    LoadPosition();
    LoadInfo();

    // FIXME: save the state to file and load when program starts
    Has_Barge = !IsDredger();
}

/*****************************************************************************/
static void
draw_cross(
    const double x,
    const double y,
    const double r,
    const double g,
    const double b
)
{
    const double dd=1;

    glColor3f(r, g, b);
    glBegin(GL_LINES);
        glVertex3f(y-dd, x+dd, 0.0);
        glVertex3f(y+dd, x-dd, 0.0);
        glVertex3f(y+dd, x+dd, 0.0);
        glVertex3f(y-dd, x-dd, 0.0);
    glEnd();

}
/*****************************************************************************/
void
Ship::Draw(
    const bool  show_shadow
)
{
    if (!Has_Valid_Name && Pos.IsValid())
    {
        // Ship hasn't yet been identified, draw a circle
        const double off_x = ugl::get_origin_x();
        const double off_y = ugl::get_origin_y();
        glColor3d(1.0, 0.0, 0.0);
        glBegin(GL_LINE_STRIP);
            for (int i=0; i<=10; ++i) {
                const double dx = 5 * cos(deg2rad(i*36)) * EnlargeFactor;
                const double dy = 5 * sin(deg2rad(i*36)) * EnlargeFactor;
                glVertex3d(Pos().Y+dy-off_x, Pos().X+dx-off_y, 0.0);
            }
        glEnd();
        return;
    }
    if (Previous_Pos.IsValid() && show_shadow) {
        // Draw shadow with half the line width
        double lastWidth=1.0;
        glGetDoublev(GL_LINE_WIDTH, &lastWidth);
        glLineWidth(lastWidth/2);
        draw_vpoints(Name+".dxf", EnlargeFactor, SHADOW_COLOR, Previous_Pos().X, Previous_Pos().Y, Previous_Pos().Heading, true);
        if (Previous_Has_Barge) {
            draw_vpoints(Name+"_Barge.dxf", EnlargeFactor, SHADOW_COLOR, Previous_Pos().X, Previous_Pos().Y, Previous_Pos().Heading, true);
        }
        glLineWidth(lastWidth);
    }

    if (Pos.IsValid()) {
        // FIXME: this is a hack to display correct colors and headings on ships.
        const FXColor   color =
            IsDredger()
                ? DIGGER_COLOR
                : SHIP_COLOR;
        draw_vpoints(Name+".dxf", EnlargeFactor, color, Pos().X, Pos().Y, Pos().Heading);
        if (IsDredger()) {
            // Name=Vesla 1, Number=2 is drawn backwards, thus the 180 degrees hack.
            const double    heading_length = 30.0;
            const double    fii = deg2rad(Pos().Heading + (Number==2 ? 180 : 0));
            const double    offset_x = ugl::get_origin_x();
            const double    offset_y = ugl::get_origin_y();
            glBegin(GL_LINE_STRIP);
            glVertex3d(Pos().Y - offset_x, Pos().X - offset_y, 0.0);
            glVertex3d(Pos().Y + heading_length*sin(fii) - offset_x, Pos().X + heading_length*cos(fii) - offset_y, -1);
            glEnd();
        } else {
            const double    factor = 10.0 * EnlargeFactor;
            const double    offset_x = ugl::get_origin_x();
            const double    offset_y = ugl::get_origin_y();
            glBegin(GL_LINE_STRIP);
            glVertex3d(Pos().Y - offset_x, Pos().X - offset_y, 0.0);
            glVertex3d(Pos().Y+Speed_DY*factor - offset_x, Pos().X+Speed_DX*factor - offset_y, -1.0);
            glEnd();
        }
        if (Has_Barge) {
            draw_vpoints(Name+"_Barge.dxf", EnlargeFactor, color, Pos().X, Pos().Y, Pos().Heading);
        }
    }

	glEnable(GL_COLOR);
    for (unsigned int gpsIndex=0; gpsIndex<2; ++gpsIndex) {
        if (GPS[gpsIndex].IsValid()) {
            draw_cross(GPS[gpsIndex]().X-ugl::get_origin_y(), GPS[gpsIndex]().Y-ugl::get_origin_x(), 1-gpsIndex, 1, gpsIndex);
        }
    }
}

/*****************************************************************************/
static inline bool
differ(
    const double    x1,
    const double    x2)
{
    return fabs(x1-x2) > 0.01;
}

/*****************************************************************************/
void
Ship::SetPosition(
        double    x,
        double    y,
        double    heading)
{
    if (!Pos.IsValid() || differ(x, Pos().X) || differ(y, Pos().Y) || differ(heading, Pos().Heading)) {
        ShipPosition    sp;
        sp.X = x;
        sp.Y = y;
        sp.Heading = heading;
        my_time_of_now(sp.Time);
        Pos = sp;
        Drawing_Dirty_ = true;
    }
}

/*****************************************************************************/
void
Ship::SetGpsPosition(
    const unsigned int  gpsIndex,
    const GpsPosition&  gpsPosition)
{
    if (gpsIndex>=0 && gpsIndex<2) {
        if (gpsIndex == 0) {
            const Option<GpsPosition>&  gpsLast = GPS[gpsIndex];
            if (gpsLast.IsValid()) {
                const double    dt = gpsPosition.Time - gpsLast().Time;
                const double    factor = dt<1.0 ? 1.0 : (1.0/dt);
                const double    dx = factor*(gpsPosition.X - gpsLast().X);
                const double    dy = factor*(gpsPosition.Y - gpsLast().Y);
                const double    rank = 1.0;
                Speed_DX = (1.0-rank)*Speed_DX + rank*dx;
                Speed_DY = (1.0-rank)*Speed_DY + rank*dy;
            }
        }
        GPS[gpsIndex] = gpsPosition;
    }
}

/*****************************************************************************/
void
Ship::SetShadowPosition(
        double    x,
        double    y,
        double    heading)
{
    if (!Previous_Pos.IsValid() || differ(x, Previous_Pos().X) || differ(y, Previous_Pos().Y) || differ(heading, Previous_Pos().Heading)) {
        ShipPosition pos;
        pos.X = x;
        pos.Y = y;
        pos.Heading = heading;
        my_time_of_now(pos.Time);
        Previous_Pos = pos;

        Drawing_Dirty_ = true;
    }
}

/*****************************************************************************/
void
Ship::SetHasBarge(
    bool value)
{
    if (Has_Barge != value) {
        Has_Barge = value;

        Drawing_Dirty_ = true;
    }
}

/*****************************************************************************/
void
Ship::SetEnlargeFactor(
    const double    NewEnlargeFactor
    )
{
    if (differ(EnlargeFactor, NewEnlargeFactor) && NewEnlargeFactor>=1.0) {
        EnlargeFactor = NewEnlargeFactor;

        Drawing_Dirty_ = true;
    }
}

/*****************************************************************************/


/// Save all ship positions to file
void
Ship::SaveShipsToFile(
        Ship& ship, 
        std::vector<Ship*>& other_ships)
{
    utils::FileConfig cfg(SHIP_POSITION_FILE);
    try 
    {
        cfg.load();
    } catch (...)
    {
        // pass
    }
    /// FIXME
    //ship.SavePosition(cfg);
    for(unsigned int i = 0; i < other_ships.size(); i++)
    {
        //other_ships[i]->SavePosition(cfg);
    }
    try 
    {
        cfg.store();
    } catch (...)
    {
        // pass
    }
}


/*****************************************************************************/
void
Ship::SetPositionAsShadow()
{
    Previous_Pos = Pos();
}

/*****************************************************************************/
void
Ship::SaveShadow()
{
    if (!Previous_Pos.IsValid())
    {
        return;
    }
    utils::FileConfig cfg(SHIP_POSITION_FILE);
    try 
    {
        cfg.load();
    } catch (...)
    {
        // pass
    }
    char xbuf[16];
    sprintf(xbuf, "Ship-%d", Number);

    cfg.set_section(xbuf);
    cfg.set_string( "Name",             Name);
    cfg.set_bool(   "PreviousValid",    Previous_Pos.IsValid());
    cfg.set_double( "PreviousX",        Previous_Pos().X);
    cfg.set_double( "PreviousY",        Previous_Pos().Y);
    cfg.set_double( "PreviousHeading",  Previous_Pos().Heading);
    cfg.set_bool(   "PreviousHasBarge", Previous_Has_Barge);
    try 
    {
        cfg.store();
    } catch (...)
    {
        // pass
    }


}

/*****************************************************************************/
double
Ship::GetSpeed()
{
    return sqrt(Speed_DX * Speed_DX + Speed_DY * Speed_DY)*1.852;
}
/*****************************************************************************/
void 
Ship::LoadPosition()
{
    utils::FileConfig cfg(SHIP_POSITION_FILE);
    try 
    {
        cfg.load();
    } catch (...)
    {
        // Couldn't read, exit
        return;
    }
    cfg.set_section(ssprintf("Ship-%d", Number));
    bool valid = false;
    cfg.get_bool(       "PreviousValid",    false,  valid);
    if (valid) {
        ShipPosition pos;
        cfg.get_double( "PreviousX",        0.0,    pos.X);
        cfg.get_double( "PreviousY",        0.0,    pos.Y);
        cfg.get_double( "PreviousHeading",  0.0,    pos.Heading);
        cfg.get_bool(   "PreviousHasBarge", false,  Previous_Has_Barge);
        Previous_Pos = pos;
    }
}

/*****************************************************************************/
bool
Ship::LoadInfo()
{
    bool r = false;
    utils::FileConfig cfg(SHIP_POSITION_FILE);
    try 
    {
        cfg.load();
    } catch (...)
    {
        // Couldn't read, exit
        return false;
    }

    std::string name=ssprintf("Ship-%d", Number);
    cfg.set_section(name);
    std::string nm;
    if (cfg.get_string( "Name", "", nm)) {
        Name = nm;
        r=true;
    }
    return r;
}

/*****************************************************************************/
bool 
Ship::LoadMyShip(
        utils::FileConfig   cfg,
        int&                number,
        std::string&        name,
        double&             gps1_dx,
        double&             gps1_dy,
        double&             gps2_dx,
        double&             gps2_dy
)
{
    cfg.set_section("MyShip");
    
    std::string ship_name;
    unsigned int num;
    double g1_dx, g1_dy, g2_dx, g2_dy;
    if(cfg.get_uint("Number", 1, num) &&
        cfg.get_string("Name",    "",ship_name) && 
        cfg.get_double("Gps1_DX", 0, g1_dx) &&
        cfg.get_double("Gps1_DY", 0, g1_dy) &&
        cfg.get_double("Gps2_DX", 0, g2_dx) &&
        cfg.get_double("Gps2_DY", 0, g2_dy)) 
    {
        name    =    ship_name;
        number  =    num;
        gps1_dx =    g1_dx;
        gps1_dy =    g1_dy;
        gps2_dx =    g2_dx;
        gps2_dy =    g2_dy;
        return true;
    } else {
        return false;
    }
}
