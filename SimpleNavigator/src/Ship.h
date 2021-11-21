/**
vim: ts=4
vim: shiftwidth=4
*/
#ifndef Ship_h_
#define Ship_h_

#include <string>	// std::string

#include <fx.h>

#include <utils/ugl.h>
#include <utils/Config.h>
#include <utils/util.h>

#include "DigNetMessages.h"
#include "globals.h"

/** Ship (i.e. ModemBox/GpsViewer on the wild.

Note: only std::vector<Ship*> allowed.
*/
class Ship {
public:
	// No outside modifying.
	std::string				Name;		///< Ship name.
	bool					Has_Valid_Name;	///< Ship has real name, GPS offsets and other needed data
    utils::Option<ShipPosition> Pos; ///< Position from the last SetPosition.
	double					EnlargeFactor;	///< Number of times ship is drawn bigger than normal.

	/// Constructor.
	Ship();

	/// Draw our ship (OpenGL context required).
	void
	Draw(
        const bool  show_shadow
    );

	/// Update ship position.
	/// The value is cached.
	void
	SetPosition(
		const double	x,
		const double	y,
		const double	heading);

	/// Set enlargement factor.
	void
	SetEnlargeFactor(
		const double	NewEnlargeFactor
		);

private:
	//utils::ugl::VPointArray	Drawing_;
	bool					Drawing_Dirty_;	///< Update coordinates before drawing?
};

#endif /* Ship_h_ */