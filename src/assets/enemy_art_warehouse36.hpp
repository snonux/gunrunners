#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 36, Prop Warehouse: the prop store's cast, drawn like the other
// styled enemies (enemy_art.cpp calls this for keys it has no routine for).
// The box is (32, 32) .. (32 + w, 32 + h) in px, facing right. Returns false
// if `key` is not one of these.
//   forklift_bot    6 x 4: a squat yellow robot forklift, a black and
//                   yellow chevron on its counterweight, a cab with a
//                   screen face (two eyes), an amber beacon on its roof, a
//                   tall mast at its nose (the forks and their carriage are
//                   drawn by the level, they move). variant 0 patrolling
//                   (frame 0/1 the wheels' turn), 1 the tell (squatting on
//                   its springs, the eyes narrowed red, exhaust smoking), 2
//                   charging (leaning back, eyes wide), 3 carrying (pleased,
//                   eyes curved), 4 dangling on a magnet (wheels hanging)
//   stagehand       3 x 5: a stagehand in black from head to toe (black
//                   cap, black blacks, black sneakers), only the white
//                   gloves and the white toe caps of the shoes standing out,
//                   a headset's tiny red light. variant 0 walking with the
//                   flat held up beside it (frame 0/1 the stride), 1
//                   crouched behind the flat, 2 the tell (leaning out, the
//                   gloves flashing up), 3 the throw (an arm out)
//   prop_mimic      4 x 4: a wooden prop chest with iron corners and a
//                   stencilled PROPS - FRAGILE, only its eyes in the gap of
//                   its lid giving it away. variant 0 shut, 1 a breath (the
//                   lid lifted a cell, eyes glinting in the dark), 2 the tell
//                   (the lid thrown open on rows of teeth, a tongue), 3 the
//                   bite (snapped half shut); frame 1 the carrier (a green
//                   tongue and a sickly green glow inside)
//   practice_dummy  3 x 5: the Throw Everything bonus's practice dummy, a
//                   sacking body stuffed with straw on two stubby wooden
//                   legs, a red and white target on its chest, stitched
//                   cross eyes. frame 0/1 the waddle
//   duck_hard_hat_vest  variant 1 over the duck item: a yellow hard hat and
//                   a hi-vis orange vest with silver stripes
bool drawWarehouse36Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

} // namespace gr
