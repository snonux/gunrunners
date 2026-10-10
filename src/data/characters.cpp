#include "data/characters.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <sys/stat.h>

namespace gr
{

namespace
{

const char* const kHumanHair[] = {"SPIKY", "BANDANA", "PONYTAIL", "LONG", "BUN", "MOHAWK", "BOB", "BUZZ CUT", "AFRO",
  "BRAID", "BALD"};
const char* const kRobotHead[] = {"ANTENNA", "TWIN ANTENNAE", "CREST", "DOME", "EAR BOLTS"};
const char* const kHumanFace[] = {"EYES", "SHADES", "BEARD", "GOGGLES", "EYE PATCH"};
const char* const kRobotFace[] = {"VISOR", "CYCLOPS", "TWIN EYES", "GRILLE"};
const char* const kOutfits[] = {"STRIPE", "DOG TAG", "SASH", "VEST", "HARNESS", "CORE LIGHT", "PLAIN"};
const char* const kBuilds[] = {"SLIM", "REGULAR", "BROAD"};

template <typename T, std::size_t N>
constexpr int count(const T (&)[N])
{
  return int(N);
}

const Color kPalette[] = {
  rgb(232, 64, 52),   // crimson
  rgb(255, 140, 40),  // orange
  rgb(255, 214, 72),  // gold
  rgb(250, 226, 150), // blonde
  rgb(150, 226, 70),  // lime
  rgb(104, 146, 64),  // green
  rgb(36, 190, 170),  // teal
  rgb(64, 226, 255),  // cyan
  rgb(64, 112, 232),  // blue
  rgb(54, 76, 150),   // navy
  rgb(132, 66, 216),  // violet
  rgb(255, 86, 184),  // magenta
  rgb(255, 160, 206), // pink
  rgb(238, 238, 248), // white
  rgb(110, 110, 124), // grey
  rgb(52, 48, 62),    // charcoal
  rgb(120, 76, 44),   // brown
  rgb(42, 30, 24),    // near black
  rgb(176, 64, 34),   // auburn
  rgb(64, 255, 220),  // mint
};

const std::pair<Color, Color> kSkin[] = {
  {rgb(252, 214, 186), rgb(224, 164, 142)},
  {rgb(246, 200, 160), rgb(214, 156, 118)},
  {rgb(226, 176, 132), rgb(188, 134, 96)},
  {rgb(198, 140, 98), rgb(160, 104, 70)},
  {rgb(170, 108, 70), rgb(126, 76, 48)},
  {rgb(124, 82, 56), rgb(90, 56, 38)},
  {rgb(90, 60, 44), rgb(62, 40, 30)},
  {rgb(150, 214, 170), rgb(100, 164, 124)}, // not from around here
  {rgb(176, 190, 250), rgb(128, 140, 206)},
};

const std::pair<Color, Color> kPlating[] = {
  {rgb(200, 208, 222), rgb(128, 138, 160)}, // silver
  {rgb(124, 132, 150), rgb(78, 84, 100)},   // gunmetal
  {rgb(236, 196, 96), rgb(172, 130, 52)},   // gold
  {rgb(216, 136, 86), rgb(150, 86, 52)},    // copper
  {rgb(238, 242, 248), rgb(178, 186, 202)}, // white
  {rgb(76, 78, 92), rgb(42, 44, 54)},       // black
  {rgb(220, 74, 64), rgb(150, 38, 40)},     // red
  {rgb(84, 140, 230), rgb(46, 84, 160)},    // blue
};

// Custom stats: what each pip buys.
constexpr int kHealthHp[kMaxPips + 1] = {0, 6, 7, 9, 10, 12};
constexpr int kAmmo[kMaxPips + 1] = {0, 8, 12, 16, 24, 32};
constexpr int kRapidPerPip = 175; // logic frames; 5 pips = a full pickup
const std::array<int, 8> kArcs[kMaxPips + 1] = {
  {2, 2, 1, 1, 0, 0, 0, 0},
  {2, 2, 1, 1, 0, 0, 0, 0}, // 6 cells (jump never goes below 2 pips)
  {2, 2, 1, 1, 0, 0, 0, 0}, // 6 cells, Rocco
  {2, 2, 1, 1, 1, 0, 0, 0}, // 7 cells, Dash (and Duke)
  {2, 2, 2, 1, 1, 0, 0, 0}, // 8 cells
  {2, 2, 2, 1, 1, 1, 0, 0}, // 9 cells, Nova
};

RunnerParts parts(int body, int build, int hair, int face, int outfit, int sleeves, int skin, int hairColor, int top,
  int pants, int boots, int glow)
{
  RunnerParts p;
  p.body = body;
  p.build = build;
  p.hair = hair;
  p.face = face;
  p.outfit = outfit;
  p.sleeves = sleeves;
  p.skin = skin;
  p.hairColor = hairColor;
  p.top = top;
  p.pants = pants;
  p.boots = boots;
  p.glow = glow;
  return p;
}

CharacterDef builtIn(const char* id, const char* name, const char* role, int hp, std::array<int, 8> arc, Weapon gun,
  int ammo, int rapid, int hpPips, int jumpPips, int powerPips, int art, RunnerParts look)
{
  CharacterDef d;
  d.id = id;
  d.name = name;
  d.role = role;
  d.maxHp = hp;
  d.jumpArc = arc;
  d.startWeapon = gun;
  d.startAmmo = ammo;
  d.startRapidFire = rapid;
  d.healthPips = hpPips;
  d.jumpPips = jumpPips;
  d.powerPips = powerPips;
  d.art = art;
  d.parts = look;
  return d;
}

const std::array<CharacterDef, kDefaultRunners>& defaults()
{
  // Dash plays exactly like Duke: 9 health, the classic jump arc. The
  // first three keep their hand-tuned art; their parts are what REMIX
  // starts from.
  static const std::array<CharacterDef, kDefaultRunners> kDefaults{{
    builtIn("dash", "DASH", "THE ALL-ROUNDER", 9, {2, 2, 1, 1, 1, 0, 0, 0}, Weapon::Normal, 0, 0, 3, 3, 3, 0,
      parts(0, 1, 0, 1, 0, 0, 1, 2, 0, 9, 16, 7)),
    builtIn("rocco", "ROCCO", "THE HEAVY", 12, {2, 2, 1, 1, 0, 0, 0, 0}, Weapon::Rocket, 12, 0, 5, 2, 5, 1,
      parts(0, 2, 1, 2, 1, 1, 4, 17, 5, 14, 15, 0)),
    builtIn("nova", "NOVA", "THE ACROBAT", 7, {2, 2, 2, 1, 1, 1, 0, 0}, Weapon::Laser, 16, 0, 2, 5, 4, 2,
      parts(0, 0, 2, 0, 2, 0, 0, 11, 10, 10, 13, 19)),
    // Jade burns through: a full flamethrower (which doubles as a jetpack)
    // and a solid ten hearts.
    builtIn("jade", "JADE", "THE PYRO", 10, {2, 2, 1, 1, 1, 0, 0, 0}, Weapon::Flame, 48, 0, 4, 3, 4, -1,
      parts(0, 0, 3, 3, 4, 1, 3, 18, 1, 15, 16, 2)),
    // Skye travels light: a high jump and every level starts in rapid fire.
    builtIn("skye", "SKYE", "THE SCOUT", 8, {2, 2, 2, 1, 1, 0, 0, 0}, Weapon::Normal, 0, 700, 3, 4, 5, -1,
      parts(0, 0, 9, 0, 3, 0, 5, 13, 6, 9, 13, 7)),
    // Bolt is a walking armoury: a full laser and plating, but jumps like
    // a fridge.
    builtIn("bolt", "BOLT", "THE MACHINE", 11, {2, 2, 1, 1, 0, 0, 0, 0}, Weapon::Laser, 32, 0, 4, 2, 5, -1,
      parts(1, 2, 0, 0, 5, 1, 0, 2, 8, 15, 15, 4)),
  }};
  return kDefaults;
}

std::vector<CharacterDef>& customs()
{
  static std::vector<CharacterDef> list;
  return list;
}

int clampIndex(int v, int n) { return n <= 0 ? 0 : std::clamp(v, 0, n - 1); }

void sanitize(RunnerParts& p)
{
  p.body = clampIndex(p.body, 2);
  p.build = clampIndex(p.build, count(kBuilds));
  p.hair = clampIndex(p.hair, hairStyleCount(p.body));
  p.face = clampIndex(p.face, faceCount(p.body));
  p.outfit = clampIndex(p.outfit, outfitCount());
  p.sleeves = clampIndex(p.sleeves, 2);
  p.skin = clampIndex(p.skin, skinToneCount(p.body));
  for (int* c : {&p.hairColor, &p.top, &p.pants, &p.boots, &p.glow})
    *c = clampIndex(*c, paletteSize());
}

std::string cleanName(const std::string& in)
{
  std::string out;
  for (char c : in)
  {
    if (c >= 'a' && c <= 'z')
      c = char(c - 'a' + 'A');
    const bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '.' ||
      c == '!' || c == '\'';
    if (ok && int(out.size()) < kMaxRunnerName)
      out += c;
  }
  while (!out.empty() && out.back() == ' ')
    out.pop_back();
  while (!out.empty() && out.front() == ' ')
    out.erase(out.begin());
  return out.empty() ? "RUNNER" : out;
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

bool RunnerParts::operator==(const RunnerParts& o) const
{
  return body == o.body && build == o.build && hair == o.hair && face == o.face && outfit == o.outfit &&
    sleeves == o.sleeves && skin == o.skin && hairColor == o.hairColor && top == o.top && pants == o.pants &&
    boots == o.boots && glow == o.glow;
}

int hairStyleCount(int body) { return body == 1 ? count(kRobotHead) : count(kHumanHair); }
const char* hairStyleName(int body, int index)
{
  return body == 1 ? kRobotHead[clampIndex(index, count(kRobotHead))] : kHumanHair[clampIndex(index, count(kHumanHair))];
}
int faceCount(int body) { return body == 1 ? count(kRobotFace) : count(kHumanFace); }
const char* faceName(int body, int index)
{
  return body == 1 ? kRobotFace[clampIndex(index, count(kRobotFace))] : kHumanFace[clampIndex(index, count(kHumanFace))];
}
int outfitCount() { return count(kOutfits); }
const char* outfitName(int index) { return kOutfits[clampIndex(index, count(kOutfits))]; }
const char* buildName(int index) { return kBuilds[clampIndex(index, count(kBuilds))]; }
int skinToneCount(int body) { return body == 1 ? count(kPlating) : count(kSkin); }
std::pair<Color, Color> skinTone(int body, int index)
{
  return body == 1 ? kPlating[clampIndex(index, count(kPlating))] : kSkin[clampIndex(index, count(kSkin))];
}
int paletteSize() { return count(kPalette); }
Color paletteColor(int index) { return kPalette[clampIndex(index, count(kPalette))]; }

const char* weaponName(Weapon w)
{
  switch (w)
  {
    case Weapon::Laser:
      return "LASER";
    case Weapon::Rocket:
      return "ROCKETS";
    case Weapon::Flame:
      return "FLAMER";
    case Weapon::Proto:
      return "PROTOTYPE";
    case Weapon::Normal:
    default:
      return "BLASTER";
  }
}

int maxAmmo(Weapon w)
{
  return w == Weapon::Flame ? 64 : 32;
}

int characterCount()
{
  return kDefaultRunners + int(customs().size());
}

const CharacterDef& characterByIndex(int index)
{
  const int n = characterCount();
  const int i = ((index % n) + n) % n;
  if (i < kDefaultRunners)
    return defaults()[std::size_t(i)];
  return customs()[std::size_t(i - kDefaultRunners)];
}

int runnerIndexById(const std::string& id)
{
  for (int i = 0; i < characterCount(); ++i)
    if (characterByIndex(i).id == id)
      return i;
  return -1;
}

int runnerPointsLeft(int healthPips, int jumpPips, int powerPips)
{
  return kRunnerBudget - healthPips - jumpPips - powerPips;
}

CharacterDef makeCustomRunner(const std::string& id, const std::string& name, const RunnerParts& look, Weapon gun,
  int healthPips, int jumpPips, int powerPips)
{
  if (gun == Weapon::Proto)
    gun = Weapon::Normal;
  int pips[3] = {healthPips, jumpPips, powerPips};
  for (int s = 0; s < 3; ++s)
    pips[s] = std::clamp(pips[s], kMinPips[s], kMaxPips);
  // Over budget (a hand-edited file): take points back from the end.
  for (int s = 2; s >= 0 && runnerPointsLeft(pips[0], pips[1], pips[2]) < 0; --s)
    while (pips[s] > kMinPips[s] && runnerPointsLeft(pips[0], pips[1], pips[2]) < 0)
      --pips[s];

  CharacterDef d;
  d.id = id;
  d.name = cleanName(name);
  d.parts = look;
  sanitize(d.parts);
  d.custom = true;
  d.healthPips = pips[0];
  d.jumpPips = pips[1];
  d.powerPips = pips[2];
  d.maxHp = kHealthHp[pips[0]];
  d.jumpArc = kArcs[pips[1]];
  d.startWeapon = gun;
  if (gun == Weapon::Normal)
    d.startRapidFire = (pips[2] - 1) * kRapidPerPip;
  else
    d.startAmmo = std::min(maxAmmo(gun), kAmmo[pips[2]] * (gun == Weapon::Flame ? 2 : 1));

  const int best = std::max({pips[0], pips[1], pips[2]});
  if (pips[0] == pips[1] && pips[1] == pips[2])
    d.role = "THE ALL-ROUNDER";
  else if (pips[0] == best)
    d.role = d.parts.body == 1 ? "THE TANK" : "THE BRUISER";
  else if (pips[1] == best)
    d.role = "THE HIGH FLYER";
  else
    d.role = "THE GUNNER";
  return d;
}

std::string newRunnerId()
{
  static std::mt19937 rng{std::random_device{}()};
  char buf[16];
  std::snprintf(buf, sizeof(buf), "c%08x", unsigned(rng()));
  return buf;
}

CharacterDef remixRunner(const CharacterDef& base)
{
  const int h = std::clamp(base.healthPips, kMinPips[0], kMaxPips);
  const int j = std::clamp(base.jumpPips, kMinPips[1], kMaxPips);
  const int p = std::clamp(base.powerPips, kMinPips[2], kMaxPips);
  return makeCustomRunner(newRunnerId(), base.name + "-2", base.parts, base.startWeapon, h, j, p);
}

int putCustomRunner(const CharacterDef& def)
{
  auto& list = customs();
  for (std::size_t i = 0; i < list.size(); ++i)
    if (list[i].id == def.id)
    {
      list[i] = def;
      list[i].custom = true;
      return kDefaultRunners + int(i);
    }
  list.push_back(def);
  list.back().custom = true;
  return kDefaultRunners + int(list.size()) - 1;
}

void removeCustomRunner(const std::string& id)
{
  auto& list = customs();
  list.erase(std::remove_if(list.begin(), list.end(), [&](const CharacterDef& d) { return d.id == id; }), list.end());
}

void clearCustomRunners() { customs().clear(); }

std::string encodeRunner(const CharacterDef& d)
{
  const auto& p = d.parts;
  std::ostringstream o;
  o << "v1 " << d.id << ' ' << int(d.startWeapon) << ' ' << d.healthPips << ' ' << d.jumpPips << ' ' << d.powerPips
    << ' ' << p.body << ' ' << p.build << ' ' << p.hair << ' ' << p.face << ' ' << p.outfit << ' ' << p.sleeves << ' '
    << p.skin << ' ' << p.hairColor << ' ' << p.top << ' ' << p.pants << ' ' << p.boots << ' ' << p.glow << ' '
    << d.name;
  return o.str();
}

bool decodeRunner(const std::string& line, CharacterDef& out)
{
  std::istringstream in(line);
  std::string version, id;
  int gun = 0, h = 0, j = 0, pw = 0;
  RunnerParts p;
  if (!(in >> version >> id >> gun >> h >> j >> pw >> p.body >> p.build >> p.hair >> p.face >> p.outfit >> p.sleeves >>
        p.skin >> p.hairColor >> p.top >> p.pants >> p.boots >> p.glow) ||
      version != "v1" || id.empty() || id[0] != 'c')
    return false;
  std::string name;
  std::getline(in >> std::ws, name);
  out = makeCustomRunner(id, name, p, Weapon(std::clamp(gun, 0, kWeaponCount - 1)), h, j, pw);
  return true;
}

void loadCustomRunners(const std::string& dir)
{
  auto& list = customs();
  list.clear();
  std::ifstream f(dir + "/runners.txt");
  std::string line;
  while (std::getline(f, line) && int(list.size()) < kMaxCustomRunners)
  {
    if (line.rfind("runner ", 0) != 0)
      continue;
    CharacterDef d;
    if (decodeRunner(line.substr(7), d) && runnerIndexById(d.id) < 0)
      list.push_back(d);
  }
}

bool saveCustomRunners(const std::string& dir)
{
  makeDirs(dir);
  const std::string path = dir + "/runners.txt";
  {
    std::ofstream o(path + ".tmp");
    if (!o)
      return false;
    o << "gunrunners-runners 1\n";
    for (const auto& d : customs())
      if (!d.transient)
        o << "runner " << encodeRunner(d) << '\n';
    if (!o)
      return false;
  }
  return std::rename((path + ".tmp").c_str(), path.c_str()) == 0;
}

CharacterDef savedRunner(int index, const std::string& encoded)
{
  CharacterDef d;
  if (!encoded.empty() && decodeRunner(encoded, d))
    return d;
  return defaults()[std::size_t(index >= 0 && index < kDefaultRunners ? index : 0)];
}

int resolveSavedRunner(int index, const std::string& encoded)
{
  if (encoded.empty())
    return index >= 0 && index < kDefaultRunners ? index : 0;
  CharacterDef d;
  if (!decodeRunner(encoded, d))
    return 0;
  if (const int found = runnerIndexById(d.id); found >= 0)
    return found;
  d.transient = true;
  return putCustomRunner(d);
}

} // namespace gr
