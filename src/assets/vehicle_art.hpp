#pragma once

#include <cairo.h>

#include <string>

namespace gr
{

// Draws the vehicle or deep-water creature for `key` (veh_tank, piranha,
// ...) into a styled sprite's canvas (see enemy_art.hpp). False if the key
// is not one of them.
bool drawVehicleArt(cairo_t* cr, const std::string& key, double w, double h, int variant, int frame);

} // namespace gr
