#pragma once

#include "frontend/game.hpp"
#include "frontend/touch_controls.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <functional>
#include <string>

namespace gr
{

// The window's renderer: accelerated with vsync, else whatever SDL has,
// drawing the 1280x720 picture letterboxed (the logical size). The rest of
// its setup is global (SDL_HINT_RENDER_SCALE_QUALITY, set before the first
// one) or set per draw (blend modes, scale), so this is all of it: made at
// startup and made again the same way after a device reset the backend does
// not survive (WindowRenderer::remake). Null if SDL has no renderer, with
// SDL_GetError saying why.
SDL_Renderer* makeWindowRenderer(SDL_Window* window);

// Whether an SDL_Renderer of this backend (SDL_RendererInfo::name) draws
// with an OpenGL or OpenGL ES context it made itself: "opengl", "opengles2",
// "opengles". Only for those can a GL context current after it was
// destroyed be an orphan of SDL's to delete (see WindowRenderer::remake).
bool backendUsesGl(const std::string& backend);

// The window's SDL_Renderer, owned by the windowed run: destroyed after
// every texture made with it (the run's members that hold textures come
// after it, so they are destroyed first), and swapped for a new one when a
// device reset left it dead (answerReset).
class WindowRenderer
{
public:
  // How a renderer for the window is made: makeWindowRenderer, or a stand-in
  // in the tests (one that fails, to reach the failure path).
  using Make = std::function<SDL_Renderer*(SDL_Window*)>;

  // Makes the first renderer at once (sdl() is null if that failed, and
  // error() says why).
  explicit WindowRenderer(SDL_Window* window, Make make = makeWindowRenderer);
  ~WindowRenderer();
  WindowRenderer(const WindowRenderer&) = delete;
  WindowRenderer& operator=(const WindowRenderer&) = delete;

  SDL_Renderer* sdl() const { return mSdl; }
  // SDL's own message from the last make that failed, taken right when it
  // failed (anything SDL does afterwards may set another); empty after one
  // that worked.
  const std::string& error() const { return mError; }

  // The old SDL_Renderer destroyed (every texture made with it must be gone
  // by then) and a new one made for the same window. On OpenGL ES (Android)
  // this also deletes the EGL context SDL made on its own when it restored
  // a lost one (see the .cpp). Null when none could be made: see error().
  SDL_Renderer* remake();

  // A render reset (resetActionOf) answered through recoverRender, with
  // `owners` (windowOwners) and, when the backend does not survive a device
  // reset, a new SDL_Renderer (remake) that `renderer` draws with from then
  // on. False when none could be made: `owners` stay dropped, error() says
  // why, and nothing may draw any more.
  bool answerReset(Renderer& renderer, ResetAction action, const TextureOwners& owners);

private:
  // mSdl made with mMake, and mError set from SDL_GetError if that failed.
  void make();

  SDL_Window* mWindow;
  Make mMake;
  SDL_Renderer* mSdl = nullptr;
  std::string mError;
};

// The windowed run's texture owners besides the renderer, for answerReset:
// the game's, then the touch controls' if there are any (null without).
// They are all of them: a holder left out keeps dead textures after a device
// reset, and Renderer::staleDraws only notices one drawn in a test (see
// there), so a new holder of textures must be added here or in
// Game::dropTextures. Inline, as TouchControls is no part of the core
// library: only the programs that use this link touch_controls.cpp.
inline TextureOwners windowOwners(Game& game, TouchControls* touch, Renderer& renderer)
{
  return TextureOwners{
    [&game, touch] {
      game.dropTextures();
      if (touch)
        touch->dropTextures();
    },
    [&game, touch, &renderer] {
      game.rebakeTextures();
      if (touch)
        touch->bakeTextures(renderer);
    }};
}

} // namespace gr
