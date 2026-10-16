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
  void fillRect(float x, float y, float w, float h, Color c, Blend b = Blend::Alpha);
  // A line of any angle, `width` px thick (laser sights, cables).
  void drawLine(float x0, float y0, float x1, float y1, float width, Color c, Blend b = Blend::Alpha);

  // A cols x rows grid of alpha values (0..1) in colour c, stretched over
  // the rectangle at (x, y) with smooth filtering between cell centres: soft
  // darkness and light pools without visible squares.
  void drawAlphaGrid(const std::vector<float>& alpha, int cols, int rows, float x, float y, float cellW, float cellH,
    Color c);

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
  // UTF-8 character): the cached texture of the WHOLE string clipped at the
  // prefix's advance. A typewriter reveal thus rasterizes the line once
  // instead of once per step. Returns the prefix's width.
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
  // Introspection for tests; neither marks a string as used.
  std::size_t textCacheSize() const { return mTextCache.size(); }
  bool textCached(const std::string& s, const TextStyle& style) const;

private:
  struct CachedText
  {
    Texture tex;
    std::uint64_t lastUse = 0;        // mFrame when last drawn or measured
    std::vector<float> prefixAdvance; // per byte count, < 0: not measured yet
  };

  CachedText& cachedText(const std::string& s, const TextStyle& style);
  Texture rasterizeText(const std::string& s, const TextStyle& style) const;
  float prefixWidth(CachedText& e, const std::string& s, std::size_t bytes, const TextStyle& style) const;
  void evictText();
  // draw() limited to the texture's leftmost srcW columns.
  void drawRegion(const Texture& t, float x, float y, int srcW, const DrawOpts& o);

  SDL_Renderer* mRenderer;
  std::unordered_map<std::string, CachedText> mTextCache;
  std::uint64_t mFrame = 0; // beginFrame calls so far: the text cache's clock
  SDL_Texture* mGrid = nullptr; // drawAlphaGrid's streaming texture
  int mGridW = 0, mGridH = 0;
};

} // namespace gr
