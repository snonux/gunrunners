#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

namespace gr
{

// Level 35, The Transmitter (theme key broadcast_mast, look "tower",
// art_tower35.cpp): a red and white lattice broadcast mast standing out of
// a sea of cloud at dawn. The masses (the base ring, the top deck) are
// red-painted steel plate, riveted, the paint worn back to grey steel at
// the seams, now and then a louvred vent or an RF hazard placard; where you
// stand, a galvanized grating lip with a white edge. The one-way ledges
// are grating walkways on white girders with a red and white toe board,
// the ladders galvanized with a safety cage. Behind: a dawn sky, deep blue
// overhead going peach and gold toward the horizon. The cloud sea, the
// lattice of the mast itself (front and back), its dishes and warning
// lights, the spire and everything else on the tower are drawn live by the
// level (world_tower_draw.cpp), keyed to the tower's own x so nothing jumps
// as it turns.
bool isTransmitter(const Theme& t);
Texture bakeTowerSolid(const Renderer& r, const Theme& t, int variant);
Texture bakeTowerSolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeTowerPlatform(const Renderer& r, const Theme& t);
Texture bakeTowerLadder(const Renderer& r, const Theme& t);
Texture bakeTowerSky(const Renderer& r, const Theme& t);
// The far and near layers turn with the tower, so the level draws them
// live; these are empty strips.
Texture bakeTowerFar(const Renderer& r, const Theme& t, int layerW);
Texture bakeTowerNear(const Renderer& r, const Theme& t, int layerW);
// The exit: the transmitter's beam head, a horn emitter on a plinth
// pointing up (96 x 144, the same frame as the family's teleporter).
Texture bakeTowerExitBase(const Renderer& r, const Theme& t);

} // namespace gr
