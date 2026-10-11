#pragma once

#include "render/renderer.hpp"

namespace gr
{

struct Art;
class World;

// Level 32's set, painted behind the tiles (world_dish_draw.cpp): the
// satellite crossing the sky past its three markers, the dishes' pedestals
// and yokes, their white bowls following the rims and feed arms through
// each turn (shaking on the groan), the catwalks on the rims, the arms'
// booms and feed horns, the struts swinging in and out, the receiver
// cabin's inside and its stilts, the hangar's and the bunker's back walls,
// the bunker's tracking screen, the uplink tower's antennas; Heatstroke's
// fixed dishes and mast. World::draw calls it before the props and tiles.
void drawDishSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha);

// The Signal Jammer's icon (a handheld jammer with a little dish and a
// grille, its LED blinking), 64 x 64 at `scale`, its top-left at (x, y):
// the HUD's weapon panel and the weapon pickup.
void drawJammerIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame);

// Heatstroke: the screen's sideways wobble (screen px) in the frames before
// left and right swap; 0 otherwise.
float dishWobble(const World& w, int frame);

} // namespace gr
