#pragma once

#include <string>

namespace gr
{

// The 42-level campaign in six episodes (docs/LEVELS.md), plus Episode 7,
// DEEP SPACE (levels 43-49), a side episode on its own title menu entry. A
// level is playable once its file exists in levels/; a run stops at the
// first level that has not been built yet.
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
// The level number a level file belongs to (from its NN_ prefix), 0 if none.
int levelNumberOfFile(const std::string& path);

} // namespace gr
