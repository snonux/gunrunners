#pragma once

#include "render/color.hpp"

#include <SDL.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace gr
{

constexpr int kScreenW = 1280;
constexpr int kScreenH = 720;

enum class Blend
{
  Alpha,
  Add,
};

// GPU (or SDL software) texture with an anchor: the point inside the texture
// that is placed at the draw position. Lets sprites carry margins for glow,
// hair or weapons without the game logic caring. It also carries the device
// generation of the Renderer that made it (Renderer::generation), so a draw
// of a texture that outlived a device reset can be counted.
class Texture
{
public:
  Texture() = default;
  Texture(SDL_Texture* tex, int w, int h, float anchorX, float anchorY, std::uint32_t generation = 0);
  ~Texture();
  Texture(Texture&& other) noexcept;
  Texture& operator=(Texture&& other) noexcept;
  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;

  SDL_Texture* get() const { return mTex; }
  int w() const { return mW; }
  int h() const { return mH; }
  float anchorX() const { return mAnchorX; }
  float anchorY() const { return mAnchorY; }
  std::uint32_t generation() const { return mGeneration; }
  explicit operator bool() const { return mTex != nullptr; }

private:
  SDL_Texture* mTex = nullptr;
  int mW = 0;
  int mH = 0;
  float mAnchorX = 0.0f;
  float mAnchorY = 0.0f;
  std::uint32_t mGeneration = 0;
};

struct DrawOpts
{
  float alpha = 1.0f;
  Color tint = rgb(255, 255, 255);
  Blend blend = Blend::Alpha;
  float scale = 1.0f;
  float angle = 0.0f; // degrees, clockwise, around the anchor
  bool cull = true;   // skip if outside 1280x720 (off for the letterbox bars)
};

struct TextStyle
{
  float size = 24.0f;
  Color color = rgb(255, 255, 255);
  Color outline = 0;
  bool italic = false;
};

enum class Align
{
  Left,
  Center,
  Right,
};

// What the GPU lost, by the SDL event that says so.
//   - Targets (SDL_RENDER_TARGETS_RESET, Direct3D 9 after a lost device):
//     SDL made the render-target textures again, empty; every other texture
//     is kept (re-uploaded by SDL from its own copy). The SDL_Renderer lives
//     on.
//   - Device (SDL_RENDER_DEVICE_RESET): every SDL_Texture is left a dead
//     handle, safe to destroy but drawing nothing (on Direct3D 11 a copy
//     from one can even crash); each must be made again. Whether the
//     SDL_Renderer survives depends on the backend (resetActionOf):
//     Direct3D 11 and 12 make a new device inside the same SDL_Renderer
//     (D3D11_HandleDeviceLost), so it draws on once the textures are made
//     again. On Android (OpenGL ES 2) the event follows an EGL context loss:
//     SDL 2.32.10's android_egl_context_restore makes a new context but
//     keeps it in the window's data only, while the renderer still holds the
//     dead one, which GLES2_ActivateRenderer then fails to make current on
//     every call. That SDL_Renderer never draws again and must be made anew.
// Not handled, as nothing reports them to this code: a WebGL context loss in
// a browser (Emscripten's SDL2 sends no event and restores no context; the
// page stays black until reloaded), and SDL3's SDL_EVENT_RENDER_DEVICE_LOST
// (a renderer gone for good) under sdl2-compat, which has no SDL2 event of
// its own to arrive as.
enum class RenderReset
{
  None,
  Targets,
  Device,
};
// None for every other event.
RenderReset renderResetOf(const SDL_Event& ev);

// What the window code does about an event (see recoverRender):
//   - None: nothing, it is no render reset.
//   - Targets: the renderer drops its capture textures (Renderer::recover).
//   - InPlace: a device reset the SDL_Renderer survives; every texture is
//     dropped and made again with the same SDL_Renderer.
//   - Recreate: a device reset it does not survive; every texture is
//     dropped, the SDL_Renderer destroyed and made again, the textures made
//     again with the new one.
enum class ResetAction
{
  None,
  Targets,
  InPlace,
  Recreate,
};
// Whether an SDL_Renderer of this backend (SDL_RendererInfo::name) still
// draws after SDL_RENDER_DEVICE_RESET: Direct3D 11 and 12 only. Every other
// backend counts as lost, OpenGL ES 2 (Android) as shown above and OpenGL or
// one not known here to be safe: making a renderer again is always safe.
bool deviceSurvivesReset(const std::string& backend);
// The action for `ev` on a renderer of `backend` (Renderer::backend).
ResetAction resetActionOf(const SDL_Event& ev, const std::string& backend);

// Thin layer over SDL_Renderer. In a window it is GPU accelerated; for
// headless recording the same calls go to SDL's software renderer.
class Renderer
{
public:
  explicit Renderer(SDL_Renderer* renderer) : mRenderer(renderer) {}

  // The SDL_Renderer drawn with. Not to be kept past the draw at hand: a
  // device reset the backend does not survive makes a new one
  // (recoverRender, rebind).
  SDL_Renderer* sdl() const { return mRenderer; }
  // Its backend, as SDL_GetRendererInfo names it ("opengles2", "direct3d11",
  // "software", ...); empty without a renderer. For resetActionOf.
  std::string backend() const;

  // Pixels are Cairo ARGB32 (premultiplied); mirrorX flips horizontally.
  Texture createTexture(
    int w,
    int h,
    const unsigned char* data,
    int stride,
    float anchorX,
    float anchorY,
    bool mirrorX = false) const;

  // Starts a frame: counts it for the text cache and, once the cache holds
  // more than kTextCacheMax strings, evicts the least recently drawn ones.
  void beginFrame();
  void clear(Color c);
  void draw(const Texture& t, float x, float y, const DrawOpts& o = {});
  // draw() limited to the texture's leftmost srcW columns (a source rect;
  // the whole texture for srcW >= its width, nothing for srcW <= 0). With
  // linear filtering at a scale other than 1 the GPU blends in a fraction
  // of an output pixel from the column past the cut.
  void drawRegion(const Texture& t, float x, float y, int srcW, const DrawOpts& o = {});
  void fillRect(float x, float y, float w, float h, Color c, Blend b = Blend::Alpha);
  // A line of any angle, `width` px thick (laser sights, cables).
  void drawLine(float x0, float y0, float x1, float y1, float width, Color c, Blend b = Blend::Alpha);

  // A cols x rows grid of alpha values (0..1) in colour c, stretched over
  // the rectangle at (x, y) with smooth filtering between cell centres: soft
  // darkness and light pools without visible squares.
  void drawAlphaGrid(const std::vector<float>& alpha, int cols, int rows, float x, float y, float cellW, float cellH,
    Color c);

  // `t` stretched over the rectangle at (x, y), faded out to the oval inside
  // it: solid where u² + v² <= `solid` (u, v from -1 to 1 across the
  // rectangle), fading linearly in u² + v² to nothing at the oval's edge.
  // Without SDL_RenderGeometry (SDL older than 2.0.18) the plain rectangle;
  // drawLens does not use it then.
  void drawFadedOval(const Texture& t, float x, float y, float w, float h, float solid);

  // Scene capture, for effects on the picture drawn so far (Level 41's
  // MIRROR MODE and BIG HEADS) without reading pixels back from the GPU,
  // which stalls its pipeline every frame. beginCapture() sends the draws
  // that follow to an offscreen texture the size of the picture in output
  // pixels (made once, made again when the window changes size or after a
  // render reset, see recover), cleared to black. grabCaptured() copies a
  // piece of it (logical units) to a second offscreen texture to draw from,
  // null when there is no capture or the piece leaves the picture.
  // endCapture() puts the picture back on the screen with a plain copy,
  // mirrored left to right if asked (flipped
  // first, at scale 1, into a third offscreen texture: SDL2's software
  // renderer misplaces a copy it flips and scales in a letterbox); false
  // without a capture. One capture at a time, begun with the screen as the
  // target; false when the renderer has no render targets or no texture that
  // big for one, and the draws then go to the screen (see the read-back
  // fallbacks below).
  bool beginCapture();
  bool capturing() const { return mCapturing; }
  // Tests: the captures begun so far.
  std::size_t capturesBegun() const { return mCapturesBegun; }
  const Texture* grabCaptured(float x, float y, float w, float h);
  bool endCapture(bool mirrorX);
  // Tests: beginCapture refuses as on a renderer without render targets, to
  // reach the read-back fallbacks.
  void refuseCaptures(bool refuse) { mRefuseCaptures = refuse; }
  // The capture's textures dropped (and the read-back fallbacks'), to be
  // made again by the next capture; a capture open ends, unshown. For a
  // targets reset: they are drawn anew every frame, so nothing is lost.
  void dropCaptureTextures();
  // Tests: whether any of those textures is held.
  bool holdsCaptureTextures() const
  {
    return mScene || mGrab || mMirrored || mFlipped || mLensPiece;
  }

  // Every texture the renderer itself holds dropped: the capture's, every
  // cached string (rasterized again when next drawn) and drawAlphaGrid's
  // texture (made again at its next call). It starts a new device
  // generation: textures made before it are stale from then on (see
  // staleDraws). Textures made with createTexture belong to their owners
  // (Art, the menus' panels, the touch controls ...), who drop and make
  // them again themselves (recoverRender).
  void dropTextures();
  // The renderer's own part of a reset event (renderResetOf): Targets drops
  // the capture's textures, Device all of its textures (dropTextures). None
  // does nothing.
  void recover(RenderReset lost);
  // Draws with `sdl` from now on, after dropTextures and after the old
  // SDL_Renderer was destroyed (which freed every texture made with it):
  // for recoverRender, when a device reset left the old one dead.
  void rebind(SDL_Renderer* sdl) { mRenderer = sdl; }
  // Tests: whether drawAlphaGrid's texture is held.
  bool holdsGridTexture() const { return mGrid != nullptr; }

  // The device generation: 0 at first, one more with every dropTextures.
  // Textures are stamped with it when made.
  std::uint32_t generation() const { return mGeneration; }
  // Tests: the draws so far (draw, drawRegion, drawFadedOval, use) of a
  // texture from an older generation, one that should have been dropped
  // and made again after a device reset. Each would draw nothing on a GPU
  // that lost its textures (or a freed texture, after a recreate); the
  // software renderer still draws it, so only this count tells.
  std::size_t staleDraws() const { return mStaleDraws; }
  // A texture's SDL handle for drawing with SDL directly (a stretched or
  // flipped copy), counted as a draw by staleDraws.
  SDL_Texture* use(const Texture& t) const
  {
    noteDraw(t);
    return t.get();
  }

  // BIG HEADS' lens: the picture drawn so far in `piece` (logical units)
  // drawn again over `dst`, faded out to an oval as drawFadedOval's `solid`.
  // Copied on the GPU from the capture; without one (refused), or without
  // SDL_RenderGeometry to fade it (SDL older than 2.0.18), read back from
  // the GPU and faded on the CPU, the slow way. False when the piece leaves
  // the picture.
  bool drawLens(const SDL_FRect& piece, const SDL_FRect& dst, float solid);
  // MIRROR MODE without a capture: the picture on the screen read back from
  // the GPU, flipped left to right on the CPU and drawn again, the slow way.
  // For when beginCapture was refused, so the picture is mirrored whenever
  // the controls are.
  void mirrorReadBack();

  // The rasterized string, cached per (string, style). The reference stays
  // valid until the next beginFrame (which may evict it).
  const Texture& text(const std::string& s, const TextStyle& style);
  float drawText(
    const std::string& s,
    float x,
    float y,
    const TextStyle& style,
    Align align = Align::Left,
    float alpha = 1.0f);
  // Left aligned, the first `bytes` bytes of s (rounded down to a whole
  // UTF-8 character): the cached texture of the WHOLE string clipped right
  // of the prefix's ink, its outline and slant included (the last letter's,
  // also before a space, and an earlier letter's leaning past it), but
  // never into the next letter's fill. A typewriter reveal thus rasterizes
  // the line once instead of once per step. Where the last letter's ink
  // and the next one's overlap in x (an italic lean, a tight pair) the cut,
  // being vertical, takes that part off the last letter; and of an outlined
  // next letter a sliver of its outline may show. Returns the prefix's
  // advance (where text after it would start), for the whole string too,
  // which is not drawText's width (the texture's, a slant's margin
  // included). A blank prefix draws nothing and returns its advance.
  float drawTextPrefix(
    const std::string& s,
    std::size_t bytes,
    float x,
    float y,
    const TextStyle& style,
    float alpha = 1.0f);

  // The cache bound: beginFrame evicts down to kTextCacheKeep strings once
  // there are more than kTextCacheMax, oldest first, and never a string drawn
  // in the previous frame (it is likely on screen again).
  static constexpr std::size_t kTextCacheMax = 400;
  static constexpr std::size_t kTextCacheKeep = 300;
  // A GPU device reset (an Android context loss) empties the cache at once
  // (dropTextures), its textures being dead from then on.
  // Introspection for tests; neither marks a string as used.
  std::size_t textCacheSize() const { return mTextCache.size(); }
  bool textCached(const std::string& s, const TextStyle& style) const;

private:
  // Where drawTextPrefix cuts a string's texture for one prefix.
  struct PrefixCut
  {
    float advance = -1.0f; // the prefix's advance; < 0: not measured yet
    int srcW = 0;          // the texture columns that show it
  };
  struct CachedText
  {
    Texture tex;
    std::uint64_t lastUse = 0;        // mFrame when last drawn or measured
    std::vector<PrefixCut> prefixCuts; // per byte count
  };

  CachedText& cachedText(const std::string& s, const TextStyle& style);
  Texture rasterizeText(const std::string& s, const TextStyle& style) const;
  PrefixCut prefixCut(CachedText& e, const std::string& s, std::size_t bytes, const TextStyle& style) const;
  void evictText();

  // Counts a draw of a texture from an older device generation.
  void noteDraw(const Texture& t) const
  {
    if (t && t.generation() != mGeneration)
      ++mStaleDraws;
  }

  SDL_Renderer* mRenderer;
  std::uint32_t mGeneration = 0;     // see generation()
  mutable std::size_t mStaleDraws = 0; // see staleDraws()
  std::unordered_map<std::string, CachedText> mTextCache;
  std::uint64_t mFrame = 0; // beginFrame calls so far: the text cache's clock
  SDL_Texture* mGrid = nullptr; // drawAlphaGrid's streaming texture
  int mGridW = 0, mGridH = 0;

  bool textureOfSize(Texture& t, SDL_TextureAccess access, int w, int h);
  void viewCapture();
  bool pixelRectOf(const SDL_FRect& r, SDL_Rect& out) const;
  const Texture* readBackOval(const SDL_FRect& piece, float solid);
  const Texture* flippedScene();
  Texture mScene;          // the capture's picture, at the output resolution
  Texture mGrab;           // grabCaptured's piece of it
  Texture mMirrored;       // endCapture's copy of it flipped left to right
  bool mCapturing = false;
  std::size_t mCapturesBegun = 0;
  bool mRefuseCaptures = false;
  float mCaptureSx = 1.0f; // logical units to the capture's pixels
  float mCaptureSy = 1.0f;
  Texture mFlipped;        // mirrorReadBack's copy of the picture
  Texture mLensPiece;      // readBackOval's faded piece of it
};

// The texture owners besides the renderer itself (Art, the game's panels,
// map, cutscene clip cache and runner editor, the touch controls), for
// recoverRender.
struct TextureOwners
{
  std::function<void()> drop;   // every texture they hold let go
  std::function<void()> rebake; // made again, or left to be made as drawn
};

// The answer to a render reset event (resetActionOf), in the one order that
// is safe:
//   - None: nothing. Targets: the renderer's capture textures dropped.
//   - InPlace and Recreate: the owners drop their textures, then the
//     renderer its own (dropTextures). Recreate then calls `remake`, which
//     destroys the old SDL_Renderer and returns a new one made as the first
//     was, and rebinds the renderer to it. Last the owners bake theirs again.
// Dropping comes first, before SDL_DestroyRenderer: that frees every texture
// the renderer still has, and a Texture destroyed afterwards would free its
// own a second time. And on OpenGL ES the dead context's texture names are
// deleted while none of the new renderer's exist, so none of its textures
// can share a name with one deleted late. On InPlace (Direct3D) no names are
// shared, and dropping first is just as safe.
// False when Recreate got no new SDL_Renderer (or had no `remake`): the
// owners are baked all the same, their textures empty and drawing nothing,
// so nothing is left half dropped; the caller should give up the window.
bool recoverRender(Renderer& r, ResetAction action, const TextureOwners& owners,
  const std::function<SDL_Renderer*()>& remake);

} // namespace gr
