#pragma once

#include "audio/audio.hpp"

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

} // namespace gr::synth
