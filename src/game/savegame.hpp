#pragma once

#include <optional>
#include <string>
#include <vector>

namespace gr
{

constexpr int kSaveSlots = 5;

// Everything needed to pick a level back up: the player's stats and
// inventory, where they stood, and which enemies, boxes, items and
// checkpoints are left. Projectiles and effects are not saved. Stored as a
// small line-based text file, one per slot.
struct SaveGame
{
  struct EnemyState
  {
    bool alive = true;
    int hp = 0, x = 0, y = 0, dir = -1, timer = 0;
    bool active = false;
    // Enemies a spawner brought in (after the level's own): their kind and
    // the platform they came onto. -1 for the level's enemies.
    int def = -1, platform = -1;
    int attach = 0; // a Grid Leech's cable, a clinger's wall
  };
  struct BreakerState
  {
    bool on = false, leechKilled = false;
    int thrownAt = -100000, leechIn = -1;
  };
  struct PlatformState
  {
    int x = 0, y = 0, balance = 0, slackLeft = 0, idle = 0, moveTick = 0, target = 1, step = 1;
    bool braked = false;
  };
  struct ItemState
  {
    int kind = 0, variant = 0, x = 0, y = 0;
    bool floating = false;
  };

  std::string levelName;
  // Campaign: which level file, and its number (0 for a stand-alone level).
  std::string levelFile;
  int levelNumber = 0;
  std::string savedAt; // local time, "YYYY-MM-DD HH:MM"
  int character = 0;
  int theme = 0;

  // Player: placed on the last solid ground they stood on.
  int x = 0, y = 0, facing = 1;
  int hp = 0, weapon = 0, ammo = 0, rapidFire = 0, turbo = 0, virus = 0;
  bool hasKey = false;
  int respawnX = 0, respawnY = 0;

  // Progress (the totals come from the level itself).
  int score = 0, gems = 0, kills = 0, merch = 0, weaponsCollected = 0, deaths = 0, frames = 0;
  bool tookDamage = false;
  std::string letters;
  bool forceFieldsOn = true;

  // Campaign extras: the prototype in hand, the level's secrets so far and
  // which props (gem caches, the bonus entrance, eggs) were used.
  int proto = -1;
  bool protoFound = false, duck = false, camera = false, bonusStar = false;
  int protoKills = 0;
  std::vector<bool> props;

  std::vector<EnemyState> enemies;
  std::vector<bool> boxes; // alive flags, in level order
  std::vector<ItemState> items;
  std::vector<bool> checkpoints;
  std::vector<PlatformState> platforms;
  std::vector<bool> hatches;
  std::vector<int> breakables; // hp left; 0 = broken
  // Power cuts (level 4): breakers, shutter progress, the all-lit moment.
  std::vector<BreakerState> breakers;
  std::vector<int> doors;
  int allLitAt = -1;
  // The tide (level 5): each zone's flood clock, the valves' padlocks, the
  // rat pipes (armed, idle) and which enemies are still in a lasting bubble.
  std::vector<int> floods;
  std::vector<int> valves;
  std::vector<int> ratPipes;
  std::vector<int> bubbled;
  // The maglev (level 6): braking, the tunnel, the passing train, and
  // which gantries have gone by (see World::snapshot).
  std::vector<int> train;
  // Chopper Down (level 7): the hunter's demo and cooldown, Black Halo's
  // phase, part HP and exit, the Pilot Seat's counters, then each latch
  // and each rappel zone's next drop (see World::snapshot).
  std::vector<int> chopper;
};

// $XDG_DATA_HOME/gunrunners/saves, or ~/.local/share/gunrunners/saves.
std::string defaultSaveDir();
// slot is 0-based; files are named slot1.sav .. slot5.sav.
std::string slotPath(const std::string& dir, int slot);

// Writes atomically (temp file + rename), creating the directory if needed.
bool writeSave(const SaveGame& save, const std::string& path, std::string* error = nullptr);
std::optional<SaveGame> readSave(const std::string& path);

} // namespace gr
