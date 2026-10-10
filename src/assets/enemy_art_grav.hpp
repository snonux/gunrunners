#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 19, Gravity Lab: Flip Walkers, Gravity Probes and Test Subjects,
// drawn like the other styled enemies (enemy_art.cpp calls this for keys it
// has no routine for). The box is (32, 32) .. (32 + w, 32 + h) in px, facing
// right, standing on its usual floor (world_draw.cpp turns it over when it
// walks a ceiling). Returns false if `key` is not one of these.
//   flip_walker   variant 0 walking, 1 turning to face you, 2 flailing
//   gravity_probe variant 0 hovering, 1 the core glowing before a shot
//   test_subject  variant % 4: 0 walking, 1 crouching, 2 flailing;
//                 + 4 a carrier (green-tinged coat); + 8 * n the bib's number
bool drawGravArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

} // namespace gr
