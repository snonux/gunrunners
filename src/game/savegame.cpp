#include "game/savegame.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace gr
{

namespace
{

constexpr const char* kMagic = "gunrunners-save";
constexpr int kVersion = 1;

std::string now()
{
  const std::time_t t = std::time(nullptr);
  std::tm tm{};
  localtime_r(&t, &tm);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
  return buf;
}

} // namespace

std::string defaultSaveDir()
{
  if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg)
    return std::string(xdg) + "/gunrunners/saves";
  if (const char* home = std::getenv("HOME"); home && *home)
    return std::string(home) + "/.local/share/gunrunners/saves";
  return "saves";
}

std::string slotPath(const std::string& dir, int slot)
{
  return dir + "/slot" + std::to_string(slot + 1) + ".sav";
}

std::string quickSavePath(const std::string& dir)
{
  return dir + "/quick.sav";
}

bool writeSave(const SaveGame& s, const std::string& path, std::string* error)
{
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path target(path);
  if (target.has_parent_path())
    fs::create_directories(target.parent_path(), ec);

  std::ostringstream o;
  o << kMagic << ' ' << kVersion << '\n';
  o << "level " << s.levelName << '\n';
  o << "saved " << (s.savedAt.empty() ? now() : s.savedAt) << '\n';
  o << "character " << s.character << '\n';
  o << "theme " << s.theme << '\n';
  o << "player " << s.x << ' ' << s.y << ' ' << s.facing << ' ' << s.hp << ' ' << s.weapon << ' ' << s.ammo << ' '
    << s.rapidFire << ' ' << int(s.hasKey) << '\n';
  o << "effects " << s.turbo << ' ' << s.virus << '\n';
  o << "respawn " << s.respawnX << ' ' << s.respawnY << '\n';
  o << "progress " << s.score << ' ' << s.gems << ' ' << s.kills << ' ' << s.merch << ' ' << s.weaponsCollected << ' '
    << s.deaths << ' ' << s.frames << ' ' << int(s.tookDamage) << '\n';
  o << "letters " << (s.letters.empty() ? "-" : s.letters) << '\n';
  o << "fields " << int(s.forceFieldsOn) << '\n';
  if (!s.levelFile.empty())
    o << "campaign " << s.levelNumber << ' ' << s.levelFile << '\n';
  o << "extras " << s.proto << ' ' << int(s.protoFound) << ' ' << int(s.duck) << ' ' << int(s.camera) << ' '
    << int(s.bonusStar) << ' ' << s.protoKills << '\n';
  if (s.god || s.cheated)
    o << "cheats " << int(s.god) << ' ' << int(s.cheated) << '\n';
  for (bool p : s.props)
    o << "prop " << int(p) << '\n';
  for (const auto& e : s.enemies)
    o << "enemy " << int(e.alive) << ' ' << e.hp << ' ' << e.x << ' ' << e.y << ' ' << e.dir << ' ' << e.timer << ' '
      << int(e.active) << ' ' << e.def << ' ' << e.platform << ' ' << e.attach << '\n';
  for (bool b : s.boxes)
    o << "box " << int(b) << '\n';
  for (const auto& it : s.items)
    o << "item " << it.kind << ' ' << it.variant << ' ' << it.x << ' ' << it.y << ' ' << int(it.floating) << '\n';
  for (bool c : s.checkpoints)
    o << "checkpoint " << int(c) << '\n';
  for (const auto& p : s.platforms)
    o << "platform " << p.x << ' ' << p.y << ' ' << p.balance << ' ' << p.slackLeft << ' ' << p.idle << ' '
      << p.moveTick << ' ' << p.target << ' ' << p.step << ' ' << int(p.braked) << '\n';
  for (bool h : s.hatches)
    o << "hatch " << int(h) << '\n';
  for (int b : s.breakables)
    o << "breakable " << b << '\n';
  for (const auto& b : s.breakers)
    o << "breaker " << int(b.on) << ' ' << b.thrownAt << ' ' << int(b.leechKilled) << ' ' << b.leechIn << '\n';
  for (int d : s.doors)
    o << "door " << d << '\n';
  if (s.allLitAt >= 0)
    o << "alllit " << s.allLitAt << '\n';
  auto list = [&](const char* key, const std::vector<int>& v) {
    if (v.empty())
      return;
    o << key << ' ' << v.size();
    for (int x : v)
      o << ' ' << x;
    o << '\n';
  };
  list("floods", s.floods);
  list("valves", s.valves);
  list("ratpipes", s.ratPipes);
  list("bubbled", s.bubbled);
  list("train", s.train);
  list("chopper", s.chopper);
  list("jungle", s.jungle);
  list("temple", s.temple);
  list("light", s.light);
  list("mine", s.mine);
  list("boulder", s.boulder);
  list("explored", s.explored);
  o << "end\n";

  const std::string tmp = path + ".tmp";
  {
    std::ofstream f(tmp, std::ios::trunc);
    if (!f || !(f << o.str()) || !f.flush())
    {
      if (error)
        *error = "cannot write " + tmp;
      return false;
    }
  }
  fs::rename(tmp, target, ec);
  if (ec)
  {
    if (error)
      *error = ec.message();
    fs::remove(tmp, ec);
    return false;
  }
  return true;
}

std::optional<SaveGame> readSave(const std::string& path)
{
  std::ifstream f(path);
  if (!f)
    return std::nullopt;
  std::string magic;
  int version = 0;
  if (!(f >> magic >> version) || magic != kMagic || version != kVersion)
    return std::nullopt;

  SaveGame s;
  bool complete = false;
  std::string line;
  std::getline(f, line);
  while (std::getline(f, line))
  {
    std::istringstream in(line);
    std::string key;
    in >> key;
    auto rest = [&]() {
      std::string r;
      std::getline(in >> std::ws, r);
      return r;
    };
    int a = 0, b = 0, c = 0, d = 0, e = 0, g = 0, h = 0, k = 0;
    if (key == "level")
      s.levelName = rest();
    else if (key == "saved")
      s.savedAt = rest();
    else if (key == "character")
      in >> s.character;
    else if (key == "theme")
      in >> s.theme;
    else if (key == "player" && in >> s.x >> s.y >> s.facing >> s.hp >> s.weapon >> s.ammo >> s.rapidFire >> a)
      s.hasKey = a != 0;
    else if (key == "effects")
      in >> s.turbo >> s.virus;
    else if (key == "respawn")
      in >> s.respawnX >> s.respawnY;
    else if (key == "progress" &&
             in >> s.score >> s.gems >> s.kills >> s.merch >> s.weaponsCollected >> s.deaths >> s.frames >> a)
      s.tookDamage = a != 0;
    else if (key == "letters")
    {
      s.letters = rest();
      if (s.letters == "-")
        s.letters.clear();
    }
    else if (key == "fields" && in >> a)
      s.forceFieldsOn = a != 0;
    else if (key == "campaign" && in >> s.levelNumber)
      s.levelFile = rest();
    else if (key == "extras" && in >> s.proto >> a >> b >> c >> d >> s.protoKills)
    {
      s.protoFound = a != 0;
      s.duck = b != 0;
      s.camera = c != 0;
      s.bonusStar = d != 0;
    }
    else if (key == "cheats" && in >> a >> b)
    {
      s.god = a != 0;
      s.cheated = b != 0;
    }
    else if (key == "prop" && in >> a)
      s.props.push_back(a != 0);
    else if (key == "enemy" && in >> a >> b >> c >> d >> e >> g >> h)
    {
      SaveGame::EnemyState es{a != 0, b, c, d, e, g, h != 0};
      if (in >> k)
      {
        es.def = k;
        in >> es.platform;
        in >> es.attach;
      }
      s.enemies.push_back(es);
    }
    else if (key == "platform")
    {
      SaveGame::PlatformState p;
      if (in >> p.x >> p.y >> p.balance >> p.slackLeft >> p.idle >> p.moveTick >> p.target >> p.step >> a)
      {
        p.braked = a != 0;
        s.platforms.push_back(p);
      }
    }
    else if (key == "hatch" && in >> a)
      s.hatches.push_back(a != 0);
    else if (key == "breakable" && in >> a)
      s.breakables.push_back(a);
    else if (key == "breaker" && in >> a >> b >> c >> d)
      s.breakers.push_back({a != 0, c != 0, b, d});
    else if (key == "door" && in >> a)
      s.doors.push_back(a);
    else if (key == "alllit")
      in >> s.allLitAt;
    else if (key == "floods" || key == "valves" || key == "ratpipes" || key == "bubbled" || key == "train" ||
             key == "chopper" || key == "jungle" || key == "temple" || key == "light" ||
             key == "mine" || key == "boulder" || key == "explored")
    {
      auto& v = key == "floods" ? s.floods
              : key == "valves" ? s.valves
              : key == "ratpipes" ? s.ratPipes
              : key == "train" ? s.train
              : key == "chopper" ? s.chopper
              : key == "jungle" ? s.jungle
              : key == "temple" ? s.temple
              : key == "light" ? s.light
              : key == "mine" ? s.mine
              : key == "boulder" ? s.boulder
              : key == "explored" ? s.explored
                                : s.bubbled;
      int n = 0;
      in >> n;
      for (int i = 0; i < n && i < 100000 && in >> a; ++i)
        v.push_back(a);
    }
    else if (key == "box" && in >> a)
      s.boxes.push_back(a != 0);
    else if (key == "item" && in >> a >> b >> c >> d >> k)
      s.items.push_back({a, b, c, d, k != 0});
    else if (key == "checkpoint" && in >> a)
      s.checkpoints.push_back(a != 0);
    else if (key == "end")
      complete = true;
  }
  // A file cut short (full disk, crash mid-write) is treated as no save.
  if (!complete || s.hp <= 0)
    return std::nullopt;
  return s;
}

} // namespace gr
