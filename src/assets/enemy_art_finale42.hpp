#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 42, The Live Finale: the network's last-ditch cast, drawn like the
// other styled enemies (enemy_art.cpp calls this for keys it has no
// routine for). The box is (32, 32) .. (32 + w, 32 + h) in px, facing
// right. Returns false if `key` is not one of these.
//   censorbar   4 x 2: a Censor Bar, a gloss-black roller cassette with
//               CENSORED stencilled on it, flown by two rotors, a red tally
//               at its nose and the bar's black lip hanging from its slot.
//               frame 0/1 the rotors
//   adbreak     6 x 8: an Ad Break, a rolling billboard on a dolly with two
//               horn speakers on top and a SAMPLES hatch under the screen.
//               frame 0-3 the ad on its screen (the Halcyon Arms shield, the
//               rifle NEW!, BUY NOW!, 42% OFF); variant 1 the jingle (the
//               horns flared, notes in the air, the hatch open)
//   sample_bot  2 x 3: a free sample, a blue gift box with a gold ribbon
//               walking on two stubby legs, big eyes, a FREE tag. frame 0/1
//               the walk
//   double      3 x 5: a Stunt Double, a runner's double in a padded black
//               stunt suit and a black mask, STUNT on the chest. variant & 3:
//               0 Dash's (a spiky yellow wig, red stripes), 1 Rocco's (a red
//               bandana, green), 2 Nova's (a pink ponytail, purple); + 4 the
//               muzzle-flash pose (the arm out, the shot going). frame 0/1
//               the run
//   duck_host   variant 1: the duck's sequined host jacket (gold sequins,
//               black satin lapels, a bow tie) and a chrome grin on its
//               beak; frame 0/1 the sequins catching the light. 0 draws
//               nothing
bool drawFinale42Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

} // namespace gr
