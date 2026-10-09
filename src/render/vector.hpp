#pragma once

#include "render/renderer.hpp"

#include <cairo.h>

#include <initializer_list>
#include <utility>

namespace gr
{

// An offscreen Cairo surface used to bake anti-aliased vector art into
// textures at startup.
class VectorImage
{
public:
  VectorImage(int w, int h);
  ~VectorImage();
  VectorImage(const VectorImage&) = delete;
  VectorImage& operator=(const VectorImage&) = delete;

  cairo_t* cr() const { return mCr; }
  int w() const { return mW; }
  int h() const { return mH; }
  Texture toTexture(const Renderer& r, float anchorX = 0.0f, float anchorY = 0.0f, bool mirrorX = false) const;

private:
  cairo_surface_t* mSurface;
  cairo_t* mCr;
  int mW;
  int mH;
};

void setColor(cairo_t* cr, Color c);
// The game's one typeface, DejaVu Sans Bold (italic is slanted). On the
// desktop fontconfig finds it; builds with GR_BUNDLED_FONT (Android) have no
// fontconfig and load the TTF with loadGameFont first. Without it Cairo
// falls back to its built-in font.
void selectGameFont(cairo_t* cr, bool italic = false);
bool loadGameFont(const char* ttfPath);
void roundedRect(cairo_t* cr, double x, double y, double w, double h, double r);
// Fills the current path with a vertical gradient (top -> bottom) and strokes
// it with an outline, preserving nothing.
void fillGradientOutline(
  cairo_t* cr,
  double y0,
  double y1,
  Color top,
  Color bottom,
  Color outline,
  double outlineWidth);
void fillOutline(cairo_t* cr, Color fill, Color outline, double outlineWidth);
// Thick line through points with an outline (limbs, vines, beams).
void strokeLimb(
  cairo_t* cr,
  std::initializer_list<std::pair<double, double>> pts,
  double width,
  Color fill,
  Color outline,
  double outlineWidth);
void radialGlow(cairo_t* cr, double cx, double cy, double radius, Color c, double alpha);

} // namespace gr
