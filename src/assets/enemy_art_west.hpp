#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 22, Dry Gulch: the Duelist, the Tumble Mine and the Window Bandit,
// drawn like the other styled enemies (enemy_art.cpp calls this for keys it
// has no routine for). The box is (32, 32) .. (32 + w, 32 + h) in px,
// facing right. Returns false if `key` is not one of these.
//   duelist        variant 0 waiting (hand loose), 1 drawn (the gun out at
//                  chest height), 2 the hand over the holster (frame 0/1
//                  twitches)
//   tumble_mine    a spiked mine bundled in a tumbleweed; variant 1 its lamp
//                  blinking red (the rattle). The level turns it as it rolls.
//   window_bandit  a masked bandit up behind a window sill, his gun out
//   tonic_bottle   Dr. Fizz's Miracle Tonic (the Virus, re-skinned)
//   duck_cowboy    a cowboy hat for the rubber duck item (2 x 2 cells)
bool drawWestArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

// Pieces the level (world_west_draw.cpp) and the cutscenes (clips.cpp)
// share.

// A cowboy hat: the brim's middle at (cx, brimY), `w` px from brim tip to
// brim tip, crown up. `felt` its colour; `tilt` radians (clockwise).
void paintCowboyHat(cairo_t* cr, double cx, double brimY, double w, Color felt, double tilt = 0.0);

// A tumbleweed: a loose ball of dry twigs `rad` px round (cx, cy). `seed`
// varies the tangle.
void paintTumbleweed(cairo_t* cr, double cx, double cy, double rad, unsigned seed);

// The Duelist's face for his wanted posters: hat, stubble, a moustache,
// `s` px to the head's width, centred at (cx, cy). sepia: printed in brown
// ink on the poster.
void paintOutlawFace(cairo_t* cr, double cx, double cy, double s, bool sepia);

// A revolver lying along +x from its grip at (0, 0) in the current
// transform, `s` px to a unit (about 26 units long, 17 tall).
void paintRevolver(cairo_t* cr, double s, Color metal);

// A saloon's false front, (0, 0) .. (w, h): the tall stepped board with its
// sign, the porch roof on posts, batwing doors and two windows, all in
// sun-bleached planks.
void paintSaloonFront(cairo_t* cr, double w, double h, const char* sign);

} // namespace gr
