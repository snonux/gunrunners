#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

namespace gr
{

// Level 28, Toon Town Throwdown (theme look "toon", art_toon.cpp): a 1930s
// rubber-hose cartoon. The masses are painted earth laid in flat with a dry
// brush, a pebble or a crack inked here and there; where you stand, a
// scalloped band of painted grass under a thick wobbly ink line, a tuft
// poking up; one-way floors are wobbly painted planks, climbables a crooked
// wooden ladder. Behind: a cream paper sky with its grain and the old
// film's dark corners, inked clouds and a grinning sun; far off pastel hills
// with ink outlines and lollipop trees, a barn and a windmill; close by
// rounder green hills, a picket fence and bushes.
bool isToon(const Theme& t);
Texture bakeToonSolid(const Renderer& r, const Theme& t, int variant);
Texture bakeToonSolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeToonPlatform(const Renderer& r, const Theme& t);
Texture bakeToonLadder(const Renderer& r, const Theme& t);
Texture bakeToonSky(const Renderer& r, const Theme& t);
Texture bakeToonFar(const Renderer& r, const Theme& t, int layerW);
Texture bakeToonNear(const Renderer& r, const Theme& t, int layerW);

} // namespace gr
