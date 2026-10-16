// The renderer's text cache and the typewriter reveal (re3):
//   - past kTextCacheMax strings beginFrame evicts the least recently drawn
//     down to kTextCacheKeep, never a string the previous frame drew (it
//     once cleared everything, and the next frame re-rasterized a screenful
//     of text at once);
//   - drawTextPrefix reveals a line from the one texture of the whole line:
//     a cutscene line typed out two letters at a time caches one texture,
//     not one per step, and looks like the prefix drawn on its own;
//   - a prefix never ends inside a UTF-8 character.
// The bundled game font is loaded as the game loads it, but loadGameFont
// only does something with GR_BUNDLED_FONT (Android, web). Desktop builds
// pick the fonts through fontconfig: DejaVu Sans Bold, and for italic the
// real DejaVu Sans Bold Oblique, whose glyph metrics hold its slant. The
// bundled font's synthesized oblique (CAIRO_FT_SYNTHESIZE_OBLIQUE: metrics
// not slanted, only the glyph paths), where a cut must stroke the slant
// itself, is only exercised on Android and web.

#include "render/renderer.hpp"
#include "render/vector.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

using namespace gr;

namespace
{

int gFailures = 0;

void check(bool ok, const std::string& what)
{
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
  if (!ok)
    ++gFailures;
}

std::string name(const char* prefix, int i)
{
  return std::string(prefix) + " " + std::to_string(i);
}

int countCached(const Renderer& r, const char* prefix, int n, const TextStyle& st)
{
  int cached = 0;
  for (int i = 0; i < n; ++i)
    cached += r.textCached(name(prefix, i), st) ? 1 : 0;
  return cached;
}

// A hot string survives the eviction; the cache drops to kTextCacheKeep.
void evictsButKeepsTheHotString(SDL_Renderer* sdl)
{
  Renderer r(sdl);
  const TextStyle st{24.0f};
  r.beginFrame();
  for (int i = 0; i < 450; ++i)
    r.text(name("cold", i), st);
  r.text("hot", st);
  check(r.textCacheSize() == 451, "every new string is cached");
  check(&r.text("hot", st) == &r.text("hot", st), "a cached string is not rasterized again");
  r.beginFrame();
  check(r.textCacheSize() == 451, "the previous frame's strings stay, even over the bound");
  r.text("hot", st);
  r.beginFrame();
  check(r.textCacheSize() == Renderer::kTextCacheKeep, "over the bound the cache drops to kTextCacheKeep, not to 0");
  check(r.textCached("hot", st), "a string drawn every frame is never evicted");
  check(countCached(r, "cold", 450, st) == int(Renderer::kTextCacheKeep) - 1, "only stale strings went");
}

// Of the stale strings the least recently drawn go first.
void evictsOldestFirst(SDL_Renderer* sdl)
{
  Renderer r(sdl);
  const TextStyle st{24.0f};
  r.beginFrame(); // frame 1
  for (int i = 0; i < 200; ++i)
    r.text(name("a", i), st);
  r.beginFrame(); // frame 2
  for (int i = 0; i < 150; ++i)
    r.text(name("b", i), st);
  r.beginFrame(); // frame 3
  for (int i = 0; i < 60; ++i)
    r.text(name("c", i), st);
  r.text(name("a", 0), st); // touched again: as recent as c
  r.beginFrame();           // frame 4: 410 strings, 110 must go
  check(r.textCacheSize() == Renderer::kTextCacheKeep, "410 strings evict down to kTextCacheKeep");
  check(countCached(r, "c", 60, st) == 60, "the previous frame's strings stay");
  check(countCached(r, "b", 150, st) == 150, "younger stale strings outlive older ones");
  check(countCached(r, "a", 200, st) == 90, "the oldest strings go first");
  check(r.textCached(name("a", 0), st), "a string drawn again is young again");
}

// No eviction at the bound, and none while one frame draws more than it.
void boundEdges(SDL_Renderer* sdl)
{
  const TextStyle st{24.0f};
  {
    Renderer r(sdl);
    r.beginFrame();
    for (int i = 0; i < int(Renderer::kTextCacheMax); ++i)
      r.text(name("s", i), st);
    for (int f = 0; f < 3; ++f)
      r.beginFrame();
    check(r.textCacheSize() == Renderer::kTextCacheMax, "exactly kTextCacheMax strings: nothing evicted");
  }
  Renderer r(sdl);
  for (int f = 0; f < 3; ++f)
  {
    r.beginFrame();
    for (int i = 0; i < 500; ++i)
      r.text(name("busy", i), st);
  }
  r.beginFrame();
  check(r.textCacheSize() == 500, "a frame drawing 500 strings keeps all 500");
}

// flow.cpp's title holds one text() reference while it adds the next
// string; neither that nor the map growing may move or drop the first.
void heldReferenceSurvives(SDL_Renderer* sdl)
{
  Renderer r(sdl);
  const TextStyle shadowSt{120.0f, rgb(40, 40, 60), 0, true};
  const TextStyle bannerSt{120.0f, rgb(255, 200, 0), rgb(0, 0, 0), true};
  r.beginFrame();
  for (int i = 0; i < 450; ++i)
    r.text(name("old", i), {24.0f});
  r.beginFrame();
  r.beginFrame(); // the old strings are stale now: down to kTextCacheKeep
  const Texture& shadow = r.text("GUNRUNNERS", shadowSt);
  SDL_Texture* const tex = shadow.get();
  const int w = shadow.w(), h = shadow.h();
  const Texture& banner = r.text("GUNRUNNERS", bannerSt);
  for (int i = 0; i < 500; ++i) // enough to rehash the map
    r.text(name("new", i), {24.0f});
  check(shadow.get() == tex && shadow.w() == w && shadow.h() == h,
    "a held text() reference survives the strings added after it in the frame");
  check(&r.text("GUNRUNNERS", shadowSt) == &shadow, "the held reference is still the cached texture");
  check(banner && banner.get() != tex, "the second style is a texture of its own");
  check(r.textCacheSize() == Renderer::kTextCacheKeep + 502, "nothing is evicted inside a frame");
}

using Pixels = std::vector<Uint32>;

// The frame after a clear to black and `draw`.
Pixels shot(Renderer& r, SDL_Surface* surface, const std::function<void()>& draw)
{
  r.clear(rgb(0, 0, 0));
  draw();
  SDL_RenderPresent(r.sdl());
  Pixels px(std::size_t(surface->w) * std::size_t(surface->h));
  for (int y = 0; y < surface->h; ++y)
  {
    const auto* row = reinterpret_cast<const Uint32*>(static_cast<const unsigned char*>(surface->pixels) + y * surface->pitch);
    std::copy(row, row + surface->w, px.begin() + std::ptrdiff_t(y) * surface->w);
  }
  return px;
}

// The rightmost column with a lit pixel, -1 for a black frame.
int rightmostLit(const Pixels& px, int w)
{
  int right = -1;
  for (std::size_t i = 0; i < px.size(); ++i)
    if (px[i] & 0xFFFFFFu)
      right = std::max(right, int(i % std::size_t(w)));
  return right;
}

// The largest difference between two pixels in any colour channel.
int channelDiff(Uint32 a, Uint32 b)
{
  int most = 0;
  for (int shift = 0; shift < 32; shift += 8)
    most = std::max(most, std::abs(int((a >> shift) & 255u) - int((b >> shift) & 255u)));
  return most;
}

// Whether a and b agree in every column left of `limit`, each channel
// within `noise` (0: exactly).
bool sameLeftOf(const Pixels& a, const Pixels& b, int w, int limit, int noise)
{
  for (std::size_t i = 0; i < a.size(); ++i)
    if (int(i % std::size_t(w)) < limit && channelDiff(a[i], b[i]) > noise)
      return false;
  return true;
}

// The lit pixels' bounding box: left, top, right, bottom (inclusive).
std::vector<int> litBox(const Pixels& px, int w)
{
  std::vector<int> box{w, int(px.size()), -1, -1};
  for (std::size_t i = 0; i < px.size(); ++i)
    if (px[i] & 0xFFFFFFu)
    {
      const int x = int(i % std::size_t(w)), y = int(i / std::size_t(w));
      box = {std::min(box[0], x), std::min(box[1], y), std::max(box[2], x), std::max(box[3], y)};
    }
  return box;
}

const std::string kLine = "Three couriers. Forty-two jobs. One city that never sleeps and never pays on time.";
const TextStyle kStyle{28.0f, rgb(255, 255, 255), rgb(255, 0, 0)};
const TextStyle kItalic{28.0f, rgb(255, 255, 255), rgb(255, 0, 0), true};
constexpr float kX = 100.0f, kY = 300.0f;
// The pixel checks' slack: the outline's width at 28 px (4.5) plus 2, and
// the columns an outlined next letter's outline (or an antialiased edge
// too faint to light a pixel) may add past the part drawn alone.
constexpr int kMargin = 7;
constexpr int kSliver = 2;
// How far an italic part may be from the part drawn alone, per channel.
// Cairo antialiases a slanted edge a little differently in a longer path
// (whether its scan converter covers a pixel row in one go depends on every
// edge in that row), so the whole line's texture and the part's own differ
// by a level or two along the tops of the letters, wherever the cut is.
// Upright edges come out the same: plain text is compared exactly. A lost
// or extra column of ink is far more than this, and the ink's last column
// is compared on its own.
constexpr int kItalicNoise = 4;

// An evicted string drawn again is rasterized again, to the same pixels.
void evictedStringReturns(Renderer& r, SDL_Surface* surface)
{
  r.beginFrame();
  const Pixels before = shot(r, surface, [&] { r.drawText("Evicted", kX, kY, kStyle); });
  r.beginFrame();
  for (int i = 0; i < 450; ++i)
    r.text(name("cold", i), {24.0f});
  r.beginFrame(); // "Evicted" is the one stale string
  check(!r.textCached("Evicted", kStyle) && r.textCacheSize() == 450, "the stale string is evicted");
  const Pixels after = shot(r, surface, [&] { r.drawText("Evicted", kX, kY, kStyle); });
  check(r.textCached("Evicted", kStyle) && r.textCacheSize() == 451, "drawn again, it is cached again");
  check(after == before, "an evicted string comes back with the same pixels");
}

// Typing a line out two letters a step caches one texture and only grows.
void revealCachesOneTexture(SDL_Renderer* sdl)
{
  Renderer r(sdl);
  std::vector<float> widths;
  for (std::size_t bytes = 0; bytes < kLine.size(); bytes += 2)
  {
    r.beginFrame();
    widths.push_back(r.drawTextPrefix(kLine, bytes, kX, kY, kStyle));
  }
  widths.push_back(r.drawTextPrefix(kLine, kLine.size(), kX, kY, kStyle));
  check(r.textCacheSize() == 1, "a revealed line is one cached texture, not one per step");
  check(r.textCached(kLine, kStyle), "the cached texture is the whole line");
  check(!r.textCached(kLine.substr(0, 10), kStyle), "no prefix is rasterized");
  check(widths.front() == 0.0f, "nothing revealed: width 0");
  check(std::is_sorted(widths.begin(), widths.end()), "the revealed width only grows");
  check(widths.back() > 0.0f, "the whole line has a width");
  const float textW = r.drawText(kLine, kX, kY, kStyle);
  check(widths.back() <= textW + 0.01f && textW - widths.back() < 1.0f,
    "the whole line returns its advance too, within a pixel of drawText's width");
  const std::size_t before = r.textCacheSize();
  check(r.drawTextPrefix("", 5, kX, kY, kStyle) == 0.0f, "an empty line reveals nothing");
  check(r.textCacheSize() == before, "an empty line caches nothing");
}

// Nothing, the whole line, and past its end.
void revealEnds(Renderer& r, SDL_Surface* surface)
{
  const Pixels blank = shot(r, surface, [] {});
  const Pixels zero = shot(r, surface, [&] { r.drawTextPrefix(kLine, 0, kX, kY, kStyle); });
  check(zero == blank, "a prefix of 0 bytes draws nothing");
  const Pixels full = shot(r, surface, [&] { r.drawText(kLine, kX, kY, kStyle); });
  const Pixels all = shot(r, surface, [&] { r.drawTextPrefix(kLine, kLine.size(), kX, kY, kStyle); });
  const Pixels past = shot(r, surface, [&] { r.drawTextPrefix(kLine, kLine.size() + 50, kX, kY, kStyle); });
  check(all == full, "the whole prefix is the line drawn as text");
  check(past == full, "a prefix past the end is the whole line");
}

// Whether every column from `from` on is background (black).
bool blankFrom(const Pixels& px, int w, int from)
{
  for (std::size_t i = 0; i < px.size(); ++i)
    if (int(i % std::size_t(w)) >= from && (px[i] & 0xFFFFFFu))
      return false;
  return true;
}

// Whether column `x` has a lit pixel.
bool litColumn(const Pixels& px, int w, int x)
{
  for (std::size_t i = std::size_t(x); i < px.size(); i += std::size_t(w))
    if (px[i] & 0xFFFFFFu)
      return true;
  return false;
}

// Where the fill of kLine's character at byte `bytes` starts, from the
// line's origin, measured as the renderer measures it: the prefix's
// advance, then that character's glyph path alone.
double nextFillLeft(const TextStyle& st, std::size_t bytes)
{
  VectorImage probe(1, 1);
  cairo_t* cr = probe.cr();
  selectGameFont(cr, st.italic);
  cairo_set_font_size(cr, st.size);
  cairo_text_extents_t ext;
  cairo_text_extents(cr, kLine.substr(0, bytes).c_str(), &ext);
  cairo_new_path(cr);
  cairo_move_to(cr, ext.x_advance, 0.0);
  cairo_text_path(cr, kLine.substr(bytes, 1).c_str());
  double x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0;
  cairo_fill_extents(cr, &x0, &y0, &x1, &y1);
  return x0;
}

// The cut never shows the next letter's fill: from the column its fill
// starts in, everything is background (a sliver of its outline, left of
// that, may show). The texture lands at lround(kX - anchor) and is cut at
// floor(anchor + fill left), so on screen the cut is at or left of
// floor(kX + fill left). The whole line is lit there: the cut hid it.
void checkNextCapped(const Pixels& p, const Pixels& full, int sw, const TextStyle& st, std::size_t bytes,
  const std::string& what)
{
  const int from = int(std::floor(double(kX) + nextFillLeft(st, bytes)));
  check(blankFrom(p, sw, from) && litColumn(full, sw, from),
    what + "nothing from the next letter's fill on (column " + std::to_string(from) + ")");
}

// A part of the line looks like that part drawn on its own (italic within
// kItalicNoise). Before a space: all of it, ending in the same column, the
// last letter's outline and slant whole (a cut at the advance took half
// the outline and the lean). Between letters: the same left of the next
// letter's outline, and ending where the part ends, give or take the last
// letter's outline where the next letter's fill is close (italic: where it
// leans over the next letter) and a sliver of the next letter's outline,
// but nothing from the next letter's fill on (checkNextCapped).
void checkCut(Renderer& r, SDL_Surface* surface, const TextStyle& st, std::size_t bytes, const Pixels& full)
{
  const int sw = surface->w;
  const int noise = st.italic ? kItalicNoise : 0;
  const std::string part = kLine.substr(0, bytes);
  const std::string what = std::string(st.italic ? "italic " : "") + "\"" + part + "\": ";
  check(kLine[bytes - 1] != ' ', what + "a cut after a letter");
  float w = 0.0f;
  const Pixels p = shot(r, surface, [&] { w = r.drawTextPrefix(kLine, bytes, kX, kY, st); });
  const Pixels sub = shot(r, surface, [&] { r.drawText(part, kX, kY, st); });
  const int right = rightmostLit(p, sw), subRight = rightmostLit(sub, sw);
  check(right >= 0 && right < rightmostLit(full, sw), what + "the rest of the line is hidden");
  if (kLine[bytes] == ' ')
  {
    check(sameLeftOf(p, sub, sw, sw, noise) && right == subRight, what + "the same pixels as the part drawn alone");
    return;
  }
  check(sameLeftOf(p, sub, sw, int(kX + w) - kMargin, noise), what + "the same pixels as the part drawn alone");
  check(right <= subRight + kSliver, what + "nothing past the part drawn alone");
  check(right >= subRight - kMargin, what + "ends where the part drawn alone ends");
  checkNextCapped(p, full, sw, st, bytes, what);
}

// A part that ends in a space (with 2-byte steps the cutscene ends on one
// every few words) keeps the letter before it whole, outline and slant
// included: the same cut as before the space (where a synthesized slant
// once went missing, the space having no ink to stroke), the same pixels
// as the part drawn alone, and an advance a space further on.
void checkSpaceCut(Renderer& r, SDL_Surface* surface, const TextStyle& st, std::size_t bytes, const Pixels& full)
{
  const int sw = surface->w;
  const int noise = st.italic ? kItalicNoise : 0;
  const std::string part = kLine.substr(0, bytes);
  const std::string what = std::string(st.italic ? "italic " : "") + "\"" + part + "\": ";
  check(kLine[bytes - 1] == ' ' && kLine[bytes - 2] != ' ', what + "a cut after a letter and a space");
  float w = 0.0f, letterW = 0.0f;
  const Pixels p = shot(r, surface, [&] { w = r.drawTextPrefix(kLine, bytes, kX, kY, st); });
  const Pixels letter = shot(r, surface, [&] { letterW = r.drawTextPrefix(kLine, bytes - 1, kX, kY, st); });
  const Pixels sub = shot(r, surface, [&] { r.drawText(part, kX, kY, st); });
  const int right = rightmostLit(p, sw);
  check(p == letter && w > letterW, what + "the cut before the space, a space further on");
  check(sameLeftOf(p, sub, sw, sw, noise) && right == rightmostLit(sub, sw),
    what + "the same pixels as the part drawn alone");
  check(right >= 0 && right < rightmostLit(full, sw), what + "the rest of the line is hidden");
}

void revealMatchesSubstring(Renderer& r, SDL_Surface* surface)
{
  const Pixels full = shot(r, surface, [&] { r.drawText(kLine, kX, kY, kStyle); });
  for (const std::size_t bytes : {4, 9, 12, 20, 5, 15})
    checkCut(r, surface, kStyle, bytes, full);
  const Pixels fullItalic = shot(r, surface, [&] { r.drawText(kLine, kX, kY, kItalic); });
  for (const std::size_t bytes : {4, 9, 5, 15})
    checkCut(r, surface, kItalic, bytes, fullItalic);
  for (const std::size_t bytes : {6, 16, 26, 32})
  {
    checkSpaceCut(r, surface, kStyle, bytes, full);
    checkSpaceCut(r, surface, kItalic, bytes, fullItalic);
  }
}

// A prefix that would end inside a UTF-8 character stops before it.
void revealUtf8(Renderer& r, SDL_Surface* surface)
{
  // "Ça va — très bien": Ç is bytes 0-1, the dash bytes 7-9.
  const std::string u = std::string("\xC3\x87") + "a va " + "\xE2\x80\x94" + " tr" + "\xC3\xA8" + "s bien";
  const Pixels blank = shot(r, surface, [] {});
  float w = -1.0f;
  const Pixels half = shot(r, surface, [&] { w = r.drawTextPrefix(u, 1, kX, kY, kStyle); });
  check(w == 0.0f && half == blank, "half of the first character draws nothing");
  check(r.drawTextPrefix(u, 2, kX, kY, kStyle) > 0.0f, "the whole first character draws");
  float w7 = 0.0f, w8 = 0.0f, w9 = 0.0f;
  const Pixels p7 = shot(r, surface, [&] { w7 = r.drawTextPrefix(u, 7, kX, kY, kStyle); });
  const Pixels p8 = shot(r, surface, [&] { w8 = r.drawTextPrefix(u, 8, kX, kY, kStyle); });
  const Pixels p9 = shot(r, surface, [&] { w9 = r.drawTextPrefix(u, 9, kX, kY, kStyle); });
  check(w8 == w7 && w9 == w7, "a cut inside the dash measures as before it");
  check(p8 == p7 && p9 == p7, "a cut inside the dash draws as before it");
  check(r.drawTextPrefix(u, 10, kX, kY, kStyle) > w7, "the whole dash draws");
  // A cut inside the line's last character: "Café", é is bytes 3-4.
  const std::string cafe = std::string("Caf") + "\xC3\xA9";
  float w3 = 0.0f, w4 = 0.0f;
  const Pixels p3 = shot(r, surface, [&] { w3 = r.drawTextPrefix(cafe, 3, kX, kY, kStyle); });
  const Pixels p4 = shot(r, surface, [&] { w4 = r.drawTextPrefix(cafe, 4, kX, kY, kStyle); });
  const Pixels p5 = shot(r, surface, [&] { r.drawTextPrefix(cafe, 5, kX, kY, kStyle); });
  const Pixels whole = shot(r, surface, [&] { r.drawText(cafe, kX, kY, kStyle); });
  check(w4 == w3 && p4 == p3, "a cut inside the last character draws as before it");
  check(p5 == whole && p4 != whole, "the whole last character draws the whole line");
}

// drawRegion scaled: the cut columns scale with the texture, from the
// scaled anchor.
void drawRegionScaled(Renderer& r, SDL_Surface* surface)
{
  constexpr int tw = 8, th = 4;
  const auto colour = [](int x) {
    return 0xFF000000u | (Uint32(30 * (x + 1)) << 16) | (Uint32(200 - 20 * x) << 8) | 0x40u;
  };
  std::vector<Uint32> px(std::size_t(tw * th));
  for (int y = 0; y < th; ++y)
    for (int x = 0; x < tw; ++x)
      px[std::size_t(y * tw + x)] = colour(x);
  Texture t = r.createTexture(tw, th, reinterpret_cast<const unsigned char*>(px.data()), tw * 4, 2.0f, 1.0f);
  SDL_SetTextureScaleMode(t.get(), SDL_ScaleModeNearest);
  DrawOpts o;
  o.scale = 3.0f;
  const Pixels p = shot(r, surface, [&] { r.drawRegion(t, 200.0f, 200.0f, 5, o); });
  check(litBox(p, surface->w) == std::vector<int>{194, 197, 208, 208},
    "scaled 3x, 5 of 8 columns cover 15 x 12 px from the scaled anchor");
  bool inOrder = true;
  for (int k = 0; k < 5; ++k) // the middle of each column's 3 px
    inOrder = inOrder && (p[std::size_t(203 * surface->w + 194 + 3 * k + 1)] & 0xFFFFFFu) == (colour(k) & 0xFFFFFFu);
  check(inOrder, "the shown columns keep their order and colours");
}

} // namespace

int main()
{
  SDL_Init(0);
  loadGameFont(GR_DATA_DIR "/fonts/DejaVuSans-Bold.ttf");
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  evictsButKeepsTheHotString(sdl);
  evictsOldestFirst(sdl);
  boundEdges(sdl);
  heldReferenceSurvives(sdl);
  revealCachesOneTexture(sdl);
  {
    Renderer r(sdl);
    evictedStringReturns(r, surface);
  }
  {
    Renderer r(sdl);
    revealEnds(r, surface);
    revealMatchesSubstring(r, surface);
    revealUtf8(r, surface);
    drawRegionScaled(r, surface);
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures ? "FAILED" : "all passed");
  return gFailures ? 1 : 0;
}
