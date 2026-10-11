#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 32, Dish Array: the desert's cast, drawn like the other styled
// enemies (enemy_art.cpp calls this for keys it has no routine for). The box
// is (32, 32) .. (32 + w, 32 + h) in px, facing right. Returns false if `key`
// is not one of these.
//   sand_shark       6 x 4: a shark the colour of the dunes, mottled, a pale
//                    belly, its back plated like a lizard's. variant 0
//                    breaching (arched up out of the sand, jaws shut), 1
//                    biting (jaws wide, rows of teeth), 2 only its fin
//                    cutting the sand (at the box's foot), 3 the tell (the
//                    fin stopped, the sand mounded up round it); frame 0/1
//                    a flick of the tail (0-1) or a tremble (2-3)
//   vulture_drone    6 x 3: a rotor drone built like a vulture: a gunmetal
//                    body hunched behind a hooded camera head with a red
//                    eye, two swept arms with ragged sheet-metal "feathers"
//                    and a rotor at each tip, talons under it. variant 0
//                    carrying scrap, 1 talons empty; +2 the carrier (rusty
//                    green, its scrap crusted green); frame 0/1 the rotors
//   scrap            2 x 2: a lump of junk metal (a bent plate, a gear, a
//                    pipe end); variant 0 rusty, 1 green-crusted (the
//                    carrier's); frame 0-3 turning as it falls
//   scorpion_mech    8 x 6: a walker built like a scorpion: an armoured
//                    carapace in sand-yellow site paint with hazard stripes,
//                    three legs a side, two pincers, a segmented tail arched
//                    over its back to a laser stinger above its middle (the
//                    top middle of the box). variant 0 walking, 1 aiming
//                    (the stinger lit red), 2 firing (white-hot); frame 0/1
//                    the step
//   duck_sunglasses  variant 1 over the duck item: black aviator sunglasses
//                    with gold rims and a glint
bool drawDishArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

} // namespace gr
