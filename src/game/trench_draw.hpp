#pragma once

#include "render/renderer.hpp"

namespace gr
{

struct Art;
struct Projectile;
class World;

// Level 30's backdrop, painted behind the tiles (world_trench_draw.cpp): the
// water darkening with depth, the sun's shafts and the drifting motes, the
// abyss and the whale crossing it, the sunken ship's hold and the cable
// station's walls, the porthole, and the glowing fiber cable along the
// route with its light pulses. World::draw calls it before the exit, the
// props and the tiles.
void drawTrenchSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame);

// A Harpoon Line shot: the harpoon and its line paying out behind it; false
// for any other shot.
bool drawTrenchShot(Renderer& r, const Art& art, const Projectile& pr, float cx, float cy, int frame);

// The Harpoon Line's icon in the HUD's weapon panel (a speargun), drawn
// where the item icon would go at (x, y); always true.
bool drawHarpoonIcon(Renderer& r, const Art& art, float x, float y);

} // namespace gr
