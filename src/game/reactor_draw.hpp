#pragma once

namespace gr
{

// Level 20's drawing (world_reactor_draw.cpp) runs from hooks without the
// render alpha; World::draw sets it here first (0..1: how far the rendered
// frame is between the previous and the current logic frame), so the rings,
// drops, arcs and the Bracer's shield move smoothly at 60 Hz.
float& reactorRenderAlpha();

} // namespace gr
