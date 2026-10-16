#include "game/savegame.hpp"

#include "base/atomic_file.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

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

// The run itself: which level and runner, the player, the score so far and
// the level-wide switches. The runner, campaign, hard and cheats lines are
// only written when set.
void writeRun(std::ostream& o, const SaveGame& s)
{
  o << kMagic << ' ' << kVersion << '\n';
  o << "level " << s.levelName << '\n';
  o << "saved " << (s.savedAt.empty() ? now() : s.savedAt) << '\n';
  o << "character " << s.character << '\n';
  if (!s.runner.empty())
    o << "runner " << s.runner << '\n';
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
  if (s.hard)
    o << "hard 1\n";
  o << "extras " << s.proto << ' ' << int(s.protoFound) << ' ' << int(s.duck) << ' ' << int(s.camera) << ' '
    << int(s.bonusStar) << ' ' << s.protoKills << '\n';
  if (s.god || s.cheated)
    o << "cheats " << int(s.god) << ' ' << int(s.cheated) << '\n';
}

// The level's things, one line each, in the level's own order.
void writeEntities(std::ostream& o, const SaveGame& s)
{
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
}

// The levels' own state as "key count values...", in this order; an empty
// list writes no line. readIntList reads them back through this same table,
// so a new key is added here only.
const std::pair<const char*, std::vector<int> SaveGame::*> kIntLists[] = {
  {"floods", &SaveGame::floods},
  {"valves", &SaveGame::valves},
  {"ratpipes", &SaveGame::ratPipes},
  {"bubbled", &SaveGame::bubbled},
  {"train", &SaveGame::train},
  {"chopper", &SaveGame::chopper},
  {"jungle", &SaveGame::jungle},
  {"temple", &SaveGame::temple},
  {"light", &SaveGame::light},
  {"mine", &SaveGame::mine},
  {"boulder", &SaveGame::boulder},
  {"sanctum", &SaveGame::sanctum},
  {"station", &SaveGame::station},
  {"green", &SaveGame::green},
  {"hull", &SaveGame::hull},
  {"grav", &SaveGame::grav},
  {"reactor", &SaveGame::reactor},
  {"zero", &SaveGame::zero},
  {"west", &SaveGame::west},
  {"manor", &SaveGame::manor},
  {"kitchen", &SaveGame::kitchen},
  {"garden", &SaveGame::garden},
  {"city", &SaveGame::city},
  {"noir", &SaveGame::noir},
  {"toon", &SaveGame::toon},
  {"signal", &SaveGame::signal},
  {"trench", &SaveGame::trench},
  {"storm", &SaveGame::storm},
  {"dish", &SaveGame::dish},
  {"magma", &SaveGame::magma},
  {"fleet", &SaveGame::fleet},
  {"tower", &SaveGame::tower},
  {"warehouse", &SaveGame::warehouse},
  {"wardrobe", &SaveGame::wardrobe},
  {"writers", &SaveGame::writers},
  {"gallery", &SaveGame::gallery},
  {"flytower", &SaveGame::flytower},
  {"sweeps", &SaveGame::sweeps},
  {"finale", &SaveGame::finale},
  {"explored", &SaveGame::explored},
  {"vehicles", &SaveGame::vehicles},
  {"swap", &SaveGame::swap},
  {"guns", &SaveGame::guns},
  {"bench", &SaveGame::bench},
};

void writeIntLists(std::ostream& o, const SaveGame& s)
{
  for (const auto& [key, member] : kIntLists)
  {
    const std::vector<int>& v = s.*member;
    if (v.empty())
      continue;
    o << key << ' ' << v.size();
    for (int x : v)
      o << ' ' << x;
    o << '\n';
  }
}

// Reading: each line is "key values...". A reader below takes the keys of
// one part of the save and returns false for any other key. A line whose
// values do not parse adds nothing, though run fields read before the bad
// value keep what was read (a failed number reads as 0).

// The rest of the line after the key, leading blanks dropped: the texts
// that may hold spaces (level name, time, runner, letters, level file).
std::string restOf(std::istream& in)
{
  std::string r;
  std::getline(in >> std::ws, r);
  return r;
}

// The run's names and switches.
bool readRunInfo(SaveGame& s, const std::string& key, std::istream& in)
{
  int a = 0, b = 0;
  if (key == "level")
    s.levelName = restOf(in);
  else if (key == "saved")
    s.savedAt = restOf(in);
  else if (key == "character")
    in >> s.character;
  else if (key == "runner")
    s.runner = restOf(in);
  else if (key == "theme")
    in >> s.theme;
  else if (key == "letters")
  {
    s.letters = restOf(in);
    if (s.letters == "-") // writeRun's mark for none
      s.letters.clear();
  }
  else if (key == "campaign")
  {
    if (in >> s.levelNumber)
      s.levelFile = restOf(in);
  }
  else if (key == "hard")
  {
    if (in >> a)
      s.hard = a != 0;
  }
  else if (key == "cheats")
  {
    if (in >> a >> b)
    {
      s.god = a != 0;
      s.cheated = b != 0;
    }
  }
  else
    return false;
  return true;
}

// The player and the run's counters. The numbers go straight into s, the
// 0/1 flags only once the whole line has parsed.
bool readRunState(SaveGame& s, const std::string& key, std::istream& in)
{
  int a = 0, b = 0, c = 0, d = 0;
  if (key == "player")
  {
    if (in >> s.x >> s.y >> s.facing >> s.hp >> s.weapon >> s.ammo >> s.rapidFire >> a)
      s.hasKey = a != 0;
  }
  else if (key == "effects")
    in >> s.turbo >> s.virus;
  else if (key == "respawn")
    in >> s.respawnX >> s.respawnY;
  else if (key == "progress")
  {
    if (in >> s.score >> s.gems >> s.kills >> s.merch >> s.weaponsCollected >> s.deaths >> s.frames >> a)
      s.tookDamage = a != 0;
  }
  else if (key == "fields")
  {
    if (in >> a)
      s.forceFieldsOn = a != 0;
  }
  else if (key == "extras")
  {
    if (in >> s.proto >> a >> b >> c >> d >> s.protoKills)
    {
      s.protoFound = a != 0;
      s.duck = b != 0;
      s.camera = c != 0;
      s.bonusStar = d != 0;
    }
  }
  else
    return false;
  return true;
}

// The things with a single value each, one line per thing: props, boxes,
// checkpoints and hatches as 0/1, breakables' hp and doors; and the all-lit
// moment.
bool readEntityValue(SaveGame& s, const std::string& key, std::istream& in)
{
  if (key == "alllit")
  {
    in >> s.allLitAt;
    return true;
  }
  std::vector<bool>* flags = key == "prop"         ? &s.props
                             : key == "box"        ? &s.boxes
                             : key == "checkpoint" ? &s.checkpoints
                             : key == "hatch"      ? &s.hatches
                                                   : nullptr;
  std::vector<int>* values = key == "breakable" ? &s.breakables : key == "door" ? &s.doors : nullptr;
  if (!flags && !values)
    return false;
  int a = 0;
  if (in >> a)
  {
    if (flags)
      flags->push_back(a != 0);
    else
      values->push_back(a);
  }
  return true;
}

// alive hp x y dir timer active, then (saves since spawners) the spawned
// kind, its platform and the attachment; without them the enemy is one of
// the level's own (def -1).
void readEnemy(SaveGame& s, std::istream& in)
{
  SaveGame::EnemyState e;
  int alive = 0, active = 0, def = 0;
  if (!(in >> alive >> e.hp >> e.x >> e.y >> e.dir >> e.timer >> active))
    return;
  e.alive = alive != 0;
  e.active = active != 0;
  if (in >> def)
  {
    e.def = def;
    in >> e.platform;
    in >> e.attach;
  }
  s.enemies.push_back(e);
}

void readPlatform(SaveGame& s, std::istream& in)
{
  SaveGame::PlatformState p;
  int braked = 0;
  if (!(in >> p.x >> p.y >> p.balance >> p.slackLeft >> p.idle >> p.moveTick >> p.target >> p.step >> braked))
    return;
  p.braked = braked != 0;
  s.platforms.push_back(p);
}

// The things with several values each: enemies, platforms, items, breakers.
bool readEntityState(SaveGame& s, const std::string& key, std::istream& in)
{
  int a = 0, b = 0, c = 0, d = 0, k = 0;
  if (key == "enemy")
    readEnemy(s, in);
  else if (key == "platform")
    readPlatform(s, in);
  else if (key == "item")
  {
    if (in >> a >> b >> c >> d >> k)
      s.items.push_back({a, b, c, d, k != 0});
  }
  else if (key == "breaker")
  {
    // Written as on, thrownAt, leechKilled, leechIn.
    if (in >> a >> b >> c >> d)
      s.breakers.push_back({a != 0, c != 0, b, d});
  }
  else
    return false;
  return true;
}

// A kIntLists line, "key count values...". The values are added to the list
// (a key read twice adds to it); a short line keeps the values it has, a
// count that does not parse or is below 1 adds none, and at most 100000 are
// read, so a corrupt count cannot run away.
bool readIntList(SaveGame& s, const std::string& key, std::istream& in)
{
  for (const auto& [name, member] : kIntLists)
  {
    if (key != name)
      continue;
    std::vector<int>& v = s.*member;
    int n = 0, x = 0;
    in >> n;
    for (int i = 0; i < n && i < 100000 && in >> x; ++i)
      v.push_back(x);
    return true;
  }
  return false;
}

using LineReader = bool (*)(SaveGame&, const std::string&, std::istream&);
constexpr LineReader kLineReaders[] = {readRunInfo, readRunState, readEntityValue, readEntityState, readIntList};

// One line into s; true for the closing "end" line. An unknown key (a newer
// game's line, or junk) is skipped. Lines after "end" are still read.
bool readLine(SaveGame& s, const std::string& line)
{
  std::istringstream in(line);
  std::string key;
  in >> key;
  if (key == "end")
    return true;
  for (LineReader reader : kLineReaders)
    if (reader(s, key, in))
      break;
  return false;
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
  writeRun(o, s);
  writeEntities(o, s);
  writeIntLists(o, s);
  o << "end\n";
  // All or nothing: a failed write leaves the old savegame in place.
  return writeFileAtomic(path, o.str(), error);
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
  std::getline(f, line); // the rest of the header line
  while (std::getline(f, line))
    if (readLine(s, line))
      complete = true;
  // A file cut short (full disk, crash mid-write) is treated as no save.
  if (!complete || s.hp <= 0)
    return std::nullopt;
  return s;
}

} // namespace gr
