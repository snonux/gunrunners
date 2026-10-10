#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace gr
{

// Level files are authored in 16x16 blocks so they stay readable in a text
// editor. The game logic works on the 8x8 "cell" grid Duke Nukem II uses:
// every block is 2x2 cells.
constexpr int kTileSize = 16;
constexpr int kCellSize = 8;
constexpr int kCellsPerTile = kTileSize / kCellSize;

enum class Tile : unsigned char
{
  Empty,
  Solid,
  Platform,   // one-way: jump up through it, land on top
  Spikes,     // lower half solid, upper half hurts
  Ladder,     // climb with up/down, jump off at the top
  Pipe,       // hang from it, move hand over hand
  ForceField, // solid until the access card is used on it
  Grate,      // solid to walk on; sludge rises through it (level 5)
  SpikesDown, // hanging from a ceiling: upper half solid, lower half hurts (level 19)
};

struct Spawn
{
  char kind; // see levels/README.md
  int tx;
  int ty;
};

// One `@ kind ...` line from a level's [entities] section: everything that
// needs parameters (layers, level-specific enemies, zones, decorations).
struct EntityDef
{
  std::string kind;
  std::string id; // first bare word after the kind, or id=
  int x = 0, y = 0;
  bool hasPos = false;
  std::map<std::string, std::string> keys;
  int line = 0;

  bool has(const std::string& k) const { return keys.count(k) != 0; }
  std::string str(const std::string& k, const std::string& def = {}) const;
  int num(const std::string& k, int def = 0) const;
  // "1/2" style speeds come back as a fraction num/den.
  float real(const std::string& k, float def = 0.0f) const;
  std::vector<int> list(const std::string& k) const;
  // rect=x0,y0,x1,y1 (inclusive blocks); false if missing.
  bool rect(const std::string& k, int& x0, int& y0, int& x1, int& y1) const;
  // path=x0,y0;x1,y1;...
  std::vector<std::pair<int, int>> path(const std::string& k) const;
};

// Tile map loaded from a plain-text level file (see levels/README.md for the
// legend). Inspired by RigelEngine's data::map::Map, but deliberately simple.
struct Level
{
  std::string name;
  // Header keys (levels/README.md). Everything is also kept in `header`.
  std::map<std::string, std::string> header;
  int episode = 0;
  std::string themeKey;
  std::string music;
  std::string weapon; // prototype in the W boxes
  int par = 0;        // seconds
  std::set<std::string> flags;
  int respawnAmmo = 0;
  // Bonus levels.
  std::string rules;
  int timer = 0; // seconds
  std::string goal;
  std::vector<EntityDef> entities;

  bool flag(const std::string& f) const { return flags.count(f) != 0; }
  std::string headerStr(const std::string& k, const std::string& def = {}) const;

  int width = 0;
  int height = 0;
  std::vector<Tile> tiles;
  std::vector<Spawn> spawns;
  std::vector<std::pair<int, int>> decorations;
  int startTx = 1, startTy = 1;
  int exitTx = 2, exitTy = 1;
  bool hasExit = false; // an X in the map (bonus levels end otherwise)

  Tile at(int tx, int ty) const;
  void set(int tx, int ty, Tile t);
  bool isSolid(int tx, int ty) const;
  int widthPx() const { return width * kTileSize; }
  int heightPx() const { return height * kTileSize; }
  int widthCells() const { return width * kCellsPerTile; }
  int heightCells() const { return height * kCellsPerTile; }

  static Level parse(const std::string& text);
  static Level loadFile(const std::string& path);
};

} // namespace gr
