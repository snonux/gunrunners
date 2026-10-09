#include "render/renderer.hpp"

#include "render/vector.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace gr
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

void Renderer::drawAlphaGrid(const std::vector<float>& alpha, int cols, int rows, float x, float y, float cellW,
  float cellH, Color c)
{
  if (cols <= 0 || rows <= 0 || alpha.size() < std::size_t(cols * rows))
    return;
  if (!mGrid || mGridW != cols || mGridH != rows)
  {
    if (mGrid)
      SDL_DestroyTexture(mGrid);
    mGrid = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, cols, rows);
    if (!mGrid)
      return;
    SDL_SetTextureBlendMode(mGrid, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(mGrid, SDL_ScaleModeLinear);
    mGridW = cols;
    mGridH = rows;
  }
  void* pixels = nullptr;
  int pitch = 0;
  if (SDL_LockTexture(mGrid, nullptr, &pixels, &pitch) != 0)
    return;
  const Uint32 rgbBits = (Uint32(redOf(c)) << 16) | (Uint32(greenOf(c)) << 8) | Uint32(blueOf(c));
  for (int j = 0; j < rows; ++j)
  {
    auto* row = reinterpret_cast<Uint32*>(static_cast<unsigned char*>(pixels) + j * pitch);
    for (int i = 0; i < cols; ++i)
    {
      const float a = std::clamp(alpha[std::size_t(j * cols + i)], 0.0f, 1.0f);
      row[i] = (Uint32(a * 255.0f + 0.5f) << 24) | rgbBits;
    }
  }
  SDL_UnlockTexture(mGrid);
  // Texel centres sit on cell centres; the outer half cells clamp.
  const SDL_FRect dst{x, y, float(cols) * cellW, float(rows) * cellH};
  SDL_RenderCopyF(mRenderer, mGrid, nullptr, &dst);
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
  if (o.cull && (dst.x >= kScreenW || dst.y >= kScreenH || dst.x + dst.w <= 0 || dst.y + dst.h <= 0))
    return;
  SDL_SetTextureBlendMode(t.get(), o.blend == Blend::Add ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
  SDL_SetTextureAlphaMod(t.get(), Uint8(std::lround(std::min(1.0f, std::max(0.0f, o.alpha)) * 255.0f)));
  SDL_SetTextureColorMod(t.get(), Uint8(redOf(o.tint)), Uint8(greenOf(o.tint)), Uint8(blueOf(o.tint)));
  if (o.angle != 0.0f)
  {
    const SDL_Point pivot{int(std::lround(t.anchorX() * o.scale)), int(std::lround(t.anchorY() * o.scale))};
    SDL_RenderCopyEx(mRenderer, t.get(), nullptr, &dst, double(o.angle), &pivot, SDL_FLIP_NONE);
  }
  else
  {
    SDL_RenderCopy(mRenderer, t.get(), nullptr, &dst);
  }
}

void Renderer::fillRect(float x, float y, float w, float h, Color c, Blend b)
{
  SDL_SetRenderDrawBlendMode(mRenderer, b == Blend::Add ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(mRenderer, Uint8(redOf(c)), Uint8(greenOf(c)), Uint8(blueOf(c)), Uint8(alphaOf(c)));
  SDL_Rect r{int(std::lround(x)), int(std::lround(y)), int(std::lround(w)), int(std::lround(h))};
  SDL_RenderFillRect(mRenderer, &r);
}

void Renderer::drawLine(float x0, float y0, float x1, float y1, float width, Color c, Blend b)
{
  const float dx = x1 - x0, dy = y1 - y0;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len < 0.5f)
    return;
#if SDL_VERSION_ATLEAST(2, 0, 18)
  const float nx = -dy / len * width * 0.5f, ny = dx / len * width * 0.5f;
  const SDL_Color col{Uint8(redOf(c)), Uint8(greenOf(c)), Uint8(blueOf(c)), Uint8(alphaOf(c))};
  const SDL_Vertex v[4] = {
    {{x0 + nx, y0 + ny}, col, {0, 0}},
    {{x1 + nx, y1 + ny}, col, {0, 0}},
    {{x1 - nx, y1 - ny}, col, {0, 0}},
    {{x0 - nx, y0 - ny}, col, {0, 0}},
  };
  const int idx[6] = {0, 1, 2, 0, 2, 3};
  SDL_SetRenderDrawBlendMode(mRenderer, b == Blend::Add ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
  SDL_RenderGeometry(mRenderer, nullptr, v, 4, idx, 6);
#else
  // Older SDL: a run of small squares.
  const int steps = int(len / std::max(1.0f, width * 0.5f)) + 1;
  for (int i = 0; i <= steps; ++i)
  {
    const float t = float(i) / float(steps);
    fillRect(x0 + dx * t - width * 0.5f, y0 + dy * t - width * 0.5f, width, width, c, b);
  }
#endif
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
    selectGameFont(cr, st.italic);
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

} // namespace gr
