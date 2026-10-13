#pragma once

#include "render/renderer.hpp"

namespace gr
{

struct Art;
struct Projectile;
class World;

// Level 35's set, painted behind the tiles (world_tower_draw.cpp): the dawn
// cloud sea far below, wisps of cloud drifting past, the far side of the
// mast's lattice seen through the near side (turning the other way), the
// near side's red and white lattice with its dishes, panel antennas and
// blinking warning lights, the deck's guard rail, the spire on the top deck
// with its big dish and the alien's flag, and the crawl space's walls under
// the base. Everything on the tower is keyed to the tower's own x (its
// turn taken off the camera), so nothing jumps when it turns. World::draw
// calls it before the exit, the props and the tiles.
void drawTowerSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha);

// A Satellite Swarm drone's shot (a cyan pellet with a trail), a Relay
// Drone's (a red ring of signal) or the Titan's lattice shot (a white-hot
// bolt crackling along the deck); false for any other shot.
bool drawTowerShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame);

// The Satellite Swarm's icon (three little satellites round a handheld
// uplink, their panels glinting), 64 x 64 at `scale`, its top-left at
// (x, y): the HUD's weapon panel and the weapon pickup.
void drawSwarmIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame);

} // namespace gr
