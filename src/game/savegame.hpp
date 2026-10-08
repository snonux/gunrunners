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
  };
  struct ItemState
  {
    int kind = 0, variant = 0, x = 0, y = 0;
    bool floating = false;
  };

  std::string levelName;
  std::string savedAt; // local time, "YYYY-MM-DD HH:MM"
  int character = 0;
  int theme = 0;

  // Player: placed on the last solid ground they stood on.
  int x = 0, y = 0, facing = 1;
  int hp = 0, weapon = 0, ammo = 0, rapidFire = 0;
  bool hasKey = false;
  int respawnX = 0, respawnY = 0;

  // Progress (the totals come from the level itself).
  int score = 0, gems = 0, kills = 0, merch = 0, weaponsCollected = 0, deaths = 0, frames = 0;
  bool tookDamage = false;
  std::string letters;
  bool forceFieldsOn = true;

  std::vector<EnemyState> enemies;
  std::vector<bool> boxes; // alive flags, in level order
  std::vector<ItemState> items;
  std::vector<bool> checkpoints;
};

// $XDG_DATA_HOME/gunrunners/saves, or ~/.local/share/gunrunners/saves.
std::string defaultSaveDir();
// slot is 0-based; files are named slot1.sav .. slot5.sav.
std::string slotPath(const std::string& dir, int slot);

// Writes atomically (temp file + rename), creating the directory if needed.
bool writeSave(const SaveGame& save, const std::string& path, std::string* error = nullptr);
std::optional<SaveGame> readSave(const std::string& path);

} // namespace gr
