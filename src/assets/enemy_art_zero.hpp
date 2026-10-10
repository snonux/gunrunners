#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 21, ZERO: Lattice Turrets and Repair Swarms, drawn like the other
// styled enemies (enemy_art.cpp calls this for keys it has no routine for).
// The box is (32, 32) .. (32 + w, 32 + h) in px, facing right. Returns false
// if `key` is not one of these.
//   lattice_turret  variant 0 riding its rail, 1 the barrel glowing red (the tell)
//   repair_swarm    frame 0-3: the nanobots' swirl
//   echo            a cyan runner-shaped hologram (world_zero_draw.cpp draws
//                   the real ones from the runner art)
bool drawZeroArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

// Pieces the level (world_zero_draw.cpp) and the cutscenes (clips.cpp) share,
// so ZERO, its racks and the studio look the same in both.

// A black server rack cabinet filling (0, 0) .. (w, h): a frame, rails, 1U
// servers with their status lights (lit or dark), a label plate at the top
// if `label` is not empty. `seed` varies the units.
void paintServerRack(cairo_t* cr, double w, double h, unsigned seed, bool lit, const char* label = "");

// ZERO's eye, a giant camera lens, centred at (cx, cy), `rad` px to the
// outside of its housing: the barrel and its knurled zoom ring, the red
// tally light, the glass, and in it the red iris round a black pupil.
// pupil: 0.15 (contracted, the tell) .. 0.45 (wide); shutter 0 open .. 1
// shut (an iris diaphragm's blades); light 0 (dark) .. 1 (lit); ring: the
// zoom ring's turn (radians).
void paintZeroEye(cairo_t* cr, double cx, double cy, double rad, double pupil, double shutter, double light, double ring);

// Lance Marquee, the show host, in design units: his feet at (120, 480),
// about 440 tall, facing right. pose 0-7 striding (a walk cycle), 8 arms
// thrown wide, 9 standing with the microphone up. mouth: open or not.
void paintLance(cairo_t* cr, int pose, bool mouthOpen);

// One member of a studio audience, head and shoulders, `s` px to a head's
// width, the chin at (x, y). kind 0 anybody (seed picks the look), 1 Black
// Halo's pilot (level 7, flight suit and helmet), 2 the spare host from
// level 16's pod (a Lance lookalike, frost on his shoulders). frame 0-5:
// the arms waving.
void paintAudienceMember(cairo_t* cr, double x, double y, double s, int kind, int frame, unsigned seed);

} // namespace gr
