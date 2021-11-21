/*
vim: ts=4
vim: shiftwidth=4
*/

#ifndef globals_h_
#define globals_h_

#include <utils/util.h> // utils::my_time.

typedef struct {
    /// Valid time.
    utils::my_time  Time;
    /// X, meters. (lambert-est: X goes up).
    double          X;
    /// Y, meters. (lambert-est: Y goes right).
    double          Y;
    /// Z, meters.
    double          Z;
} GpsPosition;

class ShipPosition {
public:
    ShipPosition(): X(0.0), Y(0.0), Z(0.0), Heading(0.0){}
    /// Valid time.
    utils::my_time  Time;
    /// X, meters. (lambert-est: X goes up).
    double          X;
    /// Y, meters. (lambert-est: Y goes right).
    double          Y;
    /// Z, meters.
    double          Z;
    /// Heading, degrees, clockwise from north (X-axis).
    double          Heading;
};

#endif /* globals_h_ */
