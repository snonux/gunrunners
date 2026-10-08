#include "data/level.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace gr
{

Tile Level::at(int tx, int ty) const
{
  // The left and right map borders act as walls, above and below is open.
  if (tx < 0 || tx >= width)
    return Tile::Solid;
  if (ty < 0 || ty >= height)
    return Tile::Empty;
  return tiles[std::size_t(ty * width + tx)];
}

void Level::set(int tx, int ty, Tile t)
{
  if (tx >= 0 && ty >= 0 && tx < width && ty < height)
    tiles[std::size_t(ty * width + tx)] = t;
}

bool Level::isSolid(int tx, int ty) const
{
  return at(tx, ty) == Tile::Solid;
}

Level Level::parse(const std::string& text)
{
  Level level;
  std::vector<std::string> rows;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line))
  {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.rfind("name=", 0) == 0)
    {
      level.name = line.substr(5);
      continue;
    }
    if (line.rfind(";", 0) == 0)
      continue;
    rows.push_back(line);
  }
  while (!rows.empty() && rows.back().empty())
    rows.pop_back();
  if (rows.empty())
    throw std::runtime_error("level has no rows");

  level.height = int(rows.size());
  for (const auto& r : rows)
    level.width = std::max(level.width, int(r.size()));
  level.tiles.assign(std::size_t(level.width * level.height), Tile::Empty);

  for (int y = 0; y < level.height; ++y)
  {
    const auto& r = rows[std::size_t(y)];
    for (int x = 0; x < int(r.size()); ++x)
    {
      const char c = r[std::size_t(x)];
      switch (c)
      {
        case '#':
          level.set(x, y, Tile::Solid);
          break;
        case '=':
          level.set(x, y, Tile::Platform);
          break;
        case '^':
          level.set(x, y, Tile::Spikes);
          break;
        case 'H':
          level.set(x, y, Tile::Ladder);
          break;
        case '-':
          level.set(x, y, Tile::Pipe);
          break;
        case 'D':
          level.set(x, y, Tile::ForceField);
          break;
        case 'P':
          level.startTx = x;
          level.startTy = y;
          break;
        case 'X':
          level.exitTx = x;
          level.exitTy = y;
          break;
        case '*':
          level.decorations.emplace_back(x, y);
          break;
        case 'g': // gem
        case 'h': // health box
        case 'm': // merchandise box
        case 'L': // laser box
        case 'R': // rocket box
        case 'F': // flamethrower box
        case 'r': // rapid fire box
        case 'k': // access card box
        case 'T': // turbo box
        case 'V': // virus
        case '1': // letters G, U, N
        case '2':
        case '3':
        case 'c': // checkpoint beacon
        case 'w': // walker
        case 'f': // flyer
        case 't': // turret
          level.spawns.push_back({c, x, y});
          break;
        default:
          break;
      }
    }
  }
  return level;
}

Level Level::loadFile(const std::string& path)
{
  std::ifstream f(path);
  if (!f)
    throw std::runtime_error("cannot open level file: " + path);
  std::stringstream ss;
  ss << f.rdbuf();
  return parse(ss.str());
}

} // namespace gr
