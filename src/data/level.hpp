#pragma once

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
};

struct Spawn
{
  char kind; // see levels/README.md
  int tx;
  int ty;
};

// Tile map loaded from a plain-text level file (see levels/README.md for the
// legend). Inspired by RigelEngine's data::map::Map, but deliberately simple.
struct Level
{
  std::string name;
  int width = 0;
  int height = 0;
  std::vector<Tile> tiles;
  std::vector<Spawn> spawns;
  std::vector<std::pair<int, int>> decorations;
  int startTx = 1, startTy = 1;
  int exitTx = 2, exitTy = 1;

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
