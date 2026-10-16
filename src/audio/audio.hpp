#pragma once

#include "audio/pending_music.hpp"
#include "audio/render_queue.hpp"
#include "audio/sound_bank.hpp"
#include "game/sound_ids.hpp"

#include <SDL.h>

#include <cstddef>
#include <cstdint>
#include <functional>
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
//
// Nothing is synthesized up front, so the first frame comes at once (all
// the effects and the built-in tracks took 4-6 s): with a sound device the
// effects render in the background from openDevice() on, at low priority
// (one played before its turn is rendered by that play, see play), and
// every track is rendered the first time it is played (see
// playMusicNamed).
class Audio
{
public:
  // A music track, stereo at kAudioRate.
  struct Track
  {
    std::vector<float> left;
    std::vector<float> right;
    bool loops = true;
  };
  // Renders a named track (the synthesizer's, see synthTrack); may throw.
  using TrackRender = std::function<std::unique_ptr<Track>(const std::string&)>;

  Audio();
  // For tests: tracks come from `renderTrack` instead of the synthesizer
  // (instant, and held back at will), and with `deferWithoutDevice` a
  // track not rendered yet starts late, as with a sound device, while the
  // test calls mix() itself (no device thread races its checks).
  Audio(TrackRender renderTrack, bool deferWithoutDevice);
  ~Audio();
  Audio(const Audio&) = delete;
  Audio& operator=(const Audio&) = delete;

  // Opens the sound device and starts rendering the sound effects in the
  // background, in sfxRenderOrder().
  bool openDevice();
  // An effect the background worker has not rendered yet is rendered here
  // on its first play (a few ms, once). With a sound device and the
  // worker, the one the worker is rendering right now is skipped this
  // once instead of waited for, so it never blocks on the worker. Without
  // them (headless recording, the browser build without threads) that
  // wait happens, so a recording has every sound.
  void play(Sfx s, float volume = 1.0f);
  // The built-in tracks, played as the named tracks synth::makeNamedMusic
  // renders the same ("menu", "theme_synthwave", "victory"), so they too
  // are rendered when first played and share the named tracks' cache
  // (Music::Level while theme_synthwave plays goes on with it). Music::None
  // stops the music.
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
  // would be had it started on time (see PendingMusic). A render that
  // failed is given up on (logged); the music playing goes on.
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
  // Jumps the music to this many seconds in. Game::playLevelMusic and a
  // music override switch in Game::tickPlay call it right after their
  // playMusicNamed, with the level's clock, so the beat signs and the
  // music agree. While a playMusicNamed waits for its render, the seek is
  // meant for that track: it starts this far in plus the frames the mixer
  // played until it arrived. Those frames are the device's, not the
  // level's: should the level clock stop or lag meanwhile (a pause menu,
  // slow frames), the late track lands that much ahead, and nothing
  // re-syncs it.
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
  // The track playing ("stop" for none) and its frame; for tests.
  std::string musicPlaying();
  std::size_t musicFrame();
  // The track a switch waits for, empty for none. Game thread only.
  const std::string& musicPending() const { return mPending.id(); }
  // The order the effects render in the background (each once): the menu's
  // first, as the title needs them at once, then the enum's order, which
  // lists those every level uses (jumps, shots, pickups) before each later
  // level's own. One played before its turn is rendered by that play.
  static std::vector<std::size_t> sfxRenderOrder();

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

  static void callback(void* user, Uint8* stream, int len);
  // Whether a sound or track may come late instead of being waited for or
  // rendered on the spot: play skips the effect the worker is rendering
  // right now, playMusicNamed starts a track not rendered yet late.
  bool deferRenders() const;
  // Calls off the switch waiting for its render (see cancelPending in
  // audio.cpp).
  void cancelPending();
  // The cached track, collecting a background render or making it now.
  Track* namedTrack(const std::string& id);
  // The cached track or a finished background render; nullptr otherwise
  // (never blocks).
  Track* readyTrack(const std::string& id);
  Track* cachedTrack(const std::string& id);
  // Caches a new track, evicting the least recently used past kKeepTracks
  // (never the one playing).
  Track* keepTrack(const std::string& id, std::unique_ptr<Track> tr);
  void startTrackLocked(const std::string& id, const Track* track, std::size_t pos);
  static std::unique_ptr<Track> synthTrack(const std::string& id);
  void mixLocked(float* out, int frames);
  void mixMusicLocked(float* out, int frames);
  void mixVoiceLocked(float* out, int frames);
  void mixSoundsLocked(float* out, int frames);

  const TrackRender mRenderTrack;
  const bool mDeferWithoutDevice; // see the test constructor
  // Indexed by Sfx; rendered ones never move, so a Voice plays from them.
  SoundBank<std::vector<float>> mSfx;
  std::unordered_map<std::string, std::unique_ptr<Track>> mNamedTracks;
  std::vector<std::string> mTrackOrder; // least recently used first
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
  std::size_t mMusicPos = 0;
  std::shared_ptr<const std::vector<float>> mVoiceClip;
  std::size_t mVoicePos = 0;
  float mVoiceVolume = 1.0f;
  float mDuck = 1.0f; // music gain, eased towards kDuckedMusic under a line
  std::mutex mMutex;
  SDL_AudioDeviceID mDevice = 0;
};

} // namespace gr
