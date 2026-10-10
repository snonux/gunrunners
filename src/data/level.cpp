#include "data/level.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
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
  return at(tx, ty) == Tile::Solid || at(tx, ty) == Tile::Grate;
}

namespace
{

std::string trim(const std::string& s)
{
  const auto b = s.find_first_not_of(" \t");
  if (b == std::string::npos)
    return {};
  const auto e = s.find_last_not_of(" \t");
  return s.substr(b, e - b + 1);
}

// Drops a `;` comment that is not inside quotes. A comment starts a line or
// follows a space, so `path=1,2;3,4` keeps its point separators.
std::string stripComment(const std::string& s)
{
  bool quoted = false;
  for (std::size_t i = 0; i < s.size(); ++i)
  {
    if (s[i] == '"')
      quoted = !quoted;
    else if (s[i] == ';' && !quoted && (i == 0 || s[i - 1] == ' ' || s[i - 1] == '\t'))
      return s.substr(0, i);
  }
  return s;
}

bool isNumber(const std::string& s)
{
  if (s.empty())
    return false;
  std::size_t i = (s[0] == '-') ? 1 : 0;
  if (i == s.size())
    return false;
  for (; i < s.size(); ++i)
    if (!std::isdigit(static_cast<unsigned char>(s[i])))
      return false;
  return true;
}

// Splits on blanks, keeping "quoted strings" together (quotes removed).
std::vector<std::string> tokenize(const std::string& s)
{
  std::vector<std::string> out;
  std::string cur;
  bool quoted = false, any = false;
  for (char c : s)
  {
    if (c == '"')
    {
      quoted = !quoted;
      any = true;
      continue;
    }
    if (!quoted && (c == ' ' || c == '\t'))
    {
      if (any)
        out.push_back(cur);
      cur.clear();
      any = false;
      continue;
    }
    cur += c;
    any = true;
  }
  if (any)
    out.push_back(cur);
  return out;
}

EntityDef parseEntity(const std::string& line, int lineNo)
{
  EntityDef e;
  e.line = lineNo;
  const auto tokens = tokenize(trim(line.substr(1)));
  if (tokens.empty())
    throw std::runtime_error("empty @ line " + std::to_string(lineNo));
  e.kind = tokens[0];
  int positional = 0;
  for (std::size_t i = 1; i < tokens.size(); ++i)
  {
    const auto& t = tokens[i];
    const auto eq = t.find('=');
    if (eq != std::string::npos)
    {
      e.keys[t.substr(0, eq)] = t.substr(eq + 1);
      continue;
    }
    if (isNumber(t))
    {
      (positional++ == 0 ? e.x : e.y) = std::stoi(t);
      e.hasPos = true;
    }
    else if (e.id.empty())
    {
      e.id = t;
    }
  }
  if (e.has("x") && e.has("y"))
  {
    e.x = e.num("x");
    e.y = e.num("y");
    e.hasPos = true;
  }
  if (e.id.empty() && e.has("id"))
    e.id = e.keys["id"];
  return e;
}

} // namespace

std::string EntityDef::str(const std::string& k, const std::string& def) const
{
  const auto it = keys.find(k);
  return it == keys.end() ? def : it->second;
}

int EntityDef::num(const std::string& k, int def) const
{
  const auto it = keys.find(k);
  if (it == keys.end() || it->second.empty())
    return def;
  try
  {
    return int(std::lround(std::stod(it->second)));
  }
  catch (...)
  {
    return def;
  }
}

float EntityDef::real(const std::string& k, float def) const
{
  const auto it = keys.find(k);
  if (it == keys.end() || it->second.empty())
    return def;
  const auto& v = it->second;
  try
  {
    const auto slash = v.find('/');
    if (slash != std::string::npos)
      return float(std::stod(v.substr(0, slash)) / std::stod(v.substr(slash + 1)));
    return float(std::stod(v));
  }
  catch (...)
  {
    return def;
  }
}

std::vector<int> EntityDef::list(const std::string& k) const
{
  std::vector<int> out;
  std::stringstream ss(str(k));
  std::string item;
  while (std::getline(ss, item, ','))
    if (isNumber(trim(item)))
      out.push_back(std::stoi(trim(item)));
  return out;
}

bool EntityDef::rect(const std::string& k, int& x0, int& y0, int& x1, int& y1) const
{
  const auto v = list(k);
  if (v.size() != 4)
    return false;
  x0 = std::min(v[0], v[2]);
  y0 = std::min(v[1], v[3]);
  x1 = std::max(v[0], v[2]);
  y1 = std::max(v[1], v[3]);
  return true;
}

std::vector<std::pair<int, int>> EntityDef::path(const std::string& k) const
{
  std::vector<std::pair<int, int>> out;
  std::stringstream ss(str(k));
  std::string point;
  while (std::getline(ss, point, ';'))
  {
    const auto comma = point.find(',');
    if (comma == std::string::npos)
      continue;
    const auto a = trim(point.substr(0, comma)), b = trim(point.substr(comma + 1));
    if (isNumber(a) && isNumber(b))
      out.emplace_back(std::stoi(a), std::stoi(b));
  }
  return out;
}

std::string Level::headerStr(const std::string& k, const std::string& def) const
{
  const auto it = header.find(k);
  return it == header.end() ? def : it->second;
}

Level Level::parse(const std::string& text)
{
  Level level;
  std::vector<std::string> rows;
  std::istringstream in(text);
  std::string line;
  // Files without sections (the PoC level) are all map after the header.
  enum class Section
  {
    Header,
    Map,
    Entities,
  } section = Section::Header;
  bool sawSections = false;
  int lineNo = 0;
  while (std::getline(in, line))
  {
    ++lineNo;
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.rfind(";", 0) == 0)
      continue;
    if (line == "[map]")
    {
      section = Section::Map;
      sawSections = true;
      continue;
    }
    if (line == "[entities]")
    {
      section = Section::Entities;
      sawSections = true;
      continue;
    }
    if (section == Section::Entities)
    {
      const auto t = trim(stripComment(line));
      if (!t.empty() && t[0] == '@')
        level.entities.push_back(parseEntity(t, lineNo));
      continue;
    }
    if (section == Section::Header)
    {
      const auto eq = line.find('=');
      const bool key = eq != std::string::npos && eq > 0 &&
        std::all_of(line.begin(), line.begin() + long(eq), [](char c) { return std::islower(static_cast<unsigned char>(c)) || c == '_'; });
      if (key)
      {
        level.header[line.substr(0, eq)] = trim(stripComment(line.substr(eq + 1)));
        continue;
      }
      if (line.empty())
        continue;
      section = Section::Map; // legacy file: the map starts right away
    }
    rows.push_back(line);
  }
  (void)sawSections;
  while (!rows.empty() && rows.back().empty())
    rows.pop_back();
  if (rows.empty())
    throw std::runtime_error("level has no rows");

  auto hnum = [&](const char* k, int def) {
    const auto v = level.headerStr(k);
    return isNumber(v) ? std::stoi(v) : def;
  };
  level.name = level.headerStr("name");
  level.episode = hnum("episode", 0);
  level.themeKey = level.headerStr("theme");
  level.music = level.headerStr("music");
  level.weapon = level.headerStr("weapon");
  level.par = hnum("par", 0);
  level.respawnAmmo = hnum("respawn_ammo", 0);
  level.rules = level.headerStr("rules");
  level.timer = hnum("timer", 0);
  level.goal = level.headerStr("goal");
  {
    std::stringstream ss(level.headerStr("flags"));
    std::string f;
    while (std::getline(ss, f, ','))
      if (!trim(f).empty())
        level.flags.insert(trim(f));
  }

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
        case 'v':
          level.set(x, y, Tile::SpikesDown);
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
        case '%':
          level.set(x, y, Tile::Grate);
          break;
        case 'P':
          level.startTx = x;
          level.startTy = y;
          break;
        case 'X':
          level.exitTx = x;
          level.exitTy = y;
          level.hasExit = true;
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
        case 'W': // prototype weapon box
        case 'B': // bonus level entrance
        case 'Q': // rubber duck
        case 'C': // candid camera
        case '$': // hidden gem cache
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
