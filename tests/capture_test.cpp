// The renderer's scene capture (Level 41's MIRROR MODE and BIG HEADS): what
// is drawn while capturing comes back on the screen pixel for pixel, mirrored
// left to right if asked; a piece of it can be grabbed (on the GPU, no read
// back) and drawn again; the faded oval BIG HEADS draws its lens with. With a
// logical size the capture works at the output resolution, is mirrored
// inside the letterbox (against a frame drawn directly and flipped by hand),
// letterbox bars left alone, and draws after it land in the letterbox again;
// a clip rectangle survives a grab at a scale of 4/3. The read-back fallbacks for a
// refused capture: the picture mirrored, the lens. And the misuse: ending or
// grabbing without a capture, a second begin, a begin with another render
// target set, grabs off the picture, a capture left open at the next frame.

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

// Back to drawing 1:1 on the whole surface after a test with a logical size.
void noLogicalSize(SDL_Renderer* sdl)
{
  SDL_RenderSetLogicalSize(sdl, 0, 0);
  SDL_RenderSetViewport(sdl, nullptr);
  SDL_RenderSetScale(sdl, 1.0f, 1.0f);
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

  // Another render target set: refused, that target kept.
  SDL_Texture* other = SDL_CreateTexture(sdl, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 16, 16);
  SDL_SetRenderTarget(sdl, other);
  const bool refused = !r.beginCapture() && !r.capturing() && SDL_GetRenderTarget(sdl) == other;
  SDL_SetRenderTarget(sdl, nullptr);
  SDL_DestroyTexture(other);
  check(other != nullptr && refused, "no capture while another render target is set");

  // The draw colour is the caller's still.
  SDL_SetRenderDrawColor(sdl, 12, 34, 56, 78);
  r.beginCapture();
  Uint8 c[4] = {};
  SDL_GetRenderDrawColor(sdl, &c[0], &c[1], &c[2], &c[3]);
  r.endCapture(false);
  check(c[0] == 12 && c[1] == 34 && c[2] == 56 && c[3] == 78, "beginCapture leaves the draw colour as it was");

  // A capture left open is dropped at the next frame.
  r.beginCapture();
  r.beginFrame();
  const bool dropped = !r.capturing();
  r.clear(rgb(0, 0, 0));
  r.fillRect(0.0f, 0.0f, 10.0f, 10.0f, rgb(255, 255, 255));
  check(dropped && at(present(sdl, surface), 5, 5) == 0xFFFFFFFFu, "beginFrame drops a capture left open");
}

// `f` with output columns x0..x1 - 1 flipped left to right among
// themselves, the rest as it is.
Frame mirroredWithin(const Frame& f, int x0, int x1)
{
  Frame m = f;
  for (int y = 0; y < kScreenH; ++y)
    for (int x = x0; x < x1; ++x)
      m[std::size_t(y) * std::size_t(kScreenW) + std::size_t(x)] = at(f, x0 + x1 - 1 - x, y);
  return m;
}

// `got` is `want` moved `d` columns left, over columns x0 + 1..x1 - 2 (the
// outermost column on either side is left out: a move brings in one from
// outside).
bool movedBy(const Frame& got, const Frame& want, int x0, int x1, int d)
{
  for (int y = 0; y < kScreenH; ++y)
    for (int x = x0 + 1; x < x1 - 1; ++x)
      if (at(got, x, y) != at(want, x + d, y))
        return false;
  return true;
}

// What testLogicalMirror draws: the capture's black over the picture, a
// red block at its top left and a green bar lower down, right of centre.
void letterboxScene(Renderer& r)
{
  r.fillRect(0.0f, 0.0f, 640.0f, 480.0f, rgb(0, 0, 0));
  r.fillRect(0.0f, 0.0f, 100.0f, 100.0f, rgb(255, 0, 0));
  r.fillRect(400.0f, 300.0f, 120.0f, 40.0f, rgb(0, 255, 0));
}

// The scene captured with the logical size of testLogicalSize, put back on
// the blue-grey surface.
Frame capturedScene(Renderer& r, SDL_Renderer* sdl, SDL_Surface* surface, bool mirrorX)
{
  r.clear(rgb(30, 60, 90));
  r.beginCapture();
  letterboxScene(r);
  r.endCapture(mirrorX);
  return present(sdl, surface);
}

// The capture against the scene drawn directly, inside the letterbox (the
// picture is output columns 160..1119). Put back unmirrored it is that
// frame exactly: the copy lands on the picture to the pixel. Mirrored it is
// that frame flipped by hand within the picture. SDL3's software renderer
// (under sdl2-compat) lands a copy it scales and flips at once one column
// left, so the mirrored one may be off by that column; one not mirrored, or
// at the wrong scale or offset, is off by far more.
void testLogicalMirror(Renderer& r, SDL_Renderer* sdl, SDL_Surface* surface)
{
  SDL_RenderSetLogicalSize(sdl, 640, 480);
  r.clear(rgb(30, 60, 90));
  letterboxScene(r);
  const Frame direct = present(sdl, surface);
  const Frame plain = capturedScene(r, sdl, surface, false);
  const Frame flipped = capturedScene(r, sdl, surface, true);
  noLogicalSize(sdl);
  const Frame want = mirroredWithin(direct, 160, 1120);
  int moved = 2;
  for (int d = 1; d >= -1 && moved == 2; --d)
    if (movedBy(flipped, want, 160, 1120, d))
      moved = d;
  if (moved == 2)
    std::printf("     mirrored: no match within a column\n");
  else
    std::printf("     mirrored: off by %d column(s)\n", moved);
  check(plain == direct, "with a logical size an unmirrored capture is the picture as drawn");
  check(want != direct && moved != 2, "with a logical size the picture is mirrored inside the letterbox");
}

// 640 x 480 letterboxed on the 1280 x 720 surface: 1.5x, bars of 160 px on
// either side. The capture is the 960 x 720 picture (testLogicalMirror
// checks it mirrored inside it). The surface is cleared to blue-grey first,
// so the bars are not the capture's black; they are compared with a frame
// cleared the same way, as SDL may paint letterbox bars itself.
void testLogicalSize(Renderer& r, SDL_Renderer* sdl, SDL_Surface* surface)
{
  SDL_RenderSetLogicalSize(sdl, 640, 480);
  r.clear(rgb(30, 60, 90));
  const Frame bars = present(sdl, surface);
  r.clear(rgb(30, 60, 90));
  r.beginCapture();
  r.fillRect(0.0f, 0.0f, 100.0f, 100.0f, rgb(255, 0, 0));
  const Texture* g = r.grabCaptured(0.0f, 0.0f, 10.0f, 10.0f);
  const bool scaled = g && g->w() == 15 && g->h() == 15;
  const bool offPicture = r.grabCaptured(600.0f, 0.0f, 50.0f, 10.0f) == nullptr;
  r.endCapture(true);
  r.fillRect(0.0f, 0.0f, 10.0f, 10.0f, rgb(255, 255, 255)); // after the capture: pixels 160..175
  const Frame f = present(sdl, surface);
  noLogicalSize(sdl);
  check(scaled && offPicture, "grabs are at the output resolution, within the picture");
  std::printf("     bars: %08X, cleared %08X\n", unsigned(at(f, 100, 70)), unsigned(at(bars, 100, 70)));
  check(at(f, 100, 70) == at(bars, 100, 70) && at(f, 1200, 70) == at(bars, 1200, 70) && at(f, 155, 5) == at(bars, 155, 5),
    "the letterbox bars stay as they were");
  check(at(f, 167, 7) == 0xFFFFFFFFu && at(f, 177, 7) != 0xFFFFFFFFu && at(f, 167, 17) != 0xFFFFFFFFu,
    "draws after the capture land in the letterboxed picture");
}

// 960 x 540 on the 1280 x 720 surface: 4/3, where SDL2 rounds a clip
// rectangle kept in pixels. One set before a grab (which switches render
// targets) is the same after it and still clips: logical 101..304 x
// 37..188 is pixels 134..405 x 49..251.
void testClipAcrossGrab(Renderer& r, SDL_Renderer* sdl, SDL_Surface* surface)
{
  SDL_RenderSetLogicalSize(sdl, 960, 540);
  r.clear(rgb(0, 0, 0));
  r.beginCapture();
  const SDL_Rect clip{101, 37, 203, 151};
  SDL_RenderSetClipRect(sdl, &clip);
  SDL_Rect before{}, after{};
  SDL_RenderGetClipRect(sdl, &before);
  const bool grabbed = r.grabCaptured(400.0f, 300.0f, 31.0f, 17.0f) != nullptr;
  SDL_RenderGetClipRect(sdl, &after);
  const bool on = SDL_RenderIsClipEnabled(sdl) == SDL_TRUE;
  r.fillRect(0.0f, 0.0f, 960.0f, 540.0f, rgb(255, 255, 255));
  SDL_RenderSetClipRect(sdl, nullptr);
  r.endCapture(false);
  const Frame f = present(sdl, surface);
  noLogicalSize(sdl);
  std::printf("     clip before %d,%d %dx%d, after %d,%d %dx%d\n", before.x, before.y, before.w, before.h, after.x, after.y,
    after.w, after.h);
  check(grabbed && on && SDL_RectEquals(&before, &after) == SDL_TRUE, "a grab leaves the clip rectangle as it was");
  check(at(f, 200, 100) == 0xFFFFFFFFu && at(f, 400, 245) == 0xFFFFFFFFu && red(at(f, 130, 100)) == 0 &&
      red(at(f, 410, 100)) == 0 && red(at(f, 200, 45)) == 0 && red(at(f, 200, 256)) == 0,
    "the clip rectangle still clips after the grab");
}

// The pattern with BIG HEADS' lens: a piece of its red block (60, 80, 100 x
// 100) drawn again over (950, 100), 200 x 200, faded out to an oval.
Frame lensFrame(Renderer& r, SDL_Renderer* sdl, SDL_Surface* surface, bool& drawn)
{
  r.clear(rgb(0, 0, 0));
  r.beginCapture();
  pattern(r);
  drawn = r.drawLens({60.0f, 80.0f, 100.0f, 100.0f}, {950.0f, 100.0f, 200.0f, 200.0f}, 0.75f);
  const bool off = !r.drawLens({-5.0f, 80.0f, 100.0f, 100.0f}, {950.0f, 400.0f, 200.0f, 200.0f}, 0.75f);
  drawn = drawn && off;
  r.endCapture(false);
  return present(sdl, surface);
}

bool lensLooksRight(const Frame& f)
{
  const int centre = red(at(f, 1050, 200)), corner = red(at(f, 953, 103)), beside = red(at(f, 940, 200));
  std::printf("     lens: centre %d, corner %d, beside %d\n", centre, corner, beside);
  return centre >= 210 && corner == 0 && beside == 0;
}

// A renderer that refuses the capture (no render targets, no texture that
// big): MIRROR MODE's picture and BIG HEADS' lens read back from the screen.
void testReadBack(Renderer& r, SDL_Renderer* sdl, SDL_Surface* surface)
{
  r.clear(rgb(0, 0, 0));
  pattern(r);
  const Frame direct = present(sdl, surface);

  r.refuseCaptures(true);
  r.clear(rgb(0, 0, 0));
  const bool refused = !r.beginCapture() && !r.capturing();
  pattern(r);
  r.mirrorReadBack();
  check(refused && present(sdl, surface) == mirrored(direct), "a refused capture: the picture mirrored, read back");

  bool drawn = false;
  const Frame readBack = lensFrame(r, sdl, surface, drawn);
  check(drawn && lensLooksRight(readBack), "a refused capture: the lens read back, faded out to an oval");
  r.refuseCaptures(false);
  const Frame gpu = lensFrame(r, sdl, surface, drawn);
  check(drawn && lensLooksRight(gpu), "the lens from the capture, faded out to an oval");
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
    testLogicalMirror(r, sdl, surface);
    testClipAcrossGrab(r, sdl, surface);
    testRoundTrip(r, sdl, surface); // the capture made again at the full size
    testReadBack(r, sdl, surface);
    testFadedOval(r, sdl, surface);
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures == 0 ? "all capture tests passed\n" : "%d capture test(s) failed\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
