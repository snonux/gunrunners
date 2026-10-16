#include "audio/audio.hpp"

#include "audio/synth.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>

namespace gr
{

namespace
{

constexpr std::size_t kMaxVoices = 24;
constexpr int kMaxSameSound = 3;
constexpr float kMusicVolume = 0.55f;
constexpr float kSfxVolume = 0.8f;
constexpr float kDuckedMusic = 0.4f;
// The duck eases over about 80 ms each way, so the music never clicks.
constexpr float kDuckStep = 1.0f / (0.08f * float(kAudioRate));
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
// A browser page without threads renders a sound or track when it first
// plays (no worker would ever finish it, so nothing can wait for one).
constexpr bool kBackgroundRender = false;
#else
constexpr bool kBackgroundRender = true;
#endif

// The id synth::makeNamedMusic renders each built-in track under.
const char* musicId(Music m)
{
  switch (m)
  {
    case Music::Menu:
      return "menu";
    case Music::Level:
      return "theme_synthwave";
    case Music::Victory:
      return "victory";
    case Music::None:
    case Music::Count:
      break;
  }
  return "stop";
}

} // namespace

// Renders nothing: the effects wait for openDevice (a window) or their
// first play (headless, see play), the tracks for theirs. The workers
// start idle.
Audio::Audio()
  : mSfx([](std::size_t i) { return synth::makeSfx(Sfx(i)); }, std::size_t(Sfx::Count), kBackgroundRender)
  , mPreloads(&Audio::synthTrack, kKeepPreloads, kBackgroundRender)
{
}

Audio::~Audio()
{
  if (mDevice)
    SDL_CloseAudioDevice(mDevice);
}

bool Audio::openDevice()
{
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
  {
    std::fprintf(stderr, "no audio: %s\n", SDL_GetError());
    return false;
  }
  SDL_AudioSpec want{};
  want.freq = kAudioRate;
  want.format = AUDIO_F32SYS;
  want.channels = 2;
  want.samples = 1024;
  want.callback = &Audio::callback;
  want.userdata = this;
  SDL_AudioSpec have{};
  mDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (!mDevice)
  {
    std::fprintf(stderr, "no audio device: %s\n", SDL_GetError());
    return false;
  }
  SDL_PauseAudioDevice(mDevice, 0);
  // Only a window plays sounds as they come: a headless recording renders
  // each on its first play instead, and would only burn a core on these.
  mSfx.prerender(sfxRenderOrder());
  return true;
}

std::vector<std::size_t> Audio::sfxRenderOrder()
{
  const Sfx first[] = {Sfx::MenuMove, Sfx::MenuSelect};
  std::vector<std::size_t> order;
  order.reserve(std::size_t(Sfx::Count));
  for (const Sfx s : first)
    order.push_back(std::size_t(s));
  for (std::size_t i = 0; i < std::size_t(Sfx::Count); ++i)
    if (std::find(std::begin(first), std::end(first), Sfx(i)) == std::end(first))
      order.push_back(i);
  return order;
}

void Audio::callback(void* user, Uint8* stream, int len)
{
  auto* self = static_cast<Audio*>(user);
  self->mix(reinterpret_cast<float*>(stream), len / int(sizeof(float) * 2));
}

void Audio::play(Sfx s, float volume)
{
  // Fetched before the lock: a render on the spot (headless) must not hold
  // up the mixer, and the bank never takes mMutex, so no lock order issue.
  const std::size_t i = std::size_t(s);
  const std::vector<float>* sound = deferRenders() ? mSfx.tryGet(i) : mSfx.get(i);
  if (!sound || sound->empty())
    return;
  const auto& data = *sound;
  std::lock_guard<std::mutex> lock(mMutex);
  // Rapid repeats of the same sound replace the oldest copy instead of
  // piling up.
  int same = 0;
  for (const auto& v : mVoices)
    if (v.id == s)
      ++same;
  if (same >= kMaxSameSound)
  {
    for (auto it = mVoices.begin(); it != mVoices.end(); ++it)
      if (it->id == s)
      {
        mVoices.erase(it);
        break;
      }
  }
  if (mVoices.size() >= kMaxVoices)
    mVoices.erase(mVoices.begin());
  mVoices.push_back({&data, 0, volume, s});
}

// As when these were rendered up front, asking for the track playing
// changes nothing and another one starts at its beginning; with a sound
// device a track not rendered yet starts once it is (the title's music a
// moment after the title, Music::Level ~3 s into a level without music=).
void Audio::playMusic(Music m)
{
  playMusicNamed(musicId(m));
}

void Audio::playMusicNamed(const std::string& id)
{
  if (id == "stop" || id.empty())
  {
    mPending.cancel();
    std::lock_guard<std::mutex> lock(mMutex);
    mTrack = nullptr;
    mTrackName = "stop";
    return;
  }
  // Already on its way: it keeps the seek it was given, and is wanted
  // again should a level start have marked its render stale since.
  if (id == mPending.id())
  {
    mPreloads.requestFirst(id);
    return;
  }
  mPending.cancel(); // the last music asked for wins, even the one playing
  {
    std::lock_guard<std::mutex> lock(mMutex);
    if (id == mTrackName)
      return;
  }
  // Rendered or collected outside the lock: the audio thread keeps playing
  // meanwhile. Only without deferRenders() does that block (1-4 s).
  Track* track = deferRenders() ? readyTrack(id) : namedTrack(id);
  if (!track)
  {
    // Not rendered yet: the music playing goes on until pollMusic sees it
    // done. Ahead of the preloads, which may be a level's worth of tracks.
    mPending.want(id);
    mPreloads.requestFirst(id);
    return;
  }
  std::lock_guard<std::mutex> lock(mMutex);
  startTrackLocked(id, track, 0);
}

void Audio::pollMusic()
{
  if (!mPending.active())
    return;
  const std::string id = mPending.id();
  Track* track = readyTrack(id);
  if (!track)
  {
    // Still queued or rendering, which this keeps at the front; or a level
    // start's markStale (or a drop) cancelled the request, which this
    // renews, so a pending track is never forgotten.
    mPreloads.requestFirst(id);
    return;
  }
  // The start frame is taken under the lock that starts it, so no frame
  // mixes in between and the beat lands where the caller's seek put it.
  std::lock_guard<std::mutex> lock(mMutex);
  startTrackLocked(id, track, mPending.startFrame(mPlayedFrames, track->left.size()));
  mPending.cancel();
}

void Audio::startTrackLocked(const std::string& id, const Track* track, std::size_t pos)
{
  mTrack = track;
  mTrackName = id;
  mMusicPos = pos;
}

// Only with a sound device and the background workers. The browser build
// without threads has no worker that would ever finish the render, so it
// renders on the spot. Headless recording (no device: the frontend mixes
// one video frame's worth per tick) does too, so the soundtrack is the
// same on every run instead of depending on how fast the workers were.
bool Audio::deferRenders() const
{
  return kBackgroundRender && mDevice != 0;
}

std::unique_ptr<Audio::Track> Audio::synthTrack(const std::string& id)
{
  auto t = synth::makeNamedMusic(id);
  auto tr = std::make_unique<Audio::Track>();
  tr->left = std::move(t.left);
  tr->right = std::move(t.right);
  tr->loops = t.loops;
  return tr;
}

Audio::Track* Audio::cachedTrack(const std::string& id)
{
  const auto it = mNamedTracks.find(id);
  return it != mNamedTracks.end() ? it->second.get() : nullptr;
}

Audio::Track* Audio::namedTrack(const std::string& id)
{
  if (Track* track = cachedTrack(id))
    return track;
  // A preload is collected (waiting for it if the worker is on it now);
  // otherwise, or when it is still queued, it is rendered here and now.
  std::unique_ptr<Track> tr = mPreloads.take(id);
  if (!tr)
    tr = synthTrack(id);
  return keepTrack(id, std::move(tr));
}

Audio::Track* Audio::readyTrack(const std::string& id)
{
  if (Track* track = cachedTrack(id))
    return track;
  std::unique_ptr<Track> tr = mPreloads.takeReady(id);
  return tr ? keepTrack(id, std::move(tr)) : nullptr;
}

// `id` is not cached yet (namedTrack and readyTrack look first), so no
// track the mixer may be playing is replaced.
Audio::Track* Audio::keepTrack(const std::string& id, std::unique_ptr<Track> tr)
{
  Track* track = tr.get();
  std::lock_guard<std::mutex> lock(mMutex);
  mNamedTracks[id] = std::move(tr);
  mTrackOrder.push_back(id);
  // A track is ~15 MB: keep the most recent few, never the one playing.
  for (std::size_t i = 0; mNamedTracks.size() > kKeepTracks && i < mTrackOrder.size();)
  {
    auto it = mNamedTracks.find(mTrackOrder[i]);
    if (it == mNamedTracks.end() || (it->second.get() != mTrack && it->second.get() != track))
    {
      if (it != mNamedTracks.end())
        mNamedTracks.erase(it);
      mTrackOrder.erase(mTrackOrder.begin() + long(i));
    }
    else
    {
      ++i;
    }
  }
  return track;
}

void Audio::preloadMusic(const std::string& id)
{
  // The queue skips ids it already has queued, rendering or finished.
  if (id.empty() || id == "stop" || mNamedTracks.count(id))
    return;
  mPreloads.request(id);
}

// A switch still waiting for its track is left to the level: playing the
// same track asks for it again (playMusicNamed), other music cancels it
// and its render goes with the other stale preloads (dropStalePreloads).
void Audio::markPreloadsStale()
{
  mPreloads.markStale();
}

void Audio::dropStalePreloads()
{
  mPreloads.dropStale();
}

void Audio::dropPreloads()
{
  mPreloads.drop();
}

void Audio::seekMusic(double seconds)
{
  const auto frame = std::uint64_t(std::max(0.0, seconds) * kAudioRate);
  std::lock_guard<std::mutex> lock(mMutex);
  if (mPending.active())
  {
    // Meant for the track on its way; the one playing until then is only
    // filling the gap.
    mPending.seek(frame, mPlayedFrames);
    return;
  }
  if (!mTrack || mTrack->left.empty())
    return;
  mMusicPos = std::size_t(frame % mTrack->left.size());
}

void Audio::playNamed(const std::string& id, float volume)
{
  auto it = mNamed.find(id);
  if (it == mNamed.end())
  {
    auto data = synth::makeNamedSfx(id);
    std::lock_guard<std::mutex> lock(mMutex);
    it = mNamed.emplace(id, std::move(data)).first;
  }
  std::lock_guard<std::mutex> lock(mMutex);
  if (mVoices.size() >= kMaxVoices)
    mVoices.erase(mVoices.begin());
  Voice v{&it->second, 0, volume, Sfx::Count};
  v.name = &it->first;
  // A TV left on a dead channel hisses until the cutscene stops it.
  v.loop = id.find("loop") != std::string::npos || id == "tv_static";
  mVoices.push_back(v);
}

void Audio::stopNamed(const std::string& id)
{
  std::lock_guard<std::mutex> lock(mMutex);
  mVoices.erase(std::remove_if(mVoices.begin(), mVoices.end(), [&](const Voice& v) { return v.name && *v.name == id; }),
    mVoices.end());
}

void Audio::stopAllNamed()
{
  std::lock_guard<std::mutex> lock(mMutex);
  mVoices.erase(std::remove_if(mVoices.begin(), mVoices.end(), [&](const Voice& v) { return v.name != nullptr; }),
    mVoices.end());
}

void Audio::voiceBlip(const std::string& speaker)
{
  const std::string key = "voice:" + speaker;
  auto it = mNamed.find(key);
  if (it == mNamed.end())
  {
    auto data = synth::makeVoiceBlip(speaker);
    std::lock_guard<std::mutex> lock(mMutex);
    it = mNamed.emplace(key, std::move(data)).first;
  }
  std::lock_guard<std::mutex> lock(mMutex);
  Voice v{&it->second, 0, 1.0f, Sfx::Count};
  v.name = &it->first;
  mVoices.push_back(v);
}

void Audio::playVoice(std::shared_ptr<const std::vector<float>> clip, float volume)
{
  std::lock_guard<std::mutex> lock(mMutex);
  mVoiceClip = std::move(clip);
  mVoicePos = 0;
  mVoiceVolume = volume;
}

void Audio::stopVoice()
{
  std::lock_guard<std::mutex> lock(mMutex);
  mVoiceClip.reset();
}

bool Audio::voicePlaying()
{
  std::lock_guard<std::mutex> lock(mMutex);
  return mVoiceClip != nullptr;
}

void Audio::mix(float* out, int frames)
{
  std::lock_guard<std::mutex> lock(mMutex);
  mixLocked(out, frames);
}

// Music, the spoken line and the sounds are summed, then soft-clipped.
void Audio::mixLocked(float* out, int frames)
{
  std::fill(out, out + frames * 2, 0.0f);
  mPlayedFrames += std::uint64_t(std::max(0, frames));
  mixMusicLocked(out, frames);
  mixVoiceLocked(out, frames);
  mixSoundsLocked(out, frames);
  for (int i = 0; i < frames * 2; ++i)
    out[i] = std::tanh(out[i]);
}

// The music, ducked (eased towards kDuckedMusic) under a spoken line.
void Audio::mixMusicLocked(float* out, int frames)
{
  static const Track kSilence;
  const auto& track = mTrack ? *mTrack : kSilence;
  const float duckTo = mVoiceClip && mVoiceVolume > 0.0f ? kDuckedMusic : 1.0f;
  float duck = mDuck;
  if (!track.left.empty())
  {
    for (int i = 0; i < frames; ++i)
    {
      if (mMusicPos >= track.left.size())
      {
        if (!track.loops)
          break;
        mMusicPos = 0;
      }
      duck += std::clamp(duckTo - duck, -kDuckStep, kDuckStep);
      out[i * 2] += track.left[mMusicPos] * kMusicVolume * duck;
      out[i * 2 + 1] += track.right[mMusicPos] * kMusicVolume * duck;
      ++mMusicPos;
    }
  }
  mDuck = track.left.empty() ? duckTo : duck;
}

void Audio::mixVoiceLocked(float* out, int frames)
{
  if (!mVoiceClip)
    return;
  const auto& d = *mVoiceClip;
  for (int i = 0; i < frames && mVoicePos < d.size(); ++i, ++mVoicePos)
  {
    const float s = d[mVoicePos] * mVoiceVolume;
    out[i * 2] += s;
    out[i * 2 + 1] += s;
  }
  if (mVoicePos >= d.size())
    mVoiceClip.reset();
}

// The sound effects and cutscene sounds; finished ones are removed.
void Audio::mixSoundsLocked(float* out, int frames)
{
  for (auto& v : mVoices)
  {
    const auto& d = *v.data;
    const float vol = v.volume * kSfxVolume;
    for (int i = 0; i < frames && v.pos < d.size(); ++i, ++v.pos)
    {
      out[i * 2] += d[v.pos] * vol;
      out[i * 2 + 1] += d[v.pos] * vol;
      if (v.loop && v.pos + 1 >= d.size())
        v.pos = std::size_t(-1); // wraps to 0 with the ++
    }
  }
  mVoices.erase(
    std::remove_if(mVoices.begin(), mVoices.end(), [](const Voice& v) { return v.pos >= v.data->size(); }),
    mVoices.end());
}

} // namespace gr
