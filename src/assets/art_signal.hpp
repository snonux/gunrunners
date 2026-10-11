#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

namespace gr
{

// Level 29, Dead Air (theme look "dead_air", art_signal.cpp): a black void
// of TV static between channels. The floors are clean white studio panels,
// matte, with a hairline seam and now and then a registration mark or a
// grey step wedge; where you stand, a bright white lip that glows faintly
// into the dark, a hint of red and blue fringing on it like a set out of
// convergence. Behind: the black of a dead tube (its faint glow, frozen
// grain, scanlines and dark corners); far off, the ghosts of lost
// broadcasts (colour bars sliced by a slipping hold, test-card rings);
// close by, drifting clouds of snow and the thin bright lines of
// interference. The moving snow over it all is drawn live
// (world_signal_draw.cpp).
bool isDeadAir(const Theme& t);
Texture bakeSignalSolid(const Renderer& r, const Theme& t, int variant);
Texture bakeSignalSolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeSignalPlatform(const Renderer& r, const Theme& t);
Texture bakeSignalLadder(const Renderer& r, const Theme& t);
Texture bakeSignalSky(const Renderer& r, const Theme& t);
Texture bakeSignalFar(const Renderer& r, const Theme& t, int layerW);
Texture bakeSignalNear(const Renderer& r, const Theme& t, int layerW);

} // namespace gr
