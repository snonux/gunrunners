#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 29, Dead Air: the void's cast, drawn like the other styled enemies
// (enemy_art.cpp calls this for keys it has no routine for). The box is
// (32, 32) .. (32 + w, 32 + h) in px, facing right. Returns false if `key`
// is not one of these.
//   ghost_image   a newsreader burnt into the tube: grey, scanned, fading
//                 out below the knees, a bright tracking line for eyes.
//                 variant 0 drifting (frame 0/1), 1 the tell and the shot
//                 (an arm out at the camera, the eyes flaring), 2 an
//                 after-image (a flat grey silhouette)
//   static_swarm  a cloud of TV snow with two slits of eyes. variant 0
//                 plain (white), 1 a carrier (green-tinted); frame 0-3 the
//                 snow churning
//   duck_bars     the rubber duck in the seven bar colours: variant 1 over
//                 the duck item
bool drawSignalArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

// The Test Card (Level 29's boss), in pieces the game turns and lights:
// each painted round the card's middle at (cx, cy), `s` times its 256 px.
// The square back plate with its grid and the round well the disc turns in.
void paintTestCardBack(cairo_t* cr, double cx, double cy, double s);
// One quarter of the disc, the top-left one (the game turns it into place):
// `c` its colour, grey and full of snow when `dead`.
void paintTestCardWedge(cairo_t* cr, double cx, double cy, double s, Color c, bool dead);
// The same quarter as a flat white shape (for the flash and the glow).
void paintTestCardWedgeMask(cairo_t* cr, double cx, double cy, double s);
// The face in the middle: a grey disc with a crosshair, two pill eyes and a
// mouth; `mouth` 0 a line, 1 open ("please stand by"), 2 a flat frown
// (beaten).
void paintTestCardFace(cairo_t* cr, double cx, double cy, double s, int mouth);

} // namespace gr
