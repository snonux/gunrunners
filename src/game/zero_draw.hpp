#pragma once

#include "render/renderer.hpp"

namespace gr
{

struct Art;
struct Projectile;

// Level 21's drawing hooks that run outside drawZeroBack/Front
// (world_zero_draw.cpp). An Echo's shot (pr.echo, cyan) or a Phase Rifle
// shot (a violet streak, hollow while it is inside a wall), centred at
// (cx, cy) on screen: true if it was one of those and is drawn.
bool drawZeroShot(Renderer& r, const Art& art, const Projectile& pr, float cx, float cy, int frame);
// The Wireframe bonus's debug view behind everything: black with a faint
// grid that scrolls with the camera.
void drawWireframeBackdrop(Renderer& r, float camX, float camY, int frame);

} // namespace gr
