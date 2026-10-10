#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 23, Fright Night Manor: the Portrait Ghost, the Haunted Armor and the
// Poltergeist's haunted objects, drawn like the other styled enemies
// (enemy_art.cpp calls this for keys it has no routine for). The box is
// (32, 32) .. (32 + w, 32 + h) in px, facing right. Returns false if `key`
// is not one of these.
//   portrait_ghost  variant 0 drifting out (faint), 1 chasing, 2 the tell
//                   (eyes flaring, claws up), 3 the lunge (stretched, mouth
//                   wide), 4 pinned (spread flat, a bolt through it);
//                   frame 0/1 the tail's wave
//   haunted_armor   variant 0 still (a cobweb on it), 1 walking (frame 0/1
//                   the stride), 2 the tell (the visor glowing, the halberd
//                   raised), 3 the swing (the halberd down in front)
//   poltergeist     variant 0 a book, 1 a soup tureen, 2 a candelabra; +3
//                   rattling (lid up, pages flapping, flames flaring);
//                   frame 0/1 the glow pulsing
//   manor_thrown    what a Poltergeist throws: variant 0 a book (frame 0/1
//                   flapping), 1 a plate, 2 a candlestick
//   duck_cape       a vampire cape for the rubber duck item (2 x 2 cells):
//                   variant 0 the cape behind it, 1 the collar's clasp and
//                   the fangs in front
bool drawManorArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

// Pieces the level (world_manor_draw.cpp) and the cutscenes (clips.cpp)
// share.

// A Portrait Ghost's face as its portrait shows it: a powdered nobleman
// with a curled wig, `s` px to the head's width, centred at (cx, cy).
// flare 0 (sleeping in the paint) .. 1 (eyes lit, about to come out).
void paintGhostFace(cairo_t* cr, double cx, double cy, double s, double flare);

// The manor on its hill in silhouette, its middle at cx and its foot at
// `base`, `s` px to a unit (the house is about 400 units wide, 360 tall):
// towers, spires, chimneys, lit windows. `lit` lights the windows.
void paintHauntedManor(cairo_t* cr, double cx, double base, double s, Color body, Color lit);

// A vampire's cape hung from the shoulders at (cx, neckY): `w` px across at
// the hem, `h` px long, black outside and blood red inside, a high stiff
// collar. swirl 0..1 is the hem's wave (a loop); open 0 (wrapped) .. 1
// (spread wide like wings).
void paintVampireCape(cairo_t* cr, double cx, double neckY, double w, double h, double swirl, double open);

// The cape's high collar alone (it stands up behind the head), for drawing
// before the wearer: points at about (cx +- w/2, neckY - h).
void paintCapeCollar(cairo_t* cr, double cx, double neckY, double w, double h);

// A Silver Crossbow bolt lying along +x from its tip at (0, 0) in the
// current transform, `len` px long: a silver shaft, a barbed head, white
// fletching.
void paintBolt(cairo_t* cr, double len);

} // namespace gr
