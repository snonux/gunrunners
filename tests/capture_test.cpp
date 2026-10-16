// The renderer's scene capture (Level 41's MIRROR MODE and BIG HEADS): what
// is drawn while capturing comes back on the screen pixel for pixel, mirrored
// left to right if asked; a piece of it can be grabbed (on the GPU, no read
// back) and drawn again; the faded oval BIG HEADS draws its lens with. With a
// logical size the capture works at the output resolution, is mirrored
// inside the letterbox (against a frame drawn directly and flipped by hand),
// letterbox bars left alone, and draws after it land in the letterbox again;
// a clip rectangle survives a grab at a scale of 4/3; at scales that are no
// whole number (16/15, 1.3125) the capture covers the picture to the pixel,
// mirrored too, and the read-back fallback reads it row for row. The
// read-back fallbacks for a refused capture: the picture mirrored, the lens.
// And the misuse: ending or grabbing without a capture, a second begin, a
// begin with another render target set, grabs off the picture, a capture
// left open at the next frame. And telling sdl2-compat (SDL2's API on top of
// SDL3, which letterboxes differently) from SDL2, also when the
// SDL_LEGACY_VERSION hint makes both report the old 2.0.x numbering.
// And render resets (af3): SDL_RENDER_TARGETS_RESET drops the capture's
// textures, SDL_RENDER_DEVICE_RESET those, the text cache and the alpha
// grid's texture too, and the game bakes its art and panels again; a frame
// drawn after either matches one drawn before. Other events do nothing, and
// a reset with nothing held, or with a capture open, is harmless.

#include "frontend/game.hpp"
#include "render/renderer.hpp"
#include "render/renderer_detail.hpp"

#include <SDL.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
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
// frame exactly, and mirrored that frame flipped by hand within the
// picture, exactly too: the capture is flipped at scale 1 into a texture of
// its own and put on the screen with a plain copy. A copy flipped at the
// letterbox's scale would not be: SDL2's software renderer adds the bar
// (160 pixels) to the copy's rectangle in logical units and scales the sum,
// putting the picture 80 columns right, and SDL3's rounds the bar down to
// whole logical units first ((int)(160 / 1.5) * 1.5 = 159), a column left.
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
  check(plain == direct, "with a logical size an unmirrored capture is the picture as drawn");
  check(want != direct && flipped == want, "with a logical size the picture is mirrored inside the letterbox, exactly");
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

// Frames of any size: the pixel at (x, y) of one `w` pixels wide.
Uint32 atOf(const Frame& f, int w, int x, int y)
{
  return f[std::size_t(y) * std::size_t(w) + std::size_t(x)];
}

// `f` (`w` pixels wide) with the picture `pic` flipped left to right within
// it, the rest as it is.
Frame mirroredIn(const Frame& f, int w, const SDL_Rect& pic)
{
  Frame m = f;
  for (int y = pic.y; y < pic.y + pic.h; ++y)
    for (int x = pic.x; x < pic.x + pic.w; ++x)
      m[std::size_t(y) * std::size_t(w) + std::size_t(x)] = atOf(f, w, 2 * pic.x + pic.w - 1 - x, y);
  return m;
}

// What testOddLetterbox draws over the whole 1280 x 720 picture: black, a
// red block at the top left, a green bar right of centre, a white strip
// along the bottom left.
void oddScene(Renderer& r)
{
  r.fillRect(0.0f, 0.0f, 1280.0f, 720.0f, rgb(0, 0, 0));
  r.fillRect(0.0f, 0.0f, 200.0f, 150.0f, rgb(255, 0, 0));
  r.fillRect(800.0f, 500.0f, 300.0f, 60.0f, rgb(0, 255, 0));
  r.fillRect(0.0f, 690.0f, 640.0f, 30.0f, rgb(255, 255, 255));
}

enum class Way
{
  Bars,     // the surface cleared, nothing drawn
  Direct,   // the scene drawn on the screen
  Captured, // through a capture
  Mirrored, // through a capture, mirrored
  ReadBack, // on the screen, mirrored by the read-back fallback
};

Frame oddFrame(Renderer& r, SDL_Renderer* sdl, SDL_Surface* surface, Way way)
{
  r.clear(rgb(30, 60, 90));
  if (way == Way::Captured || way == Way::Mirrored)
    r.beginCapture();
  if (way != Way::Bars)
    oddScene(r);
  r.endCapture(way == Way::Mirrored); // nothing without a capture
  if (way == Way::ReadBack)
    r.mirrorReadBack();
  return present(sdl, surface);
}

// 1280 x 720 on an ow x oh surface at a scale that is no whole number, its
// picture `pic` in output pixels as SDL letterboxes it: the side that fits
// sets the scale, the other is rounded down and centred. SDL2 hands the
// viewport back in whole logical units, rounded down, so the picture taken
// from them is a pixel short (1364 for 1365 columns at 16/15) or its bar a
// row short (51 for 52 at 1.3125). The capture covers the picture to its
// last column and row, as drawn; mirrored, and by the read-back fallback,
// it is the picture flipped within itself, row for row. (Bars of an even
// width: an odd one puts SDL3's picture half a pixel off the pixel grid, as
// it centres in float, and a direct draw and a copy may round apart there.)
void testOddLetterbox(int ow, int oh, const SDL_Rect& pic)
{
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, ow, oh, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer r(sdl);
    SDL_RenderSetLogicalSize(sdl, kScreenW, kScreenH);
    const Frame bars = oddFrame(r, sdl, surface, Way::Bars), direct = oddFrame(r, sdl, surface, Way::Direct);
    const Frame plain = oddFrame(r, sdl, surface, Way::Captured), mirrored = oddFrame(r, sdl, surface, Way::Mirrored);
    const Frame readBack = oddFrame(r, sdl, surface, Way::ReadBack);
    const Frame want = mirroredIn(direct, ow, pic);
    const int right = pic.x + pic.w - 1, bottom = pic.y + pic.h - 1, midY = pic.y + pic.h / 2;
    const bool edges = atOf(plain, ow, right, midY) != atOf(bars, ow, right, midY) &&
      atOf(plain, ow, pic.x + 700, bottom) != atOf(bars, ow, pic.x + 700, bottom);
    const bool barsKept = (pic.x == 0 || atOf(plain, ow, pic.x - 1, midY) == atOf(bars, ow, pic.x - 1, midY)) &&
      (right + 1 >= ow || atOf(plain, ow, right + 1, midY) == atOf(bars, ow, right + 1, midY)) &&
      (pic.y == 0 || atOf(plain, ow, pic.x + 700, pic.y - 1) == atOf(bars, ow, pic.x + 700, pic.y - 1)) &&
      (bottom + 1 >= oh || atOf(plain, ow, pic.x + 700, bottom + 1) == atOf(bars, ow, pic.x + 700, bottom + 1));
    std::printf("     %d x %d: picture %d,%d %dx%d\n", ow, oh, pic.x, pic.y, pic.w, pic.h);
    check(plain == direct, "at a scale that is no whole number the capture is the picture as drawn");
    check(edges && barsKept, "the capture covers the picture to its last column and row, the bars left alone");
    check(want != direct && mirrored == want, "mirrored, it is the picture flipped within itself");
    check(readBack == want, "the read-back fallback reads and mirrors the picture to the pixel");
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
}

// sdl2-compat from its version (2.x.50 and up) or, when SDL_LEGACY_VERSION
// turns that into 2.0.<minor>, from its revision; SDL2 from neither.
void testSdl2Compat()
{
  using detail::sdl2Compat;
  check(sdl2Compat({2, 32, 72}, "SDL-2.32.72-gfb5d9a5"), "sdl2-compat by its version");
  check(sdl2Compat({2, 0, 32}, "SDL-2.32.72-gfb5d9a5 (Fedora)"), "sdl2-compat under the legacy hint, by its revision");
  check(sdl2Compat({2, 0, 30}, "SDL-2.30.50-no-vcs"), "sdl2-compat built without git, under the legacy hint");
  check(!sdl2Compat({2, 30, 9}, "SDL-release-2.30.9-0-gc98c4fbff"), "SDL2 is not sdl2-compat");
  check(!sdl2Compat({2, 0, 32}, "SDL-release-2.32.10-0-g5d24957"), "SDL2 under the legacy hint is not sdl2-compat");
  check(!sdl2Compat({2, 0, 32}, "SDL-2.32.10-no-vcs"), "SDL2 built without git is not sdl2-compat");
  check(!sdl2Compat({2, 0, 14}, "hg-14525:e52d96ea04fc"), "an old SDL2's Mercurial revision is not sdl2-compat");
  check(!sdl2Compat({2, 0, 32}, "") && !sdl2Compat({2, 0, 32}, nullptr), "no revision: the version alone decides");
  // The SDL linked here answers the same with the hint as without.
  SDL_version v{};
  SDL_GetVersion(&v);
  const bool plain = sdl2Compat(v, SDL_GetRevision());
  SDL_SetHint("SDL_LEGACY_VERSION", "1");
  SDL_version legacy{};
  SDL_GetVersion(&legacy);
  const bool hinted = sdl2Compat(legacy, SDL_GetRevision());
  SDL_SetHint("SDL_LEGACY_VERSION", "0");
  std::printf("     SDL %d.%d.%d (legacy %d.%d.%d), %s: %s\n", v.major, v.minor, v.patch, legacy.major, legacy.minor,
    legacy.patch, SDL_GetRevision(), plain ? "sdl2-compat" : "SDL2");
  check(plain == hinted, "the SDL linked here is told apart the same under the legacy hint");
}

// The events behind RenderReset, also through SDL's queue as the game's
// loop gets them; every other event is None.
void testResetEvents()
{
  SDL_Event ev{};
  ev.type = SDL_RENDER_TARGETS_RESET;
  check(renderResetOf(ev) == RenderReset::Targets, "SDL_RENDER_TARGETS_RESET: the targets were lost");
  ev.type = SDL_RENDER_DEVICE_RESET;
  check(renderResetOf(ev) == RenderReset::Device, "SDL_RENDER_DEVICE_RESET: every texture was lost");
  bool others = true;
  const Uint32 otherTypes[] = {SDL_KEYDOWN, SDL_WINDOWEVENT, SDL_QUIT, SDL_APP_WILLENTERBACKGROUND, SDL_USEREVENT};
  for (Uint32 type : otherTypes)
  {
    ev.type = type;
    others = others && renderResetOf(ev) == RenderReset::None;
  }
  check(others, "other events are no render reset");

  SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
  SDL_Event pushed{};
  pushed.type = SDL_RENDER_DEVICE_RESET;
  SDL_PushEvent(&pushed);
  pushed.type = SDL_RENDER_TARGETS_RESET;
  SDL_PushEvent(&pushed);
  std::vector<RenderReset> got;
  while (SDL_PollEvent(&ev))
    got.push_back(renderResetOf(ev));
  check(got == std::vector<RenderReset>{RenderReset::Device, RenderReset::Targets},
    "pushed reset events come out of the queue as such");
}

// A frame with all the renderer's own textures in use: a mirrored capture
// of the pattern with a grab drawn again, a string and an alpha grid.
Frame resetScene(Renderer& r, SDL_Renderer* sdl, SDL_Surface* surface)
{
  r.beginFrame();
  r.clear(rgb(0, 0, 0));
  r.beginCapture();
  pattern(r);
  if (const Texture* g = r.grabCaptured(100.0f, 150.0f, 200.0f, 100.0f))
    r.draw(*g, 900.0f, 550.0f);
  r.drawText("DEVICE RESET", 500.0f, 600.0f, TextStyle{});
  r.drawAlphaGrid({0.0f, 0.5f, 1.0f, 0.25f, 0.75f, 0.0f}, 3, 2, 600.0f, 100.0f, 40.0f, 40.0f, rgb(255, 200, 0));
  r.endCapture(true);
  return present(sdl, surface);
}

// Renderer::recover: what each reset drops, and that the picture comes back.
void testRecover(SDL_Renderer* sdl, SDL_Surface* surface)
{
  noLogicalSize(sdl);
  Renderer r(sdl);
  // Nothing held yet: harmless.
  r.recover(RenderReset::Targets);
  r.recover(RenderReset::Device);
  check(!r.holdsCaptureTextures() && r.textCacheSize() == 0, "a reset with nothing held is harmless");
  const Frame want = resetScene(r, sdl, surface);
  check(r.holdsCaptureTextures() && r.holdsGridTexture() && r.textCacheSize() == 1, "the scene holds every kind");

  r.recover(RenderReset::None);
  check(r.holdsCaptureTextures() && r.holdsGridTexture() && r.textCacheSize() == 1, "no reset drops nothing");

  r.recover(RenderReset::Targets);
  check(!r.holdsCaptureTextures(), "a targets reset drops the capture's textures");
  check(r.holdsGridTexture() && r.textCacheSize() == 1, "a targets reset keeps the strings and the grid");
  const std::size_t begun = r.capturesBegun();
  check(resetScene(r, sdl, surface) == want && r.capturesBegun() == begun + 1,
    "after a targets reset the capture is made again, the picture as before");

  r.recover(RenderReset::Device);
  check(!r.holdsCaptureTextures() && !r.holdsGridTexture() && r.textCacheSize() == 0,
    "a device reset drops the capture's textures, the strings and the grid");
  check(resetScene(r, sdl, surface) == want, "after a device reset the picture is as before");
  check(r.holdsCaptureTextures() && r.holdsGridTexture() && r.textCacheSize() == 1,
    "after a device reset all of it is made again");

  // The read-back fallbacks' textures go too.
  r.recover(RenderReset::Targets);
  r.refuseCaptures(true);
  r.clear(rgb(0, 0, 0));
  pattern(r);
  r.mirrorReadBack();
  const bool readBackHeld = r.holdsCaptureTextures();
  r.recover(RenderReset::Targets);
  check(readBackHeld && !r.holdsCaptureTextures(), "a reset drops the read-back fallbacks' textures");
  r.refuseCaptures(false);

  // A capture open at a reset ends, unshown; the draws reach the screen.
  r.clear(rgb(0, 0, 0));
  r.beginCapture();
  pattern(r);
  r.recover(RenderReset::Device);
  const bool ended = !r.capturing() && SDL_GetRenderTarget(sdl) == nullptr;
  r.fillRect(0.0f, 0.0f, 10.0f, 10.0f, rgb(255, 255, 255));
  const Frame after = present(sdl, surface);
  check(ended && at(after, 5, 5) == 0xFFFFFFFFu && red(at(after, 100, 100)) == 0,
    "a reset ends an open capture unshown, the draws after it reach the screen");
}

// Game::recoverTextures in a level: a device reset bakes the art again and
// the frame drawn after it is the frame drawn before; a targets reset and
// no reset leave the art and the strings alone.
void testGameRecovers(SDL_Renderer* sdl, SDL_Surface* surface)
{
  noLogicalSize(sdl);
  const std::string saves = "capture_test_saves";
  std::filesystem::remove_all(saves);
  {
    Renderer r(sdl);
    GameOptions o;
    o.saveDir = saves;
    o.skipMenu = true;
    o.startLevel = 1;
    o.noCutscenes = true;
    Game g(o, r, nullptr);
    for (int i = 0; i < 90; ++i)
      g.tick(Input{});
    g.render();
    const Frame before = present(sdl, surface);
    g.render();
    check(g.world() != nullptr && present(sdl, surface) == before, "a level drawn twice is the same frame");
    const SDL_Texture* vignette = g.art().vignette.get();
    const std::size_t strings = r.textCacheSize();

    g.recoverTextures(RenderReset::None);
    g.recoverTextures(RenderReset::Targets);
    check(g.art().vignette.get() == vignette && r.textCacheSize() == strings,
      "a targets reset (or none) leaves the art and the strings alone");

    g.recoverTextures(RenderReset::Device);
    // Baked while the old textures still lived, so new ones are new handles.
    check(g.art().vignette.get() != vignette && g.art().vignette, "a device reset bakes the art again");
    check(r.textCacheSize() == 0, "a device reset empties the text cache");
    g.render();
    check(present(sdl, surface) == before, "after a device reset the level looks as before");
    check(strings > 0 && r.textCacheSize() > 0, "the strings are rasterized again as drawn");
  }
  std::filesystem::remove_all(saves);
}

} // namespace

int main()
{
  // The event queue for testResetEvents; no video needed.
  SDL_Init(SDL_INIT_EVENTS);
  testSdl2Compat();
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
  testOddLetterbox(1367, 768, {1, 0, 1365, 768});   // 16/15, side bars of 1 column
  testOddLetterbox(1680, 1049, {0, 52, 1680, 945}); // 1.3125, bars of 52 rows
  testResetEvents();
  {
    SDL_Surface* resetSurface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer* resetSdl = SDL_CreateSoftwareRenderer(resetSurface);
    testRecover(resetSdl, resetSurface);
    testGameRecovers(resetSdl, resetSurface);
    SDL_DestroyRenderer(resetSdl);
    SDL_FreeSurface(resetSurface);
  }
  SDL_Quit();
  std::printf(gFailures == 0 ? "all capture tests passed\n" : "%d capture test(s) failed\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
