#pragma once

#include "render/color.hpp"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace gr
{

// Duke Nukem II's four guns: the default blaster has unlimited ammo, the
// others hold 32 shots (64 for the flamethrower) and fall back to the
// blaster when empty.
enum class Weapon
{
  Normal,
  Laser,  // pierces enemies
  Rocket, // heavy damage
  Flame,  // pierces, and down + fire works as a jetpack
  Proto,  // the level's prototype (Player::proto)
};

constexpr int kWeaponCount = 4;
const char* weaponName(Weapon w);
int maxAmmo(Weapon w);

// What a runner looks like, as picks from the lists below (the runner
// editor walks them). Hair and face lists depend on the body: robots get
// head kits and optics instead of hair styles and faces.
struct RunnerParts
{
  int body = 0;     // 0 human, 1 robot
  int build = 1;    // 0 slim, 1 regular, 2 broad
  int hair = 0;     // hairStyleName(body, i)
  int face = 0;     // faceName(body, i)
  int outfit = 0;   // outfitName(i)
  int sleeves = 0;  // 0 sleeves, 1 bare arms
  int skin = 0;     // skinTone(body, i): skin, or the robot's plating
  int hairColor = 0, top = 0, pants = 0, boots = 0, glow = 0; // paletteColor(i)

  bool operator==(const RunnerParts& o) const;
};

int hairStyleCount(int body);
const char* hairStyleName(int body, int index);
int faceCount(int body);
const char* faceName(int body, int index);
int outfitCount();
const char* outfitName(int index);
const char* buildName(int index);
int skinToneCount(int body);
// The light and the shaded tone.
std::pair<Color, Color> skinTone(int body, int index);
int paletteSize();
Color paletteColor(int index);

// The selectable Gunrunners. They share Duke's movement rules (one cell per
// frame, same controls) and differ in health, jump arc and the gun they
// start with. The first kDefaultRunners are built in; the rest are made in
// the runner editor and kept in runners.txt next to the savegames.
struct CharacterDef
{
  std::string id; // "dash", "rocco", ...; custom runners get a random one
  std::string name;
  std::string role;
  int maxHp = 9;
  // Cells moved up per 15 Hz frame of a jump (RigelEngine's JUMP_ARC).
  std::array<int, 8> jumpArc{};
  Weapon startWeapon = Weapon::Normal;
  int startAmmo = 0;
  int startRapidFire = 0; // logic frames of rapid fire at the start of a level
  int healthPips = 3, jumpPips = 3, powerPips = 3;
  // Dash, Rocco and Nova keep their hand-tuned art (0-2); everyone else is
  // drawn from parts (-1).
  int art = -1;
  RunnerParts parts;
  bool custom = false;
  // A custom runner brought back by a savegame after it was deleted: it is
  // playable but not written back to runners.txt.
  bool transient = false;
};

constexpr int kDefaultRunners = 6;
constexpr int kMaxCustomRunners = 24;
constexpr int kMaxRunnerName = 10;
// Stat points a custom runner spreads over HEALTH, JUMP and POWER.
constexpr int kRunnerBudget = 10;
constexpr int kMinPips[3] = {1, 2, 1};
constexpr int kMaxPips = 5;

// Built-in runners followed by the custom ones. characterByIndex wraps.
int characterCount();
const CharacterDef& characterByIndex(int index);
int runnerIndexById(const std::string& id); // -1 if there is none

// A custom runner from parts, name, gun and pips: fills in health, jump
// arc, ammo and role. Pips are clamped to what the budget allows.
CharacterDef makeCustomRunner(const std::string& id, const std::string& name, const RunnerParts& parts, Weapon gun,
  int healthPips, int jumpPips, int powerPips);
// Points a custom runner may still spend. POWER is starting ammo for the
// laser, rockets and flamer, and starting rapid fire for the blaster.
int runnerPointsLeft(int healthPips, int jumpPips, int powerPips);
std::string newRunnerId();
// A built-in runner's look and stats as a starting point for a new one.
CharacterDef remixRunner(const CharacterDef& base);

// Custom runners. put adds a new one or replaces the one with the same id
// and returns its index; the roster changes, so indexes of later runners
// can move.
int putCustomRunner(const CharacterDef& def);
void removeCustomRunner(const std::string& id);
void clearCustomRunners();
// dir/runners.txt (the savegame directory).
void loadCustomRunners(const std::string& dir);
bool saveCustomRunners(const std::string& dir);
// One line, as runners.txt and savegames store a custom runner.
std::string encodeRunner(const CharacterDef& def);
bool decodeRunner(const std::string& line, CharacterDef& out);
// The roster index for a savegame's runner: built-in runners by index,
// custom ones by their stored copy (brought back as transient if deleted).
int resolveSavedRunner(int index, const std::string& encoded);
// The same without touching the roster (for listing savegames).
CharacterDef savedRunner(int index, const std::string& encoded);

} // namespace gr
