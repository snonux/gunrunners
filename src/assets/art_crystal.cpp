// Level 46, Crystal Drift (Episode 7, DEEP SPACE): floating islands of
// violet and teal crystal in Vurr's pale upper sky. Faceted crystal rock for
// the tiles, a soft lilac sky over a sea of cloud, far islands hanging in
// the haze and loose shards drifting close by.

#include "assets/art_alien.hpp"

#include "base/math.hpp"
#include "render/vector.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace gr
{

namespace
{

constexpr double kPi = 3.14159265358979;
constexpr Color kInk = rgb(30, 16, 56);

Color lighten(Color c, float t) { return lerpColor(c, rgb(255, 255, 255), t); }
Color darken(Color c, float t) { return lerpColor(c, rgb(0, 0, 0), t); }

void circle(cairo_t* cr, double x, double y, double r) { cairo_arc(cr, x, y, r, 0, 2 * kPi); }

void ellipse(cairo_t* cr, double x, double y, double rx, double ry)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_scale(cr, rx, ry);
  cairo_arc(cr, 0, 0, 1, 0, 2 * kPi);
  cairo_restore(cr);
}

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

// A crystal: a long hexagonal prism standing at (x, y) (its foot), leaning
// by `lean` radians, its lit face on the left and its shade on the right.
void prism(cairo_t* cr, double x, double y, double len, double wide, double lean, Color c, double alpha)
{
  cairo_save(cr);
  cairo_translate(cr, x, y);
  cairo_rotate(cr, lean);
  const double tip = len, hw = wide * 0.5;
  // The shade side.
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, hw, -wide * 0.2);
  cairo_line_to(cr, hw, -tip + wide * 0.6);
  cairo_line_to(cr, 0, -tip);
  cairo_close_path(cr);
  setColor(cr, withAlpha(darken(c, 0.25f), int(alpha * 255)));
  cairo_fill(cr);
  // The lit side.
  cairo_move_to(cr, 0, 0);
  cairo_line_to(cr, -hw, -wide * 0.2);
  cairo_line_to(cr, -hw, -tip + wide * 0.6);
  cairo_line_to(cr, 0, -tip);
  cairo_close_path(cr);
  setColor(cr, withAlpha(lighten(c, 0.3f), int(alpha * 255)));
  cairo_fill(cr);
  // The edge down the middle, catching the light.
  cairo_move_to(cr, 0, -tip);
  cairo_line_to(cr, 0, 0);
  cairo_set_line_width(cr, std::max(1.0, wide * 0.08));
  setColor(cr, withAlpha(lighten(c, 0.75f), int(alpha * 220)));
  cairo_stroke(cr);
  cairo_restore(cr);
}

// A floating island: a flat top, an upside-down cone of crystal rock under
// it, and spires on top.
void island(cairo_t* cr, Rng& rng, double cx, double top, double w, Color rock, Color shard, double alpha)
{
  const double depth = w * rng.range(0.5, 0.8);
  cairo_move_to(cr, cx - w * 0.5, top);
  cairo_line_to(cr, cx + w * 0.5, top);
  for (int k = 1; k <= 6; ++k)
  {
    const double t = double(k) / 6.0;
    cairo_line_to(cr, cx + w * 0.5 * (1.0 - t) + rng.range(-6, 6), top + depth * t * rng.range(0.8, 1.1));
  }
  for (int k = 5; k >= 1; --k)
  {
    const double t = double(k) / 6.0;
    cairo_line_to(cr, cx - w * 0.5 * (1.0 - t) + rng.range(-6, 6), top + depth * t * rng.range(0.8, 1.1));
  }
  cairo_close_path(cr);
  setColor(cr, withAlpha(rock, int(alpha * 255)));
  cairo_fill(cr);
  for (int s = 0; s < 4; ++s)
    prism(cr, cx + rng.range(-w * 0.4, w * 0.4), top + 2, rng.range(w * 0.12, w * 0.3), rng.range(w * 0.05, w * 0.09),
      rng.range(-0.35, 0.35), shard, alpha);
}

} // namespace

bool isCrystal(const Theme& t) { return std::string_view(t.look) == "alien_crystal"; }

// Crystal rock: violet stone packed with facets, each lit on its upper
// left. Variant 1 has a teal vein of crystal, 2 a big cut face.
Texture bakeCrystalSolid(const Renderer& r, const Theme& t, int variant)
{
  VectorImage img(64, 64);
  cairo_t* cr = img.cr();
  Rng rng(std::uint32_t(variant * 7919 + 46));
  gradient(cr, 64, 64, {{0.0, t.rock}, {1.0, darken(t.rock, 0.22f)}});
  // Facets: triangles on a jittered grid that wraps at the edges.
  const double pts[3][3][2] = {
    {{0, 0}, {22, 0}, {44, 0}}, {{0, 22}, {22, 22}, {44, 22}}, {{0, 44}, {22, 44}, {44, 44}}};
  for (int gy = 0; gy < 3; ++gy)
    for (int gx = 0; gx < 3; ++gx)
    {
      const double x = pts[gy][gx][0] + rng.range(-3, 3), y = pts[gy][gx][1] + rng.range(-3, 3);
      for (int k = 0; k < 2; ++k)
      {
        cairo_move_to(cr, x, y);
        cairo_line_to(cr, x + 22, y + (k ? 22 : 0));
        cairo_line_to(cr, x + (k ? 0 : 22), y + 22);
        cairo_close_path(cr);
        const float shade = float(rng.range(-0.2, 0.3));
        setColor(cr, withAlpha(shade > 0 ? lighten(t.rock, shade) : darken(t.rock, -shade), 150));
        cairo_fill_preserve(cr);
        cairo_set_line_width(cr, 1.0);
        setColor(cr, withAlpha(lighten(t.rockLight, 0.2f), 70));
        cairo_stroke(cr);
      }
    }
  // Glints.
  for (int i = 0; i < 6; ++i)
  {
    const double x = rng.range(4, 60), y = rng.range(4, 60);
    cairo_move_to(cr, x - 3, y);
    cairo_line_to(cr, x + 3, y);
    cairo_move_to(cr, x, y - 3);
    cairo_line_to(cr, x, y + 3);
    cairo_set_line_width(cr, 1.0);
    setColor(cr, rgba(255, 250, 255, 150));
    cairo_stroke(cr);
  }
  if (variant == 1)
  {
    double x = 0, y = rng.range(18, 46);
    cairo_move_to(cr, x, y);
    while (x < 64)
    {
      x += rng.range(8, 14);
      y = std::clamp(y + rng.range(-9, 9), 8.0, 56.0);
      cairo_line_to(cr, x, y);
    }
    cairo_set_line_width(cr, 6.0);
    setColor(cr, withAlpha(t.trim, 140));
    cairo_stroke_preserve(cr);
    cairo_set_line_width(cr, 2.0);
    setColor(cr, t.trimGlow);
    cairo_stroke(cr);
  }
  else if (variant == 2)
  {
    cairo_move_to(cr, 14, 50);
    cairo_line_to(cr, 32, 12);
    cairo_line_to(cr, 50, 50);
    cairo_close_path(cr);
    setColor(cr, withAlpha(lighten(t.rockLight, 0.2f), 120));
    cairo_fill_preserve(cr);
    setColor(cr, withAlpha(t.rockDark, 160));
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// The top of an island: a glassy rim with little teal crystals growing up
// out of it.
Texture bakeCrystalSolidTop(const Renderer& r, const Theme& t, float topOffset, int height)
{
  VectorImage img(64, height);
  cairo_t* cr = img.cr();
  Rng rng(4646u);
  const double y = topOffset;
  cairo_rectangle(cr, 0, y, 64, 10);
  fillGradientOutline(cr, y, y + 10, lighten(t.rockLight, 0.4f), t.rockLight, darken(t.rock, 0.2f), 1.0);
  for (int i = 0; i < 3; ++i)
    prism(cr, rng.range(6, 58), y + 4, rng.range(8, 16), rng.range(4, 7), rng.range(-0.5, 0.5),
      i % 2 ? t.trim : t.accentA, 0.9);
  cairo_move_to(cr, 0, y + 1);
  cairo_line_to(cr, 64, y + 1);
  cairo_set_line_width(cr, 2.0);
  setColor(cr, rgba(255, 255, 255, 180));
  cairo_stroke(cr);
  return img.toTexture(r, 0.0f, topOffset);
}

// One-way: a slab of clear crystal, lit along its top edge.
Texture bakeCrystalPlatform(const Renderer& r, const Theme& t)
{
  VectorImage img(64, 40);
  cairo_t* cr = img.cr();
  cairo_move_to(cr, 0, 1);
  cairo_line_to(cr, 64, 1);
  cairo_line_to(cr, 58, 13);
  cairo_line_to(cr, 6, 13);
  cairo_close_path(cr);
  fillGradientOutline(cr, 1, 13, lighten(t.platform, 0.4f), withAlpha(t.platformDark, 200), kInk, 1.4);
  cairo_move_to(cr, 4, 3);
  cairo_line_to(cr, 60, 3);
  cairo_set_line_width(cr, 2.0);
  setColor(cr, rgba(255, 255, 255, 200));
  cairo_stroke(cr);
  return img.toTexture(r, 0.0f, 0.0f);
}

// The sky: pale lilac going blue at the horizon, soft cloud banks low
// down, and Vurr's two moons pale in the daylight.
Texture bakeCrystalSky(const Renderer& r, const Theme& t)
{
  VectorImage img(kScreenW, kScreenH);
  cairo_t* cr = img.cr();
  gradient(cr, kScreenW, kScreenH, {{0.0, t.skyTop}, {0.6, t.skyMid}, {1.0, t.skyBottom}});
  Rng rng(4600u);
  radialGlow(cr, 980, 120, 260, rgb(255, 250, 230), 0.5);
  const double moons[2][3] = {{300, 140, 46}, {420, 90, 18}};
  for (const auto& m : moons)
  {
    circle(cr, m[0], m[1], m[2]);
    setColor(cr, rgba(255, 255, 255, 110));
    cairo_fill(cr);
    circle(cr, m[0] + m[2] * 0.35, m[1] + m[2] * 0.15, m[2] * 0.9);
    setColor(cr, withAlpha(t.skyTop, 90));
    cairo_fill(cr);
  }
  // Cloud banks: rows of soft puffs, paler further down.
  for (int row = 0; row < 4; ++row)
  {
    const double y = kScreenH * 0.62 + row * 60;
    for (double x = -60; x < kScreenW + 60; x += rng.range(50, 90))
    {
      ellipse(cr, x, y + rng.range(-10, 10), rng.range(60, 120), rng.range(22, 40));
      setColor(cr, rgba(255, 250, 255, 40 + row * 25));
      cairo_fill(cr);
    }
  }
  // Motes of crystal dust catching the sun.
  for (int i = 0; i < 90; ++i)
  {
    circle(cr, rng.range(0, kScreenW), rng.range(0, kScreenH * 0.7), rng.range(0.8, 2.0));
    setColor(cr, rgba(255, 255, 255, int(80 + 120 * rng.uniform())));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Far: islands hanging in the haze, pale and small.
Texture bakeCrystalFar(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(4701u);
  for (double x = 80; x < layerW - 80; x += rng.range(180, 320))
    island(cr, rng, x, rng.range(160, 420), rng.range(70, 150), withAlpha(t.farLayer, 170),
      lighten(t.farLayer, 0.25f), 0.6);
  // The cloud sea along the bottom.
  for (double x = 0; x < layerW; x += 70)
  {
    ellipse(cr, x, kScreenH - 60 + rng.range(-12, 12), rng.range(70, 120), rng.range(30, 50));
    setColor(cr, rgba(250, 244, 255, 150));
    cairo_fill(cr);
  }
  return img.toTexture(r, 0.0f, 0.0f);
}

// Near: bigger islands, and loose shards drifting.
Texture bakeCrystalNear(const Renderer& r, const Theme& t, int layerW)
{
  VectorImage img(layerW, kScreenH);
  cairo_t* cr = img.cr();
  Rng rng(4802u);
  for (double x = 140; x < layerW - 140; x += rng.range(380, 620))
    island(cr, rng, x, rng.range(260, 520), rng.range(140, 240), withAlpha(t.nearLayer, 200), t.trim, 0.75);
  for (int i = 0; i < 26; ++i)
    prism(cr, rng.range(0, layerW), rng.range(60, kScreenH - 120), rng.range(10, 26), rng.range(5, 10),
      rng.range(-kPi, kPi), i % 3 == 0 ? t.accentA : (i % 3 == 1 ? t.trim : lighten(t.nearLayer, 0.3f)), 0.6);
  return img.toTexture(r, 0.0f, 0.0f);
}

} // namespace gr
