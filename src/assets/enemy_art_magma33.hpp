#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 33, Magma Mainframe: the volcano's cast, drawn like the other styled
// enemies (enemy_art.cpp calls this for keys it has no routine for). The box
// is (32, 32) .. (32 + w, 32 + h) in px, facing right. Returns false if `key`
// is not one of these.
//   firewall        4 x 6: a literal firewall: a low wall of molten bricks
//                   in an iron frame, a padlock badge on it, a sheet of
//                   flame roaring off its top with two white-hot eyes in
//                   it. variant 0 burning full height, 1 the tell (the
//                   flames flicker: sliced and shifted like a bad signal,
//                   white-hot), 2 dipped (only a bed of low flames and
//                   embers, 2 cells high); frame 0-3 the flames' dance
//   lava_daemon     3 x 4: an imp of cooling lava, black crust plates over a
//                   molten core showing through the cracks, stubby horns,
//                   white-hot eyes and mouth. variant 0 perched, 1 the
//                   tell (its throwing arm wound back over its head,
//                   glowing, a fireball forming in its fist), 2 throwing
//                   (arm flung forward, hand open), 3 leaping (tucked), 4
//                   sitting roasting a marshmallow on a stick; frame 0/1
//                   its core's glow breathing
//   fan_bot         4 x 6: a wall-mounted blower: a bolted bracket, a motor
//                   box with two eye lamps and a warning light, a short
//                   cylindrical cowl and its rotor seen three-quarter on.
//                   variant 0 idle (blades still), 1 the tell (spinning up:
//                   blades smeared, the warning light red), 2 blowing (a
//                   blurred disk, louvres open); frame 0-3 the rotor's angle
//                   (four blades: frame 4 is frame 0)
//   duck_heat_suit  variant 1 over the duck item: an aluminized fire
//                   proximity hood with a gold visor, its cape on the
//                   shoulders
bool drawMagma33Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

} // namespace gr
