#ifndef WorkAreaMarking_h_
#define WorkAreaMarking_h_

#include <fx.h>

#include "DigNetMessages.h"

#include <tnt_array2d.h>    // TNT::Array2D.
#include <utils/util.h>     // utils::Option
#include <utils/Config.h>   // utils::FileConfig.
#include <utils/ugl.h>      // 


/** Row/column indices of a matrix. */
typedef struct {
    /** Row index. */
    unsigned int    Row;
    /** Column index. */
    unsigned int    Column;
} MatrixIndices;

/** Helps to keep work area log and mark sections, etc.

Note: only one is allowed.
*/
class WorkAreaMarking {
private:
    utils::FileConfig  cfg_;
public:
    /** Source of the worklog. */
    typedef enum {
        /** Central server. */
        SOURCE_SERVER   = 1,
        /** Local file. */
        SOURCE_LOCAL    = 2
    } SOURCE;

    /** Empty, do not use. */
    WorkAreaMarking();

    /** Load contents from the initialization file. Source is set to SOURCE_LOCAL. */
    WorkAreaMarking(
        const std::string&  initialization_filename
    );

    /** Destructor. */
    ~WorkAreaMarking();

    /** Working area. If this is not present, this structure is not valid. */
    utils::Option<WorkArea> Area;

    /** Angle of the workarea, degrees clockwise from north. */
    double                  Angle;

    /** Marks. */
    TNT::Array2D<WORKMARK>  Marks;

    /** Mark sources. Default is \c SOURCE_LOCAL.
    This field is updated by \c HandleRowsPacket.
    */
    std::vector<SOURCE>     MarkSources;

    /** Source of the worklog. */
    SOURCE                  Source;

    /** Draw the workAreaLog. */
    void
    draw(
        FXApp*  app,
        FXColor backgroundColor
    );

    /** Set the new working area. An initialization file is written and Source is set to SOURCE_SERVER.
    Fails if the area doesn't define any rows or columns.
    */
    bool
    SetWorkArea(
        const WorkArea&     wa
    );

    /** Handle rows sent by the server.
    Returns true if any of the 
    */
    bool
    HandleRowsPacket(
        const PACKET_WORKLOG_ROWS*  Packet,
        const unsigned int          PacketSize
    );

    /** Get row/column indices for the given point (geodetic coordinate system). */
    bool
    GetIndices(
        MatrixIndices&              indices,
        const utils::ugl::Point2&   pt
    ) const;

    /** Get corner coordinates (OpenGL coordinate system) for square given by row/col. */
    bool
    GetGLCorners(
        utils::ugl::Point2&         pt1,
        utils::ugl::Point2&         pt2,
        utils::ugl::Point2&         pt3,
        utils::ugl::Point2&         pt4,
        const MatrixIndices&        indices
    ) const;
private:
    /** Store configuration file with the fresh state. */
    void
    Store_();

    /** Unit vector on the axis direction. Valid only if Area is valid.
    */
    double                              vector_dx_;
    double                              vector_dy_;

    /** Coordinates of the corners.
    These are computed in draw() in order to take into account ugl::offset_x/y.
    */
    TNT::Array2D<utils::ugl::Point3>    MarkCorners_;
    /** MarkLog textures. Textures_[0] is special case: it is not used. */
    utils::ugl::Texture                 Textures_[WORKMARK_SIZE];
    /** Cached images. */
    FXBMPImage*                         TextureImages_[WORKMARK_SIZE];
    /** Last background color. */
    FXColor                             LastBackColor_;
}; // class WorkAreaMarking.

#endif /* WorkAreaMarking_h_ */
