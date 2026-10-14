#pragma once

#include "render/renderer.hpp"

namespace gr
{

struct Art;
struct Projectile;
class World;

// Level 39's rooms, painted behind the props and the tiles
// (world_gallery_draw.cpp), keyed to the map: the edit suite's bank of
// edit monitors and the producer's chair; the hallway's studio doors and
// posters; studio B's lit cyclorama, the news set behind the riser and
// the cameras on their pedestals; the cable corridor's wall trays and the
// catwalk's railings and legs; the panning room's jib; the tape library's
// shelving; the main gallery's vision mixer and preview monitors, the
// walls of screens in G2 and G3 (one of them showing the programme feed
// live); the forgotten studio's dusty sitcom set; the virus room's
// corrupted screen. World::draw calls it before the exit, the props and
// the tiles.
void drawGallerySet(Renderer& r, const World& w, const Art& art, float camX, float camY, int frame, float alpha);

// The Echo Mic's shot (rings of sound going out ahead of a bright core;
// an echo fainter and bluer, its rings dotted) or a crew member's shot on
// the Floor Manager's cue (a spinning cue card); false for any other shot.
bool drawGalleryShot(Renderer& r, const World& w, const Art& art, const Projectile& pr, float cx, float cy, int frame);

// The Echo Mic's icon (a silver handheld microphone, a red ON switch, rings
// of sound and a fainter echo of them), 64 x 64 at `scale`, its top-left at
// (x, y): the HUD's weapon panel and the weapon pickup.
void drawEchoMicIcon(Renderer& r, const Art& art, float x, float y, float scale, int frame);

} // namespace gr
