#pragma once

// Renderer internals exposed for their unit tests; nothing else uses them.

#include <SDL.h>

namespace gr::detail
{

// Whether SDL2's API runs on top of SDL3 (sdl2-compat), from SDL_GetVersion
// and SDL_GetRevision. The two keep the viewport and a logical size
// differently (see renderer.cpp's pictureOf).
bool sdl2Compat(const SDL_version& version, const char* revision);

} // namespace gr::detail
