#include "data/campaign.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <unordered_map>

namespace gr
{

namespace
{

const std::array<CampaignLevel, kAllLevels> kLevels{{
  {1, "rooftop_run", "ROOFTOP RUN", "cloud_nine"},
  {2, "glass_canyon", "GLASS CANYON", "free_fall"},
  {3, "club_laserdisc", "CLUB LASERDISC", "step_on_the_beat"},
  {4, "blackout", "BLACKOUT", "echo_room"},
  {5, "sludge_line", "SLUDGE LINE", "duck_rapids"},
  {6, "maglev_express", "MAGLEV EXPRESS", "light_trail"},
  {7, "chopper_down", "CHOPPER DOWN", "pilot_seat"},
  {8, "canopy_road", "CANOPY ROAD", "bounce"},
  {9, "hall_of_traps", "HALL OF TRAPS", "trapmaster"},
  {10, "sun_mirrors", "SUN MIRRORS", "negative"},
  {11, "idol_mines", "IDOL MINES", "pinball"},
  {12, "lava_heart", "LAVA HEART", "floor_lava"},
  {13, "boulder_run", "BOULDER RUN", "surf"},
  {14, "idol_awakens", "THE IDOL AWAKENS", "golden"},
  {15, "hangar_bay", "HANGAR BAY", "asteroid_belt"},
  {16, "cryo_labs", "CRYO LABS", "air_hockey"},
  {17, "hydroponics", "HYDROPONICS", "growth_spurt"},
  {18, "hull_walk", "HULL WALK", "planetoids"},
  {19, "gravity_lab", "GRAVITY LAB", "gun_gravity"},
  {20, "reactor_core", "REACTOR CORE", "stop_motion"},
  {21, "zero", "ZERO", "wireframe"},
  {22, "dry_gulch", "DRY GULCH", "high_noon"},
  {23, "fright_night_manor", "FRIGHT NIGHT MANOR", "both_sides"},
  {24, "too_many_cooks", "TOO MANY COOKS", "turntable"},
  {25, "tiny_kingdom", "TINY KINGDOM", "dandelion_drift"},
  {26, "kaiju_tonight", "KAIJU TONIGHT", "top_down"},
  {27, "the_long_rain", "THE LONG RAIN", "silent_picture"},
  {28, "toon_town_throwdown", "TOON TOWN THROWDOWN", "inkwell"},
  {29, "dead_air", "DEAD AIR", "wraparound"},
  {30, "fiber_trench", "FIBER TRENCH", "bubble_up"},
  {31, "storm_relay", "STORM RELAY", "bungee"},
  {32, "dish_array", "DISH ARRAY", "heatstroke"},
  {33, "magma_mainframe", "MAGMA MAINFRAME", "overclock"},
  {34, "zeppelin_relay", "ZEPPELIN RELAY", "human_cannonball"},
  {35, "the_transmitter", "THE TRANSMITTER", "copycat"},
  {36, "prop_warehouse", "PROP WAREHOUSE", "throw"},
  {37, "wardrobe", "WARDROBE", "totem"},
  {38, "writers_room", "WRITERS' ROOM", "breakout"},
  {39, "gallery", "THE GALLERY", "rewind"},
  {40, "fly_tower", "THE FLY TOWER", "trigger"},
  {41, "sweeps_week", "SWEEPS WEEK", "paintball"},
  {42, "live_finale", "THE LIVE FINALE", "bloopers"},
  // Episode 7: DEEP SPACE (/mnt/project-files/gunrunners-levels/space-world/DESIGN.md).
  {43, "starfall", "STARFALL", ""},
  {44, "crash_garden", "CRASH GARDEN", ""},
  {45, "hive_gullets", "HIVE GULLETS", ""},
  {46, "crystal_drift", "CRYSTAL DRIFT", ""},
  {47, "silk_canyon", "SILK CANYON", ""},
  {48, "bounder_plains", "BOUNDER PLAINS", ""},
  {49, "hive_mother", "THE HIVE MOTHER", ""},
}};

const std::array<Episode, kEpisodes> kEpisodeList{{
  {1, "NEON OVERDRIVE", "A 1987 THAT NEVER ENDED", 1, 7},
  {2, "LOST TEMPLE", "GOLD, TRAPS AND A GOLEM", 8, 14},
  {3, "STATION ZERO", "A FROZEN BASE IN ORBIT", 15, 21},
  {4, "CHANNEL SURFING", "EVERY CHANNEL IS A LEVEL", 22, 28},
  {5, "SIGNAL HUNT", "FIND THE TRANSMITTER", 29, 35},
  {6, "LIVE FINALE", "THE SHOW MUST GO ON", 36, 42},
  {7, "DEEP SPACE", "A MISDIALLED BEAM TO AN ALIEN WORLD", 43, 49},
}};

// What the disk said about each data file asked for so far, by path.
// WORLD SELECT and the title screen ask every frame (dozens of level files
// a frame once every episode is open), which is wasteful on Android storage
// and the Emscripten FS. The game never writes into levels/ or cutscenes/
// after the Android unpack at startup (prepareAndroidData), which runs
// before the first lookup, so an answer holds until something calls
// forgetDataFiles(): the title screen does each time it opens, and a level
// that would not load does (frontend/flow.cpp). Only the main thread asks
// today; the lock costs next to nothing and keeps a future background
// caller safe.
std::mutex gFilesMutex;
std::unordered_map<std::string, bool> gFiles;

} // namespace

bool dataFileExists(const std::string& path)
{
  const std::lock_guard<std::mutex> lock(gFilesMutex);
  if (const auto it = gFiles.find(path); it != gFiles.end())
    return it->second;
  // A regular file (or a link to one): a directory named like a level file
  // is not a level, though fopen() would open it on Linux. The error_code
  // overload never throws; a path it cannot reach is "no". A file that is
  // there but unreadable counts, and its load fails gracefully later.
  std::error_code ec;
  const bool found = std::filesystem::is_regular_file(path, ec);
  gFiles.emplace(path, found);
  return found;
}

void forgetDataFiles()
{
  const std::lock_guard<std::mutex> lock(gFilesMutex);
  gFiles.clear();
}

const CampaignLevel& campaignLevel(int number)
{
  return kLevels[std::size_t(std::clamp(number, 1, kAllLevels) - 1)];
}

const Episode& episode(int number) { return kEpisodeList[std::size_t(std::clamp(number, 1, kEpisodes) - 1)]; }

int episodeOfLevel(int number) { return (std::clamp(number, 1, kAllLevels) - 1) / 7 + 1; }

int firstSpaceLevel(const std::string& dataDir)
{
  for (int n = kSpaceFirst; n <= kAllLevels; ++n)
    if (!levelFile(dataDir, n).empty())
      return n;
  return 0;
}

std::string levelFile(const std::string& dataDir, int number)
{
  char buf[128];
  std::snprintf(buf, sizeof(buf), "/levels/%02d_%s.txt", number, campaignLevel(number).slug);
  const std::string path = dataDir + buf;
  return dataFileExists(path) ? path : std::string();
}

std::string bonusFile(const std::string& dataDir, int number)
{
  const auto& lv = campaignLevel(number);
  if (!*lv.bonus)
    return {};
  char buf[128];
  std::snprintf(buf, sizeof(buf), "/levels/%02d_bonus_%s.txt", number, lv.bonus);
  const std::string path = dataDir + buf;
  return dataFileExists(path) ? path : std::string();
}

namespace
{

const ExtraLevel kExtras[kExtraLevels] = {
  {"motor_pool", "MOTOR POOL"},
  {"deep_dive", "DEEP DIVE"},
};

} // namespace

const ExtraLevel& extraLevel(int index) { return kExtras[std::clamp(index, 0, kExtraLevels - 1)]; }

std::string extraFile(const std::string& dataDir, int index)
{
  if (index < 0 || index >= kExtraLevels)
    return {};
  const std::string path = dataDir + "/levels/extra_" + kExtras[index].slug + ".txt";
  return dataFileExists(path) ? path : std::string();
}

std::string cutsceneFile(const std::string& dataDir, const std::string& name)
{
  const std::string path = dataDir + "/cutscenes/" + name + ".txt";
  return dataFileExists(path) ? path : std::string();
}

int levelNumberOfFile(const std::string& path)
{
  const auto slash = path.find_last_of('/');
  const std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
  if (base.size() < 3 || !std::isdigit(static_cast<unsigned char>(base[0])) ||
      !std::isdigit(static_cast<unsigned char>(base[1])) || base[2] != '_')
    return 0;
  return std::atoi(base.substr(0, 2).c_str());
}

} // namespace gr
