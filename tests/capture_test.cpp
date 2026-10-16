// The renderer's scene capture (Level 41's MIRROR MODE and BIG HEADS): what
// is drawn while capturing comes back on the screen pixel for pixel, mirrored
// left to right if asked; a piece of it can be grabbed (on the GPU, no read
// back) and drawn again; the faded oval BIG HEADS draws its lens with. With a
// logical size the capture works at the output resolution, letterbox bars
// left alone. And the misuse: ending or grabbing without a capture, a second
// begin, grabs off the picture, a capture left open at the next frame.

#include "render/renderer.hpp"

#include <SDL.h>

#include <cstdio>
#include <cstring>
#include <vector>

using namespace gr;

namespace
{

int gFailures = 0;

void check(bool ok, const char* what)
{
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok)
    ++gFailures;
}

using Frame = std::vector<Uint32>;

// The surface once the queued draws have reached it.
Frame present(SDL_Renderer* sdl, SDL_Surface* surface)
{
  SDL_RenderPresent(sdl);
  Frame f(std::size_t(surface->w) * std::size_t(surface->h));
  for (int y = 0; y < surface->h; ++y)
    std::memcpy(&f[std::size_t(y) * std::size_t(surface->w)],
      static_cast<const unsigned char*>(surface->pixels) + y * surface->pitch, std::size_t(surface->w) * 4);
  return f;
}

Uint32 at(const Frame& f, int x, int y)
{
  return f[std::size_t(y) * std::size_t(kScreenW) + std::size_t(x)];
}

Frame mirrored(const Frame& f)
{
  Frame m(f.size());
  for (int y = 0; y < kScreenH; ++y)
    for (int x = 0; x < kScreenW; ++x)
      m[std::size_t(y) * std::size_t(kScreenW) + std::size_t(x)] = at(f, kScreenW - 1 - x, y);
  return m;
}

int red(Uint32 p)
{
  return int((p >> 16) & 255u);
}

// Something lopsided: a red block on the left, a green bar on the right, a
// half-transparent blue square over the red, a white diagonal.
void pattern(Renderer& r)
{
  r.fillRect(40.0f, 60.0f, 200.0f, 300.0f, rgb(220, 40, 40));
  r.fillRect(700.0f, 400.0f, 500.0f, 80.0f, rgb(40, 200, 60));
  r.fillRect(150.0f, 200.0f, 300.0f, 300.0f, rgba(40, 80, 255, 128));
  r.drawLine(10.0f, 700.0f, 900.0f, 30.0f, 6.0f, rgb(255, 255, 255));
}

void testRoundTrip(Renderer& r, SDL_Renderer* sdl, SDL_Surface* surface)
{
  r.clear(rgb(0, 0, 0));
  pattern(r);
  const Frame direct = present(sdl, surface);
  check(direct != mirrored(direct), "the pattern is lopsided");

  r.clear(rgb(0, 0, 0));
  const bool began = r.beginCapture();
  const bool on = r.capturing();
  pattern(r);
  const bool ended = r.endCapture(true);
  const Frame flipped = present(sdl, surface);
  check(began && on && ended && !r.capturing(), "a capture begins and ends");
  check(flipped == mirrored(direct), "a mirrored capture is the picture flipped left to right, pixel for pixel");

  r.clear(rgb(0, 0, 0));
  r.beginCapture();
  pattern(r);
  r.endCapture(false);
  check(present(sdl, surface) == direct, "an unmirrored capture is the picture as drawn");

  // After the capture the draws reach the screen again.
  r.clear(rgb(0, 0, 0));
  r.beginCapture();
  r.endCapture(true);
  r.fillRect(0.0f, 0.0f, 10.0f, 10.0f, rgb(255, 255, 255));
  check(at(present(sdl, surface), 5, 5) == 0xFFFFFFFFu, "draws after a capture reach the screen");
}

void testGrab(Renderer& r, SDL_Renderer* sdl, SDL_Surface* surface)
{
  r.clear(rgb(0, 0, 0));
  r.beginCapture();
  pattern(r);
  const Texture* g = r.grabCaptured(100.0f, 150.0f, 200.0f, 100.0f);
  const bool sized = g && g->w() == 200 && g->h() == 100;
  if (g)
    r.draw(*g, 900.0f, 550.0f);
  r.endCapture(false);
  const Frame f = present(sdl, surface);
  bool same = g != nullptr;
  for (int y = 0; y < 100 && same; ++y)
    for (int x = 0; x < 200 && same; ++x)
      same = at(f, 900 + x, 550 + y) == at(f, 100 + x, 150 + y);
  check(sized, "a grab is the size asked for");
  check(same, "a grab drawn again is a copy of that piece of the picture");

  // Grabs that cannot be: no capture, off the picture, empty.
  check(r.grabCaptured(0.0f, 0.0f, 10.0f, 10.0f) == nullptr, "no grab without a capture");
  r.beginCapture();
  check(r.grabCaptured(-1.0f, 0.0f, 10.0f, 10.0f) == nullptr, "no grab over the left edge");
  check(r.grabCaptured(1275.0f, 0.0f, 10.0f, 10.0f) == nullptr, "no grab over the right edge");
  check(r.grabCaptured(0.0f, 715.0f, 10.0f, 10.0f) == nullptr, "no grab over the bottom");
  check(r.grabCaptured(0.0f, 0.0f, 0.0f, 10.0f) == nullptr, "no empty grab");
  check(r.capturing(), "a refused grab leaves the capture on");
  r.endCapture(false);
}

void testMisuse(Renderer& r, SDL_Renderer* sdl, SDL_Surface* surface)
{
  r.clear(rgb(0, 0, 0));
  pattern(r);
  const Frame direct = present(sdl, surface);

  // Ending without a capture: nothing happens.
  r.clear(rgb(0, 0, 0));
  pattern(r);
  check(!r.endCapture(true) && present(sdl, surface) == direct, "ending without a capture changes nothing");

  // A second begin is refused; the first capture carries on.
  r.clear(rgb(0, 0, 0));
  const bool first = r.beginCapture();
  const bool second = r.beginCapture();
  pattern(r);
  const bool ended = r.endCapture(true);
  const bool again = r.endCapture(true);
  check(first && !second && ended && !again, "one capture at a time, ended once");
  check(present(sdl, surface) == mirrored(direct), "the refused second begin did not disturb the first");

  // A capture left open is dropped at the next frame.
  r.beginCapture();
  r.beginFrame();
  const bool dropped = !r.capturing();
  r.clear(rgb(0, 0, 0));
  r.fillRect(0.0f, 0.0f, 10.0f, 10.0f, rgb(255, 255, 255));
  check(dropped && at(present(sdl, surface), 5, 5) == 0xFFFFFFFFu, "beginFrame drops a capture left open");
}

// 640 x 480 letterboxed on the 1280 x 720 surface: 1.5x, bars of 160 px on
// either side. The capture is the 960 x 720 picture, mirrored inside it.
void testLogicalSize(Renderer& r, SDL_Renderer* sdl, SDL_Surface* surface)
{
  SDL_RenderSetLogicalSize(sdl, 640, 480);
  r.clear(rgb(0, 0, 0));
  r.beginCapture();
  r.fillRect(0.0f, 0.0f, 100.0f, 100.0f, rgb(255, 0, 0));
  const Texture* g = r.grabCaptured(0.0f, 0.0f, 10.0f, 10.0f);
  const bool scaled = g && g->w() == 15 && g->h() == 15;
  const bool offPicture = r.grabCaptured(600.0f, 0.0f, 50.0f, 10.0f) == nullptr;
  r.endCapture(true);
  const Frame f = present(sdl, surface);
  SDL_RenderSetLogicalSize(sdl, 0, 0);
  SDL_RenderSetViewport(sdl, nullptr);
  SDL_RenderSetScale(sdl, 1.0f, 1.0f);
  // The block at logical 0..100 lands at 540..640 mirrored: pixels 970..1120.
  check(scaled && offPicture, "grabs are at the output resolution, within the picture");
  check(red(at(f, 1040, 70)) == 255 && red(at(f, 960, 70)) == 0 && red(at(f, 1125, 70)) == 0 && red(at(f, 200, 70)) == 0,
    "with a logical size the picture is mirrored inside the letterbox");
  check(at(f, 100, 70) == at(f, 1200, 70) && red(at(f, 1200, 70)) == 0, "the letterbox bars stay as they were");
}

// A white square drawn as BIG HEADS' faded oval over 400 x 200 at (100, 100):
// solid out to 3/4 of the oval in u² + v², fading to nothing at its edge.
void testFadedOval(Renderer& r, SDL_Renderer* sdl, SDL_Surface* surface)
{
  const std::vector<Uint32> white(64 * 64, 0xFFFFFFFFu);
  const Texture tex =
    r.createTexture(64, 64, reinterpret_cast<const unsigned char*>(white.data()), 64 * 4, 0.0f, 0.0f);
  r.clear(rgb(0, 0, 0));
  r.drawFadedOval(tex, 100.0f, 100.0f, 400.0f, 200.0f, 0.75f);
  const Frame f = present(sdl, surface);
  const int centre = red(at(f, 300, 200)), inner = red(at(f, 455, 200)), fading = red(at(f, 486, 200));
  const int corner = red(at(f, 104, 104)), outside = red(at(f, 90, 200)), below = red(at(f, 300, 305));
  std::printf("     oval: centre %d, u 0.775 %d, u 0.93 %d, corner %d\n", centre, inner, fading, corner);
  check(centre == 255 && inner >= 250, "the oval is solid in the middle");
  check(fading > 100 && fading < 180, "it fades out toward its edge");
  check(corner == 0 && outside == 0 && below == 0, "nothing past the oval");

  // Nothing to draw: no texture, an empty rectangle.
  r.clear(rgb(0, 0, 0));
  r.drawFadedOval(Texture(), 100.0f, 100.0f, 400.0f, 200.0f, 0.75f);
  r.drawFadedOval(tex, 100.0f, 100.0f, 0.0f, 200.0f, 0.75f);
  const Frame none = present(sdl, surface);
  bool black = true;
  for (const Uint32 p : none)
    black = black && (p & 0x00FFFFFFu) == 0;
  check(black, "no oval without a texture or a size");
}

} // namespace

int main()
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer r(sdl);
    testRoundTrip(r, sdl, surface);
    testGrab(r, sdl, surface);
    testMisuse(r, sdl, surface);
    testLogicalSize(r, sdl, surface);
    testRoundTrip(r, sdl, surface); // the capture made again at the full size
    testFadedOval(r, sdl, surface);
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures == 0 ? "all capture tests passed\n" : "%d capture test(s) failed\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
