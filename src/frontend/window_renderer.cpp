#include "frontend/window_renderer.hpp"

#include <utility>

namespace gr
{

SDL_Renderer* makeWindowRenderer(SDL_Window* window)
{
  SDL_Renderer* r = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  // Vsync keeps the picture free of tearing; the fallback renderer has
  // none. Either way the loop paces itself to the 60 Hz ticks: see
  // runFrames in main.cpp.
  if (!r)
    r = SDL_CreateRenderer(window, -1, 0);
  if (r)
    SDL_RenderSetLogicalSize(r, kScreenW, kScreenH);
  return r;
}

bool backendUsesGl(const std::string& backend)
{
  return backend.rfind("opengl", 0) == 0;
}

namespace
{

// The backend of `r` (SDL_RendererInfo::name), empty without one.
std::string backendOf(SDL_Renderer* r)
{
  SDL_RendererInfo info{};
  if (!r || SDL_GetRendererInfo(r, &info) != 0 || !info.name)
    return {};
  return info.name;
}

} // namespace

WindowRenderer::WindowRenderer(SDL_Window* window, Make make) : mWindow(window), mMake(std::move(make))
{
  this->make();
}

WindowRenderer::~WindowRenderer()
{
  if (mSdl)
    SDL_DestroyRenderer(mSdl);
}

void WindowRenderer::make()
{
  mSdl = mMake ? mMake(mWindow) : nullptr;
  // Taken now: what runs after a failure (the run winding down, textures
  // let go) can set SDL errors of its own, such as "Parameter 'renderer' is
  // invalid", which say nothing about why there is no renderer.
  if (mSdl)
    mError.clear();
  else
    mError = mMake ? SDL_GetError() : "no way to make a renderer";
}

SDL_Renderer* WindowRenderer::remake()
{
  // On Android, SDL 2.32.10's android_egl_context_restore (on the return
  // from the background) finds the renderer's EGL context lost, makes a new
  // one with SDL_GL_CreateContext, makes it current and sends
  // SDL_RENDER_DEVICE_RESET; but it keeps that context in the window's data
  // only, where the next android_egl_context_backup overwrites it unread.
  // No one ever deletes it, so every context loss would leak one. It is
  // the context current once the old renderer is gone: the renderer only
  // tried, and failed, to make its own dead one current, and destroying it
  // deletes that one, not the current one. Once the new renderer made its
  // own context current, the orphan is deleted.
  //
  // Read only after SDL_DestroyRenderer: before it, on desktop OpenGL, the
  // current context is the old renderer's own, which SDL_DestroyRenderer
  // deletes (making none current); deleting it here too would delete it
  // twice. And only for a GL backend: under any other (Direct3D, Metal, the
  // software renderer) a current GL context is none of this renderer's
  // business; it may be one SDL uses for something else, such as the
  // window surface a software renderer draws to.
  const bool gl = backendUsesGl(backendOf(mSdl));
  if (mSdl)
    SDL_DestroyRenderer(mSdl);
  mSdl = nullptr;
  SDL_GLContext orphan = gl ? SDL_GL_GetCurrentContext() : nullptr;
  make();
  // Not when no renderer was made (the run ends anyway), nor when the
  // context is still current, as the new renderer would then draw with it.
  if (mSdl && orphan && orphan != SDL_GL_GetCurrentContext())
    SDL_GL_DeleteContext(orphan);
  return mSdl;
}

bool WindowRenderer::answerReset(Renderer& renderer, ResetAction action, const TextureOwners& owners)
{
  return recoverRender(renderer, action, owners, [this] { return remake(); });
}

} // namespace gr
