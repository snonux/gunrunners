#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 40, The Fly Tower: the fly crew, drawn like the other styled
// enemies (enemy_art.cpp calls this for keys it has no routine for). The
// box is (32, 32) .. (32 + w, 32 + h) in px, facing right. Returns false if
// `key` is not one of these.
//   rigger          3 x 5: a Rigger in blacks and a climbing harness, a
//                   hard hat with a red headlamp, a coil of rope over his
//                   shoulder, a line launcher with a laser sight. variant 0
//                   watching (the launcher low), 1 aiming down its sight
//                   (the muzzle at (94, 34) of the 96 x 160 design box, the
//                   laser's lens lit); frame 0/1 a blink
//   sandbag         3 x 5: a Sandbag Drop, a fly-crew hand in a beanie and a
//                   black hoodie, gloved hands on a hemp line running up
//                   out of the box (the level draws the line on up from
//                   (66, 0) of the design box). variant 0 holding the line,
//                   1 the knife up to it (the tell), 2 the cut (the knife
//                   swept down, the line's end flying)
//   duck_headset    variant 0 behind the duck item (the band's far side),
//                   1 over it (a black stagehand T-shirt with CREW on it,
//                   the near ear cup and the talkback mic at the beak)
bool drawFlyTower40Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

} // namespace gr
