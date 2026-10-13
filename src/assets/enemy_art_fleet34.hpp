#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 34, Zeppelin Relay: the fleet's cast, drawn like the other styled
// enemies (enemy_art.cpp calls this for keys it has no routine for). The box
// is (32, 32) .. (32 + w, 32 + h) in px, facing right. Returns false if `key`
// is not one of these.
//   sky_pirate      3 x 5: a sky pirate with a brass jet pack on his back:
//                   a leather flying cap with brass goggles pushed up, an
//                   eyepatch, a ginger beard, a striped shirt under a
//                   leather waistcoat, red pantaloons, tall boots, a cutlass
//                   and a flintlock. variant 0 on deck, cutlass low; 1 the
//                   cutlass tell (wound up over his head, the blade bright);
//                   2 the pistol tell (arm out, flintlock levelled, the
//                   cutlass held back); 3 jetting in (legs trailing, the
//                   pack's flames lit); frame 0/1 his stride (or the flames'
//                   flicker)
//   peck_parrot     3 x 3: a scarlet macaw in flying goggles, blue and
//                   yellow wings, long tail. variant 0 perched (frame 0
//                   head up, 1 pecking at the balloon under it); 1 the tell
//                   (squawking, beak wide, wings flared, crest up); 2 diving
//                   (wings swept back, beak first); 3 flying back (frame 0
//                   wings up, 1 down)
//   carrier_parrot  3 x 3: the green carrier: an Amazon parrot in a mail
//                   carrier's cap with a satchel of letters. variant 0
//                   standing, 1 mid-hop (wings out, feet tucked), 2 the
//                   tell (squawking), 3 diving, 4 flying (frame 0/1 wings)
//   balloon_mine    4 x 4: a naval mine slung under a small patched balloon,
//                   brass-capped horns, a lamp on its crown. variant 0
//                   drifting (frame 0/1 its sway), 1 lit (red-hot, the lamp
//                   blazing, the horns glowing; frame 0/1 the flash)
//   duck_aviator    variant 1 over the duck item: a leather flying cap with
//                   brass goggles pushed up and a white silk scarf
bool drawFleet34Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

} // namespace gr
