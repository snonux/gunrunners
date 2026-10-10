#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

namespace gr
{

// Level 25, Tiny Kingdom (theme look "garden", art_garden.cpp): a back
// garden at ant height in golden light. Dark loam with pebbles and roots for
// the masses, a turf of grass blades where you stand, broad leaves for the
// one-way floors, green stalks with leaf nubs for the climbables, bramble
// thorns for the spikes; behind it a golden sky full of bokeh, giant soft
// grass blades and flower heads far off, and close by the garden fence's
// weathered planks with clover and dew drops in front.
bool isGarden(const Theme& t);
Texture bakeGardenSolid(const Renderer& r, const Theme& t, int variant);
Texture bakeGardenSolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeGardenPlatform(const Renderer& r, const Theme& t);
Texture bakeGardenLadder(const Renderer& r, const Theme& t);
Texture bakeGardenSpikes(const Renderer& r, const Theme& t);
Texture bakeGardenSky(const Renderer& r, const Theme& t);
Texture bakeGardenFar(const Renderer& r, const Theme& t, int layerW);
Texture bakeGardenNear(const Renderer& r, const Theme& t, int layerW);

} // namespace gr
