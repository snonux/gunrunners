#pragma once

#include "render/color.hpp"

#include <SDL.h>

#include <cstddef>
#include <cstdint>
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
// hair or weapons without the game logic caring.
class Texture
{
public:
  Texture() = default;
  Texture(SDL_Texture* tex, int w, int h, float anchorX, float anchorY);
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
  explicit operator bool() const { return mTex != nullptr; }

private:
  SDL_Texture* mTex = nullptr;
  int mW = 0;
  int mH = 0;
  float mAnchorX = 0.0f;
  float mAnchorY = 0.0f;
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

// Thin layer over SDL_Renderer. In a window it is GPU accelerated; for
// headless recording the same calls go to SDL's software renderer.
class Renderer
{
public:
  explicit Renderer(SDL_Renderer* renderer) : mRenderer(renderer) {}

  SDL_Renderer* sdl() const { return mRenderer; }

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
  // pixels (made once, made again when the window changes size), cleared to
  // black. grabCaptured() copies a piece of it (logical units) to a second
  // offscreen texture to draw from, null when there is no capture or the
  // piece leaves the picture. endCapture() puts the picture back on the
  // screen with a plain copy, mirrored left to right if asked (flipped
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
  // The capture's textures dropped, to be made again by the next capture
  // (after SDL_RENDER_DEVICE_RESET has lost them); a capture open ends,
  // unshown. Nothing calls it yet: device resets are not handled at all.
  void dropCaptureTextures();

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
  // Not handled: textures lost to a GPU device reset (an Android context
  // loss) stay in the cache, unusable, until evicted. The wholesale flush
  // this replaced re-made them now and then; device resets are task af3.
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

  SDL_Renderer* mRenderer;
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

} // namespace gr
