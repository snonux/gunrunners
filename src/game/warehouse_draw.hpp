#pragma once

#include "render/renderer.hpp"

namespace gr
{

struct Art;
struct Projectile;
class World;

// Level 36's set, painted behind the tiles (world_warehouse_draw.cpp): the
// loading dock's back wall with its half-open roll-up door and the studio
// lot outside, the pallet racks' uprights behind the aisle shelves, the
// black curtains of the prop maze, the merch mezzanine's sign, the crane
// yard's gantry legs and its pit wall, the freight lift shafts, the
// dressing-room landing, and the props of Levels 1-35 on their shelves.
// World::draw calls it before the exit, the props and the tiles.
void drawWarehouseSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha);

// The Magnet Gun's push pulse (rings of field rolling out) or a
// Stagehand's shot (a roll of gaffer tape, spinning); false for any other
// shot.
bool drawWarehouseShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame);

// The Magnet Gun's icon (a red horseshoe magnet on a pistol grip, its coil
// and a spark between the poles), 64 x 64 at `scale`, its top-left at
// (x, y): the HUD's weapon panel and the weapon pickup.
void drawMagnetIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame);

} // namespace gr
