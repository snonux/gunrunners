// The planet Vurr (Episode 7, DEEP SPACE): organic tiles that look grown,
// not built, with bioluminescent veins, and the levels' backdrops.

#include "assets/art_alien.hpp"

#include "base/math.hpp"
#include "render/vector.hpp"

#include <cmath>
#include <string_view>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(18, 10, 30);

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

void circle(cairo_t* cr, double x, double y, double r) { cairo_arc(cr, x, y, r, 0, 2 * kPi); }

void gradient(cairo_t* cr, double w, double h, std::initializer_list<std::pair<double, Color>> stops)
{
  cairo_pattern_t* p = cairo_pattern_create_linear(0, 0, 0, h);
  for (const auto& s : stops)
    cairo_pattern_add_color_stop_rgba(p, s.first, redOf(s.second) / 255.0, greenOf(s.second) / 255.0,
      blueOf(s.second) / 255.0, alphaOf(s.second) / 255.0);
  cairo_rectangle(cr, 0, 0, w, h);
  cairo_set_source(cr, p);
  cairo_fill(cr);
  cairo_pattern_destroy(p);
}

std::string_view lookOf(const Theme& t) { return std::string_view(t.look); }

// A soft blob with a darker rim: one cell of the planet's flesh-rock.
void blob(cairo_t* cr, double x, double y, double rx, double ry, Color fill, Color rim)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, rx, ry);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
  cairo_pattern_t* p = cairo_pattern_create_radial(x - rx * 0.3, y - ry * 0.4, 0, x, y, std::max(rx, ry));
  cairo_pattern_add_color_stop_rgb(p, 0, redOf(lighten(fill, 0.25f)) / 255.0, greenOf(lighten(fill, 0.25f)) / 255.0,
    blueOf(lighten(fill, 0.25f)) / 255.0);
  cairo_pattern_add_color_stop_rgb(p, 1, redOf(fill) / 255.0, greenOf(fill) / 255.0, blueOf(fill) / 255.0);
  cairo_set_source(cr, p);
  cairo_fill_preserve(cr);
  cairo_pattern_destroy(p);
  setColor(cr, rim);
  cairo_set_line_width(cr, 1.6);
  cairo_stroke(cr);
}

// A giant mushroom silhouette for the backdrops: stalk, cap and glowing gills.
void mushroom(cairo_t* cr, double x, double base, double h, double capW, Color body, Color glow, double glowA)
{
  const double stalkW = capW * 0.16;
  cairo_move_to(cr, x - stalkW, base);
  cairo_curve_to(cr, x - stalkW * 0.6, base - h * 0.5, x - stalkW * 1.2, base - h * 0.8, x - stalkW * 0.7, base - h);
  cairo_line_to(cr, x + stalkW * 0.7, base - h);
  cairo_curve_to(cr, x + stalkW * 1.2, base - h * 0.8, x + stalkW * 0.6, base - h * 0.5, x + stalkW, base);
  cairo_close_path(cr);
  setColor(cr, body);
  cairo_fill(cr);
  // Gills glow under the cap.
  cairo_save(cr);
  cairo_translate(cr, x, base - h);
  cairo_scale(cr, capW * 0.5, capW * 0.12);
  cairo_arc(cr, 0, 0.2, 1, 0, kPi);
  cairo_restore(cr);
  setColor(cr, withAlpha(glow, int(255 * glowA)));
  cairo_fill(cr);
  radialGlow(cr, x, base - h + capW * 0.06, capW * 0.6, glow, glowA * 0.35);
  // The cap.
  cairo_save(cr);
  cairo_translate(cr, x, base - h);
  cairo_scale(cr, capW * 0.5, capW * 0.32);
  cairo_arc(cr, 0, 0, 1, kPi, 2 * kPi);
  cairo_restore(cr);
  setColor(cr, body);
  cairo_fill(cr);
}

// --- Level 45, Hive Gullets: inside the hive -------------------------------------------

bool isHive(const Theme& t) { return lookOf(t) == "alien_hive"; }

// Ribbed flesh: wavy bands of muscle with wet highlights; variant 1 has a
// cluster of glowing nodes, 2 a blue vein across it.
Texture bakeHiveSolid(const Renderer& r, const Theme& t, int variant)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  Rng rng(std::uint32_t(variant * 7919 + 11));
  gradient(cr, 64, 64, {{0.0, t.rock}, {1.0, darken(t.rock, 0.2f)}});
  // Ribs: four bands that wave and wrap at the edges, so blocks tile.
  for (int i = 0; i < 4; ++i)
  {
    const double y = 8 + i * 16;
    cairo_move_to(cr, 0, y);
    cairo_curve_to(cr, 16, y - 4, 48, y + 4, 64, y);
    cairo_line_to(cr, 64, y + 10);
    cairo_curve_to(cr, 48, y + 14, 16, y + 6, 0, y + 10);
    cairo_close_path(cr);
    cairo_pattern_t* p = cairo_pattern_create_linear(0, y - 2, 0, y + 12);
    const Color hi = lighten(t.rockLight, 0.1f), lo = darken(t.rock, 0.15f);
    cairo_pattern_add_color_stop_rgb(p, 0, redOf(hi) / 255.0, greenOf(hi) / 255.0, blueOf(hi) / 255.0);
    cairo_pattern_add_color_stop_rgb(p, 1, redOf(lo) / 255.0, greenOf(lo) / 255.0, blueOf(lo) / 255.0);
    cairo_set_source(cr, p);
    cairo_fill(cr);
    cairo_pattern_destroy(p);
    // The fold under each rib.
    cairo_move_to(cr, 0, y + 10);
    cairo_curve_to(cr, 16, y + 6, 48, y + 14, 64, y + 10);
    setColor(cr, withAlpha(t.rockDark, 220));
    cairo_set_line_width(cr, 2.2);
    cairo_stroke(cr);
    // A wet glint on top.
    cairo_move_to(cr, 6 + rng.range(0, 10), y + 2);
    cairo_line_to(cr, 22 + rng.range(0, 14), y + 1);
    setColor(cr, rgba(255, 255, 255, 70));
    cairo_set_line_width(cr, 1.6);
    cairo_stroke(cr);
  }
  if (variant == 1)
  {
    for (int i = 0; i < 3; ++i)
    {
      const double x = rng.range(12, 52), y = rng.range(12, 52);
      radialGlow(cr, x, y, 11, t.accentA, 0.5);
      circle(cr, x, y, rng.range(2.6, 3.8));
      setColor(cr, lighten(t.accentA, 0.45f));
      cairo_fill(cr);
    }
  }
  else if (variant == 2)
  {
    double x = 0, y = rng.range(18, 46);
    cairo_move_to(cr, x, y);
    while (x < 64)
    {
      const double nx = x + rng.range(10, 18), ny = std::clamp(y + rng.range(-12, 12), 8.0, 56.0);
      cairo_curve_to(cr, x + 5, y, nx - 5, ny, nx, ny);
      x = nx;
      y = ny;
    }
    cairo_set_line_width(cr, 6.0);
    setColor(cr, withAlpha(t.trim, 110));
    cairo_stroke_preserve(cr);
    cairo_set_line_width(cr, 2.4);
    setColor(cr, t.trimGlow);
    cairo_stroke(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// The floor's surface: a glossy, swollen lip of flesh with drops on it.
Texture bakeHiveSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  Rng rng(919u);
  const double y = topOffset;
  cairo_move_to(cr, 0, y + 10);
  cairo_curve_to(cr, 0, y - 3, 64, y - 3, 64, y + 10);
  cairo_line_to(cr, 64, y + 12);
  cairo_curve_to(cr, 44, y + 16, 20, y + 16, 0, y + 12);
  cairo_close_path(cr);
  fillGradientOutline(cr, y - 2, y + 14, lighten(t.rockLight, 0.25f), t.rock, darken(t.rockDark, 0.4f), 1.4);
  cairo_move_to(cr, 8, y + 1.5);
  cairo_curve_to(cr, 24, y - 0.5, 40, y - 0.5, 56, y + 1.5);
  setColor(cr, rgba(255, 255, 255, 120));
  cairo_set_line_width(cr, 2.0);
  cairo_stroke(cr);
  for (int i = 0; i < 3; ++i)
  {
    const double x = 8 + i * 22 + rng.range(-3, 3);
    circle(cr, x, y + 4 + rng.range(0, 3), rng.range(1.6, 2.6));
    setColor(cr, rgba(255, 230, 240, 170));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, topOffset);
}

// One-way: a shelf of pale cartilage.
Texture bakeHivePlatform(const Renderer& r, const Theme& t)
{
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  cairo_move_to(cr, 0, 4);
  cairo_curve_to(cr, 20, 0, 44, 0, 64, 4);
  cairo_line_to(cr, 64, 11);
  cairo_curve_to(cr, 44, 15, 20, 15, 0, 11);
  cairo_close_path(cr);
  fillGradientOutline(cr, 0, 15, lighten(t.platform, 0.35f), t.platformDark, darken(t.platformDark, 0.6f), 1.8);
  for (int x = 8; x < 60; x += 12)
  {
    cairo_move_to(cr, x, 4);
    cairo_line_to(cr, x + 2, 12);
    setColor(cr, withAlpha(t.platformDark, 160));
    cairo_set_line_width(cr, 1.4);
    cairo_stroke(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// The hive's cavity: dark wet red, far ribs glowing faintly.
Texture bakeHiveSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  gradient(cr, kScreenW, kScreenH, {{0.0, t.skyTop}, {0.55, t.skyMid}, {1.0, t.skyBottom}});
  Rng rng(4545u);
  // Ribs arching over the cavity, far away.
  for (int i = 0; i < 9; ++i)
  {
    const double x = -80 + i * 170 + rng.range(-20, 20);
    cairo_move_to(cr, x, kScreenH);
    cairo_curve_to(cr, x + 10, kScreenH * 0.3, x + 140, 40, x + 300, 0);
    cairo_set_line_width(cr, rng.range(14, 26));
    setColor(cr, withAlpha(lighten(t.skyMid, 0.18f), 120));
    cairo_stroke(cr);
  }
  // Glowing motes floating in the warm air.
  for (int i = 0; i < 80; ++i)
  {
    const double x = rng.range(0, kScreenW), y = rng.range(0, kScreenH);
    radialGlow(cr, x, y, rng.range(3, 8), i % 3 ? t.accentA : t.accentB, 0.35);
  }
  // A great heart throbbing behind the walls, low on the right.
  radialGlow(cr, 960, 520, 360, t.accentA, 0.22);
  radialGlow(cr, 960, 520, 160, lighten(t.accentA, 0.3f), 0.18);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Far: the hive's inner chambers, arch after arch, egg clusters in the dark.
Texture bakeHiveFar(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(5353u);
  for (double x = 0; x < layerW; x += rng.range(140, 220))
  {
    const double w = rng.range(160, 260), top = rng.range(120, 260);
    cairo_move_to(cr, x, kScreenH);
    cairo_line_to(cr, x, top + 80);
    cairo_curve_to(cr, x, top - 20, x + w, top - 20, x + w, top + 80);
    cairo_line_to(cr, x + w, kScreenH);
    cairo_line_to(cr, x + w - 26, kScreenH);
    cairo_line_to(cr, x + w - 26, top + 90);
    cairo_curve_to(cr, x + w - 26, top + 10, x + 26, top + 10, x + 26, top + 90);
    cairo_line_to(cr, x + 26, kScreenH);
    cairo_close_path(cr);
    setColor(cr, withAlpha(t.farLayer, 255));
    cairo_fill(cr);
    // Eggs in the arch's foot.
    for (int k = 0; k < 4; ++k)
    {
      const double ex = x + 30 + rng.range(0, w - 60), ey = kScreenH - rng.range(40, 120);
      cairo_save(cr);
      cairo_translate(cr, ex, ey);
      cairo_scale(cr, 14, 20);
      circle(cr, 0, 0, 1);
      cairo_restore(cr);
      setColor(cr, withAlpha(lighten(t.farLayer, 0.25f), 255));
      cairo_fill(cr);
      radialGlow(cr, ex, ey, 16, t.accentB, 0.25);
    }
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Near: strands of mucus hanging from above, with drops sliding down them.
Texture bakeHiveNear(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(6464u);
  for (double x = 0; x < layerW; x += rng.range(40, 110))
  {
    const double len = rng.range(80, 300), sway = rng.range(-20, 20);
    cairo_move_to(cr, x, 0);
    cairo_curve_to(cr, x + sway * 0.3, len * 0.5, x + sway, len * 0.8, x + sway * 0.9, len);
    cairo_set_line_width(cr, rng.range(2, 5));
    setColor(cr, withAlpha(lighten(t.nearLayer, 0.35f), 170));
    cairo_stroke(cr);
    circle(cr, x + sway * 0.9, len + 4, rng.range(3, 6));
    setColor(cr, withAlpha(lighten(t.rockLight, 0.3f), 200));
    cairo_fill(cr);
    radialGlow(cr, x + sway * 0.9, len + 4, 12, t.accentA, 0.3);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

} // namespace

bool isAlien(const Theme& t) { return lookOf(t).rfind("alien", 0) == 0; }

// Flesh-rock: rounded cells packed together, a vein of light now and then.
Texture bakeAlienSolid(const Renderer& r, const Theme& t, int variant)
{
  if (isHive(t))
    return bakeHiveSolid(r, t, variant);
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  Rng rng(std::uint32_t(variant * 4517 + 3));
  cairo_rectangle(cr, 0, 0, 64, 64);
  setColor(cr, darken(t.rockDark, 0.25f));
  cairo_fill(cr);
  // Cells on a jittered grid that wraps at the edges, so blocks tile.
  for (int gy = 0; gy < 3; ++gy)
    for (int gx = 0; gx < 3; ++gx)
    {
      const double cx = 11 + gx * 21 + rng.range(-3, 3), cy = 11 + gy * 21 + rng.range(-3, 3);
      const double rx = rng.range(9, 12.5), ry = rng.range(8, 11.5);
      const Color fill = lerpColor(t.rock, t.rockLight, float(rng.range(0.0, 0.35)));
      blob(cr, cx, cy, rx, ry, fill, darken(t.rockDark, 0.4f));
      // A wet highlight.
      cairo_save(cr);
      cairo_translate(cr, cx - rx * 0.35, cy - ry * 0.45);
      cairo_scale(cr, rx * 0.35, ry * 0.18);
      cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
      cairo_restore(cr);
      setColor(cr, rgba(255, 255, 255, 46));
      cairo_fill(cr);
    }
  if (variant == 1)
  {
    // Glowing pustules.
    for (int i = 0; i < 3; ++i)
    {
      const double x = rng.range(10, 54), y = rng.range(10, 54);
      radialGlow(cr, x, y, 10, t.trimGlow, 0.45);
      circle(cr, x, y, rng.range(2.4, 3.6));
      setColor(cr, lighten(t.trimGlow, 0.3f));
      cairo_fill(cr);
    }
  }
  else if (variant == 2)
  {
    // A vein of light winding through the cells.
    double x = 0, y = rng.range(20, 44);
    cairo_move_to(cr, x, y);
    while (x < 64)
    {
      const double nx = x + rng.range(10, 18), ny = std::clamp(y + rng.range(-10, 10), 8.0, 56.0);
      cairo_curve_to(cr, x + 5, y, nx - 5, ny, nx, ny);
      x = nx;
      y = ny;
    }
    cairo_set_line_width(cr, 5.0);
    setColor(cr, withAlpha(t.trim, 90));
    cairo_stroke_preserve(cr);
    cairo_set_line_width(cr, 2.0);
    setColor(cr, lighten(t.trimGlow, 0.2f));
    cairo_stroke(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// The surface: a fringe of glowing moss with little bulbs.
Texture bakeAlienSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  if (isHive(t))
    return bakeHiveSolidTop(r, t, topOffset, height);
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  Rng rng(777u);
  const double y = topOffset;
  cairo_pattern_t* p = cairo_pattern_create_linear(0, y - 18, 0, y + 6);
  cairo_pattern_add_color_stop_rgba(p, 0, redOf(t.trimGlow) / 255.0, greenOf(t.trimGlow) / 255.0, blueOf(t.trimGlow) / 255.0, 0);
  cairo_pattern_add_color_stop_rgba(p, 1, redOf(t.trimGlow) / 255.0, greenOf(t.trimGlow) / 255.0, blueOf(t.trimGlow) / 255.0, 0.30);
  cairo_rectangle(cr, 0, y - 18, 64, 24);
  cairo_set_source(cr, p);
  cairo_fill(cr);
  cairo_pattern_destroy(p);
  // The mat.
  cairo_move_to(cr, 0, y + 7);
  for (int x = 0; x <= 64; x += 4)
    cairo_line_to(cr, x, y - 1 - (x % 8 == 0 ? rng.range(0.0, 3.0) : 0.0));
  cairo_line_to(cr, 64, y + 7);
  for (int x = 64; x >= 0; x -= 8)
    cairo_line_to(cr, x, y + 7 + rng.range(0, 6));
  cairo_close_path(cr);
  fillGradientOutline(cr, y - 4, y + 12, lighten(t.trim, 0.15f), darken(t.trim, 0.45f), darken(t.trim, 0.7f), 1.4);
  // Stalks with glowing bulbs.
  for (int i = 0; i < 6; ++i)
  {
    const double x = 4 + i * 10.5 + rng.range(-2, 2), h = rng.range(5, 14), lean = rng.range(-3, 3);
    cairo_move_to(cr, x, y);
    cairo_curve_to(cr, x, y - h * 0.5, x + lean, y - h * 0.7, x + lean, y - h);
    cairo_set_line_width(cr, 1.6);
    setColor(cr, darken(t.trim, 0.2f));
    cairo_stroke(cr);
    radialGlow(cr, x + lean, y - h, 5, t.accentB, 0.5);
    circle(cr, x + lean, y - h, 1.8);
    setColor(cr, lighten(t.accentB, 0.4f));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, topOffset);
}

// One-way: a shelf fungus.
Texture bakeAlienPlatform(const Renderer& r, const Theme& t)
{
  if (isHive(t))
    return bakeHivePlatform(r, t);
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  radialGlow(cr, 32, 12, 30, t.platform, 0.25);
  cairo_move_to(cr, 0, 3);
  cairo_curve_to(cr, 16, -1, 48, -1, 64, 3);
  cairo_line_to(cr, 64, 9);
  cairo_curve_to(cr, 48, 17, 16, 17, 0, 9);
  cairo_close_path(cr);
  fillGradientOutline(cr, 0, 16, lighten(t.platform, 0.25f), t.platformDark, darken(t.platformDark, 0.55f), 1.8);
  // Gills underneath.
  for (int x = 6; x < 60; x += 6)
  {
    cairo_move_to(cr, x, 9);
    cairo_line_to(cr, x + 1, 13);
    cairo_set_line_width(cr, 1.2);
    setColor(cr, withAlpha(lighten(t.platform, 0.5f), 150));
    cairo_stroke(cr);
  }
  strokeLimb(cr, {{8, 4}, {56, 4}}, 1.6, withAlpha(lighten(t.platform, 0.6f), 200), kInk, 0.0);
  return img.toTexture(r, 0.0f, 0.0f);
}

// Twilight over Vurr: a violet sky, two moons, a ringed giant on the
// horizon and a lot of stars.
Texture bakeAlienSky(const Renderer& r, const Theme& t)
{
  if (isHive(t))
    return bakeHiveSky(r, t);
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  gradient(cr, kScreenW, kScreenH, {{0.0, t.skyTop}, {0.6, t.skyMid}, {1.0, t.skyBottom}});
  Rng rng(4343u);
  for (int i = 0; i < 260; ++i)
  {
    const double y = rng.range(0, kScreenH * 0.75);
    circle(cr, rng.range(0, kScreenW), y, rng.range(0.5, 1.6));
    setColor(cr, rgba(255, 255, 255, int(40 + 180 * (1.0 - y / (kScreenH * 0.75)) * rng.uniform())));
    cairo_fill(cr);
  }
  // A nebula smear.
  for (int i = 0; i < 9; ++i)
    radialGlow(cr, 300 + i * 90 + rng.range(-40, 40), 120 + rng.range(-50, 50), rng.range(80, 160),
      i % 2 ? t.accentA : t.accentB, 0.08);
  // The ringed giant, low on the horizon.
  {
    const double gx = 980, gy = 470, gr = 150;
    radialGlow(cr, gx, gy, gr * 1.6, t.accentB, 0.18);
    cairo_save(cr);
    circle(cr, gx, gy, gr);
    cairo_clip(cr);
    gradient(cr, kScreenW, kScreenH, {{0.0, lighten(t.accentA, 0.2f)}, {0.6, t.accentA}, {1.0, darken(t.accentA, 0.6f)}});
    for (int b = 0; b < 7; ++b)
    {
      cairo_rectangle(cr, gx - gr, gy - gr + b * 44 + rng.range(-6, 6), gr * 2, rng.range(8, 20));
      setColor(cr, withAlpha(darken(t.accentA, 0.35f), 120));
      cairo_fill(cr);
    }
    // The night side.
    circle(cr, gx + gr * 0.55, gy + gr * 0.35, gr * 1.05);
    setColor(cr, rgba(10, 4, 30, 150));
    cairo_fill(cr);
    cairo_restore(cr);
    cairo_save(cr);
    cairo_translate(cr, gx, gy);
    cairo_rotate(cr, -0.28);
    cairo_scale(cr, gr * 2.0, gr * 0.36);
    circle(cr, 0, 0, 1);
    cairo_restore(cr);
    cairo_set_line_width(cr, 7);
    setColor(cr, withAlpha(lighten(t.accentB, 0.35f), 150));
    cairo_stroke(cr);
  }
  // Two moons.
  const double moons[2][3] = {{220, 140, 46}, {410, 90, 20}};
  for (const auto& m : moons)
  {
    radialGlow(cr, m[0], m[1], m[2] * 2.4, rgb(220, 255, 240), 0.22);
    circle(cr, m[0], m[1], m[2]);
    setColor(cr, rgb(226, 240, 236));
    cairo_fill(cr);
    for (int c = 0; c < 4; ++c)
    {
      circle(cr, m[0] + rng.range(-m[2] * 0.5, m[2] * 0.5), m[1] + rng.range(-m[2] * 0.5, m[2] * 0.5),
        m[2] * rng.range(0.1, 0.22));
      setColor(cr, rgba(150, 180, 190, 120));
      cairo_fill(cr);
    }
    circle(cr, m[0] + m[2] * 0.45, m[1] - m[2] * 0.1, m[2] * 0.95);
    setColor(cr, rgba(30, 10, 60, 110));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Far: a forest of giant mushrooms against the horizon.
Texture bakeAlienFar(const Renderer& r, const Theme& t, int layerW)
{
  if (isHive(t))
    return bakeHiveFar(r, t, layerW);
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(5151u);
  // Rolling hills.
  cairo_move_to(cr, 0, kScreenH);
  for (int x = 0; x <= layerW; x += 40)
    cairo_line_to(cr, x, 560 + 30 * std::sin(x * 2 * kPi / layerW * 4) + rng.range(-8, 8));
  cairo_line_to(cr, layerW, kScreenH);
  cairo_close_path(cr);
  setColor(cr, withAlpha(t.farLayer, 255));
  cairo_fill(cr);
  for (double x = 30; x < layerW - 60; x += rng.range(90, 170))
  {
    const double h = rng.range(220, 420), w = rng.range(110, 220);
    mushroom(cr, x, 600, h, w, withAlpha(t.farLayer, 255), rng.uniform() < 0.5 ? t.accentB : t.trimGlow, 0.55);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Near: hanging glow-fronds from above and spores in the air.
Texture bakeAlienNear(const Renderer& r, const Theme& t, int layerW)
{
  if (isHive(t))
    return bakeHiveNear(r, t, layerW);
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(6262u);
  for (double x = 0; x < layerW; x += rng.range(50, 120))
  {
    const double len = rng.range(60, 220), sway = rng.range(-30, 30);
    cairo_move_to(cr, x, 0);
    cairo_curve_to(cr, x + sway * 0.2, len * 0.4, x + sway, len * 0.7, x + sway * 0.8, len);
    cairo_set_line_width(cr, rng.range(3, 7));
    setColor(cr, withAlpha(t.nearLayer, 230));
    cairo_stroke(cr);
    // Leaflets and a glowing tip.
    for (int k = 1; k < 5; ++k)
    {
      const double ty = len * k / 5.0, tx = x + sway * (ty / len) * 0.9;
      cairo_save(cr);
      cairo_translate(cr, tx, ty);
      cairo_rotate(cr, (k % 2 ? 0.7 : -0.7));
      cairo_scale(cr, 12, 4);
      circle(cr, k % 2 ? 1 : -1, 0, 1);
      cairo_restore(cr);
      setColor(cr, withAlpha(t.nearLayer, 220));
      cairo_fill(cr);
    }
    radialGlow(cr, x + sway * 0.8, len, 18, t.trimGlow, 0.5);
    circle(cr, x + sway * 0.8, len, 4);
    setColor(cr, lighten(t.trimGlow, 0.4f));
    cairo_fill(cr);
  }
  for (int i = 0; i < 90; ++i)
  {
    const double x = rng.range(0, layerW), y = rng.range(80, kScreenH);
    radialGlow(cr, x, y, 6, i % 3 ? t.accentB : t.trimGlow, 0.4);
    circle(cr, x, y, rng.range(1.0, 2.2));
    setColor(cr, rgba(240, 255, 250, 200));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

} // namespace gr
