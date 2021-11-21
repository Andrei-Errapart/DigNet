#ifndef Dxf_To_Lines_h_
#define Dxf_To_Lines_h_

#include <vector>
#include <string>

/** Line segment struct */
typedef struct {
    double  x;          ///< X coordinate
    double  y;          ///< Y coordinate
    double  width;      ///< Line width
    int     colour;     ///< Line colour, index to an array @see dxfColors array in dl_codes.h
    bool    line_start; ///< Start of a new line if true
    bool    polygon;    ///< If true then this line sequence forms a polygon and is meant to be filled when drawing a shadow
} LINE_SEGMENT;

/** Loads a .dxf from given file name into a vector. 
    Currently only supports single lines, polygons and circles with their color and width data
    @param filename name of file to load
    @param lines vector to append to. Is not cleared
    @return true on successful file open&read
*/
bool DxfLoad(
        std::string& filename, 
        std::vector<LINE_SEGMENT>& lines
);

#endif
