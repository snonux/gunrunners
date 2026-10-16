#include "render/renderer.hpp"

#include "render/vector.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace gr
{

namespace
{

// The text cache's key: the string plus everything in the style that
// changes its pixels.
std::string textKey(const std::string& s, const TextStyle& st)
{
  std::string key = s;
  key += '\x1f';
  key += std::to_string(int(st.size * 10.0f)) + ':' + std::to_string(st.color) + ':' +
    std::to_string(st.outline) + (st.italic ? "i" : "n");
  return key;
}

void setTextFont(cairo_t* cr, const TextStyle& st)
{
  selectGameFont(cr, st.italic);
  cairo_set_font_size(cr, st.size);
}

// Rounds a byte count down to a whole UTF-8 character of s (and to its
// length), so a prefix never ends inside a multi-byte character.
std::size_t utf8Floor(const std::string& s, std::size_t bytes)
{
  bytes = std::min(bytes, s.size());
  while (bytes > 0 && bytes < s.size() && (static_cast<unsigned char>(s[bytes]) & 0xC0) == 0x80)
    --bytes;
  return bytes;
}

} // namespace

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
  // Score and timer strings change constantly; keep the cache bounded. It
  // once cleared wholesale here, and the next frame re-rasterized every
  // visible string at once (100+ ms for a screenful of outlined text).
  ++mFrame;
  if (mTextCache.size() > kTextCacheMax)
    evictText();
}

void Renderer::evictText()
{
  // Candidates: strings not drawn in the previous frame (lastUse + 1 ==
  // mFrame). Of those, the least recently drawn go first, until the cache is
  // down to kTextCacheKeep; if the last frame alone drew more, it keeps them.
  using Entry = std::pair<std::uint64_t, decltype(mTextCache)::iterator>;
  std::vector<Entry> stale;
  for (auto it = mTextCache.begin(); it != mTextCache.end(); ++it)
    if (it->second.lastUse + 1 < mFrame)
      stale.emplace_back(it->second.lastUse, it);
  const std::size_t n = std::min(mTextCache.size() - kTextCacheKeep, stale.size());
  const auto older = [](const Entry& a, const Entry& b) { return a.first < b.first; };
  if (n < stale.size())
    std::nth_element(stale.begin(), stale.begin() + std::ptrdiff_t(n), stale.end(), older);
  // Erasing one element leaves the other iterators valid.
  for (std::size_t i = 0; i < n; ++i)
    mTextCache.erase(stale[i].second);
}

void Renderer::clear(Color c)
{
  SDL_SetRenderDrawColor(mRenderer, Uint8(redOf(c)), Uint8(greenOf(c)), Uint8(blueOf(c)), 255);
  SDL_RenderClear(mRenderer);
}

void Renderer::draw(const Texture& t, float x, float y, const DrawOpts& o)
{
  drawRegion(t, x, y, t.w(), o);
}

void Renderer::drawRegion(const Texture& t, float x, float y, int srcW, const DrawOpts& o)
{
  if (!t || srcW <= 0)
    return;
  srcW = std::min(srcW, t.w());
  // The whole texture copies with no source rect, as it always did.
  const SDL_Rect part{0, 0, srcW, t.h()};
  const SDL_Rect* src = srcW < t.w() ? &part : nullptr;
  const float w = float(srcW) * o.scale;
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
    SDL_RenderCopyEx(mRenderer, t.get(), src, &dst, double(o.angle), &pivot, SDL_FLIP_NONE);
  }
  else
  {
    SDL_RenderCopy(mRenderer, t.get(), src, &dst);
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
  return cachedText(s, st).tex;
}

bool Renderer::textCached(const std::string& s, const TextStyle& st) const
{
  return mTextCache.count(textKey(s, st)) != 0;
}

Renderer::CachedText& Renderer::cachedText(const std::string& s, const TextStyle& st)
{
  const std::string key = textKey(s, st);
  auto it = mTextCache.find(key);
  if (it == mTextCache.end())
    it = mTextCache.emplace(key, CachedText{rasterizeText(s, st), 0, {}}).first;
  it->second.lastUse = mFrame;
  return it->second;
}

// The expensive part (for outlined text the glyph path's stroke and fill),
// hence the cache.
Texture Renderer::rasterizeText(const std::string& s, const TextStyle& st) const
{
  cairo_text_extents_t ext;
  cairo_font_extents_t fext;
  {
    VectorImage probe(1, 1);
    setTextFont(probe.cr(), st);
    cairo_text_extents(probe.cr(), s.c_str(), &ext);
    cairo_font_extents(probe.cr(), &fext);
  }
  const double pad = st.outline ? st.size * 0.14 + 2.0 : 2.0;
  const int w = std::max(1, int(std::ceil(ext.x_advance + pad * 2.0 + (st.italic ? st.size * 0.2 : 0.0))));
  const int h = std::max(1, int(std::ceil(fext.ascent + fext.descent + pad * 2.0)));
  VectorImage img(w, h);
  cairo_t* cr = img.cr();
  setTextFont(cr, st);
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
  return img.toTexture(*this, float(pad), float(pad));
}

// The advance of s's first `bytes` bytes (a whole-character prefix): where
// the next glyph starts in the whole string's texture, since Cairo's text
// API places glyphs one advance after another, without kerning. Measured
// once per prefix (no rasterizing) and kept with the cached texture.
float Renderer::prefixWidth(CachedText& e, const std::string& s, std::size_t bytes, const TextStyle& st) const
{
  if (e.prefixAdvance.size() != s.size() + 1)
    e.prefixAdvance.assign(s.size() + 1, -1.0f);
  float& w = e.prefixAdvance[bytes];
  if (w < 0.0f)
  {
    VectorImage probe(1, 1);
    setTextFont(probe.cr(), st);
    cairo_text_extents_t ext;
    cairo_text_extents(probe.cr(), s.substr(0, bytes).c_str(), &ext);
    w = std::max(0.0f, float(ext.x_advance));
  }
  return w;
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

float Renderer::drawTextPrefix(
  const std::string& s,
  std::size_t bytes,
  float x,
  float y,
  const TextStyle& style,
  float alpha)
{
  bytes = utf8Floor(s, bytes);
  if (bytes == 0)
    return 0.0f;
  if (bytes == s.size())
    return drawText(s, x, y, style, Align::Left, alpha);
  CachedText& e = cachedText(s, style);
  const float w = prefixWidth(e, s, bytes, style);
  // The clip falls at the next glyph's origin: the last shown letter keeps
  // its body, and of an outlined next letter at most a sliver of its left
  // outline shows (less than the outline's half width).
  DrawOpts o;
  o.alpha = alpha;
  drawRegion(e.tex, x, y, int(std::lround(e.tex.anchorX() + w)), o);
  return w;
}

} // namespace gr
