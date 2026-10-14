#pragma once

#include "render/renderer.hpp"

namespace gr
{

struct Art;
struct Projectile;
class World;

// Level 42's stage, painted behind the props and the tiles
// (world_finale_draw.cpp), keyed to the map: each set wagon's painted flat
// in its slot (Neon City's skyline, the temple's stones, the station's
// viewport, the western's saloon, the toon's hills, the studio's cyc), lit
// while its trick runs, rolling in on the turntable, purple for a RERUN,
// dark in the wings; the stage door's spotlit pedestal; backstage's
// dressing-room wall and Lance's star door; the arena's main stage, its
// LED wall and the broadcast desk. World::draw calls it before the exit,
// the props and the tiles.
void drawFinaleSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha);

// An enemy shot in the finale: a Stunt Double's bolt in its runner's
// colour, the Encore Engine's beat as a gold note; false for any other shot.
bool drawFinaleShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame);

// The Encore's icon (a gold revolver cylinder with a chamber for every
// logged prototype, the live one lit in `sel`'s colour, a microphone grip),
// 64 x 64 at `scale`, its top-left at (x, y): the HUD's weapon panel and
// the weapon pickup.
void drawEncoreIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame, Color sel);

} // namespace gr
