#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

namespace gr
{

// Level 42, The Live Finale (theme key main_stage, look "main_stage",
// art_finale42.cpp): the season's last live show on the main stage,
// midnight blue, gold trim and ON AIR red. The masses are LED risers, deep
// navy panels with their pixel grid faintly showing and a few pixels lit
// gold. Where you stand, the gloss-black stage floor with the lights
// mirrored in it, a gold edge and a blue LED strip glowing under the
// nosing. The one-way ledges are chrome truss decks with a blue light line
// along the front. The ladders are chrome with lit rungs; the spikes a row
// of pyro gerbs throwing gold sparks. Behind: the arena's dark with beams
// climbing out of the show; a curved LED wall with THE LIVE FINALE and a
// big 42 on it, the audience's tiers below it lit by phones and glow
// sticks; nearer, an arched lighting rig with moving heads and gold
// streamers. The level draws its own set over that (world_finale_draw.cpp):
// the set wagons, the studio monitors, the ON AIR door, Lance's arena.
bool isFinale(const Theme& t);
Texture bakeFinaleSolid(const Renderer& r, const Theme& t, int variant);
Texture bakeFinaleSolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeFinalePlatform(const Renderer& r, const Theme& t);
Texture bakeFinaleLadder(const Renderer& r, const Theme& t);
Texture bakeFinaleSpikes(const Renderer& r, const Theme& t);
Texture bakeFinaleSky(const Renderer& r, const Theme& t);
Texture bakeFinaleFar(const Renderer& r, const Theme& t, int layerW);
Texture bakeFinaleNear(const Renderer& r, const Theme& t, int layerW);
// The exit: the master switch, a big red knife switch on a steel panel,
// MASTER over it (96 x 144, the same frame as the family's teleporter).
Texture bakeFinaleExitBase(const Renderer& r, const Theme& t);

} // namespace gr
