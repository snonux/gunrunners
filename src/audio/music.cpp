// Level soundtracks. Every level names its own track (SPEC.md); each one is a
// small arrangement in a style of its own: tempo, metre, key, mode, chord
// progression, drum groove, bass line and a set of instruments, with a lead
// tune composed from a seeded motif. Everything is synthesized here, no
// samples. Tracks loop seamlessly: notes, echoes and reverb tails that run
// past the end wrap round to the start.

#include "audio/dsp.hpp"
#include "audio/synth.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace gr::synth
{

namespace
{

// --- Fast maths ----------------------------------------------------------------

// The arrangements run hundreds of voices per track, so the per-sample sines
// and envelopes use cheap approximations (errors far below hearing).
inline double fexp(double x)
{
  if (x < -40.0)
    return 0.0;
  const double y = x * 1.4426950408889634; // log2(e)
  const double fl = std::floor(y);
  const double f = y - fl;
  const double p = 1.0 + f * (0.6931471805599453 + f * (0.2402265069591007 + f * (0.0555041086648216 + f * (0.0096181291076285 + f * 0.0013333558146428))));
  return std::ldexp(p, int(fl));
}

inline double fdecay(double t, double tau) { return fexp(-t / tau); }

// sin(2 pi x) for x in cycles.
inline double sin2pi(double x)
{
  x -= std::floor(x);
  // Fold to [-0.25, 0.25] cycles.
  double y = x < 0.5 ? x : x - 1.0;
  if (y > 0.25)
    y = 0.5 - y;
  else if (y < -0.25)
    y = -0.5 - y;
  const double a = y * 2.0 * kPi;
  const double a2 = a * a;
  return a * (1.0 + a2 * (-1.0 / 6.0 + a2 * (1.0 / 120.0 + a2 * (-1.0 / 5040.0 + a2 * (1.0 / 362880.0)))));
}

// --- Mixing bus ----------------------------------------------------------------

struct Bus
{
  explicit Bus(double seconds)
    : n(std::max(1, samples(seconds))), l(std::size_t(n), 0.0f), r(l), rev(l), echo(l)
  {
  }
  void add(int i, float v, float pan, float revSend, float echoSend)
  {
    while (i >= n)
      i -= n;
    while (i < 0)
      i += n;
    const auto k = std::size_t(i);
    l[k] += v * (1.0f - std::max(0.0f, pan));
    r[k] += v * (1.0f + std::min(0.0f, pan));
    rev[k] += v * revSend;
    echo[k] += v * echoSend;
  }
  int n;
  std::vector<float> l, r, rev, echo;
};

// Where a voice goes on the bus.
struct Send
{
  float vol = 1.0f;
  float pan = 0.0f;
  float rev = 0.2f;
  float echo = 0.0f;
};

template <typename F>
void emit(Bus& b, double t0, double len, const Send& s, F f)
{
  const int s0 = samples(t0), n = samples(len), fade = samples(0.004);
  for (int i = 0; i < n; ++i)
  {
    double v = f(double(i) / kRate);
    if (n - i < fade)
      v *= double(n - i) / fade;
    b.add(s0 + i, float(v) * s.vol, s.pan, s.rev, s.echo);
  }
}

// Attack, decay to sustain, hold until `dur`, then release.
double adsr(double t, double dur, double a, double d, double s, double r)
{
  double e;
  if (t < a)
    e = t / a;
  else if (t < a + d)
    e = 1.0 - (1.0 - s) * (t - a) / d;
  else
    e = s;
  if (t > dur)
    e *= std::max(0.0, 1.0 - (t - dur) / r);
  return e;
}

// Control-rate time: modulated filter cutoffs change every 32 samples, so the
// filters' coefficients are only recomputed that often.
double ctl(double t) { return std::floor(t * 1500.0) / 1500.0; }

// Fades a decaying voice out over its last 60 ms.
double tail(double t, double len) { return std::min(1.0, (len - t) / 0.06); }

double vibrato(double t, double rate, double depth, double delay)
{
  return 1.0 + depth * std::min(1.0, std::max(0.0, t - delay) / 0.3) * sin2pi(rate * t);
}

// --- Instruments ---------------------------------------------------------------

enum class Inst
{
  None,
  Saw,       // detuned saw lead
  Square,    // pulse lead
  Chip,      // 8-bit pulse
  Pad,       // warm saw pad
  Marimba,
  Kalimba,
  Bell,      // FM bell
  Celesta,
  Vibes,
  Flute,
  Brass,     // horn section
  Trumpet,
  Tuba,
  Organ,
  Piano,
  EPiano,
  Pluck,     // Karplus-Strong string
  Dist,      // overdriven guitar
  Slide,     // slide guitar
  Theremin,
  Strings,
  Clarinet,
  Accordion,
  Choir,
  Glitch,
  SynthBass,
  Upright,
  SubBass,
};

struct Tone
{
  double t0;    // seconds
  double dur;   // seconds the key is held
  double note;  // MIDI
  double from;  // previous note (glides), 0 = none
  float vel;    // 0..1
};

void karplus(Bus& b, const Tone& n, const Send& s, double ring, double bright, double drive, std::uint32_t seed)
{
  const double f = midiFreq(n.note);
  const int period = std::max(2, int(kRate / f));
  std::vector<float> line(static_cast<std::size_t>(period));
  Noise noise(seed);
  OnePole shape;
  for (auto& x : line)
    x = shape.lowpass(noise.next(), 1500.0 + 9000.0 * bright);
  const double len = std::min(n.dur + 0.15, ring * 2.5);
  std::size_t at = 0;
  float prev = 0.0f;
  const float keep = float(std::pow(0.001, 1.0 / (ring * f)));
  emit(b, n.t0, len, s, [&](double t) {
    const float x = line[at];
    const float y = (x + prev) * 0.5f * keep;
    prev = x;
    line[at] = y;
    at = (at + 1) % line.size();
    double v = x * adsr(t, n.dur, 0.001, 0.0, 1.0, 0.12);
    if (drive > 0.0)
      v = std::tanh(v * drive) * 0.6;
    return v * n.vel;
  });
}

void play(Bus& b, Inst inst, const Tone& n, const Send& s, std::uint32_t seed)
{
  const double f = midiFreq(n.note);
  const double v = n.vel;
  switch (inst)
  {
    case Inst::None:
      return;
    case Inst::Saw:
    {
      Osc a, c;
      Svf lp;
      emit(b, n.t0, n.dur + 0.15, s, [&](double t) {
        const double ff = f * vibrato(t, 5.5, 0.005, 0.2);
        const double x = a.step(ff * 0.996, Wave::Saw) + c.step(ff * 1.004, Wave::Saw);
        return lp.low(float(x), 1800.0 + 2200.0 * fdecay(ctl(t), 0.15), 1.2) * 0.35 * adsr(t, n.dur, 0.01, 0.2, 0.75, 0.12) * v;
      });
      return;
    }
    case Inst::Square:
    {
      Osc a;
      OnePole lp;
      emit(b, n.t0, n.dur + 0.08, s, [&](double t) {
        const double x = a.step(f * vibrato(t, 5.0, 0.004, 0.25), Wave::Square, 0.32);
        return lp.lowpass(float(x), 3200.0) * 0.3 * adsr(t, n.dur, 0.004, 0.12, 0.6, 0.06) * v;
      });
      return;
    }
    case Inst::Chip:
    {
      Osc a;
      emit(b, n.t0, n.dur + 0.03, s, [&](double t) {
        const double duty = t < 0.05 ? 0.125 : 0.25;
        const double ff = f * (t > 0.15 ? 1.0 + 0.01 * sin2pi(7.0 * t) : 1.0);
        return a.step(ff, Wave::Square, duty) * 0.22 * adsr(t, n.dur, 0.0005, 0.06, 0.7, 0.03) * v;
      });
      return;
    }
    case Inst::Pad:
    {
      std::array<Osc, 4> o;
      OnePole lp1, lp2;
      emit(b, n.t0, n.dur + 0.5, s, [&](double t) {
        double x = 0.0;
        const double det[4] = {0.993, 0.998, 1.002, 1.007};
        for (int k = 0; k < 4; ++k)
          x += o[std::size_t(k)].step(f * det[k], Wave::Saw);
        const double cut = 700.0 + 900.0 * (0.5 + 0.5 * sin2pi((n.t0 * 0.4 + ctl(t) * 0.9) / (2.0 * kPi)));
        return lp2.lowpass(lp1.lowpass(float(x), cut), cut * 1.5) * 0.22 * adsr(t, n.dur, 0.35, 0.3, 0.85, 0.5) * v;
      });
      return;
    }
    case Inst::Marimba:
    {
      const double tau = 0.32 * std::pow(440.0 / f, 0.35);
      const double len = std::min({1.4, tau * 5.0, n.dur + 0.6});
      emit(b, n.t0, len, s, [&](double t) {
        const double w = f * t;
        return (sin2pi(w) * fdecay(t, tau) + 0.35 * sin2pi(w * 3.93) * fdecay(t, tau * 0.18) +
                0.12 * sin2pi(w * 9.8) * fdecay(t, 0.012)) *
          attack(t, 0.0015) * tail(t, len) * 0.55 * v;
      });
      return;
    }
    case Inst::Kalimba:
    {
      const double len = std::min(1.6, n.dur + 0.7);
      emit(b, n.t0, len, s, [&](double t) {
        const double w = f * t;
        return (sin2pi(w) * fdecay(t, 0.55) + 0.25 * sin2pi(w * 5.4) * fdecay(t, 0.05) +
                0.12 * sin2pi(w * 2.0) * fdecay(t, 0.25)) *
          attack(t, 0.001) * tail(t, len) * 0.5 * v;
      });
      return;
    }
    case Inst::Bell:
    {
      const double len = std::min(2.8, n.dur + 1.4);
      emit(b, n.t0, len, s, [&](double t) {
        const double w = f * t;
        const double m = 2.2 * fdecay(t, 0.5) * sin2pi(w * 3.5);
        return sin2pi(w + m / (2.0 * kPi)) * fdecay(t, 0.9) * attack(t, 0.001) * tail(t, len) * 0.32 * v;
      });
      return;
    }
    case Inst::Celesta:
    {
      const double len = std::min(1.4, n.dur + 0.8);
      emit(b, n.t0, len, s, [&](double t) {
        const double w = f * t;
        const double m = 1.4 * fdecay(t, 0.12) * sin2pi(w * 4.0);
        return sin2pi(w + m / (2.0 * kPi)) * fdecay(t, 0.45) * attack(t, 0.001) * tail(t, len) * 0.35 * v;
      });
      return;
    }
    case Inst::Vibes:
    {
      const double len = std::min(2.0, n.dur + 0.9);
      emit(b, n.t0, len, s, [&](double t) {
        const double w = f * t;
        const double trem = 1.0 + 0.3 * sin2pi(5.2 * t);
        return (sin2pi(w) + 0.18 * sin2pi(w * 4.0) * fdecay(t, 0.25)) * fdecay(t, 1.1) * trem * attack(t, 0.002) * tail(t, len) * 0.36 * v;
      });
      return;
    }
    case Inst::Flute:
    {
      Osc a;
      Noise noise(seed);
      Svf bp;
      emit(b, n.t0, n.dur + 0.1, s, [&](double t) {
        const double ff = f * vibrato(t, 5.0, 0.006, 0.15);
        const double tone = a.step(ff, Wave::Sine) + 0.12 * sin2pi(ff * 2.0 * t) + 0.04 * sin2pi(ff * 3.0 * t);
        const double breath = bp.band(noise.next(), ff * 2.0, 2.0) * (0.12 + 0.3 * fdecay(t, 0.05));
        return (tone + breath) * 0.32 * adsr(t, n.dur, 0.06, 0.1, 0.85, 0.08) * v;
      });
      return;
    }
    case Inst::Brass:
    case Inst::Trumpet:
    case Inst::Tuba:
    {
      Osc a, c;
      Svf lp;
      const bool tpt = inst == Inst::Trumpet;
      const double bright = tpt ? 9.0 : inst == Inst::Tuba ? 3.0 : 5.0;
      emit(b, n.t0, n.dur + 0.1, s, [&](double t) {
        const double scoop = std::pow(2.0, -0.25 * fdecay(t, 0.03) / 12.0);
        const double ff = f * scoop * vibrato(t, 5.2, tpt ? 0.007 : 0.003, 0.3);
        const double x = a.step(ff * 0.998, Wave::Saw) + c.step(ff * 1.002, tpt ? Wave::Square : Wave::Saw, 0.45) * 0.6;
        const double swell = std::min(1.0, ctl(t) / 0.05) * 0.6 + 0.4 * fdecay(ctl(t), 0.2);
        const double cut = f * (1.5 + bright * swell);
        return lp.low(float(x), cut, 0.9) * 0.3 * adsr(t, n.dur, 0.035, 0.15, 0.8, 0.09) * v;
      });
      return;
    }
    case Inst::Organ:
    {
      emit(b, n.t0, n.dur + 0.05, s, [&](double t) {
        const double w = f * t;
        const double trem = 1.0 + 0.12 * sin2pi(6.3 * t);
        const double x = sin2pi(w) + 0.7 * sin2pi(w * 2.0) + 0.45 * sin2pi(w * 3.0) + 0.3 * sin2pi(w * 4.0) +
          0.18 * sin2pi(w * 8.0) + 0.3 * sin2pi(w * 0.5);
        const double click = sin2pi(w * 12.0) * fdecay(t, 0.004) * 0.3;
        return (x * trem + click) * 0.13 * adsr(t, n.dur, 0.006, 0.0, 1.0, 0.04) * v;
      });
      return;
    }
    case Inst::Piano:
    {
      Noise noise(seed);
      const double len = std::min(n.dur + 0.3, 3.5);
      // Six slightly stretched partials, each with its own decay.
      std::array<double, 6> inc{}, phase{}, amp{}, keep{};
      for (int k = 1; k <= 6; ++k)
      {
        const auto i = std::size_t(k - 1);
        inc[i] = f * k * std::sqrt(1.0 + 0.0004 * k * k) / kRate;
        amp[i] = 1.0 / std::pow(k, 1.3);
        const double tau = 1.6 / std::pow(k, 0.7) * std::min(2.0, std::pow(262.0 / f, 0.5));
        keep[i] = std::exp(-1.0 / (tau * kRate));
      }
      double hammer = 0.3;
      const double hammerKeep = std::exp(-1.0 / (0.004 * kRate));
      emit(b, n.t0, len, s, [&](double t) {
        double x = 0.0;
        for (std::size_t i = 0; i < 6; ++i)
        {
          x += sin2pi(phase[i]) * amp[i];
          phase[i] += inc[i];
          amp[i] *= keep[i];
        }
        x += noise.next() * hammer;
        hammer *= hammerKeep;
        return x * 0.33 * attack(t, 0.001) * adsr(t, n.dur, 0.0, 0.0, 1.0, 0.25) * v;
      });
      return;
    }
    case Inst::EPiano:
    {
      const double len = std::min(n.dur + 0.3, 3.0);
      emit(b, n.t0, len, s, [&](double t) {
        const double w = f * t;
        const double m = (0.25 + 1.2 * fdecay(t, 0.2)) * sin2pi(w);
        const double tine = 0.06 * sin2pi(w * 14.0) * fdecay(t, 0.02);
        const double trem = 1.0 + 0.18 * sin2pi(4.6 * t);
        return (sin2pi(w + m / (2.0 * kPi)) + tine) * fdecay(t, 1.5) * trem * adsr(t, n.dur, 0.002, 0.0, 1.0, 0.2) * 0.38 * v;
      });
      return;
    }
    case Inst::Pluck:
      karplus(b, n, s, 0.9, 0.6, 0.0, seed);
      return;
    case Inst::Dist:
    {
      Osc a, c, d;
      OnePole lp, hp;
      emit(b, n.t0, n.dur + 0.08, s, [&](double t) {
        const double ff = f * vibrato(t, 5.8, 0.01, 0.35);
        const double x = a.step(ff * 0.997, Wave::Saw) + c.step(ff * 1.003, Wave::Square, 0.4) + 0.5 * d.step(ff * 0.5, Wave::Saw);
        const double y = std::tanh(x * 3.5) * (0.85 + 0.15 * fdecay(t, 0.1));
        return lp.lowpass(hp.highpass(float(y), 120.0), 3200.0) * 0.18 * adsr(t, n.dur, 0.004, 0.3, 0.8, 0.06) * v;
      });
      return;
    }
    case Inst::Slide:
    {
      Osc a, c;
      OnePole lp;
      const double start = n.from > 0.0 ? n.from : n.note - 2.0;
      emit(b, n.t0, n.dur + 0.15, s, [&](double t) {
        const double note = lerp(start, n.note, std::min(1.0, t / 0.11));
        const double ff = midiFreq(note) * vibrato(t, 5.0, 0.01, 0.2);
        const double x = a.step(ff, Wave::Saw) * 0.6 + c.step(ff, Wave::Triangle);
        return lp.lowpass(float(x), 1900.0 + 1500.0 * fdecay(ctl(t), 0.2)) * 0.36 * adsr(t, n.dur, 0.01, 0.4, 0.6, 0.15) * v;
      });
      return;
    }
    case Inst::Theremin:
    {
      Osc a, c;
      const double start = n.from > 0.0 ? n.from : n.note;
      emit(b, n.t0, n.dur + 0.2, s, [&](double t) {
        const double note = lerp(start, n.note, std::min(1.0, t / 0.16));
        const double ff = midiFreq(note) * vibrato(t, 6.2, 0.014, 0.05);
        return (a.step(ff, Wave::Sine) + 0.12 * c.step(ff, Wave::Triangle)) * 0.34 * adsr(t, n.dur, 0.07, 0.1, 0.9, 0.2) * v;
      });
      return;
    }
    case Inst::Strings:
    {
      std::array<Osc, 4> o;
      OnePole lp1, lp2;
      const bool shortNote = n.dur < 0.3;
      emit(b, n.t0, n.dur + (shortNote ? 0.08 : 0.35), s, [&](double t) {
        const double ff = f * vibrato(t, 5.0, 0.004, 0.15);
        const double det[4] = {0.996, 0.999, 1.002, 1.005};
        double x = 0.0;
        for (int k = 0; k < 4; ++k)
          x += o[std::size_t(k)].step(ff * det[k], Wave::Saw);
        const double env = shortNote ? adsr(t, n.dur, 0.01, 0.05, 0.7, 0.06) : adsr(t, n.dur, 0.18, 0.2, 0.85, 0.3);
        return lp2.lowpass(lp1.lowpass(float(x), 2600.0), 3800.0) * 0.16 * env * v;
      });
      return;
    }
    case Inst::Clarinet:
    {
      Osc a, c;
      OnePole lp;
      emit(b, n.t0, n.dur + 0.06, s, [&](double t) {
        const double ff = f * vibrato(t, 4.8, 0.004, 0.3);
        const double x = a.step(ff, Wave::Square, 0.5) * 0.7 + c.step(ff, Wave::Sine) * 0.5;
        return lp.lowpass(float(x), f * 4.5) * 0.3 * adsr(t, n.dur, 0.035, 0.1, 0.9, 0.06) * v;
      });
      return;
    }
    case Inst::Accordion:
    {
      Osc a, c, d;
      OnePole lp;
      emit(b, n.t0, n.dur + 0.06, s, [&](double t) {
        const double x = a.step(f * 0.997, Wave::Square, 0.4) + c.step(f * 1.006, Wave::Saw) * 0.8 + d.step(f * 2.002, Wave::Square, 0.3) * 0.3;
        return lp.lowpass(float(x), 3000.0) * 0.17 * adsr(t, n.dur, 0.03, 0.1, 0.9, 0.05) * v;
      });
      return;
    }
    case Inst::Choir:
    {
      std::array<Osc, 3> o;
      Svf f1, f2, f3;
      emit(b, n.t0, n.dur + 0.4, s, [&](double t) {
        const double ff = f * vibrato(t, 5.0, 0.005, 0.2);
        const double x = o[0].step(ff * 0.995, Wave::Saw) + o[1].step(ff, Wave::Saw) + o[2].step(ff * 1.005, Wave::Saw);
        const double y = f1.band(float(x), 730.0, 6.0) + 0.7 * f2.band(float(x), 1090.0, 7.0) + 0.25 * f3.band(float(x), 2440.0, 8.0);
        return y * 0.28 * adsr(t, n.dur, 0.2, 0.2, 0.9, 0.4) * v;
      });
      return;
    }
    case Inst::Glitch:
    {
      Osc a;
      Noise noise(seed);
      const int hold = 6 + int(seed % 14u);
      int count = 0;
      double held = 0.0;
      const double oct = (seed >> 4) % 3 == 0 ? 2.0 : 1.0;
      emit(b, n.t0, n.dur + 0.02, s, [&](double t) {
        const double x = a.step(f * oct, Wave::Square, 0.25) + noise.next() * 0.05;
        if (count-- <= 0)
        {
          held = std::round(x * 4.0) / 4.0;
          count = hold;
        }
        return held * 0.2 * adsr(t, n.dur, 0.001, 0.04, 0.6, 0.02) * v;
      });
      return;
    }
    case Inst::SynthBass:
    {
      Osc a, c;
      Svf lp;
      emit(b, n.t0, n.dur + 0.04, s, [&](double t) {
        const double x = a.step(f, Wave::Saw) * 0.6 + c.step(f * 1.004, Wave::Square) * 0.4;
        return lp.low(float(x), 180.0 + 1500.0 * fdecay(ctl(t), 0.08), 2.0) * 0.5 * adsr(t, n.dur, 0.003, 0.12, 0.65, 0.03) * v;
      });
      return;
    }
    case Inst::Upright:
    {
      Noise noise(seed);
      OnePole lp;
      emit(b, n.t0, n.dur + 0.08, s, [&](double t) {
        const double ff = f * (1.0 + 0.01 * fdecay(t, 0.02));
        const double w = ff * t;
        const double x = sin2pi(w) + 0.35 * (1.0 - 4.0 * std::abs(std::fmod(ff * t, 1.0) - 0.5)) * fdecay(t, 0.12) +
          lp.lowpass(noise.next(), 900.0) * fdecay(t, 0.012) * 2.0;
        return x * 0.55 * fdecay(t, 0.9) * adsr(t, n.dur, 0.002, 0.0, 1.0, 0.06) * v;
      });
      return;
    }
    case Inst::SubBass:
    {
      emit(b, n.t0, n.dur + 0.05, s, [&](double t) {
        const double x = sin2pi(f * t);
        return std::tanh(x * 1.6) * 0.5 * adsr(t, n.dur, 0.006, 0.0, 1.0, 0.05) * v;
      });
      return;
    }
  }
}

// --- Percussion ----------------------------------------------------------------

enum class Perc
{
  Kick,
  Kick808,
  Snare,
  Clap,
  Rim,
  HatC,
  HatO,
  Ride,
  Crash,
  Shaker,
  Tom,      // pitch from the pattern: h, m, l
  Taiko,
  Timpani,
  Brush,
  Block,    // wood block, h or l
  Cowbell,
  Conga,    // h or l
  Tamb,
  Typer,    // typewriter key
  Ding,     // typewriter bell
  Blip,     // glitch blip
  Chop,     // rotor blade
};

void hit(Bus& b, Perc p, double t0, float vol, char ch, float pan, std::uint32_t seed)
{
  Send s{vol, pan, 0.08f, 0.0f};
  Noise noise(seed);
  switch (p)
  {
    case Perc::Kick:
    {
      double ph = 0.0;
      emit(b, t0, 0.35, s, [&](double t) {
        ph += (48.0 + 120.0 * fdecay(t, 0.03)) / kRate;
        return sin2pi(ph) * fdecay(t, 0.13) * attack(t, 0.001) * 0.95;
      });
      return;
    }
    case Perc::Kick808:
    {
      double ph = 0.0;
      emit(b, t0, 0.9, s, [&](double t) {
        ph += (42.0 + 90.0 * fdecay(t, 0.04)) / kRate;
        return std::tanh(sin2pi(ph) * 1.5) * fdecay(t, 0.4) * attack(t, 0.001) * 0.8;
      });
      return;
    }
    case Perc::Snare:
    {
      Svf bp;
      Osc o;
      emit(b, t0, 0.28, s, [&](double t) {
        return (bp.band(noise.next(), 2600.0, 0.8) * fdecay(t, 0.07) + o.step(185.0, Wave::Triangle) * fdecay(t, 0.04) * 0.5) * 0.65;
      });
      return;
    }
    case Perc::Clap:
    {
      Svf bp;
      emit(b, t0, 0.3, s, [&](double t) {
        const double bursts = t < 0.03 ? (std::fmod(t, 0.01) < 0.004 ? 1.0 : 0.3) : fdecay(t - 0.03, 0.08);
        return bp.band(noise.next(), 1400.0, 1.2) * bursts * 0.8;
      });
      return;
    }
    case Perc::Rim:
    {
      emit(b, t0, 0.06, s, [&](double t) {
        return (sin2pi(1700.0 * t) * 0.6 + noise.next() * 0.3) * fdecay(t, 0.01) * 0.6;
      });
      return;
    }
    case Perc::HatC:
    case Perc::HatO:
    {
      OnePole hp;
      const double tau = p == Perc::HatO ? 0.09 : 0.018;
      s.pan = pan - 0.2f;
      emit(b, t0, tau * 4.0, s, [&](double t) { return hp.highpass(noise.next(), 7000.0) * fdecay(t, tau) * 0.5; });
      return;
    }
    case Perc::Ride:
    {
      OnePole hp;
      emit(b, t0, 0.8, s, [&](double t) {
        const double ping = sin2pi(3150.0 * t + 2.0 * sin2pi(4420.0 * t));
        return (hp.highpass(noise.next(), 6000.0) * 0.35 + ping * 0.25) * fdecay(t, 0.35) * 0.4;
      });
      return;
    }
    case Perc::Crash:
    {
      OnePole hp;
      s.rev = 0.25f;
      emit(b, t0, 2.0, s, [&](double t) { return hp.highpass(noise.next(), 4500.0) * fdecay(t, 0.6) * attack(t, 0.002) * 0.45; });
      return;
    }
    case Perc::Shaker:
    {
      Svf bp;
      emit(b, t0, 0.08, s, [&](double t) {
        return bp.band(noise.next(), 6500.0, 1.5) * std::min(1.0, t / 0.012) * fdecay(t, 0.025) * 0.6;
      });
      return;
    }
    case Perc::Tom:
    case Perc::Taiko:
    case Perc::Timpani:
    case Perc::Conga:
    {
      double base = ch == 'h' ? 190.0 : ch == 'l' ? 95.0 : 135.0;
      double tau = 0.18, bend = 0.5, body = 1.0;
      if (p == Perc::Taiko)
      {
        base *= 0.42;
        tau = 0.35;
        bend = 0.8;
        s.rev = 0.3f;
      }
      else if (p == Perc::Timpani)
      {
        base = ch == 'h' ? 110.0 : 82.0;
        tau = 0.6;
        bend = 0.05;
        s.rev = 0.3f;
      }
      else if (p == Perc::Conga)
      {
        base = ch == 'h' ? 330.0 : 220.0;
        tau = 0.1;
        bend = 0.15;
        body = 0.8;
      }
      double ph = 0.0;
      OnePole lp;
      emit(b, t0, tau * 3.0, s, [&](double t) {
        ph += base * (1.0 + bend * fdecay(t, 0.05)) / kRate;
        const double skin = lp.lowpass(noise.next(), 1200.0) * fdecay(t, 0.02);
        return (sin2pi(ph) * body + skin) * fdecay(t, tau) * attack(t, 0.001) * 0.8;
      });
      return;
    }
    case Perc::Brush:
    {
      Svf bp;
      emit(b, t0, 0.3, s, [&](double t) {
        return bp.band(noise.next(), 3500.0, 0.6) * std::min(1.0, t / 0.05) * fdecay(t, 0.09) * 0.35;
      });
      return;
    }
    case Perc::Block:
    {
      const double f = ch == 'l' ? 820.0 : 1250.0;
      emit(b, t0, 0.12, s, [&](double t) {
        return (sin2pi(f * t) + 0.4 * sin2pi(f * 2.7 * t)) * fdecay(t, 0.025) * attack(t, 0.0005) * 0.6;
      });
      return;
    }
    case Perc::Cowbell:
    {
      Osc a, c;
      Svf bp;
      emit(b, t0, 0.3, s, [&](double t) {
        const double x = a.step(560.0, Wave::Square) + c.step(845.0, Wave::Square);
        return bp.band(float(x), 900.0, 2.0) * fdecay(t, 0.08) * 0.35;
      });
      return;
    }
    case Perc::Tamb:
    {
      OnePole hp;
      emit(b, t0, 0.2, s, [&](double t) {
        const double jingle = 0.6 + 0.4 * sin2pi(70.0 * t);
        return hp.highpass(noise.next(), 8000.0) * jingle * fdecay(t, 0.05) * 0.6;
      });
      return;
    }
    case Perc::Typer:
    {
      Svf bp;
      const double f = 1800.0 + double(seed % 900u);
      emit(b, t0, 0.05, s, [&](double t) {
        return (bp.band(noise.next(), f, 3.0) + 0.5 * sin2pi(240.0 * t)) * fdecay(t, 0.008) * 0.8;
      });
      return;
    }
    case Perc::Ding:
    {
      s.rev = 0.3f;
      emit(b, t0, 1.2, s, [&](double t) {
        return (sin2pi(2093.0 * t) + 0.3 * sin2pi(5230.0 * t) * fdecay(t, 0.1)) * fdecay(t, 0.4) * 0.3;
      });
      return;
    }
    case Perc::Blip:
    {
      const double f = 400.0 * std::pow(2.0, double(seed % 24u) / 6.0);
      const int hold = 4 + int((seed >> 5) % 10u);
      int count = 0;
      double held = 0.0;
      Osc o;
      emit(b, t0, 0.07, s, [&](double t) {
        const double x = o.step(f * (1.0 + 2.0 * t), Wave::Square);
        if (count-- <= 0)
        {
          held = x;
          count = hold;
        }
        return held * 0.25;
      });
      return;
    }
    case Perc::Chop:
    {
      Svf lp;
      emit(b, t0, 0.12, s, [&](double t) {
        return lp.low(noise.next(), 260.0, 1.5) * std::sin(kPi * std::min(1.0, t / 0.12)) * 1.6;
      });
      return;
    }
  }
}

// --- Patterns ------------------------------------------------------------------

// One drum lane: a step pattern ('x' loud, 'o' medium, '-' ghost, letters
// for pitched hits, '.' rest) looped over the bar, optionally only every Nth
// bar.
struct Lane
{
  Perc perc;
  const char* pattern;
  float vol;
  float pan;
  int every = 1;
};

enum class Groove
{
  None,
  Four,
  House,
  Rock,
  Driving,
  Break,
  Half,
  Jazz,
  BigBand,
  Bossa,
  Jungle,
  Creep,
  March,
  Panic,
  War,
  Waltz,
  Dub,
  Airy,
  Gallop,
  Rotor,
  Glitch,
  Typewriter,
  Arena,
  Storm,
  Toon,
  Blocks,
  Deep,
  Heart,
  Pulse,
  Desert,
  Shimmer,
};

struct GrooveDef
{
  std::array<Lane, 6> lanes;
  int count;
  bool fills;
};

GrooveDef grooveDef(Groove g)
{
  using P = Perc;
  switch (g)
  {
    case Groove::None:
      return {{}, 0, false};
    case Groove::Four:
      return {{{{P::Kick, "x...x...x...x...", 1.0f, 0.0f}, {P::Clap, "....x.......x...", 0.5f, 0.1f},
                {P::HatC, "-.x.-.x.-.x.-.x.", 0.35f, -0.3f}, {P::HatO, "..............x.", 0.25f, -0.3f, 2}}},
              4, true};
    case Groove::House:
      return {{{{P::Kick, "x...x...x...x...", 1.0f, 0.0f}, {P::Clap, "....x.......x...", 0.55f, 0.1f},
                {P::HatO, "..x...x...x...x.", 0.35f, -0.3f}, {P::Shaker, "-o-o-o-o-o-o-o-o", 0.3f, 0.35f}}},
              4, false};
    case Groove::Rock:
      return {{{{P::Kick, "x.....x.x.......", 1.0f, 0.0f}, {P::Snare, "....x.......x...", 0.8f, 0.05f},
                {P::HatC, "x.x.x.x.x.x.x.x.", 0.4f, -0.3f}, {P::Crash, "x...............", 0.5f, 0.3f, 4}}},
              4, true};
    case Groove::Driving:
      return {{{{P::Kick, "x...x...x...x...", 1.0f, 0.0f}, {P::Snare, "....x.......x...", 0.7f, 0.05f},
                {P::HatC, "xoxoxoxoxoxoxoxo", 0.4f, -0.3f}, {P::Crash, "x...............", 0.4f, 0.3f, 8}}},
              4, true};
    case Groove::Break:
      return {{{{P::Kick, "x.........x.....", 1.0f, 0.0f}, {P::Snare, "....x..-.-..x..-", 0.75f, 0.05f},
                {P::HatC, "x.x.x.x.x.x.x.x.", 0.35f, -0.3f}}},
              3, true};
    case Groove::Half:
      return {{{{P::Kick, "x.......-.x.....", 1.0f, 0.0f}, {P::Snare, "........x.......", 0.8f, 0.05f},
                {P::HatC, "x...x...x...x...", 0.35f, -0.3f}, {P::Tom, "............l.l.", 0.5f, 0.3f, 2}}},
              4, false};
    case Groove::Jazz:
      return {{{{P::Ride, "x...x.x.x...x.x.", 0.5f, 0.3f}, {P::HatC, "....x.......x...", 0.35f, -0.3f},
                {P::Kick, "x.........-.....", 0.45f, 0.0f}, {P::Brush, "x...x...x...x...", 0.5f, -0.1f}}},
              4, false};
    case Groove::BigBand:
      return {{{{P::Ride, "x...x.x.x...x.x.", 0.55f, 0.3f}, {P::HatC, "....x.......x...", 0.4f, -0.3f},
                {P::Kick, "x...x...x...x...", 0.4f, 0.0f}, {P::Snare, "......-.....-..o", 0.4f, 0.05f},
                {P::Crash, "x...............", 0.35f, 0.3f, 4}}},
              5, true};
    case Groove::Bossa:
      return {{{{P::Kick, "x.....x.x.....x.", 0.7f, 0.0f}, {P::Rim, "x..x..x...x..x..", 0.5f, 0.2f},
                {P::Shaker, "xooxxooxxooxxoox", 0.3f, -0.3f}}},
              3, false};
    case Groove::Jungle:
      return {{{{P::Conga, "h..hl.h.h..hl.l.", 0.6f, 0.25f}, {P::Shaker, "x-o-x-o-x-o-x-o-", 0.35f, -0.35f},
                {P::Kick, "x.......x.......", 0.7f, 0.0f}, {P::Block, "....h.......h...", 0.3f, -0.2f}}},
              4, false};
    case Groove::Creep:
      return {{{{P::Tom, "l.....l...m.....", 0.7f, 0.15f}, {P::Rim, "x.......x.......", 0.3f, -0.25f},
                {P::Kick, "x.........x.....", 0.7f, 0.0f}, {P::Shaker, "..-...-...-...-.", 0.3f, 0.3f}}},
              4, false};
    case Groove::March:
      return {{{{P::Snare, "x.-.x.--x.-.x.--", 0.6f, 0.05f}, {P::Kick, "x.......x.......", 0.8f, 0.0f},
                {P::Crash, "x...............", 0.35f, 0.3f, 4}}},
              3, true};
    case Groove::Panic:
      return {{{{P::Timpani, "h.l.h.l.h...h.l.", 0.7f, 0.0f}, {P::Snare, "xoxoxoxoxoxoxoxo", 0.4f, 0.1f},
                {P::Crash, "x...............", 0.4f, 0.3f, 2}, {P::Kick, "x...x...x...x...", 0.6f, 0.0f}}},
              4, false};
    case Groove::War:
      return {{{{P::Taiko, "x...x...x...x...", 0.9f, 0.0f}, {P::Tom, "......m.....l.l.", 0.55f, 0.25f},
                {P::Snare, "--x---x---x-xoxx", 0.3f, -0.15f}, {P::Taiko, "..l.......l.....", 0.5f, -0.3f}}},
              4, false};
    case Groove::Waltz:
      return {{{{P::Kick, "x...........", 0.75f, 0.0f}, {P::Brush, "....o...o...", 0.6f, 0.1f},
                {P::HatC, "x.-.x.-.x.-.", 0.25f, -0.3f}}},
              3, false};
    case Groove::Dub:
      return {{{{P::Kick, "x.......x.......", 1.0f, 0.0f}, {P::Rim, "........x.......", 0.6f, 0.15f},
                {P::HatC, "..x...x...x...x.", 0.35f, -0.3f}, {P::HatO, "..............x.", 0.25f, -0.3f, 2}}},
              4, false};
    case Groove::Airy:
      return {{{{P::Kick, "x.........x.....", 0.6f, 0.0f}, {P::Rim, "....x.......x...", 0.35f, 0.15f},
                {P::Shaker, "-.-.-.-.-.-.-.-.", 0.35f, -0.3f}}},
              3, false};
    case Groove::Gallop:
      return {{{{P::Kick, "x.......x.......", 0.8f, 0.0f}, {P::Block, "h.lhh.lhh.lhh.lh", 0.45f, 0.25f},
                {P::Snare, "....x.......x...", 0.45f, 0.05f}, {P::Tamb, "..x...x...x...x.", 0.3f, -0.3f}}},
              4, false};
    case Groove::Rotor:
      return {{{{P::Kick, "x...x...x...x...", 1.0f, 0.0f}, {P::Snare, "....x.......x...", 0.7f, 0.05f},
                {P::Chop, "xoxoxoxoxoxoxoxo", 0.6f, -0.2f}, {P::HatO, "..x...x...x...x.", 0.2f, 0.3f}}},
              4, true};
    case Groove::Glitch:
      return {{{{P::Kick, "x..x......x..x..", 1.0f, 0.0f}, {P::Snare, "....x.......x.x.", 0.7f, 0.05f},
                {P::Blip, "..x..xx...x.x..x", 0.5f, 0.4f}, {P::HatC, "-o-o-o-o-o-o-o-o", 0.3f, -0.35f}}},
              4, false};
    case Groove::Typewriter:
      return {{{{P::Typer, "x.x.xx.x.x.xx.xx", 0.5f, 0.3f}, {P::Kick, "x.......x.......", 0.6f, 0.0f},
                {P::Brush, "....x.......x...", 0.5f, -0.1f}, {P::Ding, "..............x.", 0.5f, 0.4f, 2}}},
              4, false};
    case Groove::Arena:
      return {{{{P::Kick, "x.x.....x.x.....", 1.0f, 0.0f}, {P::Clap, "....x.......x...", 0.7f, 0.0f},
                {P::Snare, "....x.......x...", 0.5f, 0.05f}, {P::HatC, "x.x.x.x.x.x.x.x.", 0.3f, -0.3f},
                {P::Crash, "x...............", 0.45f, 0.3f, 4}}},
              5, true};
    case Groove::Storm:
      return {{{{P::Kick, "x.x.x.x.x.x.x.x.", 0.85f, 0.0f}, {P::Snare, "....x.......x...", 0.85f, 0.05f},
                {P::Ride, "x.x.x.x.x.x.x.x.", 0.3f, 0.3f}, {P::Crash, "x...............", 0.5f, -0.3f, 2}}},
              4, true};
    case Groove::Toon:
      return {{{{P::Block, "h...l...h...l...", 0.5f, 0.3f}, {P::Snare, "....x.......x...", 0.45f, 0.05f},
                {P::Kick, "x.......x.......", 0.6f, 0.0f}, {P::Ride, "x..x.xx..x.xx..x", 0.25f, -0.3f}}},
              4, false};
    case Groove::Blocks:
      return {{{{P::Block, "h.l.h.hl.lh.l.h.", 0.55f, 0.3f}, {P::Kick, "x.......x..x....", 0.8f, 0.0f},
                {P::Shaker, "-o-o-o-o-o-o-o-o", 0.3f, -0.3f}, {P::Cowbell, "x...............", 0.25f, -0.4f, 2}}},
              4, false};
    case Groove::Deep:
      return {{{{P::Kick808, "x.......x..x....", 1.0f, 0.0f}, {P::Tom, "....l.......l..l", 0.6f, 0.2f},
                {P::Snare, "........x.......", 0.6f, 0.05f}, {P::HatC, "x.x.x.x.x.x.x.x.", 0.2f, -0.3f}}},
              4, false};
    case Groove::Heart:
      return {{{{P::HatC, "..x...x...x...x.", 0.25f, -0.3f}, {P::Kick, "x.......x.......", 0.5f, 0.0f}}}, 2, false};
    case Groove::Pulse:
      return {{{{P::Kick, "x...x...x...x...", 1.0f, 0.0f}, {P::Snare, "....x.......x...", 0.6f, 0.05f},
                {P::HatO, "..x...x...x...x.", 0.3f, -0.3f}, {P::Shaker, "xoxoxoxoxoxoxoxo", 0.25f, 0.35f}}},
              4, true};
    case Groove::Desert:
      return {{{{P::Conga, "h..l..h.h.l.l...", 0.6f, 0.2f}, {P::Tamb, "x.-.x.-.x.-.x.-.", 0.35f, -0.3f},
                {P::Kick, "x.....x.........", 0.7f, 0.0f}}},
              3, false};
    case Groove::Shimmer:
      return {{{{P::Shaker, "-.-.-.-.-.-.-.-.", 0.3f, 0.3f}, {P::Rim, "............x...", 0.25f, -0.2f, 2}}}, 2, false};
  }
  return {{}, 0, false};
}

// Bass lines per bar: 'r' root, 'o' octave, 'f' fifth, 't' third, 'l' fifth
// below, 'a' a step into the next chord's root; '-' holds, '.' rests.
// "w" alone means walk.
enum class BassPat
{
  Roots,
  Halves,
  Octaves,
  Pedal,
  Pulse16,
  Walking,
  Dub,
  Funk,
  Gallop,
  Oompah,
  Waltz,
  Rock,
  Bossa,
  Rolling,
  Halftime,
};

const char* bassPattern(BassPat p, int meter)
{
  if (meter == 3)
  {
    switch (p)
    {
      case BassPat::Walking:
        return "r---t---f---";
      case BassPat::Octaves:
      case BassPat::Pedal:
        return "r-r-o-r-o-r-";
      case BassPat::Roots:
        return "r-----------";
      default:
        return "r---....f---";
    }
  }
  switch (p)
  {
    case BassPat::Roots:
      return "r---------------";
    case BassPat::Halves:
      return "r-------f-------";
    case BassPat::Octaves:
      return "r-o-r-o-r-o-r-o-";
    case BassPat::Pedal:
      return "r.r.r.r.r.r.r.r.";
    case BassPat::Pulse16:
      return "rrrrrrrrrrrrrrrr";
    case BassPat::Walking:
      return "r---t---f---a---";
    case BassPat::Dub:
      return "r-.r..f-r-....t.";
    case BassPat::Funk:
      return "r..r..o.r.r..o.a";
    case BassPat::Gallop:
      return "r.rrr.rrr.rrr.rr";
    case BassPat::Oompah:
      return "r---....l---....";
    case BassPat::Waltz:
      return "r---....f---....";
    case BassPat::Rock:
      return "r.r.r.r.r.r.r.ra";
    case BassPat::Bossa:
      return "r-----f-r-----f-";
    case BassPat::Rolling:
      return "r-t-f-o-r-t-f-o-";
    case BassPat::Halftime:
      return "r-------....r-..";
  }
  return "r---------------";
}

// Chord comping: 'x' strikes the chord, '-' holds, '.' rests.
enum class Comp
{
  None,
  Sustain,
  Stabs,
  Skank,
  Pulse8,
  Charleston,
  Waltz,
  Rock8,
  Bossa,
  Strum,
  Halves,
};

const char* compPattern(Comp c, int meter)
{
  if (meter == 3)
    return c == Comp::Sustain ? "x-----------" : c == Comp::Pulse8 ? "x-x-x-x-x-x-" : "....x-..x-..";
  switch (c)
  {
    case Comp::None:
      return "";
    case Comp::Sustain:
      return "x---------------";
    case Comp::Stabs:
      return "..x...x...x...x.";
    case Comp::Skank:
      return "..x-..x-..x-..x-";
    case Comp::Pulse8:
      return "x.x.x.x.x.x.x.x.";
    case Comp::Charleston:
      return "x--.......x-....";
    case Comp::Waltz:
      return "....x-..x-......";
    case Comp::Rock8:
      return "x-x-x-x-x-x-x-x-";
    case Comp::Bossa:
      return "x--x--x---x--x--";
    case Comp::Strum:
      return "x-.xx-.xx-.xx-.x";
    case Comp::Halves:
      return "x-------x-------";
  }
  return "";
}

// Arpeggios: digits pick the chord tone (3 = the root an octave up).
enum class ArpPat
{
  None,
  Up16,
  Broken8,
  Alberti,
  Trance,
  Sparse,
  Bounce,
};

const char* arpPattern(ArpPat a, int meter)
{
  if (meter == 3)
    return a == ArpPat::Sparse ? "0---2---3---" : "0-1-2-3-2-1-";
  switch (a)
  {
    case ArpPat::None:
      return "";
    case ArpPat::Up16:
      return "0123012301230123";
    case ArpPat::Broken8:
      return "0-2-1-2-0-2-1-2-";
    case ArpPat::Alberti:
      return "0212021202120212";
    case ArpPat::Trance:
      return "3030202130302021";
    case ArpPat::Sparse:
      return "0---2---1---3---";
    case ArpPat::Bounce:
      return "0.3.1.3.2.3.1.3.";
  }
  return "";
}

enum class Mode
{
  Major,
  Minor,
  Dorian,
  Mixo,
  Harm,
  Phryg,
  Lydian,
};

const int* scaleOf(Mode m)
{
  static const int kScales[7][7] = {
    {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10}, {0, 2, 4, 5, 7, 9, 10},
    {0, 2, 3, 5, 7, 8, 11}, {0, 1, 3, 5, 7, 8, 10}, {0, 2, 4, 6, 7, 9, 11}};
  return kScales[int(m)];
}

// Chord progressions as scale degrees, one chord per bar.
const std::vector<int>& progression(int i)
{
  static const std::vector<std::vector<int>> kProgs = {
    {0, 5, 2, 6},             // 0  i VI III VII
    {0, 4, 5, 3},             // 1  I V vi IV
    {0, 3, 6, 2},             // 2  i iv VII III
    {1, 4, 0, 5},             // 3  ii V I vi
    {0, 6, 5, 4},             // 4  i VII VI v
    {0, 0, 3, 0, 4, 3, 0, 4}, // 5  western
    {0, 0, 3, 4},             // 6  i i iv v
    {0, 5, 3, 4},             // 7  I vi IV V
    {0, 1, 0, 1},             // 8  i bII
    {0, 6, 3, 0},             // 9  I bVII IV I
    {5, 6, 0, 0},             // 10 VI VII i i
    {0, 2, 3, 1},             // 11 I iii IV ii
    {0, 0, 5, 5},             // 12 drone
    {0, 3, 4, 3},             // 13 I IV V IV
    {0, 3, 0, 3},             // 14 i iv
    {0, 3, 0, 0, 3, 3, 0, 0, 4, 3, 0, 4}, // 15 twelve-bar blues
    {0, 3, 6, 2, 5, 1, 4, 0}, // 16 round the circle of fifths
    {0, 0, 4, 4, 3, 3, 4, 0}, // 17 waltz
    {0, 4, 5, 3, 0, 4, 3, 4}, // 18 heroic
    {0, 0, 5, 4},             // 19 i i VI v
    {0, 6, 5, 6},             // 20 i VII VI VII
    {0, 1, 0, 6},             // 21 lydian lift
    {0, 0, 4, 4},             // 22 i i V V
    {0, 5, 1, 4},             // 23 I vi ii V
  };
  return kProgs[std::size_t(i) % kProgs.size()];
}

enum Extra : unsigned
{
  kHeart = 1u << 0,   // a soft lub-dub twice a bar
  kVinyl = 1u << 1,   // crackle and hiss
  kRain = 1u << 2,    // rain bed and thunder
  kUnder = 1u << 3,   // under water: muffled, with bubbles
  kStutter = 1u << 4, // glitch repeats
  kBreath = 1u << 5,  // slow breathing swells
  kDrone = 1u << 6,   // low tonic and fifth under everything
  kSevenths = 1u << 7, // jazz chords
  kRiser = 1u << 8,   // the club's 16-bar riser and drop
};

struct Style
{
  const char* id;
  double bpm;
  int meter;
  int key; // pitch class of the tonic
  Mode mode;
  int prog;
  Groove groove;
  BassPat bassPat;
  Inst bass;
  Inst pad;
  Comp comp;
  Inst arp;
  ArpPat arpPat;
  Inst lead;
  Inst lead2;
  int density; // 0 sparse, 1 medium, 2 busy
  float swing; // 0 straight .. 1 triplet
  float reverb;
  float echo;
  unsigned extras;
  int bars;
};

// clang-format off
const Style kStyles[] = {
  // id                         bpm  m key mode         pr groove            bass pattern       bass             pad              comp              arp              arp pattern      lead             lead2            dn swing rev  echo extras             bars
  {"theme_synthwave",          120, 4, 9, Mode::Minor,  0, Groove::Four,     BassPat::Octaves,  Inst::SynthBass, Inst::Pad,       Comp::Sustain,    Inst::Square,    ArpPat::Up16,    Inst::Saw,       Inst::Bell,      1, 0.0f, 0.25f, 0.3f, 0, 16},
  {"theme_airy",                92, 4, 5, Mode::Lydian, 21, Groove::Airy,    BassPat::Roots,    Inst::SubBass,   Inst::Choir,     Comp::Sustain,    Inst::Kalimba,   ArpPat::Sparse,  Inst::Flute,     Inst::Bell,      0, 0.0f, 0.5f, 0.35f, 0, 16},
  {"theme_clubhouse",          120, 4, 6, Mode::Minor,  2, Groove::House,    BassPat::Funk,     Inst::SynthBass, Inst::Organ,     Comp::Stabs,      Inst::Saw,       ArpPat::Trance,  Inst::Square,    Inst::Pluck,     2, 0.0f, 0.25f, 0.25f, kRiser, 16},
  {"theme_heartbeat",          120, 4, 1, Mode::Minor, 12, Groove::Heart,    BassPat::Pedal,    Inst::SubBass,   Inst::Strings,   Comp::Sustain,    Inst::Bell,      ArpPat::Sparse,  Inst::Flute,     Inst::Celesta,   0, 0.0f, 0.45f, 0.2f, kHeart, 16},
  {"theme_dub",                 76, 4, 7, Mode::Dorian, 14, Groove::Dub,     BassPat::Dub,      Inst::SubBass,   Inst::Organ,     Comp::Skank,      Inst::None,      ArpPat::None,    Inst::Clarinet,  Inst::Trumpet,   1, 0.3f, 0.3f, 0.5f, 0, 16},
  {"theme_arpeggio",           138, 4, 4, Mode::Minor,  4, Groove::Driving,  BassPat::Octaves,  Inst::SynthBass, Inst::Pad,       Comp::Sustain,    Inst::Square,    ArpPat::Up16,    Inst::Saw,       Inst::Bell,      2, 0.0f, 0.2f, 0.25f, 0, 16},
  {"theme_rotor",              128, 4, 2, Mode::Minor,  6, Groove::Rotor,    BassPat::Gallop,   Inst::SynthBass, Inst::Strings,   Comp::Pulse8,     Inst::None,      ArpPat::None,    Inst::Brass,     Inst::Saw,       1, 0.0f, 0.25f, 0.0f, 0, 16},
  {"theme_marimba",            108, 4, 0, Mode::Major,  1, Groove::Jungle,   BassPat::Bossa,    Inst::Upright,   Inst::None,      Comp::None,       Inst::Kalimba,   ArpPat::Broken8, Inst::Marimba,   Inst::Flute,     2, 0.15f, 0.3f, 0.0f, 0, 16},
  {"theme_creeping_drums",      84, 4, 11, Mode::Phryg, 8, Groove::Creep,    BassPat::Halftime, Inst::SubBass,   Inst::Strings,   Comp::Sustain,    Inst::Pluck,     ArpPat::Sparse,  Inst::Clarinet,  Inst::Choir,     0, 0.0f, 0.4f, 0.15f, 0, 16},
  {"theme_brass_bells",        112, 4, 10, Mode::Major, 18, Groove::March,   BassPat::Oompah,   Inst::Tuba,      Inst::Strings,   Comp::Sustain,    Inst::Bell,      ArpPat::Sparse,  Inst::Brass,     Inst::Trumpet,   1, 0.0f, 0.35f, 0.0f, 0, 16},
  {"theme_wood_blocks",        116, 4, 2, Mode::Dorian,  2, Groove::Blocks,  BassPat::Funk,     Inst::Upright,   Inst::None,      Comp::None,       Inst::Marimba,   ArpPat::Bounce,  Inst::Pluck,     Inst::Kalimba,   2, 0.1f, 0.2f, 0.0f, 0, 16},
  {"theme_deep_drums",          90, 4, 5, Mode::Minor, 10, Groove::Deep,     BassPat::Halftime, Inst::SubBass,   Inst::Choir,     Comp::Sustain,    Inst::None,      ArpPat::None,    Inst::Strings,   Inst::Brass,     0, 0.0f, 0.4f, 0.0f, 0, 16},
  {"theme_orchestral_panic",   150, 4, 0, Mode::Harm,  19, Groove::Panic,    BassPat::Pedal,    Inst::Strings,   Inst::Brass,     Comp::Halves,     Inst::Strings,   ArpPat::Up16,    Inst::Trumpet,   Inst::Strings,   2, 0.0f, 0.3f, 0.0f, 0, 16},
  {"theme_war_drums",          120, 4, 2, Mode::Minor,  6, Groove::War,      BassPat::Pedal,    Inst::SubBass,   Inst::Choir,     Comp::Sustain,    Inst::None,      ArpPat::None,    Inst::Brass,     Inst::Choir,     0, 0.0f, 0.35f, 0.0f, 0, 16},
  {"theme_cold_ambient",        70, 4, 4, Mode::Minor, 12, Groove::None,     BassPat::Roots,    Inst::SubBass,   Inst::Strings,   Comp::Sustain,    Inst::Bell,      ArpPat::Sparse,  Inst::Celesta,   Inst::Flute,     0, 0.0f, 0.6f, 0.3f, kDrone, 16},
  {"cryo_bells",                84, 3, 6, Mode::Minor, 19, Groove::Shimmer,  BassPat::Roots,    Inst::SubBass,   Inst::Choir,     Comp::Sustain,    Inst::Bell,      ArpPat::Broken8, Inst::Vibes,     Inst::Celesta,   0, 0.0f, 0.55f, 0.25f, 0, 16},
  {"greenhouse_pulse",         104, 4, 7, Mode::Lydian, 21, Groove::Pulse,   BassPat::Pedal,    Inst::SynthBass, Inst::Pad,       Comp::Sustain,    Inst::Kalimba,   ArpPat::Up16,    Inst::EPiano,    Inst::Flute,     1, 0.0f, 0.3f, 0.2f, 0, 16},
  {"hull_breathing",            66, 4, 0, Mode::Minor, 12, Groove::None,     BassPat::Roots,    Inst::SubBass,   Inst::Strings,   Comp::Sustain,    Inst::None,      ArpPat::None,    Inst::Clarinet,  Inst::Bell,      0, 0.0f, 0.5f, 0.2f, kBreath, 16},
  {"clinical_bleeps",          124, 4, 9, Mode::Dorian, 14, Groove::Four,    BassPat::Pedal,    Inst::SynthBass, Inst::None,      Comp::None,       Inst::Chip,      ArpPat::Trance,  Inst::Square,    Inst::Celesta,   1, 0.0f, 0.2f, 0.3f, 0, 16},
  {"core_drone",                72, 4, 2, Mode::Phryg,  8, Groove::Shimmer,  BassPat::Roots,    Inst::SubBass,   Inst::Organ,     Comp::Sustain,    Inst::Glitch,    ArpPat::Sparse,  Inst::Strings,   Inst::Choir,     0, 0.0f, 0.5f, 0.3f, kDrone, 16},
  {"organ_theme",               84, 4, 2, Mode::Harm,   4, Groove::None,     BassPat::Halves,   Inst::Organ,     Inst::Organ,     Comp::Sustain,    Inst::Organ,     ArpPat::Alberti, Inst::Organ,     Inst::Choir,     1, 0.0f, 0.6f, 0.0f, 0, 16},
  {"theme_western",            100, 4, 9, Mode::Mixo,   5, Groove::Gallop,   BassPat::Oompah,   Inst::Upright,   Inst::Pluck,     Comp::Strum,      Inst::None,      ArpPat::None,    Inst::Slide,     Inst::Flute,     1, 0.15f, 0.3f, 0.0f, 0, 16},
  {"theme_theremin",            88, 4, 11, Mode::Harm, 19, Groove::Jazz,     BassPat::Walking,  Inst::Upright,   Inst::Strings,   Comp::Sustain,    Inst::Celesta,   ArpPat::Sparse,  Inst::Theremin,  Inst::Vibes,     0, 0.5f, 0.45f, 0.15f, 0, 16},
  {"theme_accordion",          150, 3, 5, Mode::Major, 17, Groove::Waltz,    BassPat::Waltz,    Inst::Tuba,      Inst::Accordion, Comp::Waltz,      Inst::None,      ArpPat::None,    Inst::Accordion, Inst::Clarinet,  2, 0.0f, 0.25f, 0.0f, 0, 16},
  {"theme_string_quartet",     100, 4, 7, Mode::Minor,  2, Groove::None,     BassPat::Halves,   Inst::Strings,   Inst::Strings,   Comp::Pulse8,     Inst::None,      ArpPat::None,    Inst::Strings,   Inst::Strings,   1, 0.0f, 0.35f, 0.0f, 0, 16},
  {"theme_monster_brass",       96, 4, 1, Mode::Harm,   6, Groove::Half,     BassPat::Octaves,  Inst::Tuba,      Inst::Brass,     Comp::Stabs,      Inst::None,      ArpPat::None,    Inst::Brass,     Inst::Tuba,      1, 0.0f, 0.3f, 0.0f, 0, 16},
  {"theme_noir_trumpet",        72, 4, 0, Mode::Dorian, 14, Groove::Jazz,    BassPat::Walking,  Inst::Upright,   Inst::EPiano,    Comp::Charleston, Inst::None,      ArpPat::None,    Inst::Trumpet,   Inst::Vibes,     0, 0.6f, 0.45f, 0.0f, kVinyl | kSevenths, 16},
  {"theme_toon_jazz",          168, 4, 5, Mode::Major, 16, Groove::Toon,     BassPat::Walking,  Inst::Upright,   Inst::Piano,     Comp::Charleston, Inst::None,      ArpPat::None,    Inst::Clarinet,  Inst::Trumpet,   2, 0.5f, 0.25f, 0.0f, kSevenths, 16},
  {"theme_glitch",             132, 4, 6, Mode::Minor,  0, Groove::Glitch,   BassPat::Funk,     Inst::SynthBass, Inst::Pad,       Comp::Sustain,    Inst::Glitch,    ArpPat::Trance,  Inst::Chip,      Inst::Square,    2, 0.0f, 0.2f, 0.2f, kStutter, 16},
  {"theme_submerged",           78, 4, 3, Mode::Major, 11, Groove::Airy,     BassPat::Roots,    Inst::SubBass,   Inst::Choir,     Comp::Sustain,    Inst::EPiano,    ArpPat::Broken8, Inst::Vibes,     Inst::Flute,     0, 0.0f, 0.5f, 0.3f, kUnder, 16},
  {"theme_rock_storm",         150, 4, 4, Mode::Minor, 20, Groove::Storm,    BassPat::Rock,     Inst::SynthBass, Inst::Dist,      Comp::Rock8,      Inst::None,      ArpPat::None,    Inst::Dist,      Inst::Strings,   1, 0.0f, 0.25f, 0.0f, kRain, 16},
  {"theme_desert_slide",        92, 4, 2, Mode::Harm,  22, Groove::Desert,   BassPat::Pedal,    Inst::Upright,   Inst::None,      Comp::None,       Inst::Pluck,     ArpPat::Broken8, Inst::Slide,     Inst::Flute,     1, 0.1f, 0.35f, 0.15f, 0, 16},
  {"theme_driving_hihat",      140, 4, 11, Mode::Minor, 2, Groove::Driving,  BassPat::Octaves,  Inst::SynthBass, Inst::Pad,       Comp::Sustain,    Inst::Pluck,     ArpPat::Up16,    Inst::Saw,       Inst::Strings,   2, 0.0f, 0.2f, 0.15f, 0, 16},
  {"theme_waltz",              144, 3, 3, Mode::Major, 17, Groove::Waltz,    BassPat::Waltz,    Inst::Upright,   Inst::Strings,   Comp::Waltz,      Inst::None,      ArpPat::None,    Inst::Piano,     Inst::Strings,   1, 0.0f, 0.45f, 0.0f, 0, 16},
  {"theme_heroic_build",       128, 4, 2, Mode::Major, 18, Groove::March,    BassPat::Halves,   Inst::Strings,   Inst::Choir,     Comp::Sustain,    Inst::Strings,   ArpPat::Broken8, Inst::Brass,     Inst::Trumpet,   1, 0.0f, 0.4f, 0.0f, 0, 16},
  {"theme_lonely_piano",        72, 4, 8, Mode::Major, 11, Groove::None,     BassPat::Roots,    Inst::Piano,     Inst::Strings,   Comp::Sustain,    Inst::Piano,     ArpPat::Alberti, Inst::Piano,     Inst::Strings,   0, 0.0f, 0.5f, 0.0f, kVinyl, 16},
  {"theme_jazz_bass",          132, 4, 10, Mode::Dorian, 3, Groove::Jazz,    BassPat::Walking,  Inst::Upright,   Inst::EPiano,    Comp::Charleston, Inst::None,      ArpPat::None,    Inst::Vibes,     Inst::Clarinet,  1, 0.6f, 0.3f, 0.0f, kSevenths, 16},
  {"theme_typewriter_clarinet",112, 4, 0, Mode::Major,  7, Groove::Typewriter, BassPat::Halves, Inst::Upright,   Inst::Piano,     Comp::Charleston, Inst::None,      ArpPat::None,    Inst::Clarinet,  Inst::Celesta,   1, 0.3f, 0.3f, 0.0f, 0, 16},
  {"theme_synth_pulse",        124, 4, 1, Mode::Minor,  7, Groove::Pulse,    BassPat::Pulse16,  Inst::SynthBass, Inst::Pad,       Comp::Sustain,    Inst::Square,    ArpPat::Trance,  Inst::Saw,       Inst::Choir,     1, 0.0f, 0.3f, 0.2f, 0, 16},
  {"theme_arena_rock",         120, 4, 9, Mode::Mixo,   9, Groove::Arena,    BassPat::Rock,     Inst::SynthBass, Inst::Dist,      Comp::Rock8,      Inst::None,      ArpPat::None,    Inst::Dist,      Inst::Choir,     1, 0.0f, 0.3f, 0.0f, 0, 16},
  {"theme_big_band",           144, 4, 5, Mode::Major, 23, Groove::BigBand,  BassPat::Walking,  Inst::Upright,   Inst::Brass,     Comp::Charleston, Inst::None,      ArpPat::None,    Inst::Trumpet,   Inst::Clarinet,  2, 0.6f, 0.3f, 0.0f, kSevenths, 16},
  // Cutscenes, the menu's companions and the bonus levels.
  {"theme_opening",            100, 4, 2, Mode::Major, 18, Groove::March,    BassPat::Halves,   Inst::Strings,   Inst::Strings,   Comp::Sustain,    Inst::Bell,      ArpPat::Sparse,  Inst::Brass,     Inst::Choir,     1, 0.0f, 0.45f, 0.0f, 0, 16},
  {"theme_episode_end",         80, 4, 7, Mode::Major,  1, Groove::None,     BassPat::Roots,    Inst::Upright,   Inst::Strings,   Comp::Sustain,    Inst::Piano,     ArpPat::Broken8, Inst::Piano,     Inst::Flute,     0, 0.0f, 0.5f, 0.0f, 0, 8},
  {"theme_cartoon_end",        160, 4, 0, Mode::Major, 16, Groove::Toon,     BassPat::Oompah,   Inst::Tuba,      Inst::Piano,     Comp::Charleston, Inst::Celesta,   ArpPat::Sparse,  Inst::Clarinet,  Inst::Trumpet,   2, 0.5f, 0.25f, 0.0f, 0, 16},
  {"theme_chiptune",           120, 4, 0, Mode::Minor,  0, Groove::Four,     BassPat::Octaves,  Inst::Chip,      Inst::None,      Comp::None,       Inst::Chip,      ArpPat::Up16,    Inst::Chip,      Inst::Chip,      2, 0.0f, 0.1f, 0.0f, 0, 16},
  {"theme_credits",            104, 4, 9, Mode::Major,  1, Groove::Four,     BassPat::Octaves,  Inst::SynthBass, Inst::Pad,       Comp::Sustain,    Inst::Pluck,     ArpPat::Up16,    Inst::Saw,       Inst::Bell,      1, 0.0f, 0.35f, 0.2f, 0, 16},
  {"theme_finale",             132, 4, 4, Mode::Minor, 10, Groove::War,      BassPat::Octaves,  Inst::Strings,   Inst::Choir,     Comp::Sustain,    Inst::Strings,   ArpPat::Up16,    Inst::Brass,     Inst::Choir,     1, 0.0f, 0.4f, 0.0f, 0, 16},
  {"theme_show",               128, 4, 10, Mode::Major, 3, Groove::BigBand,  BassPat::Walking,  Inst::Upright,   Inst::Brass,     Comp::Stabs,      Inst::None,      ArpPat::None,    Inst::Trumpet,   Inst::Brass,     2, 0.4f, 0.3f, 0.0f, kSevenths, 16},
  {"bonus_cloud_nine",         100, 4, 2, Mode::Major,  1, Groove::Airy,     BassPat::Roots,    Inst::SubBass,   Inst::Pad,       Comp::Sustain,    Inst::Kalimba,   ArpPat::Up16,    Inst::Celesta,   Inst::Flute,     1, 0.0f, 0.45f, 0.3f, 0, 16},
  {"bonus_free_fall",          150, 4, 7, Mode::Minor, 20, Groove::Driving,  BassPat::Octaves,  Inst::SynthBass, Inst::Pad,       Comp::Sustain,    Inst::Square,    ArpPat::Trance,  Inst::Saw,       Inst::Choir,     2, 0.0f, 0.3f, 0.2f, 0, 16},
  {"bonus_step_on_the_beat",   120, 4, 4, Mode::Dorian, 14, Groove::Four,    BassPat::Funk,     Inst::SynthBass, Inst::EPiano,    Comp::Stabs,      Inst::None,      ArpPat::None,    Inst::Chip,      Inst::Square,    1, 0.0f, 0.2f, 0.0f, 0, 16},
  {"bonus_echo_room",           90, 4, 9, Mode::Dorian, 12, Groove::Airy,    BassPat::Roots,    Inst::SubBass,   Inst::Choir,     Comp::Sustain,    Inst::Bell,      ArpPat::Sparse,  Inst::Kalimba,   Inst::Flute,     0, 0.0f, 0.6f, 0.6f, 0, 16},
  {"bonus_duck_rapids",        140, 4, 7, Mode::Major,  13, Groove::Toon,    BassPat::Oompah,   Inst::Tuba,      Inst::Accordion, Comp::Stabs,      Inst::Marimba,   ArpPat::Bounce,  Inst::Clarinet,  Inst::Marimba,   2, 0.0f, 0.2f, 0.0f, 0, 16},
  {"bonus_light_trail",        128, 4, 9, Mode::Minor,  0, Groove::Pulse,    BassPat::Pulse16,  Inst::SynthBass, Inst::Pad,       Comp::Sustain,    Inst::Pluck,     ArpPat::Trance,  Inst::Square,    Inst::Saw,       1, 0.0f, 0.3f, 0.3f, 0, 16},
  {"bonus_pilot_seat",         132, 4, 4, Mode::Mixo,   9, Groove::Rotor,    BassPat::Rock,     Inst::SynthBass, Inst::Brass,     Comp::Stabs,      Inst::None,      ArpPat::None,    Inst::Trumpet,   Inst::Dist,      1, 0.0f, 0.25f, 0.0f, 0, 16},
  {"bonus_bounce",             132, 4, 0, Mode::Major,  7, Groove::Jungle,   BassPat::Rolling,  Inst::Upright,   Inst::None,      Comp::None,       Inst::Marimba,   ArpPat::Bounce,  Inst::Kalimba,   Inst::Pluck,     2, 0.2f, 0.25f, 0.0f, 0, 16},
  {"bonus_trapmaster",        112, 4, 4, Mode::Phryg, 8, Groove::Creep,    BassPat::Octaves,  Inst::SubBass,   Inst::Strings,   Comp::Stabs,      Inst::Marimba,   ArpPat::Sparse,  Inst::Clarinet,  Inst::Choir,     1, 0.0f, 0.3f, 0.1f, 0, 16},
  {"bonus_negative",          108, 4, 6, Mode::Lydian, 11, Groove::Airy,    BassPat::Roots,    Inst::SubBass,   Inst::Pad,       Comp::Sustain,    Inst::Celesta,   ArpPat::Up16,    Inst::Bell,      Inst::Choir,     1, 0.0f, 0.5f, 0.4f, 0, 16},
  {"bonus_pinball",          138, 4, 1, Mode::Mixo,  15, Groove::Toon,     BassPat::Walking,  Inst::Upright,   Inst::EPiano,    Comp::Stabs,      Inst::Marimba,   ArpPat::Bounce,  Inst::Chip,      Inst::Bell,      2, 0.3f, 0.2f, 0.0f, kSevenths, 16},
  {"bonus_floor_lava",       146, 4, 2, Mode::Phryg, 16, Groove::Jungle,   BassPat::Rolling,  Inst::SynthBass, Inst::None,      Comp::None,       Inst::Marimba,   ArpPat::Bounce,  Inst::Kalimba,   Inst::Dist,      2, 0.2f, 0.25f, 0.0f, 0, 16},
};
// clang-format on

std::uint32_t hashId(const std::string& s)
{
  std::uint32_t h = 2166136261u;
  for (char c : s)
    h = (h ^ std::uint32_t(static_cast<unsigned char>(c))) * 16777619u;
  return h | 1u;
}

const Style* findStyle(const std::string& id)
{
  for (const auto& s : kStyles)
    if (id == s.id)
      return &s;
  return nullptr;
}

// Tracks the table doesn't name (the later bonus levels and any new id) get
// a style built from their name: a known arrangement in a new key, mode,
// progression and tempo, with its own tune.
Style derivedStyle(const std::string& id)
{
  const std::uint32_t h = hashId(id);
  // Pick from the level styles only (the first 41 rows), not the cutscene ones.
  const std::size_t base = std::size_t(h % 41u);
  Style s = kStyles[base];
  s.id = "";
  s.extras &= ~(kHeart | kRiser);
  s.key = int((h >> 8) % 12u);
  s.prog = int((h >> 12) % 24u);
  static const Mode kModes[] = {Mode::Major, Mode::Minor, Mode::Dorian, Mode::Mixo, Mode::Lydian};
  s.mode = kModes[(h >> 17) % 5u];
  s.bpm = std::clamp(s.bpm + double(int((h >> 20) % 21u) - 10), 66.0, 170.0);
  static const Inst kLeads[] = {Inst::Saw, Inst::Square, Inst::Marimba, Inst::Kalimba, Inst::Flute, Inst::Trumpet,
                                Inst::Clarinet, Inst::Vibes, Inst::EPiano, Inst::Pluck, Inst::Accordion, Inst::Celesta};
  s.lead = kLeads[(h >> 25) % 12u];
  return s;
}

// --- Composition ---------------------------------------------------------------

struct Theory
{
  int key;
  const int* scale;
  // Scale degree (any integer) to MIDI, with degree 0 the tonic at or below
  // `around`.
  int midi(int deg, int around) const
  {
    const int tonic = around - ((around - key) % 12 + 12) % 12;
    const int oct = deg >= 0 ? deg / 7 : -((6 - deg) / 7);
    return tonic + 12 * oct + scale[((deg % 7) + 7) % 7];
  }
};

// Moves a note by octaves into [lo, lo + 12).
int into(int note, int lo)
{
  while (note < lo)
    note += 12;
  while (note >= lo + 12)
    note -= 12;
  return note;
}

struct MelNote
{
  int bar;
  int step;
  int len;
  int deg; // scale degree, 0 = tonic
};

// An 8-bar tune: phrases A B A C | A B A' D, where A is a motif carried over
// whatever chord is under it, and D settles on the chord's root.
std::vector<MelNote> compose(std::uint32_t seed, const std::vector<int>& prog, int stepsPerBar, int density)
{
  Noise rng(seed);
  auto rnd = [&]() { return (rng.next() + 1.0f) * 0.5f; };
  auto rhythm = [&](bool cadence) {
    std::vector<std::array<int, 3>> out; // step, len, rest
    static const int kLens[3][5] = {{4, 8, 8, 12, 16}, {2, 4, 4, 6, 8}, {1, 2, 2, 2, 4}};
    const float restP = density == 0 ? 0.22f : density == 1 ? 0.14f : 0.08f;
    int pos = 0;
    while (pos < stepsPerBar)
    {
      if (cadence && pos >= stepsPerBar / 2)
      {
        out.push_back({pos, stepsPerBar - pos, 0});
        break;
      }
      int len = kLens[density][int(rnd() * 4.999f)];
      if (len == 1)
      {
        out.push_back({pos, 1, 0});
        ++pos;
        if (pos >= stepsPerBar)
          break;
      }
      len = std::min(len, stepsPerBar - pos);
      out.push_back({pos, len, pos > 0 && rnd() < restP ? 1 : 0});
      pos += len;
    }
    return out;
  };
  auto contour = [&](std::size_t n) {
    static const int kSteps[] = {-2, -1, -1, 1, 1, 2, 3, -3, 0, 1, -1, 4};
    std::vector<int> d(n, 0);
    for (std::size_t i = 1; i < n; ++i)
      d[i] = d[i - 1] + kSteps[int(rnd() * 11.999f)];
    return d;
  };

  const auto rA = rhythm(false), rB = rhythm(false), rC = rhythm(false), rD = rhythm(true);
  const auto cA = contour(rA.size()), cA2 = contour(rA.size()), cB = contour(rB.size()), cC = contour(rC.size()),
             cD = contour(rD.size());
  struct Phrase
  {
    const std::vector<std::array<int, 3>>* r;
    const std::vector<int>* c;
  };
  const Phrase plan[8] = {{&rA, &cA}, {&rB, &cB}, {&rA, &cA}, {&rC, &cC},
                          {&rA, &cA}, {&rB, &cB}, {&rA, &cA2}, {&rD, &cD}};

  std::vector<MelNote> out;
  int prev = 4;
  for (int bar = 0; bar < 8; ++bar)
  {
    const int chord = prog[std::size_t(bar) % prog.size()];
    const auto& r = *plan[bar].r;
    const auto& c = *plan[bar].c;
    // Start on the chord tone nearest the last note.
    int start = chord;
    for (int k = -14; k <= 14; ++k)
    {
      const int cand = prev + k;
      const int rel = ((cand - chord) % 7 + 7) % 7;
      if ((rel == 0 || rel == 2 || rel == 4) && std::abs(cand - prev) < std::abs(start - prev))
        start = cand;
    }
    for (std::size_t i = 0; i < r.size(); ++i)
    {
      if (r[i][2])
        continue;
      int deg = start + c[i];
      while (deg > 11)
        deg -= 7;
      while (deg < -3)
        deg += 7;
      const bool strong = r[i][0] % 4 == 0;
      const int rel = ((deg - chord) % 7 + 7) % 7;
      if (strong && rel != 0 && rel != 2 && rel != 4)
        deg += (rel == 1 || rel == 3 || rel == 5) ? -1 : 1;
      if (bar == 7 && i + 1 == r.size())
      {
        // Land on the root of the last chord.
        deg = chord + 7 * int(std::round(double(deg - chord) / 7.0));
      }
      out.push_back({bar, r[i][0], r[i][1], deg});
      prev = deg;
    }
  }
  return out;
}

int leadCentre(Inst i)
{
  switch (i)
  {
    case Inst::Flute:
    case Inst::Marimba:
    case Inst::Kalimba:
    case Inst::Vibes:
    case Inst::Piano:
    case Inst::Chip:
    case Inst::Glitch:
      return 74;
    case Inst::Bell:
    case Inst::Celesta:
      return 81;
    case Inst::Tuba:
      return 52;
    case Inst::Brass:
    case Inst::Clarinet:
    case Inst::Choir:
    case Inst::Slide:
    case Inst::Dist:
      return 65;
    default:
      return 70;
  }
}

float leadLevel(Inst i)
{
  switch (i)
  {
    case Inst::Marimba:
    case Inst::Kalimba:
    case Inst::Bell:
    case Inst::Celesta:
      return 1.15f;
    case Inst::Strings:
    case Inst::Choir:
      return 1.2f;
    default:
      return 0.9f;
  }
}

struct Options
{
  int bars = 0;        // 0 = the style's
  int layers = 9;      // level 4: 0 bass, 1 pad, 2 drums, 3 arpeggio, 4 lead
  int intro = -1;      // bars before the lead comes in (-1 = a quarter of the loop)
  bool cover = false;  // the elevator's lounge cover
};

struct Stereo
{
  std::vector<float> l, r;
};

void effects(Bus& b, const Style& st, double stepSec, std::uint32_t seed);

Stereo render(const Style& styleIn, const std::string& seedName, const Options& opt)
{
  Style st = styleIn;
  if (opt.cover)
  {
    // The lift plays it as lounge bossa.
    st.bpm = 84.0;
    st.meter = 4;
    st.groove = Groove::Bossa;
    st.bassPat = BassPat::Bossa;
    st.bass = Inst::Upright;
    st.pad = Inst::EPiano;
    st.comp = Comp::Bossa;
    st.arp = Inst::None;
    st.lead = Inst::Vibes;
    st.lead2 = Inst::Flute;
    st.swing = 0.0f;
    st.extras = (st.extras & kSevenths) | kVinyl;
  }
  const std::uint32_t seed = hashId(seedName);
  const int bars = opt.bars > 0 ? opt.bars : st.bars;
  const int spb = st.meter * 4; // 16th steps per bar
  const double beat = 60.0 / st.bpm;
  const double stepSec = beat / 4.0;
  const double total = bars * st.meter * beat;
  Bus bus(total);
  const Theory th{st.key, scaleOf(st.mode)};
  const auto& prog = progression(st.prog);
  const int intro = opt.intro >= 0 ? opt.intro : bars / 4;
  const bool sevenths = (st.extras & kSevenths) != 0;

  // Step time with swing: off-beat eighths lean late.
  auto at = [&](int bar, double step) {
    const int s = int(step);
    double shift = 0.0;
    if (s % 4 == 2)
      shift = st.swing * 0.667;
    else if (s % 2 == 1)
      shift = st.swing * 0.33;
    return (bar * spb + step + shift) * stepSec;
  };
  auto chordOf = [&](int bar) { return prog[std::size_t(bar) % prog.size()]; };

  // Drums.
  if (opt.layers >= 2)
  {
    const GrooveDef g = grooveDef(st.groove);
    std::uint32_t hs = seed;
    for (int bar = 0; bar < bars; ++bar)
      for (int li = 0; li < g.count; ++li)
      {
        const Lane& lane = g.lanes[std::size_t(li)];
        if (bar % lane.every != 0)
          continue;
        const int len = int(std::strlen(lane.pattern));
        for (int s = 0; s < spb; ++s)
        {
          const char ch = lane.pattern[s % len];
          if (ch == '.')
            continue;
          const float v = ch == 'o' ? 0.55f : ch == '-' ? 0.3f : 1.0f;
          hit(bus, lane.perc, at(bar, s), lane.vol * v * 0.6f, ch, lane.pan, hs += 7919u);
        }
      }
    if (g.fills)
      for (int bar = 7; bar < bars; bar += 8)
        for (int s = spb - 4; s < spb; ++s)
          hit(bus, Perc::Snare, at(bar, s), (0.25f + 0.08f * float(s - spb + 4)) * 0.6f, 'x', 0.1f, hs += 7919u);
  }

  // Heartbeat.
  if (st.extras & kHeart)
    for (int bb = 0; bb < bars * 2; ++bb)
      for (int k = 0; k < 2; ++k)
      {
        double ph = 0.0;
        emit(bus, bb * 2 * beat + k * 0.2, 0.25, Send{k == 0 ? 0.5f : 0.35f, 0.0f, 0.05f, 0.0f}, [&](double t) {
          ph += 48.0 * (1.0 + 0.8 * fexp(-t * 30.0)) / kRate;
          return sin2pi(ph) * fexp(-t * 14.0) * std::min(1.0, t / 0.004);
        });
      }

  // Bass.
  {
    const char* pat = bassPattern(st.bassPat, st.meter);
    const int len = int(std::strlen(pat));
    const int lo = st.bass == Inst::SubBass ? 31 : st.bass == Inst::Piano || st.bass == Inst::Organ ? 38 : 36;
    const float vol = st.bass == Inst::Strings ? 1.4f : st.bass == Inst::Tuba ? 1.2f : 1.0f;
    double prevNote = 0.0;
    for (int bar = 0; bar < bars; ++bar)
    {
      const int c = chordOf(bar), next = chordOf(bar + 1);
      const int root = into(th.midi(c, 60), lo);
      for (int s = 0; s < spb; ++s)
      {
        const char ch = pat[s % len];
        if (ch == '.' || ch == '-')
          continue;
        int hold = 1;
        while (s + hold < spb && pat[(s + hold) % len] == '-')
          ++hold;
        int note = root;
        if (ch == 'o')
          note = root + 12;
        else if (ch == 'f')
          note = into(th.midi(c + 4, 60), lo);
        else if (ch == 't')
          note = into(th.midi(c + 2, 60), lo);
        else if (ch == 'l')
          note = into(th.midi(c + 4, 60), lo) - (into(th.midi(c + 4, 60), lo) > root ? 12 : 0);
        else if (ch == 'a')
        {
          const int nr = into(th.midi(next, 60), lo);
          note = nr + (nr > root ? -1 : 1);
        }
        const double t0 = at(bar, s);
        const double dur = (at(bar, s + hold) - t0) * (st.bass == Inst::Organ ? 0.98 : 0.88);
        play(bus, st.bass, Tone{t0, dur, double(note), prevNote, ch == 'r' && s == 0 ? 0.95f : 0.8f},
             Send{vol, 0.0f, 0.05f, 0.0f}, seed + std::uint32_t(bar * 31 + s));
        prevNote = note;
      }
    }
  }

  // Low drone.
  if (st.extras & kDrone)
  {
    const int tonic = into(st.key, 36);
    for (int k : {0, 7})
    {
      const double f = midiFreq(tonic + k);
      emit(bus, 0.0, total, Send{0.12f, k ? 0.3f : -0.3f, 0.4f, 0.0f}, [&](double t) {
        return sin2pi(f * t) * (0.7 + 0.3 * sin2pi(t / total * 4.0));
      });
    }
  }

  // Chords.
  auto chordNotes = [&](int bar) {
    const int c = chordOf(bar);
    std::vector<int> notes;
    if (st.pad == Inst::Dist)
    {
      const int root = into(th.midi(c, 60), 40);
      return std::vector<int>{root, root + 7, root + 12};
    }
    for (int k = 0; k < (sevenths ? 4 : 3); ++k)
      notes.push_back(into(th.midi(c + 2 * k, 60), 53));
    std::sort(notes.begin(), notes.end());
    return notes;
  };
  if (opt.layers >= 1 && st.pad != Inst::None && st.comp != Comp::None)
  {
    const char* pat = compPattern(st.comp, st.meter);
    const int len = int(std::strlen(pat));
    const bool sustain = st.comp == Comp::Sustain;
    const float vol = (st.pad == Inst::Strings || st.pad == Inst::Choir || st.pad == Inst::Pad ? 0.55f : 0.42f);
    for (int bar = 0; bar < bars; ++bar)
    {
      const auto notes = chordNotes(bar);
      for (int s = 0; s < spb; ++s)
      {
        const char ch = pat[s % len];
        if (ch != 'x')
          continue;
        int hold = 1;
        while (s + hold < spb && pat[(s + hold) % len] == '-')
          ++hold;
        const double t0 = at(bar, s);
        const double dur = (at(bar, s + hold) - t0) * (sustain ? 1.0 : 0.8);
        float pan = -0.35f;
        for (int note : notes)
        {
          play(bus, st.pad, Tone{t0, dur, double(note), 0.0, 0.7f}, Send{vol, pan, 0.3f, 0.0f}, seed ^ std::uint32_t(note * 977 + bar));
          pan += 0.35f;
        }
      }
    }
  }

  // Arpeggio.
  if (opt.layers >= 3 && st.arp != Inst::None && st.arpPat != ArpPat::None)
  {
    const char* pat = arpPattern(st.arpPat, st.meter);
    const int len = int(std::strlen(pat));
    const int lo = st.arp == Inst::Organ || st.arp == Inst::Piano ? 55 : 60;
    for (int bar = 0; bar < bars; ++bar)
    {
      const int c = chordOf(bar);
      int tones[4];
      for (int k = 0; k < 3; ++k)
        tones[k] = into(th.midi(c + 2 * k, 60), lo);
      std::sort(tones, tones + 3);
      tones[3] = tones[0] + 12;
      for (int s = 0; s < spb; ++s)
      {
        const char ch = pat[s % len];
        if (ch < '0' || ch > '3')
          continue;
        int hold = 1;
        while (s + hold < spb && pat[(s + hold) % len] == '-')
          ++hold;
        const double t0 = at(bar, s);
        const double dur = (at(bar, s + hold) - t0) * 0.7;
        const float pan = (s % 4 < 2 ? -0.3f : 0.3f);
        play(bus, st.arp, Tone{t0, dur, double(tones[ch - '0']), 0.0, s % 4 == 0 ? 0.85f : 0.65f},
             Send{0.42f, pan, 0.25f, st.echo}, seed + std::uint32_t(s * 13 + bar * 101));
      }
    }
  }

  // Lead, then the second voice answers with the opening phrases.
  if (opt.layers >= 4 && st.lead != Inst::None)
  {
    const auto tune = compose(seed * 2654435761u + 17u, prog, spb, st.density);
    auto voice = [&](Inst inst, int fromBar, int toBar, int phraseFrom, float level) {
      const int centre = leadCentre(inst);
      double prevNote = 0.0;
      for (const auto& m : tune)
      {
        const int bar = fromBar + m.bar - phraseFrom;
        if (m.bar < phraseFrom || bar >= toBar)
          continue;
        const int note = th.midi(m.deg, centre - 5);
        const double t0 = at(bar, m.step);
        const double dur = (at(bar, m.step + m.len) - t0) * 0.92;
        play(bus, inst, Tone{t0, dur, double(note), prevNote, 0.85f},
             Send{level * leadLevel(inst), 0.08f, 0.3f, st.echo * 0.7f}, seed + std::uint32_t(m.bar * 64 + m.step));
        prevNote = note;
      }
    };
    const int leadEnd = std::min(bars, intro + 8);
    voice(st.lead, intro, leadEnd, 0, 1.0f);
    if (leadEnd < bars)
      voice(st.lead2 != Inst::None ? st.lead2 : st.lead, leadEnd, bars, 0, 0.85f);
  }

  effects(bus, st, stepSec, seed);

  // Reverb and echo returns, then loudness.
  Stereo out{bus.l, bus.r};
  {
    // Ping-pong echo at a dotted eighth, wrapping round the loop.
    if (st.echo > 0.0f)
    {
      const int n = bus.n, d = std::max(1, samples(stepSec * 3.0));
      std::vector<float> el(std::size_t(n), 0.0f), er(std::size_t(n), 0.0f);
      OnePole tone;
      for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < n; ++i)
        {
          const auto j = std::size_t(((i - d) % n + n) % n);
          el[std::size_t(i)] = tone.lowpass(bus.echo[std::size_t(i)] + er[j] * 0.45f, 3500.0);
          er[std::size_t(i)] = el[j] * 0.85f;
        }
      for (int i = 0; i < n; ++i)
      {
        const auto j = std::size_t(((i - d) % n + n) % n);
        out.l[std::size_t(i)] += el[j] * 0.6f;
        out.r[std::size_t(i)] += er[std::size_t(i)] * 0.6f;
        bus.rev[std::size_t(i)] += (el[j] + er[std::size_t(i)]) * 0.15f;
      }
    }
    // Schroeder reverb: parallel damped combs into two allpasses, per side.
    const int n = bus.n;
    const float room = 0.78f + 0.12f * st.reverb;
    for (int side = 0; side < 2; ++side)
    {
      static const int kCombs[6] = {1214, 1293, 1390, 1476, 1557, 1617};
      static const int kAll[2] = {605, 480};
      const int spread = side * 25;
      std::vector<std::vector<float>> combs(6), alls(2);
      std::array<float, 6> store{};
      std::array<std::size_t, 6> ci{};
      std::array<std::size_t, 2> ai{};
      for (int k = 0; k < 6; ++k)
        combs[std::size_t(k)].assign(std::size_t(kCombs[k] + spread), 0.0f);
      for (int k = 0; k < 2; ++k)
        alls[std::size_t(k)].assign(std::size_t(kAll[k] + spread), 0.0f);
      auto& dst = side == 0 ? out.l : out.r;
      // Warm the tank up on the loop's last few seconds so the tail wraps
      // into the start.
      const int warm = std::min(n, samples(4.0));
      for (int k = n - warm; k < 2 * n; ++k)
        {
          const int i = k < n ? k : k - n;
          const bool pass = k >= n;
          const float x = bus.rev[std::size_t(i)] * 0.02f;
          float y = 0.0f;
          for (std::size_t k = 0; k < 6; ++k)
          {
            auto& buf = combs[k];
            const float o = buf[ci[k]];
            store[k] = o * 0.75f + store[k] * 0.25f;
            buf[ci[k]] = x + store[k] * room;
            if (++ci[k] == buf.size())
              ci[k] = 0;
            y += o;
          }
          for (std::size_t k = 0; k < 2; ++k)
          {
            auto& buf = alls[k];
            const float o = buf[ai[k]];
            buf[ai[k]] = y + o * 0.5f;
            if (++ai[k] == buf.size())
              ai[k] = 0;
            y = o - y;
          }
          if (pass)
            dst[std::size_t(i)] += y * (0.6f + st.reverb);
        }
    }
  }
  if (st.extras & kUnder)
  {
    OnePole lpL, lpR;
    for (int pass = 0; pass < 2; ++pass)
      for (std::size_t i = 0; i < out.l.size(); ++i)
      {
        const float a = lpL.lowpass(out.l[i], 1100.0), c = lpR.lowpass(out.r[i], 1100.0);
        if (pass == 1)
        {
          out.l[i] = a * 1.4f;
          out.r[i] = c * 1.4f;
        }
      }
  }
  // Same loudness for every track: scale to a target RMS, then soft clip.
  double sum = 0.0;
  for (std::size_t i = 0; i < out.l.size(); ++i)
    sum += double(out.l[i]) * out.l[i] + double(out.r[i]) * out.r[i];
  const double rms = std::sqrt(sum / std::max<std::size_t>(1, out.l.size() * 2));
  const float gain = float(std::clamp(0.3 / std::max(1e-6, rms), 0.2, 8.0));
  for (std::size_t i = 0; i < out.l.size(); ++i)
  {
    out.l[i] = std::tanh(out.l[i] * gain);
    out.r[i] = std::tanh(out.r[i] * gain);
  }
  return out;
}

void effects(Bus& b, const Style& st, double stepSec, std::uint32_t seed)
{
  Noise rng(seed ^ 0x5bd1e995u);
  auto rnd = [&]() { return (rng.next() + 1.0f) * 0.5f; };
  const double total = double(b.n) / kRate;
  if (st.extras & kVinyl)
  {
    Noise n(seed);
    OnePole lp;
    for (int i = 0; i < b.n; ++i)
    {
      float v = lp.lowpass(n.next(), 4000.0) * 0.012f;
      if (rnd() < 0.0004f)
        v += (rng.next()) * 0.25f;
      b.add(i, v, 0.0f, 0.0f, 0.0f);
    }
  }
  if (st.extras & kRain)
  {
    Noise n(seed + 3u);
    OnePole lp, hp;
    for (int i = 0; i < b.n; ++i)
      b.add(i, hp.highpass(lp.lowpass(n.next(), 5000.0), 800.0) * 0.06f, 0.0f, 0.0f, 0.0f);
    const double bar = stepSec * st.meter * 4;
    for (double t = bar * 6.5; t < total; t += bar * 8)
    {
      Noise nn(seed + std::uint32_t(t * 100));
      Svf lpf;
      emit(b, t, 3.0, Send{0.6f, 0.2f, 0.4f, 0.0f}, [&](double x) {
        return lpf.low(nn.next(), sweep(1800.0, 90.0, x / 3.0), 1.0) * attack(x, 0.01) * fdecay(x, 0.9);
      });
    }
  }
  if (st.extras & kUnder)
  {
    for (int k = 0; k < int(total * 1.5); ++k)
    {
      const double t0 = rnd() * total, f0 = 300.0 + 500.0 * rnd();
      double ph = 0.0;
      emit(b, t0, 0.08, Send{0.08f, rng.next() * 0.6f, 0.3f, 0.0f}, [&](double t) {
        ph += f0 * (1.0 + t * 25.0) / kRate;
        return sin2pi(ph) * std::sin(kPi * t / 0.08);
      });
    }
  }
  if (st.extras & kBreath)
  {
    Noise n(seed + 5u);
    Svf bp;
    const double period = stepSec * st.meter * 4 * 2;
    for (int i = 0; i < b.n; ++i)
    {
      const double t = double(i) / kRate;
      const double ph = std::fmod(t, period) / period;
      const double env = std::pow(std::sin(kPi * ph), 2.0);
      b.add(i, bp.band(n.next(), 500.0 + 300.0 * env, 1.2) * float(env) * 0.18f, 0.0f, 0.3f, 0.0f);
    }
  }
  if (st.extras & kStutter)
  {
    // Repeat the sixteenth before a few steps, as if the tape skipped.
    const int step = samples(stepSec);
    for (int s = 1; s < b.n / step; ++s)
    {
      if (rnd() > 0.06f)
        continue;
      for (int i = 0; i < step; ++i)
      {
        const auto dst = std::size_t(s * step + i), src = std::size_t((s - 1) * step + i);
        if (dst >= b.l.size())
          break;
        b.l[dst] = b.l[src];
        b.r[dst] = b.r[src];
      }
    }
  }
}

// The finale's medley: four bars of the lead's section from tracks across
// the campaign, one after another.
Stereo medley()
{
  static const char* kParts[] = {"theme_synthwave", "theme_marimba", "theme_brass_bells", "theme_western",
                                 "theme_accordion", "theme_noir_trumpet", "theme_rock_storm", "theme_heroic_build"};
  Stereo out;
  for (const char* id : kParts)
  {
    Options o;
    o.bars = 4;
    o.intro = 0;
    const Stereo part = render(*findStyle(id), id, o);
    out.l.insert(out.l.end(), part.l.begin(), part.l.end());
    out.r.insert(out.r.end(), part.r.begin(), part.r.end());
  }
  return out;
}

void clubhouseDrop(Stereo& mix, double bpm)
{
  // Level 3's phrase: the riser sweeps up through bars 15-16 and the drop
  // is three sub booms on beat 1 of bars 1-3 (world_club.cpp follows it).
  const double bar = 4.0 * 60.0 / bpm;
  const int n = int(mix.l.size());
  for (int k = 0; k < 3; ++k)
  {
    const int at = samples(k * bar), len = samples(0.7);
    double ph = 0.0;
    for (int i = 0; i < len && at + i < n; ++i)
    {
      const double t = double(i) / kRate;
      ph += 55.0 * (1.0 + 2.0 * fexp(-t * 18.0)) / kRate;
      const float v = float(sin2pi(ph) * 0.6 * fexp(-t * 4.0));
      mix.l[std::size_t(at + i)] = std::tanh(mix.l[std::size_t(at + i)] + v);
      mix.r[std::size_t(at + i)] = std::tanh(mix.r[std::size_t(at + i)] + v);
    }
  }
  Osc saw;
  Noise noise(77u);
  const int from = samples(14 * bar), to = std::min(n, samples(16 * bar));
  for (int i = from; i < to; ++i)
  {
    const double u = double(i - from) / double(to - from);
    const float v = saw.step(180.0 * std::pow(10.0, u * 1.1), Wave::Saw) * float(0.10 * u) + noise.next() * float(0.07 * u * u);
    const float pan = float(std::sin(u * 20.0) * 0.4);
    mix.l[std::size_t(i)] = std::tanh(mix.l[std::size_t(i)] + v * (1.0f - std::max(0.0f, pan)));
    mix.r[std::size_t(i)] = std::tanh(mix.r[std::size_t(i)] + v * (1.0f + std::min(0.0f, pan)));
  }
}

} // namespace

MusicTrack makeStyledMusic(const std::string& id)
{
  Options opt;
  // "track@N": the same track with only its first N layers (level 4).
  const auto atSign = id.find('@');
  std::string base = id.substr(0, atSign);
  if (atSign != std::string::npos)
    opt.layers = std::atoi(id.c_str() + atSign + 1);
  // "elevator_track": a lounge cover of that track (same chords and tune).
  if (base.rfind("elevator_", 0) == 0)
  {
    base = base.substr(9);
    opt.cover = true;
  }
  Stereo mix;
  if (base == "theme_medley" && !opt.cover)
    mix = medley();
  else
  {
    const Style* known = findStyle(base);
    const Style st = known ? *known : derivedStyle(base);
    mix = render(st, base, opt);
    if (!opt.cover && (st.extras & kRiser))
      clubhouseDrop(mix, st.bpm);
  }
  MusicTrack out;
  out.left = std::move(mix.l);
  out.right = std::move(mix.r);
  out.loops = true;
  return out;
}

} // namespace gr::synth
