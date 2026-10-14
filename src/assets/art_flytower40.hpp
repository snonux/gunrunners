#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

namespace gr
{

// Level 40, The Fly Tower (theme key fly_tower, look "fly_tower",
// art_flytower40.cpp): a theatre's fly tower during the show, black steel
// in stage haze, lit by coloured beams from below. The masses are
// black-painted steel deck plate with a diamond tread, riveted at the
// seams, now and then an I-beam's web with its bolted splice or a DMX
// junction box. Where you stand, the deck's black top with a sheen of
// amber, a strip of glow tape along its front edge and the steel angle
// nosing under it. The one-way ledges are catwalks: bar grating on a
// black kick plate and a steel channel stringer (the hangers and
// handrails are drawn by the level). The ladders are black steel with
// glow-tape marks on the rails; the spikes stage-brace spikes in a tray.
// Behind: the stage house's height lost in haze, coloured beams slanting
// up through it; the back wall of sooty black brick with steel columns,
// the counterweight arbors in their tracks, purchase lines running up,
// caged worklights; nearer, battens hanging across with black velour
// borders and lamps under them. The level draws its own rigging over that
// (world_flytower_draw.cpp).
bool isFlyTower(const Theme& t);
Texture bakeFlyTowerSolid(const Renderer& r, const Theme& t, int variant);
Texture bakeFlyTowerSolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeFlyTowerPlatform(const Renderer& r, const Theme& t);
Texture bakeFlyTowerLadder(const Renderer& r, const Theme& t);
Texture bakeFlyTowerSpikes(const Renderer& r, const Theme& t);
Texture bakeFlyTowerSky(const Renderer& r, const Theme& t);
Texture bakeFlyTowerFar(const Renderer& r, const Theme& t, int layerW);
Texture bakeFlyTowerNear(const Renderer& r, const Theme& t, int layerW);
// The exit: the hatch up to the lighting bridge, a steel ladder climbing
// out of a steel frame into the bridge's light, a lit green EXIT sign and
// LIGHTING BRIDGE stencilled over it (96 x 144, the same frame as the
// family's teleporter).
Texture bakeFlyTowerExitBase(const Renderer& r, const Theme& t);

} // namespace gr
