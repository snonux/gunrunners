// Box art: draws the front and back covers of a Gunrunners retail box at
// print size with Cairo, using the game's own vector runners. tools/covers.sh
// builds and runs it.
//
// Usage: gunrunners_cover OUTDIR SHOT1.png SHOT2.png SHOT3.png SHOT4.png
// Writes OUTDIR/cover_front.png and OUTDIR/cover_back.png (1000x1400).

#include "assets/art.hpp"

#include <cairo.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace
{

constexpr int W = 1000;
constexpr int H = 1400;
constexpr double kPi = 3.14159265358979;
constexpr const char* kFont = "DejaVu Sans";

struct Rgb
{
  double r, g, b;
};

Rgb hex(unsigned v) { return {((v >> 16) & 255) / 255.0, ((v >> 8) & 255) / 255.0, (v & 255) / 255.0}; }

void set(cairo_t* cr, unsigned v, double a = 1.0)
{
  const Rgb c = hex(v);
  cairo_set_source_rgba(cr, c.r, c.g, c.b, a);
}

void stop(cairo_pattern_t* p, double at, unsigned v, double a = 1.0)
{
  const Rgb c = hex(v);
  cairo_pattern_add_color_stop_rgba(p, at, c.r, c.g, c.b, a);
}

// A small deterministic RNG so the art is the same every run.
struct Rng
{
  std::uint32_t s;
  double next()
  {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return double(s & 0xFFFFFF) / double(0x1000000);
  }
};

void glow(cairo_t* cr, double x, double y, double r, unsigned c, double a)
{
  cairo_pattern_t* p = cairo_pattern_create_radial(x, y, 0, x, y, r);
  stop(p, 0.0, c, a);
  stop(p, 0.45, c, a * 0.35);
  stop(p, 1.0, c, 0.0);
  cairo_set_source(cr, p);
  cairo_arc(cr, x, y, r, 0, 2 * kPi);
  cairo_fill(cr);
  cairo_pattern_destroy(p);
}

void font(cairo_t* cr, double size, bool bold = true, bool italic = false)
{
  cairo_select_font_face(cr, kFont, italic ? CAIRO_FONT_SLANT_OBLIQUE : CAIRO_FONT_SLANT_NORMAL,
    bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, size);
}

double textWidth(cairo_t* cr, const std::string& s)
{
  cairo_text_extents_t e;
  cairo_text_extents(cr, s.c_str(), &e);
  return e.x_advance;
}

// align: 0 left, 0.5 centre, 1 right.
void text(cairo_t* cr, const std::string& s, double x, double y, unsigned c, double align = 0.0, double a = 1.0)
{
  set(cr, c, a);
  cairo_move_to(cr, x - textWidth(cr, s) * align, y);
  cairo_show_text(cr, s.c_str());
}

// Wraps a paragraph into `width`; returns the y after the last line.
double paragraph(cairo_t* cr, const std::string& s, double x, double y, double width, double lineH, unsigned c)
{
  std::string line, word;
  auto flush = [&]() {
    text(cr, line, x, y, c);
    y += lineH;
    line.clear();
  };
  for (std::size_t i = 0; i <= s.size(); ++i)
  {
    if (i == s.size() || s[i] == ' ')
    {
      const std::string trial = line.empty() ? word : line + " " + word;
      if (textWidth(cr, trial) > width && !line.empty())
      {
        flush();
        line = word;
      }
      else
      {
        line = trial;
      }
      word.clear();
    }
    else
    {
      word += s[i];
    }
  }
  if (!line.empty())
    flush();
  return y;
}

// A runner at print size: feet at (x, y), `scale` design units to pixels.
void runner(cairo_t* cr, int kind, int pose, double x, double y, double scale, bool mirror)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, mirror ? -scale : scale, scale);
  cairo_translate(cr, -32.0, -96.0);
  gr::drawCharacterPose(cr, kind, pose);
  cairo_restore(cr);
}

// The synthwave night: sky, stars, a striped sun, the skyline and the grid.
void neonNight(cairo_t* cr, double horizon, double sunY, double sunR, std::uint32_t seed)
{
  cairo_pattern_t* sky = cairo_pattern_create_linear(0, 0, 0, horizon);
  stop(sky, 0.0, 0x0b0420);
  stop(sky, 0.45, 0x2a0b52);
  stop(sky, 0.8, 0x8a1f78);
  stop(sky, 1.0, 0xff6a5c);
  cairo_set_source(cr, sky);
  cairo_paint(cr);
  cairo_pattern_destroy(sky);

  Rng rng{seed};
  for (int i = 0; i < 220; ++i)
  {
    const double x = rng.next() * W, y = rng.next() * horizon * 0.6, r = 0.6 + rng.next() * 1.8;
    set(cr, 0xffffff, 0.3 + rng.next() * 0.6);
    cairo_arc(cr, x, y, r, 0, 2 * kPi);
    cairo_fill(cr);
  }

  // The sun, with the classic stripes cut through its lower half.
  glow(cr, W / 2.0, sunY, sunR * 1.9, 0xff4fa0, 0.45);
  cairo_save(cr);
  cairo_arc(cr, W / 2.0, sunY, sunR, 0, 2 * kPi);
  cairo_clip(cr);
  cairo_pattern_t* sun = cairo_pattern_create_linear(0, sunY - sunR, 0, sunY + sunR);
  stop(sun, 0.0, 0xfff27a);
  stop(sun, 0.5, 0xff9a3c);
  stop(sun, 1.0, 0xff2f8e);
  cairo_set_source(cr, sun);
  cairo_paint(cr);
  cairo_pattern_destroy(sun);
  for (int k = 0; k < 9; ++k)
  {
    const double y = sunY + sunR * (0.05 + 0.11 * k);
    const double h = 3.0 + k * 2.6;
    set(cr, 0x8a1f78);
    cairo_rectangle(cr, 0, y, W, h);
    cairo_fill(cr);
  }
  cairo_restore(cr);

  // Two layers of skyline with lit windows.
  for (int layer = 0; layer < 2; ++layer)
  {
    double x = -20;
    const unsigned body = layer == 0 ? 0x3a1360 : 0x1a0832;
    while (x < W)
    {
      const double bw = 50 + rng.next() * 90;
      const double bh = (layer == 0 ? 120 : 60) + rng.next() * (layer == 0 ? 260 : 200);
      const double top = horizon - bh;
      set(cr, body);
      cairo_rectangle(cr, x, top, bw, bh);
      cairo_fill(cr);
      if (rng.next() < 0.35)
      {
        cairo_rectangle(cr, x + bw * 0.45, top - 30, 4, 30);
        cairo_fill(cr);
        glow(cr, x + bw * 0.45 + 2, top - 32, 10, 0xff3050, 0.9);
      }
      for (double wy = top + 12; wy < horizon - 10; wy += 16)
        for (double wx = x + 8; wx < x + bw - 10; wx += 14)
          if (rng.next() < (layer == 0 ? 0.18 : 0.28))
          {
            set(cr, rng.next() < 0.7 ? 0xffd66a : 0x6ae6ff, layer == 0 ? 0.5 : 0.85);
            cairo_rectangle(cr, wx, wy, 6, 8);
            cairo_fill(cr);
          }
      x += bw + (layer == 0 ? 6 : 2);
    }
  }

  // The grid floor.
  cairo_pattern_t* floor = cairo_pattern_create_linear(0, horizon, 0, H);
  stop(floor, 0.0, 0x2a0b52);
  stop(floor, 1.0, 0x07020f);
  cairo_set_source(cr, floor);
  cairo_rectangle(cr, 0, horizon, W, H - horizon);
  cairo_fill(cr);
  cairo_set_line_width(cr, 2.5);
  for (int i = -14; i <= 14; ++i)
  {
    set(cr, 0xff3cc8, 0.75);
    cairo_move_to(cr, W / 2.0 + i * 12, horizon);
    cairo_line_to(cr, W / 2.0 + i * 160, H);
    cairo_stroke(cr);
  }
  for (int k = 0; k < 14; ++k)
  {
    const double y = horizon + std::pow(1.32, k) * 6;
    if (y > H)
      break;
    set(cr, 0xff3cc8, 0.4 + 0.04 * k);
    cairo_move_to(cr, 0, y);
    cairo_line_to(cr, W, y);
    cairo_stroke(cr);
  }
  glow(cr, W / 2.0, horizon, 520, 0xff4fa0, 0.35);
}

// The logo: slanted, outlined, with a chrome gradient and a neon echo.
void logo(cairo_t* cr, double cx, double baseline, double size, double maxWidth)
{
  const std::string word = "GUNRUNNERS";
  cairo_save(cr);
  font(cr, size, true, false);
  // Shrink to fit, leaving room for the slant and the neon echo.
  size *= std::min(1.0, maxWidth / (textWidth(cr, word) + size * 0.45));
  font(cr, size, true, false);
  const double w = textWidth(cr, word);
  cairo_translate(cr, cx, baseline);
  cairo_matrix_t shear = {1.0, 0.0, -0.22, 1.0, 0.0, 0.0};
  cairo_transform(cr, &shear);
  cairo_move_to(cr, -w / 2.0, 0);
  cairo_text_path(cr, word.c_str());
  cairo_path_t* path = cairo_copy_path(cr);
  cairo_new_path(cr);

  auto withPath = [&](double dx, double dy) {
    cairo_save(cr);
    cairo_translate(cr, dx, dy);
    cairo_new_path(cr);
    cairo_append_path(cr, path);
    cairo_restore(cr);
  };
  // Neon echo behind, then a deep 3D extrusion.
  withPath(10, 10);
  set(cr, 0x2ee6ff, 0.85);
  cairo_set_line_width(cr, size * 0.16);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  cairo_stroke(cr);
  for (int k = 8; k >= 1; --k)
  {
    withPath(k * 1.2, k * 1.4);
    set(cr, k > 4 ? 0x1a0630 : 0x4a0e5c);
    cairo_fill(cr);
  }
  withPath(0, 0);
  set(cr, 0x12041f);
  cairo_set_line_width(cr, size * 0.1);
  cairo_stroke_preserve(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, -size * 0.75, 0, size * 0.05);
  stop(g, 0.0, 0xffffff);
  stop(g, 0.3, 0xfff07a);
  stop(g, 0.52, 0xffb02a);
  stop(g, 0.56, 0xff6a2a);
  stop(g, 1.0, 0xff2f8e);
  cairo_set_source(cr, g);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
  // A chrome glint line across the letters.
  withPath(0, 0);
  cairo_clip(cr);
  set(cr, 0xffffff, 0.55);
  cairo_rectangle(cr, -w, -size * 0.43, w * 2, size * 0.05);
  cairo_fill(cr);
  cairo_reset_clip(cr);
  cairo_path_destroy(path);
  cairo_restore(cr);
}

void band(cairo_t* cr, double y, double h)
{
  set(cr, 0x07020f);
  cairo_rectangle(cr, 0, y, W, h);
  cairo_fill(cr);
  cairo_pattern_t* g = cairo_pattern_create_linear(0, 0, W, 0);
  stop(g, 0.0, 0x2ee6ff);
  stop(g, 0.5, 0xff3cc8);
  stop(g, 1.0, 0xffb02a);
  cairo_set_source(cr, g);
  cairo_rectangle(cr, 0, y + (y == 0 ? h - 5 : 0), W, 5);
  cairo_fill(cr);
  cairo_pattern_destroy(g);
}

void badge(cairo_t* cr, double x, double y, double w, double h, const std::string& top, const std::string& bottom,
  unsigned c)
{
  cairo_set_line_width(cr, 3);
  set(cr, c);
  cairo_rectangle(cr, x, y, w, h);
  cairo_stroke(cr);
  font(cr, h * 0.3);
  text(cr, top, x + w / 2, y + h * 0.42, 0xffffff, 0.5);
  font(cr, h * 0.22, false);
  text(cr, bottom, x + w / 2, y + h * 0.78, c, 0.5);
}

void starburst(cairo_t* cr, double cx, double cy, double r, double angle, const std::vector<std::string>& lines)
{
  cairo_save(cr);
  cairo_translate(cr, cx, cy);
  cairo_rotate(cr, angle);
  const int points = 18;
  for (int i = 0; i <= points * 2; ++i)
  {
    const double a = kPi * i / points;
    const double rr = i % 2 ? r * 0.8 : r;
    if (i == 0)
      cairo_move_to(cr, rr * std::cos(a), rr * std::sin(a));
    else
      cairo_line_to(cr, rr * std::cos(a), rr * std::sin(a));
  }
  cairo_close_path(cr);
  set(cr, 0xffe03a);
  cairo_fill_preserve(cr);
  set(cr, 0xd0202a);
  cairo_set_line_width(cr, 5);
  cairo_stroke(cr);
  font(cr, r * 0.2);
  double y = -r * 0.12 * double(lines.size() - 1);
  for (const auto& l : lines)
  {
    text(cr, l, 0, y + r * 0.08, 0xc01428, 0.5);
    y += r * 0.24;
  }
  cairo_restore(cr);
}

void frame(cairo_t* cr)
{
  cairo_set_line_width(cr, 10);
  set(cr, 0x07020f);
  cairo_rectangle(cr, 5, 5, W - 10, H - 10);
  cairo_stroke(cr);
}

void front(const std::string& path)
{
  cairo_surface_t* s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
  cairo_t* cr = cairo_create(s);
  neonNight(cr, 900, 700, 270, 42u);

  // Shots streaking across the night.
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  const struct
  {
    double x0, y0, x1, y1;
    unsigned c;
  } beams[] = {{820, 820, 1040, 760, 0x6ae6ff}, {180, 980, -40, 1000, 0xffd66a}, {640, 860, 1040, 880, 0xff5a3c}};
  for (const auto& b : beams)
  {
    set(cr, b.c, 0.35);
    cairo_set_line_width(cr, 18);
    cairo_move_to(cr, b.x0, b.y0);
    cairo_line_to(cr, b.x1, b.y1);
    cairo_stroke(cr);
    set(cr, 0xffffff, 0.9);
    cairo_set_line_width(cr, 5);
    cairo_move_to(cr, b.x0, b.y0);
    cairo_line_to(cr, b.x1, b.y1);
    cairo_stroke(cr);
  }

  // The three runners: Nova leaping, Dash charging, Rocco planted.
  glow(cr, 760, 820, 260, 0xff4fd0, 0.45);
  glow(cr, 250, 1080, 260, 0xffd040, 0.4);
  glow(cr, 520, 1130, 330, 0x40e0ff, 0.35);
  runner(cr, 2, 2, 760, 960, 4.6, true);
  runner(cr, 0, 1, 250, 1210, 4.8, true);
  runner(cr, 1, 0, 540, 1300, 6.0, false);

  band(cr, 0, 70);
  font(cr, 26);
  text(cr, "GR", 30, 46, 0xffb02a);
  font(cr, 20, false);
  text(cr, "LINUX & ANDROID  \xC2\xB7  GAMEPAD, KEYS & TOUCH", W - 30, 44, 0xffffff, 1.0);

  logo(cr, W / 2.0, 250, 170, W - 70.0);
  font(cr, 30, true);
  text(cr, "42 LEVELS  \xC2\xB7  42 PROTOTYPES  \xC2\xB7  ONE SHOW", W / 2.0, 320, 0xffffff, 0.5);

  starburst(cr, 845, 470, 105, 0.25, {"SWITCH", "RUNNERS", "MID-LEVEL!"});

  band(cr, H - 90, 90);
  badge(cr, 30, H - 75, 210, 60, "HD VECTOR", "NO PIXELS ANYWHERE", 0x2ee6ff);
  badge(cr, W - 240, H - 75, 210, 60, "STEREO", "42 ORIGINAL TRACKS", 0xff3cc8);
  font(cr, 24);
  text(cr, "DASH  \xC2\xB7  ROCCO  \xC2\xB7  NOVA", W / 2.0, H - 38, 0xffd66a, 0.5);
  frame(cr);
  cairo_surface_write_to_png(s, path.c_str());
  cairo_destroy(cr);
  cairo_surface_destroy(s);
}

void screenshot(cairo_t* cr, const std::string& file, double cx, double cy, double w, double angle)
{
  cairo_surface_t* img = cairo_image_surface_create_from_png(file.c_str());
  if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS)
  {
    std::fprintf(stderr, "cover: cannot read %s\n", file.c_str());
    cairo_surface_destroy(img);
    return;
  }
  const double iw = cairo_image_surface_get_width(img), ih = cairo_image_surface_get_height(img);
  const double h = w * ih / iw;
  cairo_save(cr);
  cairo_translate(cr, cx, cy);
  cairo_rotate(cr, angle);
  set(cr, 0x000000, 0.5);
  cairo_rectangle(cr, -w / 2 - 4, -h / 2 + 4, w + 16, h + 16);
  cairo_fill(cr);
  set(cr, 0xffffff);
  cairo_rectangle(cr, -w / 2 - 8, -h / 2 - 8, w + 16, h + 16);
  cairo_fill(cr);
  cairo_translate(cr, -w / 2, -h / 2);
  cairo_scale(cr, w / iw, h / ih);
  cairo_set_source_surface(cr, img, 0, 0);
  cairo_paint(cr);
  cairo_restore(cr);
  cairo_surface_destroy(img);
}

void back(const std::string& path, const std::vector<std::string>& shots)
{
  cairo_surface_t* s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
  cairo_t* cr = cairo_create(s);
  cairo_pattern_t* bg = cairo_pattern_create_linear(0, 0, 0, H);
  stop(bg, 0.0, 0x12052a);
  stop(bg, 0.6, 0x2a0b52);
  stop(bg, 1.0, 0x0b0420);
  cairo_set_source(cr, bg);
  cairo_paint(cr);
  cairo_pattern_destroy(bg);
  // A faint grid behind everything.
  cairo_set_line_width(cr, 1);
  set(cr, 0xff3cc8, 0.12);
  for (int x = 0; x < W; x += 40)
  {
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x, H);
  }
  for (int y = 0; y < H; y += 40)
  {
    cairo_move_to(cr, 0, y);
    cairo_line_to(cr, W, y);
  }
  cairo_stroke(cr);

  band(cr, 0, 70);
  font(cr, 26);
  text(cr, "GR", 30, 46, 0xffb02a);
  font(cr, 20, false);
  text(cr, "1 PLAYER  \xC2\xB7  3 RUNNERS  \xC2\xB7  5 SAVE SLOTS", W - 30, 44, 0xffffff, 1.0);

  logo(cr, W / 2.0, 175, 84, W - 200.0);

  font(cr, 23, false);
  double y = paragraph(cr,
    "Dash, Rocco and Nova are couriers. An anonymous client called MAX pays them to recover "
    "stolen prototype weapons from the most dangerous places there are: neon rooftops, a lost "
    "temple, a space station and stranger places still. One job per level. No questions asked.",
    60, 240, W - 120, 32, 0xffffff);
  font(cr, 25, true, true);
  text(cr, "But somebody is watching. And the ratings are through the roof.", W / 2.0, y + 8, 0xffd66a, 0.5);

  // Four screenshots, scattered like a fan.
  const double sy = y + 60;
  const double angles[4] = {-0.05, 0.04, 0.03, -0.04};
  for (std::size_t i = 0; i < shots.size() && i < 4; ++i)
  {
    const double cx = i % 2 ? 735 : 265, cy = sy + 135 + (i / 2) * 285;
    screenshot(cr, shots[i], cx, cy, 420, angles[i]);
  }

  // Features on the left, the runners on the right.
  double fy = sy + 620;
  font(cr, 26);
  text(cr, "FEATURES", 60, fy, 0x2ee6ff);
  const char* features[] = {
    "42 levels in six episodes, each with its own twist",
    "A prototype weapon hidden in every level",
    "Bonus levels behind every flickering TV",
    "Turbo Mode, and a Virus you'd rather not catch",
    "Switch runners in the middle of a level",
    "Every level has its own soundtrack",
  };
  font(cr, 20, false);
  fy += 38;
  for (const char* f : features)
  {
    set(cr, 0xff3cc8);
    cairo_arc(cr, 72, fy - 7, 6, 0, 2 * kPi);
    cairo_fill(cr);
    text(cr, f, 90, fy, 0xffffff);
    fy += 33;
  }

  const struct
  {
    int kind;
    const char* name;
    const char* line;
    const char* line2;
    unsigned c;
  } crew[] = {{0, "DASH", "9 hearts", "all-rounder", 0xffd66a},
              {1, "ROCCO", "12 hearts", "starts with rockets", 0x9ae070},
              {2, "NOVA", "7 hearts", "jumps highest", 0xff7ad0}};
  const double ry = sy + 600;
  for (int i = 0; i < 3; ++i)
  {
    const double cx = 670 + i * 115;
    glow(cr, cx, ry + 120, 70, crew[i].c, 0.35);
    runner(cr, crew[i].kind, 0, cx, ry + 175, 1.45, false);
    font(cr, 20);
    text(cr, crew[i].name, cx, ry + 205, crew[i].c, 0.5);
    font(cr, 13, false);
    text(cr, crew[i].line, cx, ry + 225, 0xffffff, 0.5);
    text(cr, crew[i].line2, cx, ry + 242, 0xffffff, 0.5);
  }

  // The bottom strip: a barcode and the small print.
  band(cr, H - 110, 110);
  set(cr, 0xffffff);
  cairo_rectangle(cr, 40, H - 95, 200, 80);
  cairo_fill(cr);
  Rng rng{2026u};
  double bx = 52;
  while (bx < 228)
  {
    const double bw = 1.5 + std::floor(rng.next() * 3) * 1.5;
    set(cr, 0x000000);
    cairo_rectangle(cr, bx, H - 88, bw, 52);
    cairo_fill(cr);
    bx += bw + 1.5 + std::floor(rng.next() * 3) * 1.5;
  }
  font(cr, 12, false);
  text(cr, "4 2 0 4 2 0  0 0 4 2", 140, H - 22, 0x000000, 0.5);
  font(cr, 17, false);
  text(cr, "Free software under the GNU GPL, version 2 or later.", 270, H - 72, 0xffffff);
  text(cr, "Original art, music and sound, all synthesized in code.", 270, H - 46, 0xffffff);
  text(cr, "Built for Linux. Keyboard and gamepad.", 270, H - 20, 0x2ee6ff);
  frame(cr);
  cairo_surface_write_to_png(s, path.c_str());
  cairo_destroy(cr);
  cairo_surface_destroy(s);
}

// The app icon, as Android's adaptive icon layers (432 px: 108 dp at
// xxxhdpi; launchers mask it to a circle or squircle and show the middle
// 66%) plus a flat 512 px icon for the store listing.
void icon(const std::string& dir)
{
  constexpr int kLayer = 432;
  const double s = kLayer / 1000.0;

  auto layer = [&](const char* name, bool background) {
    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kLayer, kLayer);
    cairo_t* cr = cairo_create(surf);
    cairo_scale(cr, s, s);
    if (background)
      neonNight(cr, 640, 470, 250, 7);
    else
    {
      // Rocco head and shoulders, with his rocket launcher.
      glow(cr, 500, 520, 300, 0x40e0ff, 0.35);
      runner(cr, 1, 0, 470, 1240, 9.0, false);
    }
    cairo_surface_write_to_png(surf, (dir + "/" + name).c_str());
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
  };
  layer("icon_background.png", true);
  layer("icon_foreground.png", false);

  // The flat icon: both layers under a rounded mask.
  constexpr int kFlat = 512;
  cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kFlat, kFlat);
  cairo_t* cr = cairo_create(surf);
  const double r = kFlat * 0.18;
  cairo_new_sub_path(cr);
  cairo_arc(cr, kFlat - r, r, r, -kPi / 2, 0);
  cairo_arc(cr, kFlat - r, kFlat - r, r, 0, kPi / 2);
  cairo_arc(cr, r, kFlat - r, r, kPi / 2, kPi);
  cairo_arc(cr, r, r, r, kPi, 3 * kPi / 2);
  cairo_close_path(cr);
  cairo_clip(cr);
  // Show the same middle part a launcher shows.
  const double crop = kLayer * 0.17;
  cairo_scale(cr, kFlat / (kLayer - 2 * crop), kFlat / (kLayer - 2 * crop));
  cairo_translate(cr, -crop, -crop);
  for (const char* name : {"icon_background.png", "icon_foreground.png"})
  {
    cairo_surface_t* img = cairo_image_surface_create_from_png((dir + "/" + name).c_str());
    cairo_set_source_surface(cr, img, 0, 0);
    cairo_paint(cr);
    cairo_surface_destroy(img);
  }
  cairo_surface_write_to_png(surf, (dir + "/icon.png").c_str());
  cairo_destroy(cr);
  cairo_surface_destroy(surf);
}

} // namespace

int main(int argc, char** argv)
{
  if (argc == 3 && std::string(argv[1]) == "--icon")
  {
    icon(argv[2]);
    std::printf("wrote %s/icon.png and the adaptive icon layers\n", argv[2]);
    return 0;
  }
  if (argc < 2)
  {
    std::fprintf(stderr, "usage: %s OUTDIR [SHOT.png ...]   or   %s --icon OUTDIR\n", argv[0], argv[0]);
    return 1;
  }
  const std::string dir = argv[1];
  std::vector<std::string> shots(argv + 2, argv + argc);
  front(dir + "/cover_front.png");
  back(dir + "/cover_back.png", shots);
  std::printf("wrote %s/cover_front.png and %s/cover_back.png\n", dir.c_str(), dir.c_str());
  return 0;
}
