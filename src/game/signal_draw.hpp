#pragma once

#include "render/renderer.hpp"

namespace gr
{

struct Art;
struct Projectile;
class World;

// Level 29's backdrop, painted behind the tiles (world_signal_draw.cpp):
// the live snow over the void (and, for one frame, its thanks), the Test
// Card's arena drawn on the dark (a great test-card circle, its grid, a
// band of bars and a step wedge), and at the far end the colour bars that
// bend into a sea-floor cable once the runner tunes in. World::draw calls
// it before the exit, the props and the tiles.
void drawSignalSet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame);

// A Tri-Tone Blaster shot, glowing in its tint; false for any other shot.
bool drawSignalShot(Renderer& r, const Art& art, const Projectile& pr, float cx, float cy, int frame);

} // namespace gr
