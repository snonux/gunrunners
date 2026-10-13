#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 35, The Transmitter: the mast's cast, drawn like the other styled
// enemies (enemy_art.cpp calls this for keys it has no routine for). The box
// is (32, 32) .. (32 + w, 32 + h) in px, facing right. Returns false if `key`
// is not one of these.
//   mast_climber    3 x 5: a rigger in an orange hi-vis vest with silver
//                   stripes, a safety harness and tool belt, a white hard
//                   hat with a red stripe and safety glasses, a big
//                   adjustable wrench. variant 0 walking (frame 0/1 the
//                   stride), 1 the tell (the wrench raised high behind its
//                   head, glinting), 2 the swing (the wrench brought down in
//                   front), 3 climbing a ladder (seen from behind, frame 0/1
//                   which hand is up), 4 the Copycat dummy (a crash-test
//                   mannequin in an orange hard hat; frame 0/1 the stride),
//                   5 the dummy in the air (arms up), 6 the rigger falling
//                   (arms flung up)
//   relay_drone     4 x 3: a white relay quadcopter seen from the side, a
//                   small dish and a whip antenna on top, a blaster pod
//                   under its nose. variant 0 flying, 1 the tell (the
//                   muzzle charging white-hot, its lamp red); frame 0-3 the
//                   rotors' blades
//   feedback_node   4 x 4: a squat steel cabinet with cooling fins and two
//                   horn speakers, a stack of ceramic insulators and a
//                   copper coil ring round an emitter sphere on top.
//                   variant 0 idle (the sphere dull, a faint blue hum), 1
//                   the tell (sphere and coil white-hot), 2 the pulse (blue
//                   arcs off the sphere), 3 cooling (the coil glowing a dull
//                   red); frame 0/1 the flicker
//   halcyon_titan   10 x 12: the Halcyon Titan, a big glossy white mech in
//                   Halcyon blue trim: a domed head with a visor slit,
//                   missile pods on its shoulders, a laser cannon on its
//                   near arm at a runner's head height, a flamethrower
//                   under it, a reactor pack on its back, the shuttered
//                   power core in its chest and four vent panels outlined
//                   (front two on its chest and hip, back two on its pack
//                   and back hip). variant 0 standing, 1 crouched (the stomp
//                   coming), 2 and 3 the two halves of a stride
//   duck_hard_hat   variant 1 over the duck item: a yellow hard hat with a
//                   ridge and a little red beacon
bool drawTower35Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

// Where the Titan's parts sit in its 320 x 384 px box (facing right, the
// feet on the bottom edge), for the effects the level draws over it
// (world_tower_draw.cpp). Crouched (variant 1), everything above the hips
// is kTitanCrouch px lower.
constexpr float kTitanCrouch = 26.0f;
constexpr float kTitanCoreX = 186.0f, kTitanCoreY = 182.0f, kTitanCoreR = 26.0f;
constexpr float kTitanLaserX = 316.0f, kTitanLaserY = 229.0f;   // the laser cannon's muzzle
constexpr float kTitanFlameX = 300.0f, kTitanFlameY = 286.0f;   // the flamethrower's nozzle
constexpr float kTitanPackX = 48.0f, kTitanPackY = 180.0f;      // the reactor pack's core
constexpr float kTitanPodX = 104.0f, kTitanPodY = 78.0f;        // the near missile pod's tubes
constexpr float kTitanEyeX = 204.0f, kTitanEyeY = 84.0f;
// The vent panels: front-left, front-right, back-left, back-right as
// (x, y, w, h) boxes.
constexpr float kTitanPanel[4][4] = {
  {232.0f, 146.0f, 24.0f, 48.0f},
  {226.0f, 264.0f, 26.0f, 36.0f},
  {26.0f, 140.0f, 24.0f, 50.0f},
  {78.0f, 264.0f, 26.0f, 36.0f},
};

} // namespace gr
