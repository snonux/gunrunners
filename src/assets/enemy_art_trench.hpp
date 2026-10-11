#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 30, Fiber Trench: the sea floor's cast, drawn like the other styled
// enemies (enemy_art.cpp calls this for keys it has no routine for). The box
// is (32, 32) .. (32 + w, 32 + h) in px, facing right. Returns false if `key`
// is not one of these.
//   anglerfish        6 x 4: a fat dark fish with an underbite, its rod
//                     arching up over its head (the lure is lit live).
//                     variant 0 waiting, 1 the tell (eyes glinting, jaw
//                     dropping), 2 the lunge (jaws wide), 3 the old one in
//                     the cave (pale, barnacled, whiskered, asleep);
//                     frame 0/1 its tail and fins swaying
//   box_jelly         4 x 4: a glassy box-shaped bell with four bundles of
//                     tentacles. variant 0 violet, 1 the carrier (green);
//                     frame 0 relaxed, 1 contracting, 2 contracted (the
//                     tell), 3 relaxed, tentacles swaying the other way
//   cable_eel_head    3 x 3: the eel's head, black as cable jacket with a
//                     glowing line down its side. variant 0 jaws shut, 1
//                     open with a spark between them
//   cable_eel_body    2 x 2: one round section of its body (drawn in a
//                     chain from the box's mouth to the head)
//   junction_box      2 x 2: a steel junction box on the rock, its mouth on
//                     the right. variant 0 an eel's, 1 the quiet one (a
//                     speaker grille, no mouth); frame 1 lit up (humming)
//   duck_diving_mask  variant 1 over the duck item: a diving mask and a
//                     snorkel
bool drawTrenchArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

// Where the Anglerfish's lure hangs, from the bottom centre of its sprite
// (facing right; mirror x for left), in px.
constexpr float kAnglerLureX = 108.0f;
constexpr float kAnglerLureY = -138.0f;
// Its eye, the same way.
constexpr float kAnglerEyeX = 54.0f;
constexpr float kAnglerEyeY = -88.0f;

} // namespace gr
