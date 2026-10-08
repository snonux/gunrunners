#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gr
{

struct Vec2
{
  float x = 0.0f;
  float y = 0.0f;
};

struct Rect
{
  float x = 0.0f;
  float y = 0.0f;
  float w = 0.0f;
  float h = 0.0f;

  bool intersects(const Rect& o) const
  {
    return x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h;
  }

  float cx() const { return x + w * 0.5f; }
  float cy() const { return y + h * 0.5f; }
  float right() const { return x + w; }
  float bottom() const { return y + h; }
};

template <typename T>
T clampTo(T v, T lo, T hi)
{
  return std::min(std::max(v, lo), hi);
}

// Small deterministic xorshift RNG. Determinism matters: the headless
// recorder must produce the same clip on every run.
class Rng
{
public:
  explicit Rng(std::uint32_t seed = 0x2545F491u) : mState(seed ? seed : 1u) {}

  std::uint32_t next()
  {
    mState ^= mState << 13;
    mState ^= mState >> 17;
    mState ^= mState << 5;
    return mState;
  }

  float uniform() { return float(next() & 0xFFFFFFu) / float(0x1000000); }
  float range(float a, float b) { return a + (b - a) * uniform(); }
  int irange(int a, int b) { return a + int(next() % std::uint32_t(b - a + 1)); }

private:
  std::uint32_t mState;
};

inline std::uint32_t hash2(int x, int y)
{
  std::uint32_t h = std::uint32_t(x) * 374761393u + std::uint32_t(y) * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

} // namespace gr
