#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

namespace gr
{

// Level 33, Magma Mainframe (theme key volcano_datacenter, look "magma",
// art_magma33.cpp): a data centre built inside a live volcano. The masses
// are black basalt in columns, their joints lit orange from below, a seam of
// magma glowing through now and then and a thin cable conduit with a small
// blue status light run down a joint. Where you stand, a dark rock lip warm
// with reflected glow. The one-way floors are steel grating catwalks with a
// blue light strip, the ladders yellow-railed steel. Behind: the smoky dark
// of the cone lit orange from below; far off, the cavern wall cut into
// terraces of server racks with their blue lights. Level 12's "magma" look
// (Lava Heart) keeps its own art: this one is told apart by the theme's name.
// The floors' steel decks, the racks, the back aisles, the coolant stations,
// the crater, the mainframe, the mast and the airship are drawn live by the
// level (world_magma_draw.cpp).
bool isMagmaMainframe(const Theme& t);
Texture bakeMainframeSolid(const Renderer& r, const Theme& t, int variant);
Texture bakeMainframeSolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeMainframePlatform(const Renderer& r, const Theme& t);
Texture bakeMainframeLadder(const Renderer& r, const Theme& t);
Texture bakeMainframeSky(const Renderer& r, const Theme& t);
// The far layer is taller than the screen (anchored `kMainframeFarRise` px
// down) so it still covers the screen after the whole 150-block climb.
constexpr int kMainframeFarRise = 600;
Texture bakeMainframeFar(const Renderer& r, const Theme& t, int layerW);
// The near layer repeats up the climb, so the level draws it live; this is
// an empty strip.
Texture bakeMainframeNear(const Renderer& r, const Theme& t, int layerW);
// The exit: the airship's boarding ramp, a gangway up to an open hatch in
// the gondola (96 x 144, the same frame as the family's teleporter).
Texture bakeMainframeExitBase(const Renderer& r, const Theme& t);

} // namespace gr
