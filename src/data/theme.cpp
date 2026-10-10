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

const std::array<Variant, 8> kVariants{{
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
  {"club_laserdisc",
   {ThemeId::NeonOverdrive,
    "CLUB LASERDISC",
    "THREE FLOORS OF BASS AND LASERS",
    rgb(8, 4, 20), rgb(40, 10, 70), rgb(90, 20, 110),
    rgb(34, 24, 58), rgb(80, 60, 130), rgb(14, 10, 28),
    rgb(255, 60, 200), rgb(255, 150, 240),
    rgb(60, 220, 255), rgb(20, 90, 140),
    rgb(120, 255, 90), rgb(210, 255, 190),
    rgb(255, 230, 60), rgb(0, 230, 255),
    rgb(60, 30, 100), rgb(36, 18, 64),
    rgb(60, 56, 80), rgb(150, 140, 190), rgb(24, 22, 34), rgb(255, 60, 200),
    rgb(255, 255, 255), "club"}},
  {"blackout",
   {ThemeId::NeonOverdrive,
    "BLACKOUT",
    "A CITY DISTRICT WITH THE POWER CUT",
    rgb(4, 6, 16), rgb(16, 22, 44), rgb(40, 44, 70),
    rgb(52, 56, 70), rgb(96, 100, 120), rgb(26, 28, 38),
    rgb(255, 170, 60), rgb(255, 214, 140),
    rgb(120, 130, 150), rgb(50, 56, 70),
    rgb(255, 60, 60), rgb(255, 170, 160),
    rgb(255, 170, 60), rgb(70, 230, 255),
    rgb(24, 28, 50), rgb(14, 16, 30),
    rgb(60, 60, 80), rgb(120, 118, 150), rgb(20, 20, 30), rgb(255, 230, 90),
    rgb(255, 255, 255)}},
  {"sludge_line",
   {ThemeId::LostTemple,
    "SLUDGE LINE",
    "THE STORM DRAINS UNDER NEON CITY",
    rgb(10, 16, 12), rgb(26, 40, 28), rgb(44, 62, 40),
    rgb(78, 84, 70), rgb(118, 126, 100), rgb(44, 48, 40),
    rgb(120, 200, 60), rgb(190, 250, 110),
    rgb(120, 130, 120), rgb(60, 66, 60),
    rgb(150, 230, 60), rgb(220, 255, 150),
    rgb(255, 200, 40), rgb(120, 230, 90),
    rgb(30, 44, 30), rgb(20, 30, 20),
    rgb(110, 120, 90), rgb(160, 175, 130), rgb(50, 56, 44), rgb(255, 200, 40),
    rgb(240, 255, 220), "sewer"}},
  {"maglev_express",
   {ThemeId::StationZero,
    "MAGLEV EXPRESS",
    "THE MIDNIGHT TRAIN IN THE RAIN",
    rgb(6, 8, 22), rgb(26, 26, 60), rgb(70, 40, 84),
    rgb(150, 156, 174), rgb(214, 220, 234), rgb(66, 70, 88),
    rgb(255, 40, 60), rgb(255, 150, 160),
    rgb(120, 200, 255), rgb(50, 90, 130),
    rgb(255, 60, 60), rgb(255, 180, 170),
    rgb(255, 60, 90), rgb(80, 200, 255),
    rgb(24, 26, 54), rgb(14, 14, 30),
    rgb(70, 74, 92), rgb(160, 168, 196), rgb(26, 28, 38), rgb(255, 60, 60),
    rgb(255, 255, 255), "maglev"}},
  {"chopper_down",
   {ThemeId::NeonOverdrive,
    "CHOPPER DOWN",
    "A BUILDING SITE BY THE BAY, A GUNSHIP OVERHEAD",
    rgb(4, 8, 20), rgb(18, 30, 58), rgb(56, 52, 84),
    rgb(84, 80, 92), rgb(140, 134, 146), rgb(40, 38, 48),
    rgb(255, 170, 30), rgb(255, 220, 120),
    rgb(210, 70, 40), rgb(120, 36, 24),
    rgb(255, 60, 40), rgb(255, 170, 150),
    rgb(255, 190, 40), rgb(120, 220, 255),
    rgb(22, 30, 52), rgb(14, 18, 34),
    rgb(56, 64, 58), rgb(120, 134, 112), rgb(26, 30, 28), rgb(255, 60, 40),
    rgb(255, 255, 255), "crane"}},
  {"jungle_canopy",
   {ThemeId::LostTemple,
    "CANOPY ROAD",
    "MORNING MIST OVER THE JUNGLE CANOPY",
    rgb(70, 150, 140), rgb(160, 210, 170), rgb(236, 236, 190),
    rgb(96, 88, 64), rgb(140, 128, 96), rgb(60, 54, 40),
    rgb(80, 180, 60), rgb(170, 240, 110),
    rgb(160, 110, 60), rgb(100, 64, 30),
    rgb(170, 60, 50), rgb(240, 160, 130),
    rgb(255, 200, 40), rgb(120, 230, 90),
    rgb(70, 130, 96), rgb(30, 80, 46),
    rgb(120, 104, 70), rgb(170, 150, 110), rgb(64, 56, 40), rgb(255, 140, 30),
    rgb(255, 250, 220)}},
  {"sandstone_traps",
   {ThemeId::LostTemple,
    "HALL OF TRAPS",
    "TORCHLIGHT IN THE SANDSTONE HALLS",
    rgb(40, 22, 14), rgb(96, 60, 34), rgb(150, 100, 56),
    rgb(176, 136, 86), rgb(222, 186, 126), rgb(110, 80, 48),
    rgb(214, 170, 96), rgb(255, 210, 140),
    rgb(180, 130, 70), rgb(110, 76, 40),
    rgb(220, 60, 40), rgb(255, 170, 130),
    rgb(255, 200, 60), rgb(255, 120, 40),
    rgb(70, 44, 26), rgb(40, 24, 14),
    rgb(150, 136, 110), rgb(200, 186, 150), rgb(84, 74, 58), rgb(255, 70, 40),
    rgb(255, 240, 210), "tomb"}},
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
