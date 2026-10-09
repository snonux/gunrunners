#pragma once

#include "render/color.hpp"

#include <string>

namespace gr
{

// What an enemy does each frame. Most of the campaign's enemies are one of
// these behaviours with their own numbers (SPEC.md section 5); the ones that
// need more get their own kind.
enum class EnemyKind
{
  Walker, // patrol: turns at walls and ledges
  Flyer,  // hover over the player's head and dive
  Turret, // static, aimed shots in 8 directions
  Camera, // the candid camera: a harmless shootable prop
  Crawler, // cling: creeps along walls and ceilings, spits sparks
  Rider,   // ride: sweeps along a rail (Squeegee Drone)
  Sniper,  // static: tracks you with a laser line, then fires along it
  Bouncer, // patrols his post; shots from the front only stagger him
  Disco,   // static: eight light shards on beat 1 of every bar
  Raver,   // static: lobs glowsticks that leave a puddle
  Stepper, // moves only on the beat (Step on the Beat's cardboard Bouncers)
  Stalker, // chases in the dark, freezes in any light; shots pass through it in the dark
  Looter,  // grabs a loose pick-up and runs off to the stash
  Leech,   // crawls along a power cable to pull its breaker
  Gator,   // swims under the sludge, lunges up out of it
  Keeper,  // walks to its valve and floods the zone; then guards it
  Hopper,  // leaps car to car toward you; its landing sends a shockwave along the roof
  RailDrone, // hovers alongside the train and drops caltrops
  Decoupler, // climbs out of a coupling and unhooks it
};

enum EnemyFlag : unsigned
{
  kEnemyHarmless = 1u << 0,  // no contact damage
  kEnemyBeatDive = 1u << 1,  // flyer that dives only on beat 1, once per bar
  kEnemyNoTally = 1u << 2,   // not counted for "all enemies destroyed"
  kEnemyCarrier = 1u << 3,   // attacks infect with the Virus instead of hurting
  kEnemyRobot = 1u << 4,     // the Signal Jammer can turn it
};

// Which baked art an enemy uses.
enum class EnemyLook
{
  Walker,
  Flyer,
  Turret,
  Camera,
  Styled, // drawn by its own art routine (assets/enemy_art.cpp, keyed by `key`)
};

struct EnemyDef
{
  const char* key;
  const char* name;
  EnemyKind kind;
  EnemyLook look;
  int w, h; // cells
  int hp;
  int score;
  int stepEvery; // frames per cell moved (walkers 2 = 1/2 cell per frame)
  int cooldown;  // frames between attacks
  int tell;      // telegraph frames before an attack
  int range;     // turrets: horizontal reach in cells
  unsigned flags;
  Color tint; // 0 = the theme's enemy colours
  int weight = 1; // on weighted platforms (level 2's pulleys)
};

// Index into the enemy table, -1 if unknown.
int enemyIndex(const std::string& key);
const EnemyDef& enemyDef(int index);
int enemyDefCount();

} // namespace gr
