#include "data/enemies.hpp"

#include <array>

namespace gr
{

namespace
{

using K = EnemyKind;
using L = EnemyLook;

const EnemyDef kEnemies[] = {
  // The PoC's classic three (map characters w, f, t).
  {"walker", "WALKER BOT", K::Walker, L::Walker, 3, 3, 3, 250, 2, 0, 0, 0, 0, 0},
  {"flyer", "FLYER DRONE", K::Flyer, L::Flyer, 3, 3, 2, 500, 1, 24, 0, 0, 0, 0},
  {"turret", "TURRET", K::Turret, L::Turret, 3, 2, 4, 1000, 0, 20, 0, 22, 0, 0},
  {"candid_camera", "CANDID CAMERA", K::Camera, L::Camera, 2, 2, 1, 5000, 0, 0, 0, 0,
   kEnemyHarmless | kEnemyNoTally, 0},

  // Episode 1.
  {"chrome_cop", "CHROME COP", K::Walker, L::Walker, 3, 3, 3, 250, 2, 0, 0, 0, 0, 0, 3},
  {"hover_lens", "HOVER LENS", K::Flyer, L::Flyer, 3, 3, 2, 500, 1, 30, 8, 0, kEnemyBeatDive, 0},
  {"roof_turret", "ROOF TURRET", K::Turret, L::Turret, 3, 2, 4, 1000, 0, 30, 8, 22, 0, 0},
  // Level 2. Crawler: range is how far its spark flies. Sniper: range is
  // how many rows below it it wakes; tell is the tracking time (it then
  // holds still 8 frames).
  {"glass_crawler", "GLASS CRAWLER", K::Crawler, L::Styled, 3, 2, 2, 300, 2, 30, 10, 6, 0, 0, 0},
  {"squeegee_drone", "SQUEEGEE DRONE", K::Rider, L::Styled, 4, 4, 5, 400, 1, 90, 15, 0, kEnemyHarmless, 0, 0},
  {"penthouse_sniper", "PENTHOUSE SNIPER", K::Sniper, L::Styled, 3, 4, 4, 1500, 0, 45, 15, 14, 0, 0},
};

constexpr int kEnemyCount = int(sizeof(kEnemies) / sizeof(kEnemies[0]));

} // namespace

int enemyIndex(const std::string& key)
{
  for (int i = 0; i < kEnemyCount; ++i)
    if (key == kEnemies[i].key)
      return i;
  return -1;
}

const EnemyDef& enemyDef(int index) { return kEnemies[index]; }

int enemyDefCount() { return kEnemyCount; }

} // namespace gr
