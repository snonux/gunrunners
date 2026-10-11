#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

namespace gr
{

// Level 30, Fiber Trench (theme look "fiber_trench", art_trench.cpp): the
// sea floor off the coast, from the sunlit shelf down to the trench. The
// masses are reef rock, slate blue and worn smooth, crusted here and there
// with sponges, barnacles and little coral heads; where you stand, a lip of
// pale silt with tufts of sea grass. Fire coral bristles where it hurts.
// Behind: open water from the bright blue under the surface to the black of
// the deep, the sun's shafts slanting down; far off, reef ridges and arches
// and a wreck in the blue haze; close by, a kelp forest in silhouette. The
// depth, the moving light, the motes, the cable and the wreck's hull are
// drawn live by the level (world_trench_draw.cpp). Deep Dive keeps its own
// "reef" look.
bool isFiberTrench(const Theme& t);
Texture bakeTrenchSolid(const Renderer& r, const Theme& t, int variant);
Texture bakeTrenchSolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeTrenchSpikes(const Renderer& r, const Theme& t);
Texture bakeTrenchSky(const Renderer& r, const Theme& t);
Texture bakeTrenchFar(const Renderer& r, const Theme& t, int layerW);
Texture bakeTrenchNear(const Renderer& r, const Theme& t, int layerW);
// The exit: the cable station's relay, the fiber coming in at its foot
// (96 x 144, the same frame as the family's teleporter).
Texture bakeTrenchExitBase(const Renderer& r, const Theme& t);

} // namespace gr
