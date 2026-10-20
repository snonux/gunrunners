// The title menu plays its own anthem in Duke mode, like the levels (Paul,
// 2026-10-18: "duke nukem style music" on the start screen). Checks that
//   - Music::Menu ("menu") is the theme_title arrangement: a loop of 16 bars
//     of 4/4 at 132 BPM, not the old calm synthwave loop (20 s, which the
//     diner's radio, theme_diner, still plays);
//   - it is as loud as Level 1's metal theme (within 2 dB), so the music
//     does not jump in volume from the title into a level;
//   - the first 4 bars (riff only, under the GUNRUNNERS shout) are quieter
//     than the bars after them, where the lead guitar comes in.

#include "audio/synth.hpp"

#include <cmath>
#include <cstdio>
#include <string>

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

// RMS in dB of both channels from `from` to `to` seconds.
double rmsDb(const synth::MusicTrack& t, double from, double to)
{
  const std::size_t a = std::size_t(from * kAudioRate);
  const std::size_t b = std::min(t.left.size(), std::size_t(to * kAudioRate));
  double sum = 0.0;
  for (std::size_t i = a; i < b; ++i)
    sum += double(t.left[i]) * t.left[i] + double(t.right[i]) * t.right[i];
  return 10.0 * std::log10(sum / double(std::max<std::size_t>(1, 2 * (b - a))) + 1e-12);
}

} // namespace

int main()
{
  const synth::MusicTrack menu = synth::makeNamedMusic("menu");
  const synth::MusicTrack title = synth::makeMusic(Music::Menu);
  const synth::MusicTrack level = synth::makeNamedMusic("theme_synthwave");
  const synth::MusicTrack diner = synth::makeNamedMusic("theme_diner");

  const double bar = 4.0 * 60.0 / 132.0;
  const double len = double(menu.left.size()) / kAudioRate;
  check(menu.loops && menu.right.size() == menu.left.size(), "the menu track is a stereo loop");
  check(std::fabs(len - 16.0 * bar) < 0.01, "the menu track is 16 bars at 132 BPM (" + std::to_string(len) + " s)");
  check(title.left == menu.left, "Music::Menu renders the same track as \"menu\"");
  check(std::fabs(double(diner.left.size()) / kAudioRate - 20.0) < 0.05 && diner.left != menu.left,
        "theme_diner keeps the old calm loop");

  const double menuDb = rmsDb(menu, 0.0, len);
  const double levelDb = rmsDb(level, 0.0, double(level.left.size()) / kAudioRate);
  std::printf("menu %.1f dB, Level 1 %.1f dB\n", menuDb, levelDb);
  check(std::fabs(menuDb - levelDb) < 2.0, "as loud as Level 1's metal theme (within 2 dB)");

  const double introDb = rmsDb(menu, 0.0, 4.0 * bar);
  const double hookDb = rmsDb(menu, 4.0 * bar, 12.0 * bar);
  std::printf("intro %.1f dB, hook %.1f dB\n", introDb, hookDb);
  check(introDb < hookDb - 1.0, "the riff intro under the shout is quieter than the hook");

  std::printf(gFailures ? "%d FAILED\n" : "all passed\n", gFailures);
  return gFailures ? 1 : 0;
}
