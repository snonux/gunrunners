#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 31, Storm Relay: the cliff's cast, drawn like the other styled
// enemies (enemy_art.cpp calls this for keys it has no routine for). The box
// is (32, 32) .. (32 + w, 32 + h) in px, facing right. Returns false if `key`
// is not one of these.
//   storm_gull       4 x 3: a big herring gull, grey back, black wingtips.
//                    variant 0 wheeling (frame 0-3: wings up, half, down,
//                    half), 1 the cry before a dive (wings folded up, head
//                    thrown back, beak wide; frame 0/1 a shudder), 2 diving
//                    (wings swept back tight, drawn level: the level tilts
//                    it along the dive)
//   fry_gull         3 x 3: the beach gull and its french fry. variant 0 on
//                    its post (frame 0 fry in its beak, 1 tossing it back, 2
//                    the gulp, 3 pecking a new one from the carton at its
//                    feet), 1 flapping off with the fry (frame 0/1)
//   kite_drone       4 x 4: a diamond kite of storm-yellow and navy sailcloth
//                    on steel spars, a ducted rotor at each wing tip, a
//                    camera eye in the middle and a ribbon tail. variant 0
//                    flying (amber eye), 1 the tell (rotors screaming, eye
//                    red), 2 struck (scorched, eye white, sparks); frame 0/1
//                    the rotors and the tail
//   foghorn          6 x 6: a brass horn on a wooden stand, a leather bellows
//                    behind it. variant 0 idle, 1 the bellows swelling, 2
//                    full (the inhale), 3 the blast (bellows squeezed, the
//                    bell shaking); +4 the carrier (green-crusted, dripping);
//                    frame 0/1 a tremble on the tell and the blast
//   duck_sou_wester  variant 1 over the duck item: a yellow oilskin
//                    sou'wester, its brim long at the back, and its strap
bool drawStormArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

// Where the Foghorn's bell mouth is, from the bottom centre of its sprite
// (facing right; mirror x for left), in px for the 6 x 6 cell box.
constexpr float kFoghornMouthX = 88.0f;
constexpr float kFoghornMouthY = -100.0f;

} // namespace gr
