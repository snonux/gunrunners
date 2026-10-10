// Level 22, Dry Gulch: drawing (fuses, barrels, the drawbridge, metal,
// prizes, posters, the piano, the tonic's label, the duel's bell and the
// High Noon HUD). See world_west.cpp for the logic.

#include "game/world.hpp"

namespace gr
{

void World::drawWestBack(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)r;
  (void)camX;
  (void)camY;
  (void)frame;
  (void)alpha;
}

void World::drawWestFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  (void)r;
  (void)camX;
  (void)camY;
  (void)frame;
  (void)alpha;
}

void World::drawWestHud(Renderer& r, int frame) const
{
  (void)r;
  (void)frame;
}

} // namespace gr
