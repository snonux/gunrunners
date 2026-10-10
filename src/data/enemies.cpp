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
  // Level 3. Bouncer: range is how far either side of his post he guards.
  // Raver: range is how far he throws.
  {"bouncer", "BOUNCER", K::Bouncer, L::Styled, 4, 6, 8, 800, 2, 30, 10, 6, 0, 0, 2},
  {"disco_drone", "DISCO DRONE", K::Disco, L::Styled, 3, 3, 4, 600, 0, 30, 8, 0, 0, 0},
  {"glow_raver", "GLOW RAVER", K::Raver, L::Styled, 3, 4, 2, 350, 0, 30, 15, 20, 0, 0},
  {"cardboard_bouncer", "CARDBOARD BOUNCER", K::Stepper, L::Styled, 4, 6, 2, 300, 0, 0, 0, 0, 0, 0},
  // Level 4. Stalker: range is the lunge in cells. Looter: range is how far
  // (cells) it spots a pick-up.
  {"night_stalker", "NIGHT STALKER", K::Stalker, L::Styled, 3, 5, 4, 700, 1, 30, 8, 4, 0, 0},
  {"looter", "LOOTER", K::Looter, L::Styled, 3, 4, 2, 400, 1, 0, 0, 24, kEnemyHarmless, 0},
  {"grid_leech", "GRID LEECH", K::Leech, L::Styled, 2, 2, 3, 500, 2, 0, 8, 0, kEnemyCarrier, 0},
  // Level 5. Gator: range is how close (cells) a runner must be for a
  // lunge. Rats come out of rat pipes and are not part of the tally.
  {"sludge_gator", "SLUDGE GATOR", K::Gator, L::Styled, 4, 2, 3, 600, 2, 45, 15, 6, 0, 0},
  {"pipe_rat", "PIPE RAT", K::Walker, L::Styled, 2, 1, 1, 100, 1, 0, 0, 0, kEnemyNoTally, 0},
  {"valve_keeper", "VALVE KEEPER", K::Keeper, L::Styled, 3, 4, 10, 900, 2, 0, 30, 0, 0, 0, 3},
  // Level 6: Maglev Express.
  {"track_hopper", "TRACK HOPPER", K::Hopper, L::Styled, 3, 4, 4, 500, 2, 45, 10, 12, 0, 0},
  {"rail_drone", "RAIL DRONE", K::RailDrone, L::Styled, 4, 2, 3, 600, 1, 45, 10, 0, 0, 0},
  {"decoupler", "DECOUPLER", K::Decoupler, L::Styled, 3, 4, 6, 800, 0, 75, 15, 24, 0, 0},
  // Level 7: Chopper Down. Troopers: range is how far they see you (cells).
  {"rappel_trooper", "RAPPEL TROOPER", K::Trooper, L::Styled, 3, 5, 3, 500, 2, 30, 10, 20, 0, 0, 2},
  {"hover_biker", "HOVER BIKER", K::Biker, L::Styled, 5, 3, 5, 700, 1, 45, 10, 0, 0, 0, 3},
  {"shield_trooper", "SHIELD TROOPER", K::Shield, L::Styled, 3, 5, 6, 900, 2, 40, 10, 20, 0, 0, 2},
  // Pilot Seat's cardboard targets: they only run along their roofs.
  {"cardboard_runner", "CARDBOARD RUNNER", K::Walker, L::Styled, 3, 5, 2, 500, 1, 0, 0, 0, kEnemyHarmless | kEnemyNoTally, 0},
  {"cardboard_truck", "CARDBOARD TRUCK", K::Walker, L::Styled, 8, 4, 6, 1500, 1, 0, 0, 0, kEnemyHarmless | kEnemyNoTally, 0},
  // Level 8: Canopy Road. Howler: range is how far (cells) it throws.
  // Viper: range is how far to the side (cells) it notices a runner below.
  {"howler", "HOWLER", K::Howler, L::Styled, 3, 3, 2, 300, 0, 40, 12, 24, 0, 0},
  {"viper", "CANOPY VIPER", K::Viper, L::Styled, 2, 2, 2, 450, 0, 45, 12, 4, 0, 0},
  {"cutter", "BRIDGE CUTTER", K::Cutter, L::Styled, 3, 5, 3, 600, 1, 30, 10, 20, 0, 0, 2},
  // Level 9: Hall of Traps. Guardian: range is how far (cells) it notices
  // you on its floor. Dart Face: cooldown is its rhythm. Scarabs: hp is the
  // beetle count (set from count=), score is per beetle.
  {"guardian", "STONE GUARDIAN", K::Guardian, L::Styled, 4, 6, 6, 1200, 4, 40, 14, 24, 0, 0, 3},
  {"dartface", "DART FACE", K::DartFace, L::Styled, 2, 2, 4, 500, 0, 30, 10, 0, 0, 0},
  {"scarabs", "SCARAB TIDE", K::Scarabs, L::Styled, 8, 1, 8, 50, 2, 15, 0, 0, 0, 0},
  {"treasure_hunter", "TREASURE HUNTER", K::Hunter, L::Styled, 3, 5, 2, 200, 2, 0, 0, 0, kEnemyHarmless, 0},
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
