#pragma once

#include "audio/pending_music.hpp"
#include "audio/render_queue.hpp"
#include "game/sound_ids.hpp"

#include <SDL.h>

#include <array>
#include <cstdint>
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
  // "stop"); tracks are synthesized the first time they are needed. With
  // a sound device and the background worker, one not rendered yet never
  // blocks the caller: the music playing goes on, the track is rendered
  // next (ahead of the other preloads) and pollMusic starts it once done.
  // Without them (headless recording, the browser build without threads)
  // it is rendered, or its render in progress waited for, right here.
  void playMusicNamed(const std::string& id);
  // Called once per tick on the game thread (Game::tick): starts the track
  // a playMusicNamed is waiting for once its render is done, where it
  // would be had it started on time (see PendingMusic).
  void pollMusic();
  // Queues a named track for the background renderer (one worker thread,
  // oldest request first) so a later playMusicNamed is instant.
  void preloadMusic(const std::string& id);
  // A new level starts: cancels the queued preloads and marks the finished
  // ones stale; a preloadMusic of one before dropStalePreloads keeps it
  // (a track two levels share is not rendered twice).
  void markPreloadsStale();
  // Frees the finished preloads still stale (the level has asked for all
  // it wants by then).
  void dropStalePreloads();
  // Forgets every preload nobody played (back to the title): queued ones
  // are cancelled and finished ones freed. Played tracks stay cached.
  void dropPreloads();
  // Jumps the music to this many seconds in (keeps the beat in sync with
  // the level's music clock after a pause). While a playMusicNamed waits
  // for its render, the seek is meant for that track: it starts this far
  // in plus whatever played until it arrived.
  void seekMusic(double seconds);
  // Cutscene sounds by name; names containing "loop" repeat until stopped.
  void playNamed(const std::string& id, float volume = 1.0f);
  void stopNamed(const std::string& id);
  void stopAllNamed();
  void voiceBlip(const std::string& speaker);
  // A runner's spoken line (mono, kAudioRate): one at a time on its own
  // channel, with the music ducked under it. A new line replaces the one
  // playing. volume 0 is silence.
  void playVoice(std::shared_ptr<const std::vector<float>> clip, float volume);
  void stopVoice();
  bool voicePlaying();
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
  // Whether playMusicNamed may start a track late instead of blocking.
  bool deferSwitches() const;
  // The cached track, collecting a background render or making it now.
  Track* namedTrack(const std::string& id);
  // The cached track or a finished background render; nullptr otherwise
  // (never blocks).
  Track* readyTrack(const std::string& id);
  Track* cachedTrack(const std::string& id);
  // Caches a new track, evicting the oldest past kKeepTracks.
  Track* keepTrack(const std::string& id, std::unique_ptr<Track> tr);
  void startTrackLocked(const std::string& id, const Track* track, std::size_t pos);
  static std::unique_ptr<Track> synthTrack(const std::string& id);
  void mixLocked(float* out, int frames);
  void mixMusicLocked(float* out, int frames);
  void mixVoiceLocked(float* out, int frames);
  void mixSoundsLocked(float* out, int frames);

  std::array<std::vector<float>, std::size_t(Sfx::Count)> mSfx;
  std::array<Track, std::size_t(Music::Count)> mTracks;
  std::unordered_map<std::string, std::unique_ptr<Track>> mNamedTracks;
  std::vector<std::string> mTrackOrder; // oldest first
  static constexpr std::size_t kKeepTracks = 10;
  // Finished preloads not played yet, at ~12 MB each (~100 MB at most).
  // A level's own track is claimed as it starts; what it preloads beyond
  // that is at most its 3 variants (the club's chiptune, the cab's cover,
  // Level 35's waltz) and 4 '@' layers (the fifth plays from the start),
  // 7 even if one level had both, so 8 holds a level's whole set. Stale
  // ones go first, then the oldest.
  static constexpr std::size_t kKeepPreloads = 8;
  RenderQueue<Track> mPreloads;
  std::unordered_map<std::string, std::vector<float>> mNamed;
  const Track* mTrack = nullptr; // what is playing
  std::string mTrackName;
  // The track playMusicNamed waits for (game thread only, see pollMusic).
  PendingMusic mPending;
  std::uint64_t mPlayedFrames = 0; // every frame mixed so far, the music's clock
  std::vector<Voice> mVoices;
  Music mMusic = Music::None;
  std::size_t mMusicPos = 0;
  std::shared_ptr<const std::vector<float>> mVoiceClip;
  std::size_t mVoicePos = 0;
  float mVoiceVolume = 1.0f;
  float mDuck = 1.0f; // music gain, eased towards kDuckedMusic under a line
  std::mutex mMutex;
  SDL_AudioDeviceID mDevice = 0;
};

} // namespace gr
