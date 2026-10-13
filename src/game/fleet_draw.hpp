#pragma once

#include "render/renderer.hpp"

namespace gr
{

struct Art;
struct Projectile;
class World;

// Level 34's ships, painted behind the tiles (world_fleet_draw.cpp): each
// ship's balloon and rigging, its hull with portholes, brass name board and
// stern propeller, its deck (the moving ones; the steady ones' decks are
// tiles) and deck rails, the Looter's rotors and flag, the bomb bay's
// inside; Human Cannonball's circus bunting and banner. World::draw calls it
// before the exit, the props and the tiles, so the bomb bay's walls and the
// steady decks are drawn over it.
void drawFleetSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha);

// The Grapple Gun's hook in flight (a three-pronged grappling hook) or a Sky
// Pirate's pistol ball; false for any other shot.
bool drawFleetShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame);

// The Grapple Gun's icon (a brass line gun with a rope drum and the hook at
// its muzzle), 64 x 64 at `scale`, its top-left at (x, y): the HUD's weapon
// panel and the weapon pickup.
void drawGrappleIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame);

} // namespace gr
