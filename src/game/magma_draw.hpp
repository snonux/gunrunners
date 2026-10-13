#pragma once

#include "render/renderer.hpp"

namespace gr
{

struct Art;
struct Projectile;
class World;

// Level 33's set, painted behind the tiles (world_magma_draw.cpp): the near
// layer of hanging cables and pipes repeating up the climb, the glow of the
// lava welling up behind everything, the back aisles of server racks on the
// server floors, the cooling shaft's pipes and stencils, the coolant
// stations at the checkpoints, the crater's night sky with its ash and
// stars, the mainframe on the crater floor, the mooring mast and the
// airship over it; Overclock's racks and speed lines. World::draw calls it
// before the exit, the props and the tiles.
void drawMagmaSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha);

// A Heat Sink fireball (a big ball of flame, bigger the more it holds,
// trailing fire) or a Lava Daemon's lobbed fireball (a small one with a
// spark trail); false for any other shot.
bool drawMagmaShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame);

// The Heat Sink's icon (a finned heat sink on a grip with an intake funnel,
// its core glowing), 64 x 64 at `scale`, its top-left at (x, y): the HUD's
// weapon panel and the weapon pickup.
void drawSinkIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame);

} // namespace gr
