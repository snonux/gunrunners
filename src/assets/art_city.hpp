#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

namespace gr
{

// Level 26, Kaiju Tonight (theme look "city", art_city.cpp): a tabletop
// model city at night on a 1950s monster-movie soundstage. The masses are
// the set's plywood base, the grain showing, a screw and a knot here and
// there; where you stand, the model street (a sidewalk, the
// asphalt with its painted centre line, the curb and the base's painted
// edge); one-way floors are tread-plate steel planks; climbables a
// galvanized manhole ladder. Behind: a painted Technicolor night with a
// brushy moon, clouds and stars and the glow of the burning city low down;
// far off, cardboard skyline cut-outs with lit windows (their braces and the
// seam where two backdrop canvases meet showing), and close by the studio's
// lighting truss, its lamps, cables and a bank of smoke-machine haze.
bool isCity(const Theme& t);
Texture bakeCitySolid(const Renderer& r, const Theme& t, int variant);
Texture bakeCitySolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeCityPlatform(const Renderer& r, const Theme& t);
Texture bakeCityLadder(const Renderer& r, const Theme& t);
Texture bakeCitySky(const Renderer& r, const Theme& t);
Texture bakeCityFar(const Renderer& r, const Theme& t, int layerW);
Texture bakeCityNear(const Renderer& r, const Theme& t, int layerW);

} // namespace gr
