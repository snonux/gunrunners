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
// grid's texture too; a frame drawn after either matches one drawn before.
// Other events do nothing, and a reset with nothing held, or with a capture
// open, is harmless. Which backends keep their SDL_Renderer through a device
// reset (Direct3D 11 and 12) and which have it made again; recoverRender's
// order (owners drop, the renderer drops, a new SDL_Renderer, owners bake),
// also when no renderer can be made. The stale-draw count that catches an
// owner left out. And the game through both paths on the software renderer
// (made again on its surface for the recreate path): a level, its pause
// menu and map, the title, runner select, the runner editor and a cutscene
// look as before, with no texture from before the reset drawn. And the
// window code's renderer (frontend/window_renderer) on a window of SDL's
// dummy video driver: made with the game's logical size and made again with
// it on a recreate, the new SDL_Renderer the one drawn with, the game's and
// the touch controls' textures made again (touch controls left out draw
// stale ones); a remake that fails drops the textures, bakes none against no
// renderer and keeps SDL's own error. Which backends own a GL context.

#include "frontend/game.hpp"
#include "frontend/touch_controls.hpp"
#include "frontend/window_renderer.hpp"
#include "render/renderer.hpp"
#include "render/renderer_detail.hpp"

#include <SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <utility>
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

// A software renderer on its own surface that a Recreate can replace, as
// the window code's remake does: the old one destroyed, a new one made the
// same way (on the same surface, no logical size).
struct Screen
{
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  int remade = 0;
  bool failRemake = false; // remake destroys the old one and makes none

  Screen() = default;
  Screen(const Screen&) = delete;
  Screen& operator=(const Screen&) = delete;
  ~Screen()
  {
    if (sdl)
      SDL_DestroyRenderer(sdl);
    SDL_FreeSurface(surface);
  }
  SDL_Renderer* remake()
  {
    SDL_DestroyRenderer(sdl);
    sdl = failRemake ? nullptr : SDL_CreateSoftwareRenderer(surface);
    ++remade;
    return sdl;
  }
};

// The renderer's own part of a reset, the way the window code answers one
// (recoverRender) with no other texture owners: Targets drops the capture's
// textures, InPlace (a device reset) all of the renderer's, None nothing.
void resetRenderer(Renderer& r, ResetAction action)
{
  recoverRender(r, action, TextureOwners{}, nullptr);
}

// What each reset drops, and that the picture comes back.
void testRecover(SDL_Renderer* sdl, SDL_Surface* surface)
{
  noLogicalSize(sdl);
  Renderer r(sdl);
  // Nothing held yet: harmless.
  resetRenderer(r, ResetAction::Targets);
  resetRenderer(r, ResetAction::InPlace);
  check(!r.holdsCaptureTextures() && r.textCacheSize() == 0, "a reset with nothing held is harmless");
  const Frame want = resetScene(r, sdl, surface);
  check(r.holdsCaptureTextures() && r.holdsGridTexture() && r.textCacheSize() == 1, "the scene holds every kind");

  resetRenderer(r, ResetAction::None);
  check(r.holdsCaptureTextures() && r.holdsGridTexture() && r.textCacheSize() == 1, "no reset drops nothing");

  resetRenderer(r, ResetAction::Targets);
  check(!r.holdsCaptureTextures(), "a targets reset drops the capture's textures");
  check(r.holdsGridTexture() && r.textCacheSize() == 1, "a targets reset keeps the strings and the grid");
  const std::size_t begun = r.capturesBegun();
  check(resetScene(r, sdl, surface) == want && r.capturesBegun() == begun + 1,
    "after a targets reset the capture is made again, the picture as before");

  resetRenderer(r, ResetAction::InPlace);
  check(!r.holdsCaptureTextures() && !r.holdsGridTexture() && r.textCacheSize() == 0,
    "a device reset drops the capture's textures, the strings and the grid");
  check(resetScene(r, sdl, surface) == want, "after a device reset the picture is as before");
  check(r.holdsCaptureTextures() && r.holdsGridTexture() && r.textCacheSize() == 1,
    "after a device reset all of it is made again");
  check(r.staleDraws() == 0, "...and none of it is a texture from before the reset");
}

// Resets around the read-back fallbacks and an open capture.
void testRecoverFallbacks(SDL_Renderer* sdl, SDL_Surface* surface)
{
  noLogicalSize(sdl);
  Renderer r(sdl);
  // The read-back fallbacks' textures go too.
  r.refuseCaptures(true);
  r.clear(rgb(0, 0, 0));
  pattern(r);
  r.mirrorReadBack();
  const bool readBackHeld = r.holdsCaptureTextures();
  resetRenderer(r, ResetAction::Targets);
  check(readBackHeld && !r.holdsCaptureTextures(), "a reset drops the read-back fallbacks' textures");
  r.refuseCaptures(false);

  // A capture open at a reset ends, unshown; the draws reach the screen.
  r.clear(rgb(0, 0, 0));
  r.beginCapture();
  pattern(r);
  resetRenderer(r, ResetAction::InPlace);
  const bool ended = !r.capturing() && SDL_GetRenderTarget(sdl) == nullptr;
  r.fillRect(0.0f, 0.0f, 10.0f, 10.0f, rgb(255, 255, 255));
  const Frame after = present(sdl, surface);
  check(ended && at(after, 5, 5) == 0xFFFFFFFFu && red(at(after, 100, 100)) == 0,
    "a reset ends an open capture unshown, the draws after it reach the screen");
}

// The decision the window code takes (resetActionOf): which backends keep
// their SDL_Renderer through a device reset.
void testResetActions()
{
  SDL_Event ev{};
  ev.type = SDL_RENDER_DEVICE_RESET;
  check(resetActionOf(ev, "direct3d11") == ResetAction::InPlace &&
      resetActionOf(ev, "direct3d12") == ResetAction::InPlace,
    "Direct3D 11 and 12 survive a device reset: the textures are made again in place");
  bool lost = true;
  for (const char* backend : {"opengles2", "opengl", "opengles", "direct3d", "metal", "software", ""})
    lost = lost && resetActionOf(ev, backend) == ResetAction::Recreate && !deviceSurvivesReset(backend);
  check(lost, "OpenGL ES 2 (Android) and every other backend: the renderer is made again");
  ev.type = SDL_RENDER_TARGETS_RESET;
  check(resetActionOf(ev, "direct3d") == ResetAction::Targets && resetActionOf(ev, "opengles2") == ResetAction::Targets,
    "a targets reset drops the targets on every backend");
  ev.type = SDL_WINDOWEVENT;
  check(resetActionOf(ev, "opengles2") == ResetAction::None, "other events are no reset on any backend");
}

// The stale-draw count that catches a texture owner left out of a recovery.
void testStaleDraws(SDL_Renderer* sdl)
{
  Renderer r(sdl);
  const Uint32 white = 0xFFFFFFFFu;
  const auto pixel = [&r, &white] {
    return r.createTexture(1, 1, reinterpret_cast<const unsigned char*>(&white), 4, 0.0f, 0.0f);
  };
  Texture kept = pixel();
  const std::uint32_t gen = r.generation();
  r.draw(kept, 10.0f, 10.0f);
  resetRenderer(r, ResetAction::Targets);
  r.draw(kept, 10.0f, 10.0f);
  check(kept.generation() == gen && r.generation() == gen && r.staleDraws() == 0,
    "a texture of the current generation is no stale draw, also after a targets reset");
  resetRenderer(r, ResetAction::InPlace);
  check(r.generation() == gen + 1, "a device reset starts a new generation");
  // An owner that forgot to drop it: every kind of draw counts, culled too.
  r.draw(kept, 10.0f, 10.0f);
  r.draw(kept, -500.0f, -500.0f);
  r.drawFadedOval(kept, 0.0f, 0.0f, 20.0f, 20.0f, 0.5f);
  check(r.use(kept) == kept.get(), "use hands over the texture's handle");
  check(r.staleDraws() == 4, "draws of a texture from before a device reset are counted");
  Texture fresh = pixel();
  r.draw(fresh, 10.0f, 10.0f);
  r.draw(Texture(), 10.0f, 10.0f);
  check(fresh.generation() == gen + 1 && r.staleDraws() == 4, "one made after it, or none at all, is not");
  Texture moved = std::move(kept);
  r.draw(moved, 10.0f, 10.0f);
  check(r.staleDraws() == 5, "a moved texture keeps its generation");
}

// recoverRender's order: the owners drop while the renderer still holds its
// own textures and the old SDL_Renderer lives, the renderer drops its own,
// the SDL_Renderer is made again (Recreate only), the owners bake again.
void testRecoverOrder()
{
  Screen s;
  Renderer r(s.sdl);
  std::vector<std::string> steps;
  const Uint32 white = 0xFFFFFFFFu;
  Texture owned;
  const TextureOwners owners{
    [&] {
      steps.push_back(r.textCacheSize() > 0 && r.holdsGridTexture() ? "drop" : "drop late");
      owned = Texture();
    },
    [&] {
      steps.push_back(r.textCacheSize() == 0 && !r.holdsGridTexture() ? "rebake" : "rebake early");
      owned = r.createTexture(1, 1, reinterpret_cast<const unsigned char*>(&white), 4, 0.0f, 0.0f);
    }};
  const auto remake = [&] {
    steps.push_back(owned ? "remake early" : "remake");
    return s.remake();
  };
  using Steps = std::vector<std::string>;
  const Frame want = resetScene(r, s.sdl, s.surface);
  check(recoverRender(r, ResetAction::Recreate, owners, remake) && steps == Steps{"drop", "remake", "rebake"},
    "Recreate: the owners drop, then the renderer its own, the renderer is made again, the owners bake");
  check(r.sdl() == s.sdl && s.remade == 1 && owned && owned.generation() == r.generation(),
    "...the renderer draws with the new SDL_Renderer, the owner's texture made with it");
  check(resetScene(r, s.sdl, s.surface) == want, "with a new SDL_Renderer the picture is as before");
  steps.clear();
  check(recoverRender(r, ResetAction::InPlace, owners, remake) && steps == Steps{"drop", "rebake"} && s.remade == 1,
    "InPlace: the same without a new SDL_Renderer");
  check(resetScene(r, s.sdl, s.surface) == want, "...the picture again as before");
  steps.clear();
  check(recoverRender(r, ResetAction::Targets, owners, remake) && recoverRender(r, ResetAction::None, owners, remake) &&
      steps.empty() && r.textCacheSize() == 1 && !r.holdsCaptureTextures(),
    "Targets drops the capture's textures only, None nothing; the owners keep theirs");
  check(!recoverRender(r, ResetAction::Recreate, owners, nullptr) && r.sdl() == s.sdl && !owned &&
      steps == Steps{"drop"},
    "Recreate without a way to make a renderer fails, the renderer kept, nothing baked again with it");
  // Bake the owner again for the next case (as an InPlace does).
  check(recoverRender(r, ResetAction::InPlace, owners, remake) && owned, "...and an InPlace bakes again");
  steps.clear();
  resetScene(r, s.sdl, s.surface);
  s.failRemake = true;
  check(!recoverRender(r, ResetAction::Recreate, owners, remake) && r.sdl() == nullptr && !owned &&
      steps == Steps{"drop", "remake"},
    "a renderer that cannot be made again fails, the owners dropped and not baked against no renderer");
}

// One screen of the game across both kinds of device reset: drawn twice the
// same, then after the textures were made again in place and after the
// renderer itself was made again, the same picture with no texture from
// before the reset drawn.
void checkRecovers(Game& g, Renderer& r, Screen& s, const std::string& what)
{
  g.render();
  const Frame before = present(r.sdl(), s.surface);
  g.render();
  check(present(r.sdl(), s.surface) == before, (what + ": drawn twice is the same frame").c_str());
  const TextureOwners owners{[&g] { g.dropTextures(); }, [&g] { g.rebakeTextures(); }};
  const auto remake = [&s] { return s.remake(); };
  for (const ResetAction action : {ResetAction::InPlace, ResetAction::Recreate})
  {
    const char* how = action == ResetAction::InPlace ? "in place" : "with a new renderer";
    const std::size_t stale = r.staleDraws();
    recoverRender(r, action, owners, remake);
    g.render();
    const Frame after = present(r.sdl(), s.surface);
    check(r.staleDraws() == stale, (what + " (" + how + "): no texture from before the reset is drawn").c_str());
    check(g.art().vignette.generation() == r.generation(), (what + " (" + how + "): the art is baked again").c_str());
    check(after == before, (what + " (" + how + "): it looks as before").c_str());
    // A stale texture would be a freed one with a new renderer.
    if (r.staleDraws() != stale)
      return;
  }
}

GameOptions resetOptions(const std::string& saves)
{
  GameOptions o;
  o.saveDir = saves;
  o.noCutscenes = true;
  return o;
}

// A level, its pause menu and its map, a recovery on each.
void testLevelRecovers(Screen& s, Renderer& r, const std::string& saves)
{
  GameOptions o = resetOptions(saves);
  o.skipMenu = true;
  o.startLevel = 1;
  Game g(o, r, nullptr);
  for (int i = 0; i < 90; ++i)
    g.tick(Input{});
  check(g.world() != nullptr && g.inLevel(), "a level is being played");
  checkRecovers(g, r, s, "a level");
  g.pauseLevel();
  check(!g.inLevel(), "the pause menu is open");
  checkRecovers(g, r, s, "the pause menu");
  Input key;
  key.pause = true;
  g.tick(key);
  g.tick(Input{});
  key = Input{};
  key.map = true;
  g.tick(key);
  g.tick(Input{});
  check(!g.inLevel(), "the map is open");
  checkRecovers(g, r, s, "the map");
}

// The title screen, runner select, the runner editor and a cutscene.
void testMenusRecover(Screen& s, Renderer& r, const std::string& saves)
{
  {
    Game g(resetOptions(saves), r, nullptr);
    checkRecovers(g, r, s, "the title screen");
  }
  {
    GameOptions o = resetOptions(saves);
    o.levelPath = std::string(GR_DATA_DIR) + "/levels/01_rooftop_run.txt"; // runner select first
    Game g(o, r, nullptr);
    checkRecovers(g, r, s, "runner select");
    g.render();
    const Frame cards = present(r.sdl(), s.surface);
    for (int i = 0; i < 25; ++i)
      g.tick(Input{});
    Input key;
    key.down = true; // EDIT / REMIX under the card
    g.tick(key);
    g.tick(Input{});
    key = Input{};
    key.confirm = true;
    g.tick(key);
    g.tick(Input{});
    g.render();
    check(present(r.sdl(), s.surface) != cards, "the runner editor is open");
    checkRecovers(g, r, s, "the runner editor");
  }
  {
    GameOptions o = resetOptions(saves);
    o.cutscene = "brief_01";
    Game g(o, r, nullptr);
    for (int i = 0; i < 120; ++i)
      g.tick(Input{});
    checkRecovers(g, r, s, "a cutscene");
  }
}

// Game::dropTextures and rebakeTextures through recoverRender on every kind
// of screen, with the software renderer taken through both paths.
void testGameRecovers()
{
  const std::string saves = "capture_test_saves";
  std::filesystem::remove_all(saves);
  {
    Screen s; // outlives the renderer and the games
    Renderer r(s.sdl);
    testLevelRecovers(s, r, saves);
    testMenusRecover(s, r, saves);
    check(s.remade == 7, "every screen went through a new renderer too");
  }
  std::filesystem::remove_all(saves);
}

// The window's SDL_Renderer has the game's 1280x720 logical size.
bool gameLogicalSize(SDL_Renderer* sdl)
{
  int w = 0, h = 0;
  SDL_RenderGetLogicalSize(sdl, &w, &h);
  return w == kScreenW && h == kScreenH;
}

// A frame of the game with the touch overlay over it, in both layouts.
void drawWindow(Game& g, TouchControls& t, Renderer& r)
{
  for (const TouchControls::Layout layout : {TouchControls::Layout::Menu, TouchControls::Layout::Play})
  {
    r.beginFrame();
    g.render();
    t.configure(layout, 1);
    t.draw(r);
  }
}

// A recreate through the window renderer: a new SDL_Renderer with the
// logical size, stored where the renderer draws, every owner's textures
// made again with it. Then the touch controls left out of the owners (in
// place, so their old textures are still SDL's to draw): caught drawing
// textures from before the reset.
void testWindowRemake(WindowRenderer& w, Renderer& r, Game& g, TouchControls& t, const int& made)
{
  drawWindow(g, t, r);
  // Undone on the old renderer: only one made again by makeWindowRenderer
  // has it.
  SDL_RenderSetLogicalSize(w.sdl(), 0, 0);
  std::size_t stale = r.staleDraws();
  check(w.answerReset(r, ResetAction::Recreate, windowOwners(g, &t, r)) && made == 2 && w.error().empty(),
    "window: a recreate makes the window's SDL_Renderer again");
  check(w.sdl() != nullptr && r.sdl() == w.sdl() && gameLogicalSize(w.sdl()),
    "window: ...the renderer draws with the new one, which has the logical size again");
  drawWindow(g, t, r);
  check(r.staleDraws() == stale && g.art().vignette.generation() == r.generation(),
    "window: ...the game's and the touch controls' textures are made again with it");
  stale = r.staleDraws();
  check(w.answerReset(r, ResetAction::InPlace, windowOwners(g, nullptr, r)) && made == 2,
    "window: an in-place reset keeps the SDL_Renderer");
  drawWindow(g, t, r);
  check(r.staleDraws() > stale, "window: touch controls left out of the owners draw textures from before the reset");
  t.dropTextures();
  t.bakeTextures(r);
}

// A remake that fails: no renderer left, the textures dropped and none baked
// against no renderer, and SDL's error from the failed make kept, whatever
// SDL says after it.
void testWindowRemakeFails(WindowRenderer& w, Renderer& r, Game& g, TouchControls& t, bool& failMake)
{
  int drops = 0, rebakes = 0;
  const TextureOwners owners = windowOwners(g, &t, r);
  TextureOwners counted;
  counted.drop = [&] {
    ++drops;
    owners.drop();
  };
  counted.rebake = [&] {
    ++rebakes;
    owners.rebake();
  };
  failMake = true;
  check(!w.answerReset(r, ResetAction::Recreate, counted) && w.sdl() == nullptr && r.sdl() == nullptr,
    "window: a renderer that cannot be made again fails the reset, none is left");
  check(drops == 1 && rebakes == 0, "window: ...the textures dropped, none baked against no renderer");
  // What a rebake against no renderer used to leave behind to be logged.
  SDL_SetError("Parameter 'renderer' is invalid");
  check(w.error() == "capture_test: no renderer this time", "window: ...the error kept is SDL's own from the make");
}

// The window renderer on `window`, made by makeWindowRenderer until told to
// fail, with a game and touch controls drawing with it.
void testWindowRenderer(SDL_Window* window, const std::string& saves)
{
  bool failMake = false;
  int made = 0;
  WindowRenderer w(window, [&failMake, &made](SDL_Window* win) -> SDL_Renderer* {
    ++made;
    if (!failMake)
      return makeWindowRenderer(win);
    SDL_SetError("capture_test: no renderer this time");
    return nullptr;
  });
  check(w.sdl() != nullptr && w.error().empty() && gameLogicalSize(w.sdl()),
    "window: the window's renderer is made with the game's logical size");
  if (!w.sdl())
    return;
  Renderer r(w.sdl()); // after w: gone before the SDL_Renderer
  Game g(resetOptions(saves), r, nullptr);
  TouchControls t(r, true);
  testWindowRemake(w, r, g, t, made);
  testWindowRemakeFails(w, r, g, t, failMake);
}

// A window without a screen: SDL's dummy video driver, which draws with its
// software renderer (asked for by name, so no GL driver is tried first).
// Last of the tests, as the hints stay set until SDL_Quit.
void testWindow()
{
  check(backendUsesGl("opengl") && backendUsesGl("opengles2") && backendUsesGl("opengles") &&
      !backendUsesGl("software") && !backendUsesGl("direct3d11") && !backendUsesGl("metal") && !backendUsesGl(""),
    "only the OpenGL backends own a GL context");
  SDL_SetHintWithPriority(SDL_HINT_VIDEODRIVER, "dummy", SDL_HINT_OVERRIDE);
  SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
  const bool video = SDL_InitSubSystem(SDL_INIT_VIDEO) == 0;
  SDL_Window* window = video
    ? SDL_CreateWindow("capture_test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, kScreenW, kScreenH, 0)
    : nullptr;
  check(window != nullptr, "a window on SDL's dummy video driver");
  if (!window)
    std::printf("     (%s)\n", SDL_GetError());
  if (window)
  {
    const std::string saves = "capture_test_window_saves";
    std::filesystem::remove_all(saves);
    testWindowRenderer(window, saves);
    std::filesystem::remove_all(saves);
    SDL_DestroyWindow(window);
  }
  if (video)
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
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
  testResetActions();
  {
    Screen s;
    testRecover(s.sdl, s.surface);
    testRecoverFallbacks(s.sdl, s.surface);
    testStaleDraws(s.sdl);
  }
  testRecoverOrder();
  testGameRecovers();
  testWindow();
  SDL_Quit();
  std::printf(gFailures == 0 ? "all capture tests passed\n" : "%d capture test(s) failed\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
