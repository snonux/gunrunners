#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 16, Cryo Labs: Puck Drones, Sleeper Pods and their mutants, Lab
// Arms, drawn like the other styled enemies (enemy_art.cpp calls this for
// keys it has no routine for). The box is (32, 32) .. (32 + w, 32 + h) in
// px, facing right. Returns false if `key` is not one of these.
bool drawCryoArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

} // namespace gr
