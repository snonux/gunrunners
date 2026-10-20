#pragma once

#include "audio/audio.hpp"

#include <string>
#include <vector>

namespace gr::synth
{

struct MusicTrack
{
  std::vector<float> left;
  std::vector<float> right;
  bool loops = true;
};

// Mono, 48 kHz.
std::vector<float> makeSfx(Sfx id);
MusicTrack makeMusic(Music m);
// Cutscene sounds by name (synth_named.cpp), voice blips per speaker, and
// music tracks by the ids the level headers and cutscenes use.
std::vector<float> makeNamedSfx(const std::string& id);
std::vector<float> makeVoiceBlip(const std::string& speaker);
MusicTrack makeNamedMusic(const std::string& id);
// Level soundtracks (music.cpp): each id has its own arrangement.
MusicTrack makeStyledMusic(const std::string& id);
// For tests: which bars of a level soundtrack's loop carry its tune, and
// whether it plays in Duke mode (renders the track to find out).
struct TuneBars
{
  bool metal = false;
  std::vector<bool> bars;
};
TuneBars styledTuneBars(const std::string& id);

} // namespace gr::synth
