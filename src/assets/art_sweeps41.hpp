#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

namespace gr
{

// Level 41, Sweeps Week (theme key sweeps_week, look "sweeps_week",
// art_sweeps41.cpp): the studio of a Saturday-night variety show in
// ratings week, burgundy velvet and gold. The masses are the stage's
// velvet skirting, deep pleats catching the light, a sequin glinting here
// and there. Where you stand, the stage's black lacquer deck with a
// reflection along it, a gold nosing and a row of little marquee bulbs on
// the fascia under it. The one-way ledges are box-seat balcony fronts: a
// brass rail over tufted red velvet with gold buttons. The ladders are
// brass with rubber treads; the spikes a tray of footlights burning
// white-hot. Behind: the studio's dark with follow spots crossing it; a
// gold sequin curtain in deep folds under a swagged valance, the show's
// name in marquee bulbs on it; nearer, a lighting truss of PAR cans and
// glitter stars hanging on wires. The level draws its own set over that
// (world_sweeps_draw.cpp): the vote screens, the stands and their seats,
// the orchestra pit, the turntables.
bool isSweeps(const Theme& t);
Texture bakeSweepsSolid(const Renderer& r, const Theme& t, int variant);
Texture bakeSweepsSolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeSweepsPlatform(const Renderer& r, const Theme& t);
Texture bakeSweepsLadder(const Renderer& r, const Theme& t);
Texture bakeSweepsSpikes(const Renderer& r, const Theme& t);
Texture bakeSweepsSky(const Renderer& r, const Theme& t);
Texture bakeSweepsFar(const Renderer& r, const Theme& t, int layerW);
Texture bakeSweepsNear(const Renderer& r, const Theme& t, int layerW);
// The exit: the stage door, red lacquer with a gold star on it, STAGE DOOR
// over it in a frame of marquee bulbs (96 x 144, the same frame as the
// family's teleporter).
Texture bakeSweepsExitBase(const Renderer& r, const Theme& t);

} // namespace gr
