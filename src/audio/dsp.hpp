#pragma once

// Building blocks for the procedural audio: band-limited oscillators, noise,
// filters and envelopes. Shared by synth.cpp (game sounds and music) and
// synth_named.cpp (cutscene sounds and voices).

#include "audio/audio.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace gr::synth
{

constexpr double kPi = 3.14159265358979;
constexpr double kRate = double(kAudioRate);

inline int samples(double seconds) { return int(seconds * kRate); }

inline double midiFreq(double note) { return 440.0 * std::pow(2.0, (note - 69.0) / 12.0); }

class Noise
{
public:
  explicit Noise(std::uint32_t seed) : mState(seed) {}
  float next()
  {
    mState ^= mState << 13;
    mState ^= mState >> 17;
    mState ^= mState << 5;
    return float(mState & 0xFFFFFF) / float(0x800000) - 1.0f;
  }

private:
  std::uint32_t mState;
};

inline double polyBlep(double t, double dt)
{
  if (t < dt)
  {
    t /= dt;
    return t + t - t * t - 1.0;
  }
  if (t > 1.0 - dt)
  {
    t = (t - 1.0) / dt;
    return t * t + t + t + 1.0;
  }
  return 0.0;
}

enum class Wave
{
  Sine,
  Triangle,
  Saw,
  Square,
};

// Band-limited oscillator (PolyBLEP), so the sounds stay clean and modern
// rather than buzzy.
class Osc
{
public:
  float step(double freq, Wave w, double duty = 0.5)
  {
    const double dt = std::min(0.45, freq / kRate);
    double out = 0.0;
    switch (w)
    {
      case Wave::Sine:
        out = std::sin(2.0 * kPi * mPhase);
        break;
      case Wave::Triangle:
        out = 1.0 - 4.0 * std::abs(mPhase - 0.5);
        break;
      case Wave::Saw:
        out = 2.0 * mPhase - 1.0 - polyBlep(mPhase, dt);
        break;
      case Wave::Square:
      {
        out = mPhase < duty ? 1.0 : -1.0;
        out += polyBlep(mPhase, dt);
        out -= polyBlep(std::fmod(mPhase + 1.0 - duty, 1.0), dt);
        break;
      }
    }
    mPhase += dt;
    if (mPhase >= 1.0)
      mPhase -= 1.0;
    return float(out);
  }
  void reset(double phase = 0.0) { mPhase = phase; }

private:
  double mPhase = 0.0;
};

class OnePole
{
public:
  float lowpass(float x, double cutoff)
  {
    const double a = 1.0 - std::exp(-2.0 * kPi * std::min(cutoff, kRate * 0.45) / kRate);
    mY += a * (double(x) - mY);
    return float(mY);
  }
  float highpass(float x, double cutoff) { return x - lowpass(x, cutoff); }

private:
  double mY = 0.0;
};

// Chamberlin state-variable filter: resonant low/band pass.
class Svf
{
public:
  float low(float x, double cutoff, double q)
  {
    process(x, cutoff, q);
    return float(mLow);
  }
  float band(float x, double cutoff, double q)
  {
    process(x, cutoff, q);
    return float(mBand);
  }

private:
  void process(float x, double cutoff, double q)
  {
    const double f = 2.0 * std::sin(kPi * std::min(cutoff, kRate / 6.5) / kRate);
    const double damp = 1.0 / std::max(0.5, q);
    mLow += f * mBand;
    const double high = double(x) - mLow - damp * mBand;
    mBand += f * high;
  }
  double mLow = 0.0;
  double mBand = 0.0;
};

inline double lerp(double a, double b, double t) { return a + (b - a) * t; }
// Exponential sweep from a to b over t in 0..1.
inline double sweep(double a, double b, double t) { return a * std::pow(b / a, std::clamp(t, 0.0, 1.0)); }
inline double decay(double t, double tau) { return std::exp(-t / tau); }
// Short linear attack to avoid clicks.
inline double attack(double t, double a = 0.003) { return std::min(1.0, t / a); }

template <typename F>
inline std::vector<float> render(double seconds, F f)
{
  std::vector<float> out(std::size_t(samples(seconds)));
  for (std::size_t i = 0; i < out.size(); ++i)
  {
    const double t = double(i) / kRate;
    out[i] = float(f(t, seconds));
  }
  // Fade the last 5 ms.
  const std::size_t fade = std::min<std::size_t>(out.size(), std::size_t(kRate * 0.005));
  for (std::size_t i = 0; i < fade; ++i)
    out[out.size() - 1 - i] *= float(i) / float(fade);
  return out;
}

// A note made of one oscillator with a plucky envelope; used for jingles.
inline void addTone(std::vector<float>& buf, double start, double len, double freq, Wave w, double vol, double tau)
{
  Osc o;
  const int s0 = samples(start);
  const int n = samples(len);
  for (int i = 0; i < n && s0 + i < int(buf.size()); ++i)
  {
    const double t = double(i) / kRate;
    const double env = attack(t, 0.004) * decay(t, tau) * std::min(1.0, (len - t) / 0.01);
    buf[std::size_t(s0 + i)] += float(o.step(freq, w) * env * vol);
  }
}


} // namespace gr::synth
