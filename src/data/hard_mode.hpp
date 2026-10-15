#pragma once

#include "data/level.hpp"

namespace gr
{

// HARD (chosen on runner select, kept in the profile and in savegames):
// every campaign level gets more enemies. Each of the level's own enemies
// whose kind fights on its own (patrollers, guards, flyers, turrets) gets a
// twin of the same kind a few blocks along the same floor (or the same
// stretch of air, in the same set pieces as its original). Where that
// leaves fewer than about half again the level's enemies, newcomers (the
// level's own patroller, else one from its episode) take open floors along
// the gems. None goes near the start, a checkpoint or the exit, and at
// most two share 24 blocks of a floor. Bonus levels stay as they are. The
// extras go after the level's own entities, so everything the level places
// keeps its order in both modes. Returns how many it added.
// GR_HARD_DEBUG=1 prints why an enemy got no twin.
int addHardModeEnemies(Level& level);

} // namespace gr
