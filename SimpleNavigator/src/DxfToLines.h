#ifndef Dxf_To_Lines_h_
#define Dxf_To_Lines_h_

#include <vector>
#include <string>

typedef struct {
    double  x;      ///< X coordinate
    double  y;      ///< Y coordinate
    double  width;  ///< Line width
    int     colour; ///< Line colour, index to an array @see dxfColors array in dl_codes.h
    bool    line_start; ///< Start of a new line if true
    bool    polygon;    ///< If true then this is meant to be filled when drawing a shadow
} LINE_SEGMENT;

void DxfLoad(std::string& filename, std::vector<LINE_SEGMENT>& lines);

#endif
