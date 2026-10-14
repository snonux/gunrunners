#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 39, The Gallery: the studio crew, drawn like the other styled
// enemies (enemy_art.cpp calls this for keys it has no routine for). The
// box is (32, 32) .. (32 + w, 32 + h) in px, facing right. Returns false if
// `key` is not one of these.
//   floormgr        3 x 5: the Floor Manager, in studio blacks and cargo
//                   trousers, a headset and its boom mic, a walkie on his
//                   belt, a clipboard. variant 0 walking (frame 0/1 the
//                   stride), 1 pointing at the runner, 2-4 counting down
//                   with his hand up (three, two, one fingers), 5 the cue
//                   (his arm swept down at the runner: "you're on")
//   wrangler        4 x 5: the Cable Wrangler, a stocky crew hand in a
//                   hi-vis vest over a hoodie, cap on backwards, work
//                   gloves, a coil of cable over his shoulder; his back
//                   hand holds the cable he drags (the cable is drawn by
//                   the level from (34, 104) of the 128 x 160 design box).
//                   variant 0 walking (frame 0/1), 1 the whip (leaning
//                   back, both hands yanking), 2 the cable dropped (hands
//                   up, startled)
//   boomop          4 x 5: the Boom Op, big headphones over a beanie, a
//                   sound bag with its meters on his chest, both hands up
//                   on the boom's butt (the pole and the mic are drawn by
//                   the level from the hands at (70, 26) of the 128 x 160
//                   design box). variant 0 holding, 1 braced for a pass
//                   (knees bent, leaning back), 2 sweeping (leaning in)
//   duck_headphones variant 0 behind the duck item (the far ear cup and
//                   the band's far side), 1 over it (the near cup and the
//                   band, the coiled lead hanging down to the desk)
bool drawGallery39Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

} // namespace gr
