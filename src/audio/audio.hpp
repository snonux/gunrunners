#pragma once

#include "game/sound_ids.hpp"

#include <SDL.h>

#include <array>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
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
  // Music by the ids level headers and cutscenes use ("theme_synthwave",
  // "stop"); tracks are synthesized the first time they are needed.
  void playMusicNamed(const std::string& id);
  // Synthesizes a named track now so a later playMusicNamed is instant.
  void preloadMusic(const std::string& id);
  // Jumps the music to this many seconds in (keeps the beat in sync with
  // the level's music clock after a pause).
  void seekMusic(double seconds);
  // Cutscene sounds by name; names containing "loop" repeat until stopped.
  void playNamed(const std::string& id, float volume = 1.0f);
  void stopNamed(const std::string& id);
  void stopAllNamed();
  void voiceBlip(const std::string& speaker);
  // Mixes `frames` stereo frames (interleaved L/R floats in -1..1).
  void mix(float* out, int frames);

private:
  struct Voice
  {
    const std::vector<float>* data;
    std::size_t pos;
    float volume;
    Sfx id;
    const std::string* name = nullptr; // named sounds
    bool loop = false;
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
  std::unordered_map<std::string, std::unique_ptr<Track>> mNamedTracks;
  std::unordered_map<std::string, std::vector<float>> mNamed;
  const Track* mTrack = nullptr; // what is playing
  std::string mTrackName;
  std::vector<Voice> mVoices;
  Music mMusic = Music::None;
  std::size_t mMusicPos = 0;
  std::mutex mMutex;
  SDL_AudioDeviceID mDevice = 0;
};

} // namespace gr
