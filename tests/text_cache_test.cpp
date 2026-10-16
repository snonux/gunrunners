// The renderer's text cache and the typewriter reveal (re3):
//   - past kTextCacheMax strings beginFrame evicts the least recently drawn
//     down to kTextCacheKeep, never a string the previous frame drew (it
//     once cleared everything, and the next frame re-rasterized a screenful
//     of text at once);
//   - drawTextPrefix reveals a line from the one texture of the whole line:
//     a cutscene line typed out two letters at a time caches one texture,
//     not one per step, and looks like the prefix drawn on its own;
//   - a prefix never ends inside a UTF-8 character.

#include "render/renderer.hpp"

#include <SDL.h>

#include <algorithm>
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

// Whether a and b agree in every column left of `limit`.
bool sameLeftOf(const Pixels& a, const Pixels& b, int w, int limit)
{
  for (std::size_t i = 0; i < a.size(); ++i)
    if (int(i % std::size_t(w)) < limit && a[i] != b[i])
      return false;
  return true;
}

const std::string kLine = "Three couriers. Forty-two jobs. One city that never sleeps and never pays on time.";
const TextStyle kStyle{28.0f, rgb(255, 255, 255), rgb(255, 0, 0)};
constexpr float kX = 100.0f, kY = 300.0f;

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

// A part of the line looks like that part drawn on its own: identical left
// of the next glyph's outline, and ending where it ends (the clip cuts at
// most the last letter's outline or adds a sliver of the next one's).
void revealMatchesSubstring(Renderer& r, SDL_Surface* surface)
{
  const Pixels full = shot(r, surface, [&] { r.drawText(kLine, kX, kY, kStyle); });
  for (const std::size_t bytes : {4, 9, 12, 20})
  {
    const std::string part = kLine.substr(0, bytes);
    const std::string what = "\"" + part + "\": ";
    check(kLine[bytes - 1] != ' ' && kLine[bytes] != ' ', what + "a cut between letters");
    float w = 0.0f;
    const Pixels p = shot(r, surface, [&] { w = r.drawTextPrefix(kLine, bytes, kX, kY, kStyle); });
    const Pixels sub = shot(r, surface, [&] { r.drawText(part, kX, kY, kStyle); });
    const int right = rightmostLit(p, surface->w);
    check(sameLeftOf(p, sub, surface->w, int(kX + w) - 4), what + "the same pixels as the part drawn alone");
    check(right >= 0 && right <= int(kX + w) + 1, what + "nothing right of the clip");
    check(std::abs(right - rightmostLit(sub, surface->w)) <= 6, what + "ends where the part drawn alone ends");
    check(right < rightmostLit(full, surface->w), what + "the rest of the line is hidden");
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
}

} // namespace

int main()
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  evictsButKeepsTheHotString(sdl);
  evictsOldestFirst(sdl);
  boundEdges(sdl);
  revealCachesOneTexture(sdl);
  {
    Renderer r(sdl);
    revealEnds(r, surface);
    revealMatchesSubstring(r, surface);
    revealUtf8(r, surface);
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures ? "FAILED" : "all passed");
  return gFailures ? 1 : 0;
}
