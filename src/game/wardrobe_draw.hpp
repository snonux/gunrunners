#pragma once

#include "render/renderer.hpp"

namespace gr
{

struct Art;
class World;

// Level 37's set, painted behind the props and the tiles
// (world_wardrobe_draw.cpp): each room's furniture against the wall, keyed
// to the map. The fitting room's stall walls and long mirrors and its coat
// stand; the costume racks' long rails of costumes and hat shelves (the
// SIZE 42 jacket among them); the makeup hall's bulb-ringed mirrors under
// their gilt frames, its counter of wig heads and powder; the sewing room's
// machines, fabric bolts, thread rack and dress form, the needle pit's
// walls; the archive's tall shelves of labelled boxes and the vault's steel
// walls, its round door and the triple booth round the bonus static; the
// rooms' hanging signs and the battens the ceiling needles hang from. In
// the Totem bonus, a stage: footlights and a painted flat. World::draw
// calls it before the exit, the props and the tiles.
void drawWardrobeSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha);

// The runner as Level 37 dresses them (World::drawPlayer asks before it
// draws the runner's sprite `tex` at (x, y) with `o`): in the Gorgazilla
// suit, sitting at the bulb mirror, or as the OUT OF ORDER booth's
// rubber-hose cartoon (inked, bouncing). True if it drew them.
bool drawWardrobeRunner(Renderer& r, const World& w, const Art& art, const Texture& tex, float x, float y, const DrawOpts& o,
  int frame);

// The Decoy Launcher's icon (a stubby brass launcher with a little
// mannequin's head peeping out of its bell), 64 x 64 at `scale`, its
// top-left at (x, y): the HUD's weapon panel and the weapon pickup.
void drawDecoyIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame);

} // namespace gr
