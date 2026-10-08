#include "audio/audio.hpp"

#include "audio/synth.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gr
{

namespace
{

constexpr std::size_t kMaxVoices = 24;
constexpr int kMaxSameSound = 3;
constexpr float kMusicVolume = 0.55f;
constexpr float kSfxVolume = 0.8f;

} // namespace

Audio::Audio()
{
  for (int i = 0; i < int(Sfx::Count); ++i)
    mSfx[std::size_t(i)] = synth::makeSfx(Sfx(i));
  for (int i = 0; i < int(Music::Count); ++i)
  {
    auto t = synth::makeMusic(Music(i));
    mTracks[std::size_t(i)].left = std::move(t.left);
    mTracks[std::size_t(i)].right = std::move(t.right);
    mTracks[std::size_t(i)].loops = t.loops;
  }
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
  return true;
}

void Audio::callback(void* user, Uint8* stream, int len)
{
  auto* self = static_cast<Audio*>(user);
  self->mix(reinterpret_cast<float*>(stream), len / int(sizeof(float) * 2));
}

void Audio::play(Sfx s, float volume)
{
  std::lock_guard<std::mutex> lock(mMutex);
  const auto& data = mSfx[std::size_t(s)];
  if (data.empty())
    return;
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

void Audio::playMusic(Music m)
{
  std::lock_guard<std::mutex> lock(mMutex);
  if (m == mMusic && mTrackName.empty())
    return;
  mMusic = m;
  mTrack = &mTracks[std::size_t(m)];
  mTrackName.clear();
  mMusicPos = 0;
}

void Audio::playMusicNamed(const std::string& id)
{
  if (id == "stop" || id.empty())
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mMusic = Music::None;
    mTrack = nullptr;
    mTrackName = "stop";
    return;
  }
  {
    std::lock_guard<std::mutex> lock(mMutex);
    if (id == mTrackName)
      return;
  }
  // Synthesize outside the lock: the audio thread keeps playing meanwhile.
  Track* track = nullptr;
  auto it = mNamedTracks.find(id);
  if (it == mNamedTracks.end())
  {
    auto t = synth::makeNamedMusic(id);
    auto tr = std::make_unique<Track>();
    tr->left = std::move(t.left);
    tr->right = std::move(t.right);
    tr->loops = t.loops;
    track = tr.get();
    std::lock_guard<std::mutex> lock(mMutex);
    mNamedTracks[id] = std::move(tr);
  }
  else
  {
    track = it->second.get();
  }
  std::lock_guard<std::mutex> lock(mMutex);
  mTrack = track;
  mTrackName = id;
  mMusic = Music::None;
  mMusicPos = 0;
}

void Audio::seekMusic(double seconds)
{
  std::lock_guard<std::mutex> lock(mMutex);
  if (!mTrack || mTrack->left.empty())
    return;
  const std::size_t n = mTrack->left.size();
  mMusicPos = std::size_t(std::max(0.0, seconds) * kAudioRate) % n;
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
  v.loop = id.find("loop") != std::string::npos;
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

void Audio::mix(float* out, int frames)
{
  std::lock_guard<std::mutex> lock(mMutex);
  mixLocked(out, frames);
}

void Audio::mixLocked(float* out, int frames)
{
  std::fill(out, out + frames * 2, 0.0f);

  static const Track kSilence;
  const auto& track = mTrack ? *mTrack : kSilence;
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
      out[i * 2] += track.left[mMusicPos] * kMusicVolume;
      out[i * 2 + 1] += track.right[mMusicPos] * kMusicVolume;
      ++mMusicPos;
    }
  }

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

  for (int i = 0; i < frames * 2; ++i)
    out[i] = std::tanh(out[i]);
}

} // namespace gr
