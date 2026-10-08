#include "data/theme.hpp"

#include <array>
#include <string_view>

namespace gr
{

namespace
{

const std::array<Theme, 3> kThemes{{
  {ThemeId::NeonOverdrive,
   "NEON OVERDRIVE",
   "SYNTHWAVE ROOFTOPS OF NEON CITY",
   rgb(18, 6, 40), rgb(110, 20, 110), rgb(255, 120, 70),
   rgb(42, 30, 78), rgb(70, 54, 120), rgb(24, 16, 46),
   rgb(0, 240, 255), rgb(150, 255, 255),
   rgb(255, 60, 200), rgb(140, 20, 110),
   rgb(255, 40, 90), rgb(255, 170, 200),
   rgb(255, 230, 60), rgb(0, 240, 255),
   rgb(52, 20, 80), rgb(30, 10, 50),
   rgb(170, 180, 205), rgb(230, 238, 250), rgb(80, 86, 110), rgb(255, 40, 80),
   rgb(255, 255, 255)},
  {ThemeId::LostTemple,
   "LOST TEMPLE",
   "JUNGLE RUINS FULL OF GOLD AND TRAPS",
   rgb(40, 120, 120), rgb(120, 190, 150), rgb(230, 230, 170),
   rgb(110, 100, 80), rgb(150, 140, 110), rgb(70, 62, 48),
   rgb(70, 170, 50), rgb(150, 230, 90),
   rgb(150, 95, 50), rgb(95, 55, 25),
   rgb(200, 190, 160), rgb(250, 245, 220),
   rgb(255, 200, 40), rgb(255, 120, 30),
   rgb(60, 120, 90), rgb(25, 70, 40),
   rgb(135, 130, 105), rgb(185, 180, 150), rgb(70, 66, 52), rgb(255, 170, 0),
   rgb(255, 250, 220)},
  {ThemeId::StationZero,
   "STATION ZERO",
   "A FROZEN ORBITAL BASE GONE ROGUE",
   rgb(2, 4, 16), rgb(10, 20, 50), rgb(30, 50, 90),
   rgb(90, 104, 124), rgb(140, 156, 176), rgb(50, 58, 72),
   rgb(255, 200, 0), rgb(255, 240, 120),
   rgb(120, 200, 230), rgb(60, 110, 140),
   rgb(120, 220, 255), rgb(230, 250, 255),
   rgb(255, 120, 30), rgb(80, 255, 140),
   rgb(30, 40, 70), rgb(16, 22, 40),
   rgb(235, 235, 240), rgb(255, 255, 255), rgb(120, 125, 140), rgb(80, 255, 140),
   rgb(220, 240, 255)},
}};

// Per-level palettes: a family's art routines in the level's own colours.
struct Variant
{
  const char* key;
  Theme theme;
};

const std::array<Variant, 1> kVariants{{
  {"glass_canyon",
   {ThemeId::NeonOverdrive,
    "GLASS CANYON",
    "MIRRORED TOWERS AT PINK DAWN",
    rgb(54, 40, 104), rgb(226, 118, 168), rgb(255, 204, 170),
    rgb(64, 104, 146), rgb(150, 200, 232), rgb(30, 48, 80),
    rgb(255, 226, 240), rgb(255, 176, 220),
    rgb(196, 218, 240), rgb(86, 106, 140),
    rgb(255, 60, 110), rgb(255, 190, 210),
    rgb(255, 220, 120), rgb(120, 230, 255),
    rgb(120, 88, 150), rgb(80, 58, 110),
    rgb(180, 190, 212), rgb(240, 245, 255), rgb(80, 90, 112), rgb(255, 60, 120),
    rgb(255, 255, 255)}},
}};

} // namespace

int themeCount() { return int(kThemes.size()); }

int themeTotal() { return themeCount() + int(kVariants.size()); }

const Theme& themeByIndex(int index)
{
  const int n = themeCount();
  if (index >= n && index < themeTotal())
    return kVariants[std::size_t(index - n)].theme;
  return kThemes[std::size_t(((index % n) + n) % n)];
}

int themeIndexForKey(const char* key)
{
  for (std::size_t i = 0; i < kVariants.size(); ++i)
    if (std::string_view(key) == kVariants[i].key)
      return themeCount() + int(i);
  return -1;
}

} // namespace gr
