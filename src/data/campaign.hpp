#pragma once

#include <string>

namespace gr
{

// The 42-level campaign in six episodes (docs/LEVELS.md), plus Episode 7,
// DEEP SPACE (levels 43-49), a side episode open from the start in WORLD
// SELECT (frontend/worlds.cpp). A level is playable once its file exists in
// levels/ when the game looks for it (the answer is kept until the title
// screen opens again, see dataFileExists()); a run stops at the first level
// that has not been built yet.
struct CampaignLevel
{
  int number;
  const char* slug;  // levels/NN_slug.txt
  const char* title;
  const char* bonus; // levels/NN_bonus_<bonus>.txt, empty if none
};

struct Episode
{
  int number;
  const char* name;
  const char* tagline;
  int first, last; // level numbers
};

constexpr int kCampaignLevels = 42; // the main story ends here
constexpr int kSpaceFirst = 43;     // Episode 7, DEEP SPACE
constexpr int kAllLevels = 49;
constexpr int kEpisodes = 7;

const CampaignLevel& campaignLevel(int number); // 1..49
const Episode& episode(int number);             // 1..7
inline bool spaceLevel(int number) { return number >= kSpaceFirst && number <= kAllLevels; }
// The first space level whose file exists, 0 if none has been built yet.
int firstSpaceLevel(const std::string& dataDir);
int episodeOfLevel(int number);
std::string levelFile(const std::string& dataDir, int number);
std::string bonusFile(const std::string& dataDir, int number); // empty if none
std::string cutsceneFile(const std::string& dataDir, const std::string& name);
// Levels outside the 42-level numbering (levels/extra_SLUG.txt), played
// from the EXTRAS card in WORLD SELECT.
struct ExtraLevel
{
  const char* slug;
  const char* title;
};
constexpr int kExtraLevels = 2;
const ExtraLevel& extraLevel(int index); // 0..kExtraLevels-1
std::string extraFile(const std::string& dataDir, int index); // empty if none
// The level number a level file belongs to (from its NN_ prefix), 0 if none.
int levelNumberOfFile(const std::string& path);

// Whether a data file exists: a regular file, not a directory of that name.
// The disk is asked once per path and the answer kept, so the menus can call
// levelFile() and friends every frame; the *File() functions above all go
// through it. The game never writes into levels/ or cutscenes/ after the
// Android unpack at startup (prepareAndroidData), which runs before the
// first lookup. forgetDataFiles() drops what was kept: the title screen
// calls it each time it opens (Game::goTitle), so level files added while
// the game runs show up there. A file deleted mid-run still counts as found
// until then; loading it fails gracefully (a notice such as "THAT LEVEL
// WOULD NOT LOAD", or NO SIGNAL for a bonus level) and forgets what was
// kept too (frontend/flow.cpp), so a menu stops offering it the next time
// it opens: WORLD SELECT's cards at once (drawn every frame), a list
// already open (EXTRAS, the Bonus Channel) only once it is opened again.
bool dataFileExists(const std::string& path);
void forgetDataFiles();

} // namespace gr
