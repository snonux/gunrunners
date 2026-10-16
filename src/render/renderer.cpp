#include "render/renderer.hpp"

#include "render/vector.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>
#include <initializer_list>
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

// The width of an outlined string's stroke (half of it reaches outside the
// glyphs).
double outlineWidth(const TextStyle& st)
{
  return double(st.size) * 0.16;
}

// The start of the UTF-8 character that ends at byte `end` (> 0).
std::size_t utf8Back(const std::string& s, std::size_t end)
{
  std::size_t i = end - 1;
  while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80)
    --i;
  return i;
}

// The end of the UTF-8 character that starts at byte `at` (< s.size()).
std::size_t utf8Next(const std::string& s, std::size_t at)
{
  std::size_t i = at + 1;
  while (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80)
    ++i;
  return i;
}

// The horizontal ink of `str` set at (x, 0) as rasterizeText sets it: its
// stroke `stroke` wide (outline included), or its fill for 0. False when it
// has no ink (spaces). Path extents, not glyph metrics: they include the
// slant Cairo synthesizes for the bundled font's oblique (Android, web),
// which its glyph metrics leave out. (Outlines use round joins, so for
// upright text the glyph box plus half the stroke is the same.)
bool pathInk(cairo_t* cr, const std::string& str, double x, double stroke, double& left, double& right)
{
  double y0 = 0.0, y1 = 0.0;
  cairo_new_path(cr);
  cairo_move_to(cr, x, 0.0);
  cairo_text_path(cr, str.c_str());
  if (stroke > 0.0)
  {
    cairo_set_line_width(cr, stroke);
    cairo_stroke_extents(cr, &left, &y0, &right, &y1);
  }
  else
    cairo_fill_extents(cr, &left, &y0, &right, &y1);
  cairo_new_path(cr);
  return right > left;
}

// Where a prefix of whole characters lies in its whole string's texture,
// from the string's origin. Cairo's toy text API sets glyph after glyph one
// advance apart, without kerning, so substrings measured on their own line
// up with the whole string.
struct PrefixInk
{
  double advance = 0.0; // where the rest of the string starts
  bool inked = false;   // false: the prefix is blank
  double right = 0.0;   // its rightmost ink, outline included
  bool nextInked = false;
  double nextLeft = 0.0; // the next character's leftmost fill
};

// How many characters before the last inked one prefixInk strokes too: a
// slanted letter leans past a narrow one after it ("f."), by about a fifth
// of its height, less than three characters' advance.
constexpr int kInkLookBack = 3;

// Where the characters prefixInk strokes start in s[0, bytes): back over
// the blank characters that end it (a reveal step that ends on a space),
// over the last inked character, and over kInkLookBack more.
std::size_t inkWindow(cairo_t* cr, const std::string& s, std::size_t bytes)
{
  std::size_t at = bytes;
  double left = 0.0, right = 0.0;
  while (at > 0)
  {
    const std::size_t from = utf8Back(s, at);
    const bool inked = pathInk(cr, s.substr(from, at - from), 0.0, 0.0, left, right);
    at = from;
    if (inked)
      break;
  }
  for (int k = 0; k < kInkLookBack && at > 0; ++k)
    at = utf8Back(s, at);
  return at;
}

// Strokes the last few characters, up to the last inked one (a constant
// cost per reveal step but for a run of blanks): their path extents hold a
// synthesized slant, the last letter's and an earlier one's leaning past
// it, even when the prefix ends in a space. Earlier characters count by
// their glyph boxes plus half the stroke, exact for upright text (round
// joins); a slant that far back cannot reach past the window.
PrefixInk prefixInk(const std::string& s, std::size_t bytes, const TextStyle& st)
{
  VectorImage probe(1, 1);
  cairo_t* cr = probe.cr();
  setTextFont(cr, st);
  const std::size_t windowAt = inkWindow(cr, s, bytes);
  cairo_text_extents_t ext;
  cairo_text_extents(cr, s.substr(0, windowAt).c_str(), &ext);
  const double windowOrigin = ext.x_advance;
  cairo_text_extents(cr, s.substr(0, bytes).c_str(), &ext);
  PrefixInk ink;
  ink.advance = std::max(0.0, ext.x_advance);
  const double stroke = st.outline ? outlineWidth(st) : 0.0;
  double left = 0.0, right = 0.0;
  if (pathInk(cr, s.substr(windowAt, bytes - windowAt), windowOrigin, stroke, left, right))
  {
    ink.inked = true;
    ink.right = right;
  }
  if (ext.width > 0.0)
  {
    ink.inked = true;
    ink.right = std::max(ink.right, ext.x_bearing + ext.width + stroke * 0.5);
  }
  // The next character alone: a blank one (a space) leaves the following
  // letter a space's width away, far enough not to matter.
  double nextRight = 0.0;
  if (bytes < s.size())
    ink.nextInked = pathInk(cr, s.substr(bytes, utf8Next(s, bytes) - bytes), ink.advance, 0.0, ink.nextLeft, nextRight);
  return ink;
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

// The current viewport in output pixels (x, y: its offset, the letterbox
// bar), and the scale to them from logical units. SDL2 folds a logical size
// into the render scale, but sdl2-compat (SDL2 on top of SDL3) reports only
// the scale set with SDL_RenderSetScale, so with a logical size the scale is
// worked out from the output size the way SDL2 letterboxes: the largest
// that fits both ways.
struct Picture
{
  int x = 0, y = 0, w = 0, h = 0;
  float sx = 1.0f, sy = 1.0f;
};

Picture pictureOf(SDL_Renderer* sdl)
{
  Picture p;
  SDL_Rect vp;
  SDL_RenderGetViewport(sdl, &vp);
  int lw = 0, lh = 0, ow = 0, oh = 0;
  SDL_RenderGetLogicalSize(sdl, &lw, &lh);
  if (lw > 0 && lh > 0 && SDL_GetRendererOutputSize(sdl, &ow, &oh) == 0)
    p.sx = p.sy = std::min(float(ow) / float(lw), float(oh) / float(lh));
  else
    SDL_RenderGetScale(sdl, &p.sx, &p.sy);
  p.x = int(std::lround(float(vp.x) * p.sx));
  p.y = int(std::lround(float(vp.y) * p.sy));
  p.w = int(std::lround(float(vp.w) * p.sx));
  p.h = int(std::lround(float(vp.h) * p.sy));
  return p;
}

// The clip rectangle across a switch of render targets: SDL2 drops it (SDL3
// keeps one per target, and it is left alone). SDL2 keeps it in pixels and
// converts at the scale of the moment when it is set and read. At the same
// scale the logical round trip is exact with the double precision of newer
// SDL2, but older ones keep whole (or float) pixels and an edge can move by
// a pixel at a scale like 4/3. So the clip is saved in both units and put
// back in pixels, at scale 1, when the logical one does not come back the
// same: at scale 1 whole pixels convert exactly.
struct SavedClip
{
  bool on = false;
  SDL_Rect logical{}, pixels{};
};

SDL_Rect clipAtScale(SDL_Renderer* sdl, float sx, float sy)
{
  SDL_Rect r{};
  SDL_RenderSetScale(sdl, sx, sy);
  SDL_RenderGetClipRect(sdl, &r);
  return r;
}

// The clip of a target drawn at the scale (sx, sy), left at that scale.
SavedClip saveClip(SDL_Renderer* sdl, float sx, float sy)
{
  SavedClip c;
  c.on = SDL_RenderIsClipEnabled(sdl) == SDL_TRUE;
  if (c.on)
  {
    c.pixels = clipAtScale(sdl, 1.0f, 1.0f);
    c.logical = clipAtScale(sdl, sx, sy);
  }
  return c;
}

// Back on the target after the switch, left at the scale (sx, sy).
void restoreClip(SDL_Renderer* sdl, const SavedClip& c, float sx, float sy)
{
  if (c.on && SDL_RenderIsClipEnabled(sdl) != SDL_TRUE)
  {
    SDL_RenderSetScale(sdl, sx, sy);
    SDL_RenderSetClipRect(sdl, &c.logical);
    const SDL_Rect px = clipAtScale(sdl, 1.0f, 1.0f);
    if (SDL_RectEquals(&px, &c.pixels) != SDL_TRUE)
      SDL_RenderSetClipRect(sdl, &c.pixels);
  }
  SDL_RenderSetScale(sdl, sx, sy);
}

// drawLens read back: alpha 1 where u² + v² <= solid (u, v from -1 to 1
// across the piece), falling linearly in u² + v² to 0 at the oval's edge,
// as drawFadedOval's mesh does on the GPU.
void fadeToOval(std::vector<Uint32>& px, int w, int h, float solid)
{
  const float band = 1.0f - std::clamp(solid, 0.0f, 0.99f);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
    {
      const float u = (float(x) + 0.5f) / float(w) * 2.0f - 1.0f, v = (float(y) + 0.5f) / float(h) * 2.0f - 1.0f;
      const float a = std::clamp((1.0f - (u * u + v * v)) / band, 0.0f, 1.0f);
      Uint32& p = px[std::size_t(y) * std::size_t(w) + std::size_t(x)];
      p = (p & 0x00FFFFFFu) | (Uint32(std::lround(a * 255.0f)) << 24);
    }
}

#if SDL_VERSION_ATLEAST(2, 0, 18)
// drawFadedOval's mesh: a solid middle inside three rings at u² + v² =
// solid, halfway to 1 and 1, alpha 1, 1/2 and 0. Between rings alpha runs
// linear in the radius, within a percent or two of linear in u² + v² over
// such narrow bands.
constexpr int kOvalSides = 48, kOvalRings = 3;
using OvalVertices = std::array<SDL_Vertex, 1 + kOvalSides * kOvalRings>;
using OvalIndices = std::array<int, 3 * kOvalSides + 6 * kOvalSides * (kOvalRings - 1)>;

OvalVertices ovalVertices(float x, float y, float w, float h, float solid)
{
  solid = std::clamp(solid, 0.0f, 0.99f);
  OvalVertices v;
  v[0] = {{x + w * 0.5f, y + h * 0.5f}, {255, 255, 255, 255}, {0.5f, 0.5f}};
  for (int k = 0; k < kOvalRings; ++k)
  {
    const float share = float(k) / float(kOvalRings - 1);
    const float radius = std::sqrt(solid + (1.0f - solid) * share);
    const Uint8 a = Uint8(std::lround((1.0f - share) * 255.0f));
    for (int i = 0; i < kOvalSides; ++i)
    {
      const float angle = float(i) * 6.2831853f / float(kOvalSides);
      const float tu = 0.5f + 0.5f * radius * std::cos(angle), tv = 0.5f + 0.5f * radius * std::sin(angle);
      v[std::size_t(1 + k * kOvalSides + i)] = {{x + w * tu, y + h * tv}, {255, 255, 255, a}, {tu, tv}};
    }
  }
  return v;
}

// The middle as a fan round vertex 0, each band between two rings as quads.
const OvalIndices& ovalIndices()
{
  static const OvalIndices idx = [] {
    OvalIndices out{};
    std::size_t n = 0;
    for (int i = 0; i < kOvalSides; ++i)
    {
      const int next = (i + 1) % kOvalSides;
      for (const int c : {0, 1 + i, 1 + next})
        out[n++] = c;
      for (int k = 0; k + 1 < kOvalRings; ++k)
      {
        const int a = 1 + k * kOvalSides + i, b = 1 + k * kOvalSides + next;
        for (const int c : {a, a + kOvalSides, b, b, a + kOvalSides, b + kOvalSides})
          out[n++] = c;
      }
    }
    return out;
  }();
  return idx;
}
#endif

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
  // A capture left open (a draw that returned before ending it) would
  // swallow this frame: drop it, the screen is the target again.
  if (mCapturing)
  {
    SDL_SetRenderTarget(mRenderer, nullptr);
    mCapturing = false;
  }
  // Score and timer strings change constantly; keep the cache bounded. It
  // once cleared wholesale here, and the next frame re-rasterized every
  // visible string at once (100+ ms for a screenful of outlined text).
  ++mFrame;
  if (mTextCache.size() > kTextCacheMax)
    evictText();
}

void Renderer::evictText()
{
  // Candidates: the strings the previous frame did not draw (it drew those
  // with lastUse + 1 == mFrame). Of the candidates, the least recently drawn
  // go first, until the cache is down to kTextCacheKeep; if the last frame
  // alone drew more, it keeps them.
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

void Renderer::drawFadedOval(const Texture& t, float x, float y, float w, float h, float solid)
{
  if (!t || w <= 0.0f || h <= 0.0f)
    return;
  SDL_SetTextureBlendMode(t.get(), SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 18)
  const OvalVertices v = ovalVertices(x, y, w, h, solid);
  const OvalIndices& idx = ovalIndices();
  SDL_RenderGeometry(mRenderer, t.get(), v.data(), int(v.size()), idx.data(), int(idx.size()));
#else
  (void)solid;
  const SDL_FRect dst{x, y, w, h};
  SDL_RenderCopyF(mRenderer, t.get(), nullptr, &dst);
#endif
}

// Makes `t` a w x h texture of the given access unless it already is one
// (each member keeps to one access, so its size tells).
bool Renderer::textureOfSize(Texture& t, SDL_TextureAccess access, int w, int h)
{
  if (t && t.w() == w && t.h() == h)
    return true;
  t = Texture();
  SDL_Texture* tex = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_ARGB8888, access, w, h);
  if (!tex)
    return false;
  t = Texture(tex, w, h, 0.0f, 0.0f);
  return true;
}

// The capture's view: all of the texture, logical units scaled to its
// pixels. Set after every switch to it: SDL2 resets the view on
// SDL_SetRenderTarget (SDL3 keeps one per target).
void Renderer::viewCapture()
{
  SDL_RenderSetViewport(mRenderer, nullptr);
  SDL_RenderSetScale(mRenderer, mCaptureSx, mCaptureSy);
}

// `r` (logical units) in the current target's pixels: the capture's, or the
// screen's inside the letterbox. False when it is empty or leaves the
// picture.
bool Renderer::pixelRectOf(const SDL_FRect& r, SDL_Rect& out) const
{
  Picture p;
  if (mCapturing)
  {
    p.w = mScene.w();
    p.h = mScene.h();
    p.sx = mCaptureSx;
    p.sy = mCaptureSy;
  }
  else
    p = pictureOf(mRenderer);
  const SDL_Rect in{int(std::lround(r.x * p.sx)), int(std::lround(r.y * p.sy)), int(std::lround(r.w * p.sx)),
    int(std::lround(r.h * p.sy))};
  if (in.w <= 0 || in.h <= 0 || in.x < 0 || in.y < 0 || in.x + in.w > p.w || in.y + in.h > p.h)
    return false;
  out = {p.x + in.x, p.y + in.y, in.w, in.h};
  return true;
}

bool Renderer::beginCapture()
{
  if (mCapturing || mRefuseCaptures || SDL_GetRenderTarget(mRenderer) != nullptr ||
    SDL_RenderTargetSupported(mRenderer) != SDL_TRUE)
    return false;
  const Picture p = pictureOf(mRenderer);
  if (p.w <= 0 || p.h <= 0 || !textureOfSize(mScene, SDL_TEXTUREACCESS_TARGET, p.w, p.h))
    return false;
  // Copied back (and grabbed) pixel for pixel: no filtering, so a pixel's
  // rounding never blurs it.
  SDL_SetTextureScaleMode(mScene.get(), SDL_ScaleModeNearest);
  if (SDL_SetRenderTarget(mRenderer, mScene.get()) != 0)
    return false;
  mCapturing = true;
  mCaptureSx = p.sx;
  mCaptureSy = p.sy;
  viewCapture();
  // Cleared to black, the draw colour left as it was.
  Uint8 cr = 0, cg = 0, cb = 0, ca = 0;
  SDL_GetRenderDrawColor(mRenderer, &cr, &cg, &cb, &ca);
  SDL_SetRenderDrawColor(mRenderer, 0, 0, 0, 255);
  SDL_RenderClear(mRenderer);
  SDL_SetRenderDrawColor(mRenderer, cr, cg, cb, ca);
  return true;
}

const Texture* Renderer::grabCaptured(float x, float y, float w, float h)
{
  SDL_Rect src{};
  if (!mCapturing || !pixelRectOf({x, y, w, h}, src) || !textureOfSize(mGrab, SDL_TEXTUREACCESS_TARGET, src.w, src.h))
    return nullptr;
  // A texture cannot be drawn into itself: the piece goes through mGrab.
  // Switching targets drops the clip rectangle (SDL2); it comes back after,
  // to the pixel (saveClip).
  const SavedClip clip = saveClip(mRenderer, mCaptureSx, mCaptureSy);
  bool ok = SDL_SetRenderTarget(mRenderer, mGrab.get()) == 0;
  if (ok)
  {
    SDL_SetTextureBlendMode(mScene.get(), SDL_BLENDMODE_NONE);
    ok = SDL_RenderCopy(mRenderer, mScene.get(), &src, nullptr) == 0;
  }
  if (SDL_SetRenderTarget(mRenderer, mScene.get()) != 0)
  {
    // The scene is lost: drop the capture rather than draw on into mGrab.
    SDL_SetRenderTarget(mRenderer, nullptr);
    mCapturing = false;
    return nullptr;
  }
  viewCapture();
  restoreClip(mRenderer, clip, mCaptureSx, mCaptureSy);
  return ok ? &mGrab : nullptr;
}

bool Renderer::endCapture(bool mirrorX)
{
  if (!mCapturing)
    return false;
  mCapturing = false;
  if (SDL_SetRenderTarget(mRenderer, nullptr) != 0)
    return false;
  // The screen's own view is back (viewport, scale, clip): the picture over
  // the whole viewport, replacing what is under it.
  SDL_Rect vp;
  SDL_RenderGetViewport(mRenderer, &vp);
  const SDL_Rect dst{0, 0, vp.w, vp.h};
  SDL_SetTextureBlendMode(mScene.get(), SDL_BLENDMODE_NONE);
  return SDL_RenderCopyEx(mRenderer, mScene.get(), nullptr, &dst, 0.0, nullptr,
           mirrorX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE) == 0;
}

void Renderer::dropCaptureTextures()
{
  if (mCapturing)
  {
    SDL_SetRenderTarget(mRenderer, nullptr);
    mCapturing = false;
  }
  mScene = Texture();
  mGrab = Texture();
  mFlipped = Texture();
  mLensPiece = Texture();
}

bool Renderer::drawLens(const SDL_FRect& piece, const SDL_FRect& dst, float solid)
{
#if SDL_VERSION_ATLEAST(2, 0, 18)
  if (mCapturing)
  {
    const Texture* grab = grabCaptured(piece.x, piece.y, piece.w, piece.h);
    if (grab)
      drawFadedOval(*grab, dst.x, dst.y, dst.w, dst.h, solid);
    return grab != nullptr;
  }
#endif
  const Texture* faded = readBackOval(piece, solid);
  if (faded)
    SDL_RenderCopyF(mRenderer, faded->get(), nullptr, &dst);
  return faded != nullptr;
}

// drawLens the slow way: the piece read back from the current target (the
// capture, or the screen), faded out to the oval on the CPU, uploaded to a
// texture kept for the next frame.
const Texture* Renderer::readBackOval(const SDL_FRect& piece, float solid)
{
  SDL_Rect src{};
  if (!pixelRectOf(piece, src) || !textureOfSize(mLensPiece, SDL_TEXTUREACCESS_STREAMING, src.w, src.h))
    return nullptr;
  std::vector<Uint32> px(std::size_t(src.w) * std::size_t(src.h));
  if (SDL_RenderReadPixels(mRenderer, &src, SDL_PIXELFORMAT_ARGB8888, px.data(), src.w * 4) != 0)
    return nullptr;
  fadeToOval(px, src.w, src.h, solid);
  SDL_UpdateTexture(mLensPiece.get(), nullptr, px.data(), src.w * 4);
  SDL_SetTextureBlendMode(mLensPiece.get(), SDL_BLENDMODE_BLEND);
  return &mLensPiece;
}

void Renderer::mirrorReadBack()
{
  if (mCapturing)
    return; // endCapture mirrors a capture
  const Picture p = pictureOf(mRenderer);
  if (p.w <= 0 || p.h <= 0 || !textureOfSize(mFlipped, SDL_TEXTUREACCESS_STREAMING, p.w, p.h))
    return;
  // The picture's rectangle rather than null (the viewport): SDL writes no
  // more than the rectangle, whatever its viewport rounds to.
  const SDL_Rect src{p.x, p.y, p.w, p.h};
  std::vector<Uint32> px(std::size_t(p.w) * std::size_t(p.h));
  if (SDL_RenderReadPixels(mRenderer, &src, SDL_PIXELFORMAT_ARGB8888, px.data(), p.w * 4) != 0)
    return;
  SDL_UpdateTexture(mFlipped.get(), nullptr, px.data(), p.w * 4);
  SDL_SetTextureBlendMode(mFlipped.get(), SDL_BLENDMODE_NONE);
  SDL_Rect vp;
  SDL_RenderGetViewport(mRenderer, &vp);
  const SDL_Rect dst{0, 0, vp.w, vp.h};
  SDL_RenderCopyEx(mRenderer, mFlipped.get(), nullptr, &dst, 0.0, nullptr, SDL_FLIP_HORIZONTAL);
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
    cairo_set_line_width(cr, outlineWidth(st));
    cairo_stroke_preserve(cr);
  }
  setColor(cr, st.color);
  cairo_fill(cr);
  return img.toTexture(*this, float(pad), float(pad));
}

// Where to cut the whole string's texture to show its first `bytes` bytes
// (a whole-character prefix): measured once per prefix (no rasterizing) and
// kept with the cached texture.
Renderer::PrefixCut Renderer::prefixCut(CachedText& e, const std::string& s, std::size_t bytes,
  const TextStyle& st) const
{
  if (e.prefixCuts.size() != s.size() + 1)
    e.prefixCuts.assign(s.size() + 1, PrefixCut{});
  PrefixCut& c = e.prefixCuts[bytes];
  if (c.advance >= 0.0f)
    return c;
  const PrefixInk ink = prefixInk(s, bytes, st);
  c.advance = float(ink.advance);
  c.srcW = 0;
  if (ink.inked)
  {
    // Right of the prefix's ink, outline included (and a hair for the
    // path's fixed-point rounding at the texture's origin), but never into
    // the next letter's fill: where the two overlap in x (italic, a tight
    // pair) the last letter loses that part instead.
    const double a = double(e.tex.anchorX());
    double cut = std::ceil(a + ink.right + 1.0 / 64.0);
    if (ink.nextInked)
      cut = std::min(cut, std::floor(a + ink.nextLeft));
    c.srcW = int(std::max(0.0, cut));
  }
  return c;
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
  CachedText& e = cachedText(s, style);
  const PrefixCut cut = prefixCut(e, s, bytes, style);
  // A vertical cut (see prefixCut): of an outlined next letter at most a
  // sliver of its left outline shows, in the outline's colour. With linear
  // filtering at a fractional output scale (the game's window) the GPU
  // also blends in under an output pixel of the texel column past the cut.
  DrawOpts o;
  o.alpha = alpha;
  drawRegion(e.tex, x, y, bytes == s.size() ? e.tex.w() : cut.srcW, o);
  return cut.advance;
}

} // namespace gr
