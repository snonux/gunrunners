#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

namespace gr
{

// Level 31, Storm Relay (theme look "storm", art_storm.cpp): a sea cliff
// and its lighthouse at night in a thunderstorm. The masses are wet black
// cliff rock, basalt split into slabs with a cold sheen of rain on every
// upper edge, barnacles and limpets low down and seeps of water in the
// cracks; where you stand, a glossy wet lip with pebbles and a little sea
// thrift. The one-way floors are rusted steel grating, the ladders rusted
// iron. Behind: a night sky of heavy storm cloud over a dark sea, the
// horizon a pale strip far off; far away, cloud banks with rain curtains;
// close by, low scud blowing past. The exit is the relay dish on the
// lighthouse's lamp room. The rain, the sheet lightning, the surf and
// everything on the cliff are drawn live (world_storm_draw.cpp).
bool isStormCliffs(const Theme& t);
// Where the sea's horizon sits on the screen (the sky bakes it; the level
// draws its waves and the ship on it).
constexpr float kStormHorizonY = 408.0f;
Texture bakeStormSolid(const Renderer& r, const Theme& t, int variant);
Texture bakeStormSolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeStormPlatform(const Renderer& r, const Theme& t);
Texture bakeStormLadder(const Renderer& r, const Theme& t);
Texture bakeStormSky(const Renderer& r, const Theme& t);
Texture bakeStormFar(const Renderer& r, const Theme& t, int layerW);
Texture bakeStormNear(const Renderer& r, const Theme& t, int layerW);
// The exit: the relay dish on its tripod (96 x 144, the same frame as the
// family's teleporter).
Texture bakeStormExitBase(const Renderer& r, const Theme& t);

} // namespace gr
