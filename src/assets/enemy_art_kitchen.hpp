#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 24, Too Many Cooks: the Meatball, the Cleaver Bot and the Egg
// Timer, drawn like the other styled enemies (enemy_art.cpp calls this for
// keys it has no routine for). The box is (32, 32) .. (32 + w, 32 + h) in
// px, facing right. Returns false if `key` is not one of these.
//   meatball     variant 0/1/2 small, big, giant; +3 the moldy carrier
//                (green-grey fur, flies round it); frame 0-3 the roll (the
//                crust turns), the sauce and the eyes stay upright
//   cleaver_bot  variant 0 at rest (the cleaver at its side), 1 the tell
//                (arm up, the cleaver over its head, the eye red), 2 the
//                follow-through (arm out, the hand empty); frame 0/1 the
//                steam from its vent
//   egg_timer    variant 0-11 the dial (0 run out .. 11 full), +12 ringing
//                (the bell shaking, ring lines), 24 asleep (eyes shut);
//                frame 0/1 the feet's toddle (and the shake)
//   cleaver      a thrown cleaver lying along +x, the blade at the tip:
//                variant 0 flying (a glint), frame 0/1 the glint
//   duck_chef    a chef's hat for the rubber duck item (2 x 2 cells)
bool drawKitchenArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

// Pieces the level (world_kitchen_draw.cpp) and the cutscenes (clips.cpp)
// share.

// A chef's toque: a pleated band `w` px wide with its bottom edge centred
// at (cx, bandY), the puffed crown `h` px tall above it; `tilt` radians.
void paintChefHat(cairo_t* cr, double cx, double bandY, double w, double h, double tilt);

// The chef's giant hand in his white sleeve, `s` px to a unit (the hand is
// about 120 units across), the wrist at (0, 0). pose 0: palm up and flat,
// the fingers pointing left (-x), the top of the palm at y = -6, the sleeve
// going up (-y) `sleeve` units; pose 1: a fist with the thumb up, the
// knuckles to the left, the sleeve going right (+x).
void paintChefHand(cairo_t* cr, double s, int pose, double sleeve);

// A chef's dinner bell, brass, hung from its bracket: the bracket at
// (cx, top), the bell `s` px tall below it. swing (radians) and ring 0..1
// (the shine flaring).
void paintChefBell(cairo_t* cr, double cx, double top, double s, double swing, double ring);

// The golden whisk, its handle's end at (0, 0) along -y (standing up),
// `len` px tall.
void paintGoldenWhisk(cairo_t* cr, double len);

// A meatball, round, at (cx, cy), radius `rad`: the browned crust turned
// by `roll` radians, flecks of parsley, a glossy cap of tomato sauce on
// top. moldy: furry green-grey instead.
void paintMeatball(cairo_t* cr, double cx, double cy, double rad, double roll, bool moldy, bool sauce);

// A cleaver lying along +x from its handle's end at (0, 0): `len` px long,
// the blade's back along y = -h/2.
void paintCleaver(cairo_t* cr, double len);

} // namespace gr
