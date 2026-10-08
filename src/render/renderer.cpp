#include "render/renderer.hpp"

#include "render/vector.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace td
{

Texture::Texture(SDL_Texture* tex, int w, int h, float anchorX, float anchorY)
  : mTex(tex), mW(w), mH(h), mAnchorX(anchorX), mAnchorY(anchorY)
{
}

Texture::~Texture()
{
  if (mTex)
    SDL_DestroyTexture(mTex);
}

Texture::Texture(Texture&& o) noexcept
  : mTex(o.mTex), mW(o.mW), mH(o.mH), mAnchorX(o.mAnchorX), mAnchorY(o.mAnchorY)
{
  o.mTex = nullptr;
}

Texture& Texture::operator=(Texture&& o) noexcept
{
  if (this != &o)
  {
    if (mTex)
      SDL_DestroyTexture(mTex);
    mTex = o.mTex;
    mW = o.mW;
    mH = o.mH;
    mAnchorX = o.mAnchorX;
    mAnchorY = o.mAnchorY;
    o.mTex = nullptr;
  }
  return *this;
}

Texture Renderer::createTexture(
  int w,
  int h,
  const unsigned char* data,
  int stride,
  float anchorX,
  float anchorY,
  bool mirrorX) const
{
  // Cairo stores premultiplied alpha; SDL's blend modes expect straight alpha.
  std::vector<Uint32> px(std::size_t(w) * std::size_t(h));
  for (int y = 0; y < h; ++y)
  {
    const auto* row = reinterpret_cast<const Uint32*>(data + y * stride);
    for (int x = 0; x < w; ++x)
    {
      const Uint32 p = row[mirrorX ? w - 1 - x : x];
      const Uint32 a = p >> 24;
      Uint32 out = 0;
      if (a == 255)
        out = p;
      else if (a > 0)
      {
        const Uint32 r = std::min<Uint32>(255, ((p >> 16) & 255) * 255 / a);
        const Uint32 g = std::min<Uint32>(255, ((p >> 8) & 255) * 255 / a);
        const Uint32 b = std::min<Uint32>(255, (p & 255) * 255 / a);
        out = (a << 24) | (r << 16) | (g << 8) | b;
      }
      px[std::size_t(y) * std::size_t(w) + std::size_t(x)] = out;
    }
  }
  SDL_Texture* tex = SDL_CreateTexture(
    mRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
  SDL_UpdateTexture(tex, nullptr, px.data(), w * 4);
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
  return Texture(tex, w, h, mirrorX ? float(w) - anchorX : anchorX, anchorY);
}

void Renderer::beginFrame()
{
  // Score and timer strings change constantly; keep the cache bounded.
  if (mTextCache.size() > 400)
    mTextCache.clear();
}

void Renderer::clear(Color c)
{
  SDL_SetRenderDrawColor(mRenderer, Uint8(redOf(c)), Uint8(greenOf(c)), Uint8(blueOf(c)), 255);
  SDL_RenderClear(mRenderer);
}

void Renderer::draw(const Texture& t, float x, float y, const DrawOpts& o)
{
  if (!t)
    return;
  const float w = float(t.w()) * o.scale;
  const float h = float(t.h()) * o.scale;
  SDL_Rect dst{
    int(std::lround(x - t.anchorX() * o.scale)),
    int(std::lround(y - t.anchorY() * o.scale)),
    int(std::lround(w)),
    int(std::lround(h))};
  if (dst.x >= kScreenW || dst.y >= kScreenH || dst.x + dst.w <= 0 || dst.y + dst.h <= 0)
    return;
  SDL_SetTextureBlendMode(t.get(), o.blend == Blend::Add ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
  SDL_SetTextureAlphaMod(t.get(), Uint8(std::lround(std::min(1.0f, std::max(0.0f, o.alpha)) * 255.0f)));
  SDL_SetTextureColorMod(t.get(), Uint8(redOf(o.tint)), Uint8(greenOf(o.tint)), Uint8(blueOf(o.tint)));
  SDL_RenderCopy(mRenderer, t.get(), nullptr, &dst);
}

void Renderer::fillRect(float x, float y, float w, float h, Color c, Blend b)
{
  SDL_SetRenderDrawBlendMode(mRenderer, b == Blend::Add ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(mRenderer, Uint8(redOf(c)), Uint8(greenOf(c)), Uint8(blueOf(c)), Uint8(alphaOf(c)));
  SDL_Rect r{int(std::lround(x)), int(std::lround(y)), int(std::lround(w)), int(std::lround(h))};
  SDL_RenderFillRect(mRenderer, &r);
}

const Texture& Renderer::text(const std::string& s, const TextStyle& st)
{
  std::string key = s;
  key += '\x1f';
  key += std::to_string(int(st.size * 10.0f)) + ':' + std::to_string(st.color) + ':' +
    std::to_string(st.outline) + (st.italic ? "i" : "n");
  auto it = mTextCache.find(key);
  if (it != mTextCache.end())
    return it->second;

  const auto setFont = [&](cairo_t* cr) {
    cairo_select_font_face(
      cr,
      "DejaVu Sans",
      st.italic ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL,
      CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, st.size);
  };

  cairo_text_extents_t ext;
  cairo_font_extents_t fext;
  {
    VectorImage probe(1, 1);
    setFont(probe.cr());
    cairo_text_extents(probe.cr(), s.c_str(), &ext);
    cairo_font_extents(probe.cr(), &fext);
  }
  const double pad = st.outline ? st.size * 0.14 + 2.0 : 2.0;
  const int w = std::max(1, int(std::ceil(ext.x_advance + pad * 2.0 + (st.italic ? st.size * 0.2 : 0.0))));
  const int h = std::max(1, int(std::ceil(fext.ascent + fext.descent + pad * 2.0)));
  VectorImage img(w, h);
  cairo_t* cr = img.cr();
  setFont(cr);
  cairo_move_to(cr, pad, pad + fext.ascent);
  cairo_text_path(cr, s.c_str());
  if (st.outline)
  {
    setColor(cr, st.outline);
    cairo_set_line_width(cr, st.size * 0.16);
    cairo_stroke_preserve(cr);
  }
  setColor(cr, st.color);
  cairo_fill(cr);
  auto [pos, ok] = mTextCache.emplace(key, img.toTexture(*this, float(pad), float(pad)));
  (void)ok;
  return pos->second;
}

float Renderer::drawText(
  const std::string& s,
  float x,
  float y,
  const TextStyle& style,
  Align align,
  float alpha)
{
  const Texture& t = text(s, style);
  const float tw = float(t.w()) - t.anchorX() * 2.0f;
  if (align == Align::Center)
    x -= tw * 0.5f;
  else if (align == Align::Right)
    x -= tw;
  DrawOpts o;
  o.alpha = alpha;
  draw(t, x, y, o);
  return tw;
}

} // namespace td
