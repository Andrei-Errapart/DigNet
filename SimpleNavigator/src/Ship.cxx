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

static const FXColor		SHIP_COLOR =	FXRGB(255,0,255);
static const FXColor		SHADOW_COLOR =	FXRGB(80,80,80);
static const FXColor		DIGGER_COLOR =	FXRGB(80,80,255);

static const char* SHIP_POSITION_FILE = "ShadowPositions.ini";

static const unsigned char MAX_SHIPS=10;
#define	NUMBER_OF_ELEMENTS(c_array)	(sizeof(c_array) / sizeof(c_array[0]))

using namespace utils;
using namespace fxutils;

static void
getgraphics(const std::string&          plan_filename,
            std::vector<std::vector<LINE_SEGMENT>* >&   graphics)
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
		//ugl::VPointArray&	vpoints,
		const std::string&	filename,
		const double		enlargement_factor,
		const FXColor		color,
		const double		x,
		const double		y,
		const double		heading,
		const bool		    is_shadow = false)
{
	const double	offset_x = ugl::get_origin_x();
	const double	offset_y = ugl::get_origin_y();
	//TRACE_ENTER("draw_vpoints");

    std::vector<std::vector<LINE_SEGMENT>*> plans;
    getgraphics(filename, plans);
	const TransformMatrix2	trn(deg2rad(heading), enlargement_factor);
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
		    const LINE_SEGMENT	pl = planlines[j];
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
		    const double		dx1 = trn.tx(pl.y, pl.x);
		    const double		dy1 = trn.ty(pl.y, pl.x);
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
Ship::Ship():
    Name("")
	,Drawing_Dirty_(false)
	,EnlargeFactor(0.95)
{
}

/*****************************************************************************/
void
Ship::Draw(
    const bool  show_shadow
)
{
    if (Pos.IsValid()) {
        draw_vpoints(Name+".dxf", EnlargeFactor, SHIP_COLOR, Pos().X, Pos().Y, Pos().Heading);
    }
}

/*****************************************************************************/
static inline bool
differ(
	const double	x1,
	const double	x2)
{
	return fabs(x1-x2) > 0.01;
}

/*****************************************************************************/
void
Ship::SetPosition(
		double	x,
		double	y,
		double	heading)
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
Ship::SetEnlargeFactor(
	const double	NewEnlargeFactor
	)
{
	if (differ(EnlargeFactor, NewEnlargeFactor) && NewEnlargeFactor>=1.0) {
		EnlargeFactor = NewEnlargeFactor;

		Drawing_Dirty_ = true;
	}
}

