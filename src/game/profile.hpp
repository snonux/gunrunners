#pragma once

#include <map>
#include <set>
#include <string>

namespace gr
{

// What the player has unlocked across all runs, kept next to the savegames
// in profile.txt: the Arsenal, Bonus Stars, rubber ducks, candid camera
// shots, cutscenes seen (the Reruns menu) and how far the campaign got.
struct Profile
{
  std::set<std::string> protos;    // prototype keys logged to the Arsenal
  std::map<std::string, int> protoKills; // best kill count per prototype
  std::set<int> stars;             // levels whose bonus level was beaten
  std::set<int> ducks;             // levels whose rubber duck was found
  std::set<int> cameras;           // levels whose candid camera was shot
  std::set<std::string> cutscenes; // seen cutscenes
  std::map<int, int> scores;       // best score per level (level 6's billboard shows level 2's)
  int reached = 1;                 // highest level started
  bool duckMode = false;           // all 42 ducks: quack mode is available
  bool fullscreen = false;         // the window covers the screen, no borders

  static Profile load(const std::string& dir);
  bool save(const std::string& dir) const;
};

} // namespace gr
