#include "audio/voice_clip.hpp"

#include "audio/audio.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdlib>

#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_PUSHDATA_API
#include "third_party/stb_vorbis.c"

namespace gr
{

bool readDataFile(const std::string& path, std::vector<unsigned char>& out)
{
  SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
  if (!rw)
    return false;
  const Sint64 size = SDL_RWsize(rw);
  out.resize(size > 0 ? std::size_t(size) : 0);
  const bool ok = size > 0 && SDL_RWread(rw, out.data(), 1, out.size()) == out.size();
  SDL_RWclose(rw);
  return ok;
}

namespace
{

// Linear resampling of mono floats to the mixer's rate.
std::vector<float> toMixRate(std::vector<float> mono, int rate)
{
  if (rate == kAudioRate || mono.empty())
    return mono;
  const double step = double(rate) / double(kAudioRate);
  const std::size_t n = std::size_t(double(mono.size()) / step);
  std::vector<float> out(n);
  for (std::size_t i = 0; i < n; ++i)
  {
    const double at = double(i) * step;
    const std::size_t a = std::size_t(at);
    const std::size_t b = a + 1 < mono.size() ? a + 1 : a;
    const float t = float(at - double(a));
    out[i] = mono[a] + (mono[b] - mono[a]) * t;
  }
  return out;
}

} // namespace

std::vector<float> decodeOggVorbis(const std::vector<unsigned char>& data)
{
  if (data.empty())
    return {};
  int channels = 0, rate = 0;
  short* pcm = nullptr;
  const int frames = stb_vorbis_decode_memory(data.data(), int(data.size()), &channels, &rate, &pcm);
  if (frames <= 0 || channels <= 0 || rate <= 0 || !pcm)
  {
    std::free(pcm);
    return {};
  }
  // Mixed down to mono.
  std::vector<float> mono(static_cast<std::size_t>(frames));
  for (int i = 0; i < frames; ++i)
  {
    int sum = 0;
    for (int c = 0; c < channels; ++c)
      sum += pcm[std::size_t(i) * std::size_t(channels) + std::size_t(c)];
    mono[std::size_t(i)] = float(sum) / (32768.0f * float(channels));
  }
  std::free(pcm);
  return toMixRate(std::move(mono), rate);
}

std::vector<float> decodeOggVorbisRange(const std::vector<unsigned char>& data, int start, int count)
{
  if (data.empty() || start < 0 || count <= 0)
    return {};
  int error = 0;
  stb_vorbis* v = stb_vorbis_open_memory(data.data(), int(data.size()), &error, nullptr);
  if (!v)
    return {};
  const int rate = int(stb_vorbis_get_info(v).sample_rate);
  std::vector<float> mono;
  if (stb_vorbis_seek(v, unsigned(start)))
  {
    mono.resize(std::size_t(count));
    // Channel 0 (the banks are mono).
    float* channel = mono.data();
    const int got = stb_vorbis_get_samples_float(v, 1, &channel, count);
    mono.resize(std::size_t(std::max(0, got)));
  }
  stb_vorbis_close(v);
  return toMixRate(std::move(mono), rate);
}

} // namespace gr
