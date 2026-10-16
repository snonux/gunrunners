#pragma once

// Renderer internals exposed for their unit tests; nothing else uses them.

#include <SDL.h>
#include <cairo.h>

#include <cstddef>
#include <string>

namespace gr::detail
{

// Whether SDL2's API runs on top of SDL3 (sdl2-compat), from SDL_GetVersion
// and SDL_GetRevision. The two keep the viewport and a logical size
// differently (see renderer.cpp's pictureOf).
bool sdl2Compat(const SDL_version& version, const char* revision);

// Where the characters drawTextPrefix strokes to find a prefix's ink start
// in s[0, bytes) (bytes: whole UTF-8 characters, at most s.size()): back
// over the blank characters that end it (a reveal step that ends on a
// space), over the last inked character, and on over whole characters
// until the window covers half an em of advance up to that character's end
// (zero-width ones cover nothing), or to 0. 0 for a blank prefix. `cr` has
// the text's font and size set. The cut it serves is cached per prefix.
std::size_t inkWindow(cairo_t* cr, const std::string& s, std::size_t bytes);

} // namespace gr::detail
