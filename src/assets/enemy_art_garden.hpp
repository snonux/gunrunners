#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 25, Tiny Kingdom: the Soldier Ant, the Jumping Spider and the
// Bombardier Beetle, drawn like the other styled enemies (enemy_art.cpp
// calls this for keys it has no routine for). The box is (32, 32) ..
// (32 + w, 32 + h) in px, facing right. Returns false if `key` is not one
// of these.
//   soldier_ant        variant 0 marching, 1 the tell (the column halts,
//                      antennae flared up, alarm marks), 2 angry (mandibles
//                      open, eyes red); +3 the sick carrier column (green,
//                      sickly, blotched, drooling); frame 0-3 the march
//                      (a tripod gait)
//   jumping_spider     variant 0 stalking, 1 crouched to leap (body down,
//                      legs bunched, the big eyes glinting), 2 leaping (legs
//                      stretched back, front legs reaching); frame 0/1 the
//                      palps twitching and a step
//   bombardier_beetle  variant 0 walking, 1-3 the tell (the abdomen tip
//                      glowing and swelling more each step), 4 just fired
//                      (the rear tipped up, the tip smoking); frame 0/1 a step
//   duck_pith          a pith helmet for the rubber duck item (2 x 2 cells)
bool drawGardenArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

} // namespace gr
