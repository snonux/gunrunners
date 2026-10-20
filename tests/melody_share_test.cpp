// Paul (2026-10-20): the level music should play without its melody at
// least half the time ("it can get annoying"). For every soundtrack a level
// file names (music=...), checks that in Duke mode
//   - the tune (lead guitar, then the answering signature instrument) plays
//     in at most half of the loop's bars, and in at least one, so each
//     level keeps its own melody;
// and that the title's anthem (theme_title) has no tune at all. The
// finale's medley (a string of other tracks' tunes) is not a style of its
// own and is skipped; its parts are level tracks.

#include "audio/synth.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace gr;

namespace
{

int gFailures = 0;

void check(bool ok, const std::string& what)
{
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
  if (!ok)
    ++gFailures;
}

} // namespace

int main(int argc, char** argv)
{
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 2)
  {
    std::printf("usage: melody_share_test LEVELS_DIR\n");
    return 2;
  }
  std::set<std::string> ids;
  const std::regex music("music[= ]([A-Za-z_0-9]+)");
  for (const auto& entry : std::filesystem::directory_iterator(argv[1]))
  {
    if (entry.path().extension() != ".txt")
      continue;
    std::ifstream in(entry.path());
    std::string line;
    while (std::getline(in, line))
      for (std::sregex_iterator it(line.begin(), line.end(), music), end; it != end; ++it)
        ids.insert((*it)[1]);
  }
  ids.erase("theme_medley");
  check(ids.size() > 50, "level files name " + std::to_string(ids.size()) + " soundtracks");

  // Rendering takes most of a second a track: spread them over the cores.
  const std::vector<std::string> list(ids.begin(), ids.end());
  std::vector<synth::TuneBars> maps(list.size());
  std::atomic<std::size_t> next{0};
  std::vector<std::thread> workers;
  for (unsigned w = 0; w < std::max(1u, std::thread::hardware_concurrency()); ++w)
    workers.emplace_back([&] {
      for (std::size_t i; (i = next++) < list.size();)
        maps[i] = synth::styledTuneBars(list[i]);
    });
  for (auto& w : workers)
    w.join();

  int metal = 0;
  for (std::size_t i = 0; i < list.size(); ++i)
  {
    const std::string& id = list[i];
    const synth::TuneBars& t = maps[i];
    if (!t.metal)
      continue;
    ++metal;
    int on = 0;
    std::string map;
    for (bool b : t.bars)
    {
      on += b ? 1 : 0;
      map += b ? 'M' : '.';
    }
    const int bars = int(t.bars.size());
    check(on >= 1 && on * 2 <= bars, id + ": tune in " + std::to_string(on) + " of " + std::to_string(bars) + " bars " + map);
  }
  check(metal > 50, std::to_string(metal) + " soundtracks play in Duke mode");

  const synth::TuneBars title = synth::styledTuneBars("theme_title");
  int titleOn = 0;
  for (bool b : title.bars)
    titleOn += b ? 1 : 0;
  check(title.metal && !title.bars.empty() && titleOn == 0, "the title's anthem has no tune");

  std::printf(gFailures ? "%d FAILED\n" : "all passed\n", gFailures);
  return gFailures ? 1 : 0;
}
