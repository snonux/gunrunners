#pragma once

#include "render/renderer.hpp"

namespace gr
{

struct Art;
struct Projectile;
class World;

// Level 41's studio, painted behind the tiles (world_sweeps_draw.cpp),
// keyed to the map: the back rows of the audience climbing into the dark
// behind the stands, the black understage under the stage floor (the
// orchestra pit, the audience pit, the hollow under the stands). World::draw
// calls it before the props and the tiles.
void drawSweepsSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha);

// An Audience Plant's tomato, an Applause Cannon burst, or in Paintball the
// runner's paintballs; false for any other shot.
bool drawSweepsShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame);

// The Applause Cannon's icon (a brass bell-mouthed cannon with a foam hand
// clapping on top of it, gold stars popping out), 64 x 64 at `scale`, its
// top-left at (x, y): the HUD's weapon panel and the weapon pickup.
void drawApplauseCannonIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame);

} // namespace gr
