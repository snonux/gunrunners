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
  if (m == mMusic)
    return;
  mMusic = m;
  mMusicPos = 0;
}

void Audio::mix(float* out, int frames)
{
  std::lock_guard<std::mutex> lock(mMutex);
  mixLocked(out, frames);
}

void Audio::mixLocked(float* out, int frames)
{
  std::fill(out, out + frames * 2, 0.0f);

  const auto& track = mTracks[std::size_t(mMusic)];
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
    }
  }
  mVoices.erase(
    std::remove_if(mVoices.begin(), mVoices.end(), [](const Voice& v) { return v.pos >= v.data->size(); }),
    mVoices.end());

  for (int i = 0; i < frames * 2; ++i)
    out[i] = std::tanh(out[i]);
}

} // namespace gr
