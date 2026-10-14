#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 41, Sweeps Week: the studio's cast, drawn like the other styled
// enemies (enemy_art.cpp calls this for keys it has no routine for). The
// box is (32, 32) .. (32 + w, 32 + h) in px, facing right. Returns false if
// `key` is not one of these. BIG HEADS: +2 (the comic) or +4 (the plant)
// on the variant draws the same pose with a giant head, its top up in the
// texture's margin.
//   comic           3 x 5: the Warm-up Comic in a yellow-checked suit, a red
//                   bow tie and a pompadour, a microphone at his grin.
//                   variant 0 working the room (frame: the mouth), 1 the
//                   wind-up (a rubber chicken up behind him)
//   audience_plant  3 x 5: an Audience Plant in a trench coat, a fedora,
//                   sunglasses and a stuck-on moustache, an AUDIENCE pass
//                   on a lanyard. variant 0 sitting, arms folded (the only
//                   one not clapping), 1 getting up, 2 standing, a tomato
//                   up to throw; frame 0/1 a twitch of the moustache
//   cuedrone        4 x 3: a Cue Card Drone, a white quadcopter with a
//                   camera dome and a red tally, two arms reaching down
//                   for its card (the level hangs the card). variant 0
//                   level, 1 nose down in a swoop; frame 0/1 the rotors
//   crowd           3 x 5: an audience member on the tiers. variant =
//                   look * 16 + palette * 2 + side: look 0 a fan (palette
//                   0..3 the clothes), 1 a Chrome Cop with a box of
//                   popcorn, 2 a Bouncer with a foam finger, 3 a Mannequin
//                   (never claps), 4 Black Halo's pilot in a flight jacket;
//                   side 1 the head turned right (mirror it to look left).
//                   frame 0/1 the clap
//   duck_bowtie     variant 1: the duck's sequined bow tie (frame 0/1 the
//                   sequins catching the light); 0 draws nothing
bool drawSweeps41Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

} // namespace gr
