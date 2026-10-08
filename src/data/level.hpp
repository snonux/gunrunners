#pragma once

#include <string>
#include <vector>

namespace gr
{

constexpr int kTileSize = 16;

enum class Tile : unsigned char
{
  Empty,
  Solid,
  Platform, // one-way: can jump up through it, land on top
  Spikes,
  Crate,    // solid, breakable by shooting
};

struct Spawn
{
  char kind; // see level file legend
  int tx;
  int ty;
};

// Tile map loaded from a plain-text level file (see levels/README.md for the
// legend). Inspired by RigelEngine's data::map::Map, but deliberately simple
// so levels can be edited in any text editor.
struct Level
{
  std::string name;
  int width = 0;
  int height = 0;
  std::vector<Tile> tiles;
  std::vector<int> crateHp;
  std::vector<Spawn> spawns;
  std::vector<std::pair<int, int>> decorations;
  int startTx = 1, startTy = 1;
  int exitTx = 2, exitTy = 1;

  Tile at(int tx, int ty) const;
  void set(int tx, int ty, Tile t);
  bool isSolid(int tx, int ty) const;
  int widthPx() const { return width * kTileSize; }
  int heightPx() const { return height * kTileSize; }

  static Level parse(const std::string& text);
  static Level loadFile(const std::string& path);
};

} // namespace gr
