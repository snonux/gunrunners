#pragma once

#include <string>

namespace gr
{

// The 42-level campaign in six episodes (docs/LEVELS.md). A level is
// playable once its file exists in levels/; the campaign stops at the first
// level that has not been built yet.
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

constexpr int kCampaignLevels = 42;
constexpr int kEpisodes = 6;

const CampaignLevel& campaignLevel(int number); // 1..42
const Episode& episode(int number);             // 1..6
int episodeOfLevel(int number);
std::string levelFile(const std::string& dataDir, int number);
std::string bonusFile(const std::string& dataDir, int number); // empty if none
std::string cutsceneFile(const std::string& dataDir, const std::string& name);
// The level number a level file belongs to (from its NN_ prefix), 0 if none.
int levelNumberOfFile(const std::string& path);

} // namespace gr
