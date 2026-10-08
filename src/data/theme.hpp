#pragma once

#include "render/color.hpp"

namespace gr
{

enum class ThemeId
{
  NeonOverdrive = 0,
  LostTemple = 1,
  StationZero = 2,
};

// A theme is one of the three design directions from docs/DESIGN.md. The
// level layout stays the same; tiles, backdrop, enemies and props change.
struct Theme
{
  ThemeId id;
  const char* name;
  const char* tagline;
  Color skyTop, skyMid, skyBottom;
  Color rock, rockLight, rockDark;
  Color trim, trimGlow;
  Color platform, platformDark;
  Color hazard, hazardLight;
  Color accentA, accentB;
  Color farLayer, nearLayer;
  Color enemyBody, enemyLight, enemyDark, enemyEye;
  Color hudText;
};

// The three families the T key cycles through.
int themeCount();
// Families plus the campaign's per-level palettes (indices from
// themeCount() on); anything out of range wraps onto a family.
int themeTotal();
const Theme& themeByIndex(int index);
// A level's own palette by its theme= key (e.g. glass_canyon), -1 if none.
int themeIndexForKey(const char* key);

} // namespace gr
