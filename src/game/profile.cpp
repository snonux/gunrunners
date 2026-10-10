#include "game/profile.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace gr
{

namespace
{

template <typename T>
void readSet(std::istringstream& in, std::set<T>& out)
{
  T v;
  while (in >> v)
    out.insert(v);
}

template <typename T>
void writeSet(std::ostream& o, const char* key, const std::set<T>& s)
{
  o << key;
  for (const auto& v : s)
    o << ' ' << v;
  o << '\n';
}

void makeDirs(const std::string& dir)
{
  std::string cur;
  for (std::size_t i = 0; i < dir.size(); ++i)
  {
    cur += dir[i];
    if (dir[i] == '/' && i > 0)
      ::mkdir(cur.c_str(), 0755);
  }
  ::mkdir(dir.c_str(), 0755);
}

} // namespace

Profile Profile::load(const std::string& dir)
{
  Profile p;
  std::ifstream f(dir + "/profile.txt");
  std::string line;
  while (std::getline(f, line))
  {
    std::istringstream in(line);
    std::string key;
    in >> key;
    if (key == "protos")
      readSet(in, p.protos);
    else if (key == "stars")
      readSet(in, p.stars);
    else if (key == "ducks")
      readSet(in, p.ducks);
    else if (key == "cameras")
      readSet(in, p.cameras);
    else if (key == "cutscenes")
      readSet(in, p.cutscenes);
    else if (key == "kills")
    {
      std::string k;
      int n = 0;
      while (in >> k >> n)
        p.protoKills[k] = n;
    }
    else if (key == "scores")
    {
      int n = 0, v = 0;
      while (in >> n >> v)
        p.scores[n] = v;
    }
    else if (key == "reached")
      in >> p.reached;
    else if (key == "space_reached")
      in >> p.spaceReached;
    else if (key == "duckmode")
      in >> p.duckMode;
    else if (key == "fullscreen")
      in >> p.fullscreen;
    else if (key == "touchsize")
      in >> p.touchSize;
    else if (key == "control")
    {
      std::string name;
      in >> name;
      auto& values = p.controls[name];
      int v = 0;
      while (in >> v)
        values.push_back(v);
    }
  }
  return p;
}

bool Profile::save(const std::string& dir) const
{
  makeDirs(dir);
  const std::string path = dir + "/profile.txt";
  {
    std::ofstream o(path + ".tmp");
    if (!o)
      return false;
    o << "gunrunners-profile 1\n";
    writeSet(o, "protos", protos);
    o << "kills";
    for (const auto& [k, n] : protoKills)
      o << ' ' << k << ' ' << n;
    o << '\n';
    writeSet(o, "stars", stars);
    writeSet(o, "ducks", ducks);
    writeSet(o, "cameras", cameras);
    writeSet(o, "cutscenes", cutscenes);
    o << "scores";
    for (const auto& [n, v] : scores)
      o << ' ' << n << ' ' << v;
    o << '\n';
    o << "reached " << reached << '\n';
    if (spaceReached > 0)
      o << "space_reached " << spaceReached << '\n';
    o << "duckmode " << duckMode << '\n';
    o << "fullscreen " << fullscreen << '\n';
    o << "touchsize " << touchSize << '\n';
    for (const auto& [name, values] : controls)
    {
      o << "control " << name;
      for (int v : values)
        o << ' ' << v;
      o << '\n';
    }
    if (!o)
      return false;
  }
  return std::rename((path + ".tmp").c_str(), path.c_str()) == 0;
}

} // namespace gr
