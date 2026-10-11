#pragma once

#include "render/renderer.hpp"

namespace gr
{

struct Art;
struct Projectile;
class World;

// Level 31's backdrop, painted behind the tiles (world_storm_draw.cpp): the
// sheet lightning in the clouds and the distant bolts, the rain far off,
// the sea's moving whitecaps, the surf rolling in at the beach, the ship on
// the horizon answering the lamp, the back walls of the fisherman's hut,
// the sea caves (their arches open to the storm), the smugglers' room and
// the lighthouse (its windows, the lamp room's glass), the lamp's turning
// beam, and Bungee's lighthouse. World::draw calls it before the exit, the
// props and the tiles.
void drawStormSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame);

// A Tesla Rod in flight: the copper rod and its sparks; false for any other
// shot.
bool drawStormShot(Renderer& r, const Art& art, const Projectile& pr, float cx, float cy, int frame);

// The Tesla Rod's icon (a copper rod, its glass tip glowing, sparks), 64 x
// 64 at `scale`, its top-left at (x, y): the HUD's weapon panel and the
// weapon pickup.
void drawTeslaIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame);

} // namespace gr
