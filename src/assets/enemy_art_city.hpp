#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 26, Kaiju Tonight: the model city's toy army and its monster, drawn
// like the other styled enemies (enemy_art.cpp calls this for keys it has
// no routine for). The box is (32, 32) .. (32 + w, 32 + h) in px, facing
// right. Returns false if `key` is not one of these.
//   model_tank   a tin toy tank, olive drab with a white star. variant 0
//                patrolling, 1 the tell (the turret lifted on its ring, the
//                barrel up, a flash at the muzzle); 2 and 3 the same seen
//                from straight above (the Top-Down Rampage), the barrel
//                along +x; frame 0/1 the tracks rolling
//   toy_jet      a tin toy jet fighter hung on its wire. variant 0 flying
//                (frame 0/1 the exhaust flickering), 1-4 spinning out of
//                its formation, a quarter turn more each, smoking
//   maser_truck  a flatbed truck with a parabolic maser dish. variant 0
//                patrolling (the emitter idling), 1 charging (the emitter
//                and its coils lit), 2 powerless (its sector blacked out:
//                dark glass, no glow); frame 0/1 the wheels turning
//   gorgazilla   a stuntman in a rubber lizard suit, 4 x 6 cells. variant
//                0-3 the idle sway, 4 the roar (rearing back, jaws open), 5
//                the tail coiling, 6 the tail swiping, 7/8 staggering
//                (dizzy), 9-13 sitting down while the zipper on his back
//                opens (13: the man's head out), 14 the empty suit slumped
//   tired_man    the stuntman out of the suit, in his sweaty undershirt with
//                a towel round his neck: variant 0 walking (frame 0-3), 1
//                waving
//   duck_suit    the rubber duck item in a rubber monster suit: variant 0
//                behind the duck (the tail), 1 over it (the suit, the hood)
bool drawCityArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

// Gorgazilla in design px (the 4 x 6 cell box is 128 x 192): his feet's
// middle at (cx, feetY), facing right, `s` times as big. pose as the
// gorgazilla variants above; frame varies the sway. The cutscenes use it
// (pose 15: waving one hand, the idle stance).
void paintGorgazilla(cairo_t* cr, double cx, double feetY, double s, int pose, int frame);

} // namespace gr
