#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 37, Wardrobe: the costume department's cast, drawn like the other
// styled enemies (enemy_art.cpp calls this for keys it has no routine for).
// The box is (32, 32) .. (32 + w, 32 + h) in px, facing right. Returns
// false if `key` is not one of these.
//   mannequin        3 x 5: a jointed shop mannequin in pale varnished
//                    wood, ball joints at shoulders, elbows, hips and
//                    knees, a blank egg of a head. variant 0-3 the frozen
//                    poses of its walk (arms and legs caught mid-step),
//                    4-7 the same poses twitching (the head jerked to one
//                    side, the near arm flung up); frame 0-2 what it wears
//                    (a tape measure round its neck, a half-pinned
//                    waistcoat, a feathered hat and a sash)
//   powder_puffer    4 x 4: a big pink powder puff with beady eyes and
//                    lashes, a satin ribbon on top, a rubber squeeze bulb
//                    at its back and a brass atomiser nozzle at its front.
//                    variant 0 hovering, 1 the tell (the bulb squeezed
//                    flat, the puff swollen), 2 puffing (powder blasting
//                    from the nozzle); 3-5 the same for the carrier: sickly
//                    green, "EXP 09/26" stamped on it. frame 0/1 the blink
//   rack_runner      6 x 6: a rolling garment rack crammed with coats,
//                    angry eyes glaring out between them, chrome uprights,
//                    a top rail of hangers, four casters. variant 0 parked,
//                    1 the tell (the hangers rattling, coats swinging), 2
//                    rolling (the coats swept back); frame 0/1 the casters
//                    and hangers
//   gorgazilla_suit  3 x 5: the rubber Gorgazilla suit hanging limp on a
//                    hanger, its head flopped forward, its tail on the floor
//   gorgazilla_worn  4 x 6: the suit worn, the runner's eyes peering out of
//                    its open jaws. variant 0 standing, 1 and 2 the stride,
//                    3 in the air
//   duck_boa_top_hat variant 0 behind the duck item (the back of a pink
//                    feather boa), 1 over it (a tall black top hat with a
//                    red band, the boa's front)
bool drawWardrobe37Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

} // namespace gr
