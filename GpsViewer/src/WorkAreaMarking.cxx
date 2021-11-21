#include <windows.h>
#include <gl/gl.h>

#include <utils/math.h>
#include <utils/mystring.h>

#include <vector>   // std::vector.
#include <string>   // std::string.

#include <fx.h>

#include "WorkAreaMarking.h"
#include "resources.h"



using namespace utils;
using namespace std;

//=====================================================================================//
/** Reload texture.
*/
static void
reload_texture(
    ugl::Texture&           texture,
    const unsigned char*    bmp_image,
    FXColor                 background_color,
    FXBMPImage*&            bmp,
    FXApp*                  app
)
{
    if (bmp == 0) {
        bmp = new FXBMPImage(app, bmp_image);
    }
    const unsigned int          h = bmp->getHeight();
    const unsigned int          w = bmp->getWidth();
    std::vector<ugl::rgba>      row(w);
    const ugl::rgba             pixel_background = {
                                    FXREDVAL(background_color),
                                    FXGREENVAL(background_color),
                                    FXBLUEVAL(background_color),
                                    0x00 };

    if (!texture.IsOpen()) {
        texture.open(h, w);
    }

    for (unsigned int i=0; i<h; ++i) {
        for (unsigned int j=0; j<w; ++j) {
            const FXColor   c = bmp->getPixel(j, i);
            const ugl::rgba pixel = { FXREDVAL(c), FXGREENVAL(c), FXBLUEVAL(c), 0xFF };
            if (pixel.r==0xFF && pixel.g==0xFF && pixel.b==0xFF) {
                row[j] = pixel_background;
            } else {
                row[j] = pixel;
            }
        }
        texture.update_row(row, i);
    }
}

//=====================================================================================//
WorkAreaMarking::WorkAreaMarking()
:
    cfg_("")
    ,Source(SOURCE_LOCAL)
    ,LastBackColor_(0)
    ,Angle(0)
{
    memset(TextureImages_, 0, sizeof(TextureImages_));
}

//=====================================================================================//
WorkAreaMarking::WorkAreaMarking(
    const std::string&  initialization_filename
)
:
    cfg_(initialization_filename)
   ,Source(SOURCE_LOCAL)
   ,Angle(0)
{
    memset(TextureImages_, 0, sizeof(TextureImages_));

    try {
        cfg_.load();

        // 1. load workarea.
        bool    ok = true;
        {
            cfg_.set_section("WorkArea");
            WorkArea    tmp;

            std::string stime;
            std::string spos;
            ok =
                   cfg_.get_string("Change_Time", stime)
                && cfg_.get_double("Start_X",       tmp.Start_X)
                && cfg_.get_double("Start_Y",       tmp.Start_Y)
                && cfg_.get_double("End_X",         tmp.End_X)
                && cfg_.get_double("End_Y",         tmp.End_Y)
                && cfg_.get_double("Step_Forward",  tmp.Step_Forward)
                && cfg_.get_string("Pos_Sideways",  spos);
            ok = ok && my_time_of_string(stime, tmp.Change_Time);
            vector<string>  vpos;
            split(spos, ";", vpos);
            for (unsigned int i=0; i<vpos.size(); ++i) {
                const std::string&  s = vpos[i];
                tmp.Pos_Sideways.push_back(double_of(s));
            }

            ok = ok && tmp.Pos_Sideways.size()>1;

            if (ok) {
                const double    length = sqrt(sqr(tmp.Start_X-tmp.End_X) + sqr(tmp.Start_Y-tmp.End_Y));
                const int       nrows = round(length / tmp.Step_Forward);
                const int       ncols = static_cast<int>(tmp.Pos_Sideways.size())-1;
                ok = ok && nrows>0 && ncols>0;

                // if (still ok)
                if (ok) {
                    // Unit vector.
                    vector_dx_ = (tmp.End_X - tmp.Start_X) / length;
                    vector_dy_ = (tmp.End_Y - tmp.Start_Y) / length;
                    Angle = rad2deg(atan2(vector_dy_, vector_dx_));

                    // Load the stored data.
                    Marks = TNT::Array2D<WORKMARK>(nrows, ncols);
                    cfg_.set_section("WorkLog");
                    string  key;
                    string  row;
                    for (int i=0; i<nrows; ++i) {
                        for (int j=0; j<ncols; ++j) {
                            Marks[i][j] = WORKMARK_EMPTY;
                        }
                        ssprintf(key, "Row_%04d", i);
                        if (cfg_.get_string(key, row)) {
                            for (unsigned int j=0; j<row.size(); ++j) {
                                const char  c = row[j];
                                if (c>='0' && c<='3') {
                                    const unsigned int  n = c-'0';
                                    if (n>=0 && n<WORKMARK_SIZE) {
                                        Marks[i][j] = static_cast<WORKMARK>(n);
                                    }
                                }
                            }
                        }
                    }

                    // Mark it done.
                    MarkSources.resize(nrows);
                    for (int i=0; i<nrows; ++i) {
                        MarkSources[i] = SOURCE_LOCAL;
                    }

                    Area = tmp;
                    Source = SOURCE_LOCAL;
                }
            }
        }
    } catch (const std::exception&) {
        // pass.
    }
}

//=====================================================================================//
WorkAreaMarking::~WorkAreaMarking()
{
}

//=====================================================================================//
void
WorkAreaMarking::draw(
    FXApp*  app,
    FXColor backgroundColor
)
{
    if (Area.IsValid()) {
        const unsigned int  nrows = Marks.dim1();
        const unsigned int  ncols = Marks.dim2();

        // 1. Create coordinates, if needed.
        if (MarkCorners_.dim1()==0) {
            const WorkArea  wa = Area();
            const double    origin_x = ugl::get_origin_x();
            const double    origin_y = ugl::get_origin_y();

            MarkCorners_ = TNT::Array2D<ugl::Point3>(nrows+1, ncols+1);

            for (unsigned int i=0; i<=nrows; ++i) {
                const double    dx_i = vector_dx_ * i * wa.Step_Forward;
                const double    dy_i = vector_dy_ * i * wa.Step_Forward;
                for (unsigned int j=0; j<=ncols; ++j) {
                    const double    sideways = wa.Pos_Sideways[j];
                    const double    dx_j = -vector_dy_ * sideways;
                    const double    dy_j = vector_dx_ * sideways;
                    ugl::Point3&    pt = MarkCorners_[i][j];
                    pt.x = wa.Start_Y + dy_i + dy_j - origin_x;
                    pt.y = wa.Start_X + dx_i + dx_j - origin_y;
                    pt.z = -1.0;
                }
            }
        }

        // 2. Create textures, if needed.
        if (!Textures_[WORKMARK_DREDGED].IsOpen() || LastBackColor_!=backgroundColor) {
            reload_texture(
                Textures_[WORKMARK_DREDGED], resources::workmark_dredged, backgroundColor,
                TextureImages_[WORKMARK_DREDGED], app);
            reload_texture(
                Textures_[WORKMARK_OK], resources::workmark_ok, backgroundColor,
                TextureImages_[WORKMARK_OK], app);
            reload_texture(
                Textures_[WORKMARK_PROBLEM], resources::workmark_problem, backgroundColor,
                TextureImages_[WORKMARK_PROBLEM], app);
            LastBackColor_ = backgroundColor;
        }

        // 3. Draw :)
        glColor4f(1.0, 1.0, 1.0, 1.0);
        glEnable(GL_TEXTURE_2D);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        for (unsigned int i=0; i<nrows; ++i) {
            for (unsigned int j=0; j<ncols; ++j) {
                const WORKMARK  wm = Marks[i][j];
                if (wm!=WORKMARK_EMPTY) {
                    const ugl::Point3&  screen1 = MarkCorners_[i  ][j];
                    const ugl::Point3&  screen2 = MarkCorners_[i+1][j];
                    const ugl::Point3&  screen3 = MarkCorners_[i+1][j+1];
                    const ugl::Point3&  screen4 = MarkCorners_[i  ][j+1];
                    Textures_[wm].draw(
                        screen1, screen2, screen3, screen4,
                        0, 0, 1, 1
                    );
                }
            }
        }
        glDisable(GL_TEXTURE_2D);
    }
}

//=====================================================================================//
bool
WorkAreaMarking::SetWorkArea(
    const WorkArea&     wa)
{
    const double    length = sqrt(sqr(wa.Start_X-wa.End_X) + sqr(wa.Start_Y-wa.End_Y));
    const int       nrows = round(length / wa.Step_Forward);
    const int       ncols = static_cast<int>(wa.Pos_Sideways.size())-1;

    const bool      ok = nrows>0 && ncols>0;

    // if (still ok)
    if (ok) {
        // Unit vector.
        vector_dx_ = (wa.End_X - wa.Start_X) / length;
        vector_dy_ = (wa.End_Y - wa.Start_Y) / length;
        Angle = rad2deg(atan2(vector_dy_, vector_dx_));

        // Transfer present data.
        TNT::Array2D<WORKMARK>  new_marks(nrows, ncols);
        const int   nrows2 = mymin<int>(nrows, Marks.dim1());
        const int   ncols2 = mymin<int>(ncols, Marks.dim2());
        for (int i=0; i<nrows; ++i) {
            for (int j=0; j<ncols; ++j) {
                new_marks[i][j] = WORKMARK_EMPTY;
            }
        }
        for (int i=0; i<nrows2; ++i) {
            for (int j=0; j<ncols2; ++j) {
                new_marks[i][j] = Marks[i][j];
            }
        }
        Marks = new_marks;
        MarkCorners_ = TNT::Array2D<ugl::Point3>();
        Area = wa;
        if (MarkSources.size() != nrows) {
            MarkSources.resize(nrows);
            for (int i=0; i<nrows; ++i) {
                MarkSources[i] = SOURCE_LOCAL;
            }
        }
        Source = SOURCE_SERVER;

        Store_();
    }

    return ok;
}

//=====================================================================================//
bool
WorkAreaMarking::HandleRowsPacket(
    const PACKET_WORKLOG_ROWS*  Packet,
    const unsigned int          PacketSize
)
{
    const unsigned int    BITS_IN_MARK = 2;
    const unsigned int  nrows = Marks.dim1();
    const unsigned int  ncols = Marks.dim2();
    bool                any_updates = false;
    if (nrows>0 && ncols>0) {
        const unsigned int  nbytes = PacketSize - sizeof(PACKET_WORKLOG_ROWS) + 1;
        const unsigned int  newrows = nbytes * 8 / BITS_IN_MARK / ncols;
        unsigned int        total_bit_index = 0;
        for (unsigned int i=0; i<newrows; ++i) {
            const unsigned int  row_index = Packet->start_row + i;
            if (row_index < nrows) {
                for (unsigned int col_index=0; col_index<ncols; ++col_index) {
                    const unsigned int    byte_index = total_bit_index / 8;
                    const unsigned int    bit_shift = (8 - BITS_IN_MARK) - (total_bit_index & 0x07);
                    const unsigned int    b = Packet->marks[byte_index];
                    const WORKMARK      wm = static_cast<WORKMARK>((b >> bit_shift) & ((1<<BITS_IN_MARK)-1));

                    if (Marks[row_index][col_index] != wm) {
                        Marks[row_index][col_index] = wm;
                        any_updates = true;
                    }

                    total_bit_index += BITS_IN_MARK;
                }
                if (row_index < MarkSources.size()) {
                    MarkSources[row_index] = SOURCE_SERVER;
                }
            }
       }
    }
    if (any_updates) {
        Store_();
    }
    return any_updates;
}

//=====================================================================================//
void
WorkAreaMarking::Store_()
{
    if (Area.IsValid()) {
        // yes :P
        const WorkArea wa = Area();
        cfg_.set_section("WorkArea");
        cfg_.set_string("Change_Time", wa.Change_Time.ToString());
        cfg_.set_double("Start_X", wa.Start_X);
        cfg_.set_double("Start_Y", wa.Start_Y);
        cfg_.set_double("End_X", wa.End_X);
        cfg_.set_double("End_Y", wa.End_Y);
        cfg_.set_double("Step_Forward", wa.Step_Forward);
        // wa.Pos_Sideways
        {
            std::string s;
            std::string xbuf;
            for (unsigned int i=0; i<wa.Pos_Sideways.size(); ++i) {
                if (i>0) {
                    s += ";";
                }
                ssprintf(xbuf, "%4.2f", wa.Pos_Sideways[i]);
                s += xbuf;
            }
            cfg_.set_string("Pos_Sideways", s);
        }

        cfg_.set_section("WorkLog");
        const unsigned int  nrows = Marks.dim1();
        const unsigned int  ncols = Marks.dim2();
        string  rowbuf;
        string  keybuf;
        rowbuf.resize(ncols);
        for (unsigned int i=0; i<nrows; ++i) {
            for (unsigned int j=0; j<ncols; ++j) {
                rowbuf[j] = '0' + Marks[i][j];
            }
            ssprintf(keybuf, "Row_%04d", i);
            cfg_.set_string(keybuf, rowbuf);
        }

        try {
            cfg_.store();
        } catch (const std::exception&) {
            // pass.
            // FIXME: what to do?
        }
    }
}

//=====================================================================================//
bool
WorkAreaMarking::GetIndices(
    MatrixIndices&              indices,
    const utils::ugl::Point2&   pt
) const
{
    if (Area.IsValid()) {
        const WorkArea&         wa = Area();
        const double            odx = pt.x - wa.Start_X;
        const double            ody = pt.y - wa.Start_Y;
        const TransformMatrix2  trn(-deg2rad(Angle));
        const double            Pos_Forward  = trn.ty(ody, odx);
        const double            Pos_Sideways = trn.tx(ody, odx);
        const double            length = sqrt(sqr(wa.Start_X-wa.End_X) + sqr(wa.Start_Y-wa.End_Y));
        const unsigned int      n = wa.Pos_Sideways.size();
        if (length>0 && n>0
            && Pos_Forward>=0 && Pos_Forward<=length
            && Pos_Sideways>=wa.Pos_Sideways[0] && Pos_Sideways<=wa.Pos_Sideways[n-1]) {
            MatrixIndices   r;
            r.Row = static_cast<int>(Pos_Forward/wa.Step_Forward);
            bool    col_found = false;
            for (unsigned int i=0; i+1<n; ++i) {
                if (Pos_Sideways>=wa.Pos_Sideways[i] && Pos_Sideways<=wa.Pos_Sideways[i+1]) {
                    col_found = true;
                    r.Column = i;
                    break;
                }
            }
            if (col_found) {
                // yes.
                indices = r;
                return true;
            }
        }
    }
    return false;
}

//=====================================================================================//
bool
WorkAreaMarking::GetGLCorners(
    utils::ugl::Point2&     pt1,
    utils::ugl::Point2&     pt2,
    utils::ugl::Point2&     pt3,
    utils::ugl::Point2&     pt4,
    const MatrixIndices&    indices
) const
{
    const unsigned int  row = indices.Row;
    const unsigned int  col = indices.Column;
    const bool  ok = row+1<MarkCorners_.dim1() && col+1<MarkCorners_.dim2();
    if (ok) {
        const ugl::Point3&  screen1 = MarkCorners_[row  ][col];
        const ugl::Point3&  screen2 = MarkCorners_[row+1][col];
        const ugl::Point3&  screen3 = MarkCorners_[row+1][col+1];
        const ugl::Point3&  screen4 = MarkCorners_[row  ][col+1];
        pt1.x = screen1.x;
        pt1.y = screen1.y;
        pt2.x = screen2.x;
        pt2.y = screen2.y;
        pt3.x = screen3.x;
        pt3.y = screen3.y;
        pt4.x = screen4.x;
        pt4.y = screen4.y;
    }
    return ok;
}
