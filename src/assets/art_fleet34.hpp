#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"
#include "render/vector.hpp"

namespace gr
{

// Level 34, Zeppelin Relay (theme key sunset_airships, look "fleet",
// art_fleet34.cpp): a fleet of airships at sunset over a sea of clouds.
// The masses are an airship's hull: varnished mahogany strakes with brass
// rivet strips, now and then a brass porthole or a louvred vent. Where you
// stand on them, a brass-capped gunwale. The one-way floors are honey teak
// deck planking with a brass rub rail along its edge, the ladders rope
// ladders with wooden rungs. Behind: a sky going from lavender overhead to
// peach and gold at the horizon, the sun low and huge, thin bands of stratus
// lit pink; far off, a level of cloud tops with the rest of the fleet as
// small silhouettes; near, the cumulus banks of the cloud sea. The ships
// themselves (balloons, rigging, hulls, propellers, names), the rings and
// everything else in the fleet are drawn live by the level
// (world_fleet_draw.cpp). The Human Cannonball bonus shares the look: its
// floor is a carrier's flight deck.
bool isZeppelinFleet(const Theme& t);
Texture bakeFleetSolid(const Renderer& r, const Theme& t, int variant);
Texture bakeFleetSolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeFleetPlatform(const Renderer& r, const Theme& t);
Texture bakeFleetLadder(const Renderer& r, const Theme& t);
Texture bakeFleetSky(const Renderer& r, const Theme& t);
Texture bakeFleetFar(const Renderer& r, const Theme& t, int layerW);
Texture bakeFleetNear(const Renderer& r, const Theme& t, int layerW);
// The exit: the signal ship's radio shack, its door open on a warm light
// and its relay aerial over the roof (96 x 144, the same frame as the
// family's teleporter).
Texture bakeFleetExitBase(const Renderer& r, const Theme& t);
// The hull's planking (the masses' look, without portholes or vents) as a
// repeating cairo pattern, its tile's top-left at the user-space origin,
// so the ships drawn live match the hull blocks. The caller destroys it.
cairo_pattern_t* fleetHullPattern();

} // namespace gr
