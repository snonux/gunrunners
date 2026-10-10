#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 20, Reactor Core: Conduit Sparks, Shield Drones and Isotope Imps,
// drawn like the other styled enemies (enemy_art.cpp calls this for keys it
// has no routine for). The box is (32, 32) .. (32 + w, 32 + h) in px, facing
// right. Returns false if `key` is not one of these.
//   conduit_spark  variant 0 riding its wire (frame: which crackle)
//   shield_drone   variant 0 hovering, 1 its emitter flaring (a blocked hit)
//   isotope_imp    variant % 4: 0 scuttling, 1 glowing before a lunge,
//                  2 lunging, 3 saluting; + 4 the yellow hard hat;
//                  + 8 * heat (0..4, a pulse lived through each: hotter)
bool drawReactorArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

} // namespace gr
