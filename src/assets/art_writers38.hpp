#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

namespace gr
{

// Level 38, Writers' Room (theme key writers_room, look "writers_room",
// art_writers38.cpp): the show's writers' room at night, lit by desk
// lamps. The masses are pencil-grey plaster walls, faint graph ruling and a
// skirting board, now and then a pencilled tally or a coffee ring; where
// you stand, worn blue-grey office carpet on a wooden edge. The one-way
// ledges are script lines: a strip of typed script page (the sticky note
// and the FLOOR plank are drawn by the level instead). The spikes are a
// row of sharpened pencils standing points up in a pencil tray; the ladder
// two long yellow pencils with wooden rulers for rungs. Behind: a dark
// ceiling of acoustic tiles and strip lights, the room's grey back wall
// with tall night windows over the city (blinds half down), whiteboards
// and framed show posters; nearer, bookcases of script binders, filing
// cabinets, a water cooler and lamps glowing. The furniture (desks, the
// corkboard, the typewriter, the final draft) is drawn by the level
// (world_writers_draw.cpp).
bool isWritersRoom(const Theme& t);
Texture bakeWritersSolid(const Renderer& r, const Theme& t, int variant);
Texture bakeWritersSolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeWritersPlatform(const Renderer& r, const Theme& t);
Texture bakeWritersLadder(const Renderer& r, const Theme& t);
Texture bakeWritersSpikes(const Renderer& r, const Theme& t);
Texture bakeWritersSky(const Renderer& r, const Theme& t);
Texture bakeWritersFar(const Renderer& r, const Theme& t, int layerW);
Texture bakeWritersNear(const Renderer& r, const Theme& t, int layerW);
// The exit: the writers' room's back door, open, warm corridor light
// spilling out, a lit EXIT sign over it and THE END on its frosted glass
// (96 x 144, the same frame as the family's teleporter).
Texture bakeWritersExitBase(const Renderer& r, const Theme& t);

} // namespace gr
