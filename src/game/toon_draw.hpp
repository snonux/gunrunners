#pragma once

#include "render/renderer.hpp"

namespace gr
{

struct Art;
class World;

// Level 28's set, painted behind the tiles (world_toon_draw.cpp): the
// backstage walls behind enclosed rooms (the cartoon studio, the basement
// road, Buddy Boom's dressing room), the big top's striped canvas, its
// bunting and spotlights, and the Inkwell's ruled page. World::draw calls
// it before the exit, the props and the tiles.
void drawToonSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame);

// True for the blocks Level 28 paints itself instead of the tile art: a
// piano standing as a step, the Inkwell's inked lines.
bool toonDrawsBlock(const World& w, int tx, int ty);

} // namespace gr
