#pragma once

#include "render/color.hpp"

#include <SDL.h>

#include <string>
#include <unordered_map>

namespace td
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

  void beginFrame();
  void clear(Color c);
  void draw(const Texture& t, float x, float y, const DrawOpts& o = {});
  void fillRect(float x, float y, float w, float h, Color c, Blend b = Blend::Alpha);

  const Texture& text(const std::string& s, const TextStyle& style);
  float drawText(
    const std::string& s,
    float x,
    float y,
    const TextStyle& style,
    Align align = Align::Left,
    float alpha = 1.0f);

private:
  SDL_Renderer* mRenderer;
  std::unordered_map<std::string, Texture> mTextCache;
};

} // namespace td
