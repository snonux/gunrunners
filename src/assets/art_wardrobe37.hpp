#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

namespace gr
{

// Level 37, Wardrobe (theme key wardrobe, look "wardrobe",
// art_wardrobe37.cpp): MaxTV's costume department after hours, lit by warm
// bulbs. The masses are dark mahogany wainscot, a raised panel to a block
// with a gilt bead round it, now and then a brass grille or a wardrobe
// door's keyhole; where you stand, a crimson carpet runner with a gold
// braid edge. The one-way ledges are the top bars of the makeup hall's
// gilt mirror frames, bulbs set in them (the cutting tables over the
// needle pit are drawn by the level instead). The needles are a pincushion
// strip bristling with sewing needles and coloured pins; the ladder a
// wooden library ladder on brass rails. Behind: a dark ceiling, cream
// walls hung with swagged crimson velvet drapes and gold fringe, wall
// sconces, and, nearer, rows of costume racks with hat boxes. The rooms'
// own furniture (mirrors, sewing machines, the archive's shelves, the
// vault) is drawn by the level (world_wardrobe_draw.cpp).
bool isWardrobe(const Theme& t);
Texture bakeWardrobeSolid(const Renderer& r, const Theme& t, int variant);
Texture bakeWardrobeSolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeWardrobePlatform(const Renderer& r, const Theme& t);
Texture bakeWardrobeLadder(const Renderer& r, const Theme& t);
Texture bakeWardrobeSpikes(const Renderer& r, const Theme& t);
Texture bakeWardrobeSky(const Renderer& r, const Theme& t);
Texture bakeWardrobeFar(const Renderer& r, const Theme& t, int layerW);
Texture bakeWardrobeNear(const Renderer& r, const Theme& t, int layerW);
// The exit: the stage door, ajar, stage light spilling out round it, a
// gold star on it and a lamp over it (96 x 144, the same frame as the
// family's teleporter).
Texture bakeWardrobeExitBase(const Renderer& r, const Theme& t);

} // namespace gr
