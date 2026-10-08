#include "render/vector.hpp"

#include <algorithm>

namespace gr
{

VectorImage::VectorImage(int w, int h)
  : mSurface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h))
  , mCr(cairo_create(mSurface))
  , mW(w)
  , mH(h)
{
  cairo_set_line_join(mCr, CAIRO_LINE_JOIN_ROUND);
  cairo_set_line_cap(mCr, CAIRO_LINE_CAP_ROUND);
}

VectorImage::~VectorImage()
{
  cairo_destroy(mCr);
  cairo_surface_destroy(mSurface);
}

Texture VectorImage::toTexture(const Renderer& r, float anchorX, float anchorY, bool mirrorX) const
{
  cairo_surface_flush(mSurface);
  return r.createTexture(
    mW,
    mH,
    cairo_image_surface_get_data(mSurface),
    cairo_image_surface_get_stride(mSurface),
    anchorX,
    anchorY,
    mirrorX);
}

void setColor(cairo_t* cr, Color c)
{
  cairo_set_source_rgba(
    cr, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, alphaOf(c) / 255.0);
}

void roundedRect(cairo_t* cr, double x, double y, double w, double h, double r)
{
  const double pi = 3.14159265358979;
  r = std::min(r, std::min(w, h) / 2.0);
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + w - r, y + r, r, -pi / 2, 0);
  cairo_arc(cr, x + w - r, y + h - r, r, 0, pi / 2);
  cairo_arc(cr, x + r, y + h - r, r, pi / 2, pi);
  cairo_arc(cr, x + r, y + r, r, pi, 3 * pi / 2);
  cairo_close_path(cr);
}

static void addStop(cairo_pattern_t* p, double offset, Color c)
{
  cairo_pattern_add_color_stop_rgba(
    p, offset, redOf(c) / 255.0, greenOf(c) / 255.0, blueOf(c) / 255.0, alphaOf(c) / 255.0);
}

void fillGradientOutline(
  cairo_t* cr,
  double y0,
  double y1,
  Color top,
  Color bottom,
  Color outline,
  double outlineWidth)
{
  cairo_pattern_t* p = cairo_pattern_create_linear(0, y0, 0, y1);
  addStop(p, 0.0, top);
  addStop(p, 1.0, bottom);
  cairo_set_source(cr, p);
  if (outlineWidth > 0.0)
  {
    cairo_fill_preserve(cr);
    setColor(cr, outline);
    cairo_set_line_width(cr, outlineWidth);
    cairo_stroke(cr);
  }
  else
  {
    cairo_fill(cr);
  }
  cairo_pattern_destroy(p);
}

void fillOutline(cairo_t* cr, Color fill, Color outline, double outlineWidth)
{
  setColor(cr, fill);
  if (outlineWidth > 0.0)
  {
    cairo_fill_preserve(cr);
    setColor(cr, outline);
    cairo_set_line_width(cr, outlineWidth);
    cairo_stroke(cr);
  }
  else
  {
    cairo_fill(cr);
  }
}

void strokeLimb(
  cairo_t* cr,
  std::initializer_list<std::pair<double, double>> pts,
  double width,
  Color fill,
  Color outline,
  double outlineWidth)
{
  auto path = [&]() {
    bool first = true;
    for (const auto& p : pts)
    {
      if (first)
        cairo_move_to(cr, p.first, p.second);
      else
        cairo_line_to(cr, p.first, p.second);
      first = false;
    }
  };
  if (outlineWidth > 0.0)
  {
    path();
    setColor(cr, outline);
    cairo_set_line_width(cr, width + outlineWidth * 2.0);
    cairo_stroke(cr);
  }
  path();
  setColor(cr, fill);
  cairo_set_line_width(cr, width);
  cairo_stroke(cr);
}

void radialGlow(cairo_t* cr, double cx, double cy, double radius, Color c, double alpha)
{
  cairo_pattern_t* p = cairo_pattern_create_radial(cx, cy, 0, cx, cy, radius);
  addStop(p, 0.0, withAlpha(c, int(255 * alpha)));
  addStop(p, 0.4, withAlpha(c, int(110 * alpha)));
  addStop(p, 1.0, withAlpha(c, 0));
  cairo_set_source(cr, p);
  cairo_arc(cr, cx, cy, radius, 0, 6.2831853);
  cairo_fill(cr);
  cairo_pattern_destroy(p);
}

} // namespace gr
