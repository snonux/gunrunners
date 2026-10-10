#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

namespace gr
{

// Episode 7, DEEP SPACE: the planet Vurr's own tiles and backdrops
// (art_alien.cpp). A theme whose look starts with "alien" gets organic
// tiles in its palette; the rest of the look picks the backdrop
// ("alien_garden": Crash Garden's fungus forest at twilight, "alien_hive":
// inside the hive, Level 45).
bool isAlien(const Theme& t);
Texture bakeAlienSolid(const Renderer& r, const Theme& t, int variant);
Texture bakeAlienSolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeAlienPlatform(const Renderer& r, const Theme& t);
Texture bakeAlienSky(const Renderer& r, const Theme& t);
Texture bakeAlienFar(const Renderer& r, const Theme& t, int layerW);
Texture bakeAlienNear(const Renderer& r, const Theme& t, int layerW);

} // namespace gr
