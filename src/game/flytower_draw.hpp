#pragma once

#include "render/renderer.hpp"

namespace gr
{

struct Art;
struct Projectile;
class World;

// Level 40's tower, painted behind the props and the tiles
// (world_flytower_draw.cpp), keyed to the map: the stage house's sooty back
// wall and its counterweight arbors, battens with black borders hanging in
// the haze, moving-head beams sweeping up out of the show; the roof's
// trusses and the loft blocks on the grid deck; the hanger rods and
// handrails of every catwalk; the scorched steel behind the pyro wall; the
// wings at the foot of the tower (the black legs, the lit cyc beyond, the
// stage manager's prompt desk). In Trigger Happy, the scaffold's poles and
// braces. World::draw calls it before the exit, the props and the tiles.
void drawFlyTowerSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha);

// A Rigger's shot: a red-hot dart off his line launcher, its line trailing;
// false for any other shot.
bool drawFlyTowerShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame);

// The Pyro Rig's icon (a flame wand on an orange fuel tank, its hose, the
// pilot flame flickering at the nozzle), 64 x 64 at `scale`, its top-left
// at (x, y): the HUD's weapon panel and the weapon pickup.
void drawPyroRigIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame);

} // namespace gr
