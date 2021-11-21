#include <vector>
#include <string>
#include <utils.h>
#include <utils/math.h> // deg2rad
#include <dl_dxf.h>

#include <dl_creationadapter.h>

#include "DxfToLines.h"

using namespace std;

class DxfToLines :
    public DL_CreationAdapter
{

    std::vector<LINE_SEGMENT>&  lines_;
    int                         num_polylines_left_;
    bool                        new_polyline_;

    LINE_SEGMENT                last_line_;
    int                         last_color_;
    double                      last_width_;

public:
    DxfToLines(std::vector<LINE_SEGMENT>& lines);
    ~DxfToLines(void);
    void addLine(const DL_LineData& data);
    void addPolyline(const DL_PolylineData &data);
    void addVertex(const DL_VertexData &data);
    void endSequence();
    void addCircle(const DL_CircleData &data);

    const std::vector<LINE_SEGMENT>& getLines() const {return lines_;}
};


DxfToLines::DxfToLines(std::vector<LINE_SEGMENT>& lines): lines_(lines)
{
}

DxfToLines::~DxfToLines(void)
{
}

void 
DxfToLines::addLine(const DL_LineData& data)
{
    // X and Y are swapped in DXF
    DL_Attributes attr = getAttributes();
    if (lines_.size() == 0 || (last_line_.y != data.x1 || last_line_.x != data.y1) || (last_line_.colour != attr.getColor()) || (last_line_.width != attr.getWidth())){
        TRACE_PRINT("info", ("New line"));
        last_line_.line_start=true;
        last_line_.polygon = false;
        last_line_.y = data.x1;
        last_line_.y = data.x1;
        last_line_.x = data.y1;
        last_line_.colour = attr.getColor();
        last_line_.width = attr.getWidth();
        // Default color is black
        if (last_line_.colour  == 256){
            last_line_.colour = 0;
        }
        // Default width is 3
        if (last_line_.width == -1){
            last_line_.width = 3;
        }
        lines_.push_back(last_line_);
    }
    last_line_.line_start=false;
    last_line_.polygon = false;
    last_line_.y = data.x1;
    last_line_.y = data.x2;
    last_line_.x = data.y2;
    last_line_.colour = attr.getColor();
    last_line_.width = attr.getWidth();
    if (last_line_.colour  == 256){
        last_line_.colour = 0;
    }
    // Default width is 3
    if (last_line_.width == -1){
        last_line_.width = 3;
    }
    TRACE_PRINT("info", ("Line form %1.3f,%1.3f to %1.3f,%1.3f", data.x1, data.y1, data.x2, data.y2));
    lines_.push_back(last_line_);
}

void 
DxfToLines::addPolyline(const DL_PolylineData &data)
{
    TRACE_PRINT("info", ("AddPolyline(): %d %d %d %d", data.number, data.m, data.n, data.flags));
    num_polylines_left_ = data.number;
    new_polyline_ = true;
}

void 
DxfToLines::addVertex(const DL_VertexData &data)
{
    TRACE_PRINT("info", ("AddPolyline(): %3.1f %3.1f %3.1f %3.1f", data.x, data.y, data.z, data.bulge));
    if (new_polyline_){
        last_line_.line_start=true;
    } else {
        last_line_.line_start=false;
    }
    new_polyline_ = false;
    last_line_.polygon = true;
    // X and Y are swapped in DXF
    last_line_.y = data.x;
    last_line_.x = data.y;
    DL_Attributes attr = getAttributes();
    last_line_.colour = attr.getColor();
    last_line_.width = attr.getWidth();
    // Default color is black
    if (last_line_.colour  == 256){
        last_line_.colour = 0;
    }
    // Default width is 3
    if (last_line_.width == -1){
        last_line_.width = 3;
    }
    lines_.push_back(last_line_);
}

void
DxfToLines::endSequence(){
    TRACE_PRINT("info", ("EndSequence()"));
}
void 
DxfToLines::addCircle(
    const DL_CircleData &data)
{
    const int lines = 10;
    DL_PolylineData pol(lines, 0, 0, 0);
    addPolyline(pol);

    for (int i=0; i<=lines; ++i) {
        DL_VertexData vert;
        const double    dx = data.radius * cos(utils::deg2rad(i*(360.0/lines)));
        const double    dy = data.radius * sin(utils::deg2rad(i*(360.0/lines)));
        vert.x = data.cx+dx;
        vert.y = data.cy+dy;
        vert.z = data.cz;
        addVertex(vert);
    }
}


bool 
DxfLoad(
    std::string& filename, 
    std::vector<LINE_SEGMENT>& lines)
{
    DxfToLines* dtl = new DxfToLines(lines);
    DL_Dxf* dxf= new  DL_Dxf();
    const bool result = dxf->in(filename, dtl);
    delete dxf;
    delete dtl;
    return result;
}
