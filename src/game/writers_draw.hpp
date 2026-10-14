#pragma once

#include "render/renderer.hpp"

namespace gr
{

struct Art;
struct Projectile;
class World;

// Level 38's set, painted behind the props and the tiles
// (world_writers_draw.cpp), keyed to the map: the desks with their paper
// stacks, mugs and lamps, the doorway's coat hooks and the sticky note,
// the FLOOR pit's dark, the corkboard with its index cards, the filing
// cabinet's shell and its snack drawer, the big typewriter behind its keys
// with the paper rising out of it, the final draft's page with PAGE 42 in
// its corner and the line THE RUNNERS EXIT by the door, the top shelf.
// In the Breakout bonus, a pinboard and the show's motto. World::draw
// calls it before the exit, the props and the tiles.
void drawWritersSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha);

// The Red Pen's shot (a streak of red ink behind a felt nib, drips flying
// off it) or the sketch guard's (a pencil lead spinning); false for any
// other shot.
bool drawWritersShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame);

// The Red Pen's icon (a fat red marker, its cap clipped on the back, a red
// scribble from its nib), 64 x 64 at `scale`, its top-left at (x, y): the
// HUD's weapon panel and the weapon pickup.
void drawRedPenIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame);

} // namespace gr
