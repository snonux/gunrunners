#pragma once

#include "game/sound_ids.hpp"

#include <SDL.h>

#include <array>
#include <mutex>
#include <vector>

namespace gr
{

enum class Music
{
  None,
  Menu,
  Level,
  Victory,
  Count,
};

constexpr int kAudioRate = 48000;

// Software mixer for the synthesized sounds and music (see synth.cpp).
// In a window it feeds an SDL audio device; when recording headlessly the
// frontend pulls exactly one video frame's worth of samples per tick, so
// the soundtrack stays in sync with the video.
class Audio
{
public:
  Audio();
  ~Audio();
  Audio(const Audio&) = delete;
  Audio& operator=(const Audio&) = delete;

  bool openDevice();
  void play(Sfx s, float volume = 1.0f);
  void playMusic(Music m);
  // Mixes `frames` stereo frames (interleaved L/R floats in -1..1).
  void mix(float* out, int frames);

private:
  struct Voice
  {
    const std::vector<float>* data;
    std::size_t pos;
    float volume;
    Sfx id;
  };
  struct Track
  {
    std::vector<float> left;
    std::vector<float> right;
    bool loops = true;
  };

  static void callback(void* user, Uint8* stream, int len);
  void mixLocked(float* out, int frames);

  std::array<std::vector<float>, std::size_t(Sfx::Count)> mSfx;
  std::array<Track, std::size_t(Music::Count)> mTracks;
  std::vector<Voice> mVoices;
  Music mMusic = Music::None;
  std::size_t mMusicPos = 0;
  std::mutex mMutex;
  SDL_AudioDeviceID mDevice = 0;
};

} // namespace gr
