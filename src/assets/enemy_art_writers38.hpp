#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Level 38, Writers' Room: the script's cast, drawn like the other styled
// enemies (enemy_art.cpp calls this for keys it has no routine for). The
// box is (32, 32) .. (32 + w, 32 + h) in px, facing right. Returns false if
// `key` is not one of these.
//   typo                2 x 2: a crawling blot of black ink with drips,
//                       two goggling eyes and six spidery ink legs; its
//                       letter is drawn over it by the level. variant 0/1
//                       the crawl, 2 the tell (sprung up off its legs, the
//                       ink splashing), 3-5 the same for the carrier (ink
//                       gone a sickly green, a fever sweat, a thermometer)
//   deadline            4 x 5: a twin-bell alarm clock on long legs in
//                       sneakers, its dial a face (the hands are drawn by
//                       the level). variant 0-4 how far behind it is (calm,
//                       flushed, sweating, red and steaming, frantic), 5-9
//                       the same ringing (the hammer blurred between the
//                       bells, ring marks); frame 0/1 the stride
//   writersblock        4 x 4: a heavy block of grey concrete, cracked and
//                       chipped, cobwebbed, a sullen face low on it (its
//                       word BLOCK goes above). variant 0 sitting (lids
//                       down), 1 the tell (eyes open, teeth gritted, grit
//                       trickling), 2 sliding (eyes squeezed, leaning)
//   sketch_guard        3 x 5: a sentry sketched in pencil on paper white,
//                       graphite outlines doubled and hatched, a helmet and
//                       a big mustache, a long yellow pencil for a rifle.
//                       variant 0 at ease (the rifle sloped), 1 the tell
//                       (the rifle coming up), 2 aiming (it fires)
//   duck_glasses_pencil variant 0 behind the duck item (the inside of a
//                       coffee mug, its far rim and the coffee), 1 over it
//                       (the mug's front with its handle and slogan, little
//                       reading glasses on the beak, a pencil behind the ear)
bool drawWriters38Art(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

} // namespace gr
