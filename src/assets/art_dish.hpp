#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

namespace gr
{

// Level 32, Dish Array (theme look "dish", art_dish.cpp): a radio telescope
// array out in the desert at midday. The masses are warm sandstone, laid
// down in wavy strata with cross-bedding, pebbles and an iron-red band now
// and then; where you stand, a crust of pale wind-blown sand with ripples, a
// pebble or two and a tuft of dry grass. The one-way floors are the dishes'
// white-painted steel catwalks, the ladders galvanized steel. Behind: a deep
// blue sky paling to a hot haze at the horizon; far off, red mesas and
// buttes, their strata lit orange on the sunny side; close by, pale dunes
// with a line of small dishes and a power line across them. The exit is an
// uplink antenna on its tripod. The satellite, the heat shimmer, the sand
// sea, the dishes and everything at the array are drawn live by the level
// (world_dish_draw.cpp).
bool isDishArray(const Theme& t);
// Where the desert's horizon sits on the screen (the sky bakes it).
constexpr float kDishHorizonY = 440.0f;
Texture bakeDishSolid(const Renderer& r, const Theme& t, int variant);
Texture bakeDishSolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeDishPlatform(const Renderer& r, const Theme& t);
Texture bakeDishLadder(const Renderer& r, const Theme& t);
Texture bakeDishSky(const Renderer& r, const Theme& t);
Texture bakeDishFar(const Renderer& r, const Theme& t, int layerW);
Texture bakeDishNear(const Renderer& r, const Theme& t, int layerW);
// The exit: an uplink antenna, a small white dish on a tripod with a red
// beacon (96 x 144, the same frame as the family's teleporter).
Texture bakeDishExitBase(const Renderer& r, const Theme& t);

} // namespace gr
