#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 28, Toon Town Throwdown: the 1930s rubber-hose cartoon's cast,
// drawn like the other styled enemies (enemy_art.cpp calls this for keys it
// has no routine for). The box is (32, 32) .. (32 + w, 32 + h) in px,
// facing right. Returns false if `key` is not one of these.
//   grinning_bomb  a black cartoon bomb on legs with a lit fuse. variant 0
//                  chasing with a toothy grin, 1 the tell (it stops
//                  smiling: a frown, the fuse fizzing), 2 the lunge
//                  (stretched forward, teeth gritted); frame 0/1 the legs
//   piano_bird     a stork in a bow tie. variant 0 carrying (its feet
//                  gripping the piano's ropes), 1 empty-footed; frame 0/1
//                  the wingbeat
//   rubber_goon    a bulldog heavy in a striped sweater and a bowler. variant
//                  0 walking (frame 0/1), 1 the tell (the arm wound up into a
//                  coil), 2 the punch (the arm stretched out of the box: the
//                  rest is goon_arm), 3 squashed flat (h < 100), 4 flat and
//                  wobbling after a bounce
//   goon_arm       the punch's stretched rubber arm, a fist at its right end
//                  (w long)
//   duck_gloves    the rubber duck item's white gloves: variant 1 over it
bool drawToonArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

// Buddy Boom, MaxTV's mascot, in design px (his 9 x 15 cell box is 288 x
// 480): his feet's middle at (cx, feetY), facing right, `s` times as big.
// Poses: 0/1 walking, 2 the mallet raised overhead, 3 bent over the
// mallet after a slam (his nose down at (+144, -96)), 4 spinning (front
// view; frame 0-3), 5 hopping, 6 a pie wound up, 7 the pie thrown, 8 riding
// the pencil (arms out, balancing), 9 standing on his drawing (smug; frame
// taps a foot), 10 drawing with the giant pencil, 11 pointing the eraser
// down, 12 sitting dizzy, 13 falling, 14 deflating (frame 0-4: 4 is the
// empty skin), 15 waving (the cutscenes). Standing, his nose is at
// (+144, -352). The nose is black; the game paints the red flash over it.
void paintBuddy(cairo_t* cr, double cx, double feetY, double s, int pose, int frame);

// An upright piano, 192 x 128 px at (x, y) (its top-left), `s` times as
// big. variant 0 hanging or falling, 1 landed (a leg buckled, keys
// knocked loose).
void paintPiano(cairo_t* cr, double x, double y, double s, int variant);

// A white cartoon glove with its cuff, centred at (x, y), the fingers
// pointing along `angle`; open or a fist.
void paintToonGlove(cairo_t* cr, double x, double y, double size, double angle, bool open);

// The giant yellow pencil, its eraser end at (x0, y0) and its point at
// (x1, y1), `width` across.
void paintToonPencil(cairo_t* cr, double x0, double y0, double x1, double y1, double width);

// A cream pie on its tin plate, centred at (x, y), `s` times 68 px wide.
void paintCreamPie(cairo_t* cr, double x, double y, double s);

} // namespace gr
