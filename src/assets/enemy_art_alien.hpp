#pragma once

#include "data/theme.hpp"
#include "render/vector.hpp"

#include <string>

namespace gr
{

// Episode 7, DEEP SPACE: the aliens of Vurr and their goo, drawn like the
// other styled enemies (enemy_art.cpp calls this for keys it has no routine
// for). The box is (32, 32) .. (32 + w, 32 + h) in px, facing right.
// Returns false if `key` is not one of these.
bool drawAlienArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);
// Level 45, Hive Gullets: its aliens, the Gullet Tubes' mouths, valves and
// pores (enemy_art_hive.cpp), drawn the same way.
bool drawHiveArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);
// Level 43, Starfall: its aliens, the asteroids, the probe, the pad and the
// clouds (enemy_art_starfall.cpp).
bool drawStarfallArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);
// Level 46, Crystal Drift: the Swap Crystals and its aliens
// (enemy_art_crystal.cpp).
bool drawCrystalArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);
// Level 47, Silk Canyon: the Loom Spider, Cocoon Pod, Dropling and the
// cocoons on the webs (enemy_art_silk.cpp).
bool drawSilkArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);
// Level 48, Bounder Plains: the Thorn Hog, Sky Gulper and thornbushes
// (enemy_art_plains.cpp).
bool drawPlainsArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);
// Level 49, The Hive Mother: her, her throne, the Egg Guards and Spore
// Nurses (enemy_art_mother.cpp).
bool drawMotherArt(cairo_t* cr, const Theme& t, const std::string& key, double w, double h, int variant, int frame);

} // namespace gr
