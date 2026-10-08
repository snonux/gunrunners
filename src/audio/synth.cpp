// Procedural sound: every effect and the music are synthesized at startup
// from oscillators, noise, filters and envelopes. Nothing is sampled, so the
// soundtrack is original and ships under the game's GPL license.

#include "audio/synth.hpp"

#include "audio/dsp.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace gr::synth
{


std::vector<float> makeSfx(Sfx id)
{
  switch (id)
  {
    case Sfx::Jump:
    {
      Osc o;
      OnePole lp;
      return render(0.2, [&](double t, double) {
        const double f = sweep(180.0, 620.0, t / 0.16);
        return lp.lowpass(o.step(f, Wave::Square, 0.35), 3500.0) * 0.32 * attack(t) * decay(t, 0.08);
      });
    }
    case Sfx::Land:
    {
      Osc o;
      Noise n(11);
      OnePole lp;
      return render(0.14, [&](double t, double) {
        const double thump = o.step(sweep(110.0, 45.0, t / 0.1), Wave::Sine) * decay(t, 0.045);
        const double dust = lp.lowpass(n.next(), 900.0) * decay(t, 0.03);
        return (thump * 0.55 + dust * 0.5) * attack(t);
      });
    }
    case Sfx::Shot:
    {
      Osc o;
      Noise n(21);
      OnePole hp;
      return render(0.16, [&](double t, double) {
        const double f = sweep(1500.0, 260.0, t / 0.12);
        const double tone = o.step(f, Wave::Square, 0.25) * decay(t, 0.05);
        const double click = hp.highpass(n.next(), 2000.0) * decay(t, 0.008);
        return (tone * 0.26 + click * 0.35) * attack(t, 0.001);
      });
    }
    case Sfx::LaserShot:
    {
      Osc a, b;
      OnePole hp;
      return render(0.26, [&](double t, double) {
        const double f = sweep(2600.0, 480.0, t / 0.2);
        const double v = a.step(f, Wave::Saw) * 0.6 + b.step(f * 1.505, Wave::Sine) * 0.4;
        return hp.highpass(float(v), 300.0) * 0.28 * attack(t, 0.001) * decay(t, 0.09);
      });
    }
    case Sfx::RocketShot:
    {
      Noise n(31);
      Svf bp;
      Osc o;
      return render(0.5, [&](double t, double) {
        const double whoosh = bp.band(n.next(), sweep(300.0, 2400.0, t / 0.4), 2.0) * decay(t, 0.18);
        const double boom = o.step(sweep(90.0, 40.0, t / 0.3), Wave::Sine) * decay(t, 0.09);
        return (whoosh * 0.55 + boom * 0.6) * attack(t, 0.004);
      });
    }
    case Sfx::FlameShot:
    {
      Noise n(41);
      Svf bp;
      return render(0.18, [&](double t, double) {
        const double crackle = n.next() > 0.92f ? 1.0 : 0.0;
        const double roar = bp.band(n.next(), 700.0 + 300.0 * std::sin(t * 90.0), 1.2);
        return (roar * 0.55 + crackle * 0.15) * attack(t, 0.01) * decay(t, 0.07);
      });
    }
    case Sfx::EnemyShot:
    {
      Osc o, lfo;
      return render(0.2, [&](double t, double) {
        const double trem = 0.6 + 0.4 * lfo.step(38.0, Wave::Sine);
        return o.step(sweep(1100.0, 280.0, t / 0.18), Wave::Triangle) * trem * 0.3 * attack(t) * decay(t, 0.08);
      });
    }
    case Sfx::Hit:
    {
      Osc o;
      Noise n(51);
      return render(0.09, [&](double t, double) {
        return (o.step(sweep(420.0, 160.0, t / 0.08), Wave::Square, 0.4) * 0.2 + n.next() * 0.2 * decay(t, 0.01)) *
          attack(t, 0.001) * decay(t, 0.04);
      });
    }
    case Sfx::Explosion:
    case Sfx::SmallExplosion:
    {
      const bool big = id == Sfx::Explosion;
      const double len = big ? 0.9 : 0.45;
      Noise n(big ? 61 : 67);
      Svf lp;
      Osc o;
      double held = 0.0;
      int counter = 0;
      return render(len, [&](double t, double total) {
        // Sample-and-hold noise gives the crunchy rumble.
        if (counter-- <= 0)
        {
          held = n.next();
          counter = int(lerp(2.0, 14.0, t / total));
        }
        const double rumble = lp.low(float(held * 0.6 + n.next() * 0.4), sweep(3200.0, 160.0, t / total), 1.1);
        const double thump = o.step(sweep(80.0, 30.0, t / 0.4), Wave::Sine) * decay(t, 0.12);
        return (rumble * 0.7 + thump * 0.6) * attack(t, 0.002) * decay(t, big ? 0.28 : 0.13);
      });
    }
    case Sfx::BoxBreak:
    {
      Noise n(71);
      Svf bp;
      Osc o;
      return render(0.25, [&](double t, double) {
        const double crunch = bp.band(n.next(), 1400.0, 1.5) * (0.6 + 0.4 * std::sin(t * 260.0));
        const double knock = o.step(sweep(240.0, 140.0, t / 0.1), Wave::Triangle) * decay(t, 0.03);
        return (crunch * 0.5 + knock * 0.4) * attack(t, 0.001) * decay(t, 0.07);
      });
    }
    case Sfx::Gem:
    {
      std::vector<float> b(std::size_t(samples(0.3)));
      addTone(b, 0.0, 0.3, midiFreq(88), Wave::Sine, 0.25, 0.08);
      addTone(b, 0.0, 0.3, midiFreq(100), Wave::Sine, 0.08, 0.05);
      addTone(b, 0.06, 0.24, midiFreq(93), Wave::Sine, 0.25, 0.09);
      addTone(b, 0.06, 0.24, midiFreq(105), Wave::Sine, 0.08, 0.05);
      return b;
    }
    case Sfx::Item:
    case Sfx::Tally:
    {
      const bool tally = id == Sfx::Tally;
      std::vector<float> b(std::size_t(samples(tally ? 0.35 : 0.4)));
      const int notes[4] = {72, 76, 79, 84};
      for (int i = 0; i < (tally ? 2 : 4); ++i)
      {
        const double start = i * (tally ? 0.07 : 0.05);
        addTone(b, start, 0.25, midiFreq(notes[i] + (tally ? 7 : 0)), Wave::Square, 0.12, 0.07);
        addTone(b, start, 0.25, midiFreq(notes[i] + 12 + (tally ? 7 : 0)), Wave::Sine, 0.1, 0.09);
      }
      return b;
    }
    case Sfx::Health:
    {
      Osc a, b;
      return render(0.4, [&](double t, double) {
        const double f = sweep(330.0, 990.0, t / 0.3);
        return (a.step(f, Wave::Sine) * 0.3 + b.step(f * 2.0, Wave::Triangle) * 0.1) * attack(t, 0.01) * decay(t, 0.15);
      });
    }
    case Sfx::WeaponPickup:
    {
      std::vector<float> b(std::size_t(samples(0.7)));
      Noise n(81);
      for (std::size_t i = 0; i < std::size_t(samples(0.06)); ++i)
        b[i] += n.next() * 0.3f * float(decay(double(i) / kRate, 0.015));
      const int notes[3] = {57, 64, 69};
      for (int i = 0; i < 3; ++i)
      {
        addTone(b, 0.05 + i * 0.07, 0.6 - i * 0.07, midiFreq(notes[i]), Wave::Saw, 0.13, 0.25);
        addTone(b, 0.05 + i * 0.07, 0.6 - i * 0.07, midiFreq(notes[i] + 12), Wave::Square, 0.06, 0.2);
      }
      OnePole lp;
      for (auto& s : b)
        s = lp.lowpass(s, 4000.0);
      return b;
    }
    case Sfx::Letter:
    {
      std::vector<float> b(std::size_t(samples(0.5)));
      const int notes[3] = {76, 79, 83};
      for (int i = 0; i < 3; ++i)
        addTone(b, i * 0.04, 0.45, midiFreq(notes[i]), Wave::Triangle, 0.18, 0.18);
      return b;
    }
    case Sfx::LettersComplete:
    {
      std::vector<float> b(std::size_t(samples(1.1)));
      const int notes[6] = {72, 76, 79, 84, 88, 91};
      for (int i = 0; i < 6; ++i)
      {
        addTone(b, i * 0.07, 1.0 - i * 0.07, midiFreq(notes[i]), Wave::Square, 0.08, 0.3);
        addTone(b, i * 0.07, 1.0 - i * 0.07, midiFreq(notes[i]), Wave::Triangle, 0.12, 0.35);
      }
      return b;
    }
    case Sfx::Hurt:
    {
      Osc o, lfo;
      Noise n(91);
      OnePole lp;
      return render(0.25, [&](double t, double) {
        const double f = sweep(460.0, 110.0, t / 0.22) * (1.0 + 0.06 * lfo.step(30.0, Wave::Sine));
        const double v = o.step(f, Wave::Square, 0.3) * 0.7 + n.next() * 0.3 * decay(t, 0.03);
        return lp.lowpass(float(v), 2500.0) * 0.32 * attack(t) * decay(t, 0.1);
      });
    }
    case Sfx::Death:
    {
      Osc o, lfo;
      OnePole lp;
      return render(0.9, [&](double t, double) {
        const double f = sweep(700.0, 70.0, t / 0.85) * (1.0 + 0.08 * lfo.step(12.0, Wave::Sine));
        return lp.lowpass(o.step(f, Wave::Square, 0.5), 3000.0) * 0.28 * attack(t) * decay(t, 0.4);
      });
    }
    case Sfx::AttachClimbable:
    {
      Osc a, b;
      Noise n(101);
      return render(0.18, [&](double t, double) {
        const double ring = a.step(2350.0, Wave::Sine) * 0.5 + b.step(3720.0, Wave::Sine) * 0.3;
        return (ring * decay(t, 0.04) + n.next() * 0.3 * decay(t, 0.004)) * 0.35 * attack(t, 0.001);
      });
    }
    case Sfx::Key:
    {
      std::vector<float> b(std::size_t(samples(0.5)));
      addTone(b, 0.0, 0.25, midiFreq(91), Wave::Sine, 0.25, 0.08);
      addTone(b, 0.1, 0.4, midiFreq(96), Wave::Sine, 0.25, 0.12);
      addTone(b, 0.1, 0.4, midiFreq(108), Wave::Sine, 0.06, 0.06);
      return b;
    }
    case Sfx::ForceFieldOff:
    {
      Osc o, lfo;
      Svf lp;
      return render(0.9, [&](double t, double) {
        const double f = sweep(900.0, 60.0, t / 0.8) * (1.0 + 0.03 * lfo.step(45.0, Wave::Sine));
        return lp.low(o.step(f, Wave::Saw), sweep(5000.0, 300.0, t / 0.8), 3.0) * 0.3 * attack(t) * decay(t, 0.35);
      });
    }
    case Sfx::Checkpoint:
    {
      std::vector<float> b(std::size_t(samples(0.9)));
      const int notes[4] = {67, 71, 74, 79};
      for (int i = 0; i < 4; ++i)
        addTone(b, i * 0.08, 0.8 - i * 0.08, midiFreq(notes[i]), Wave::Triangle, 0.16, 0.3);
      addTone(b, 0.32, 0.55, midiFreq(91), Wave::Sine, 0.08, 0.25);
      return b;
    }
    case Sfx::Teleport:
    {
      Osc a, b, lfo;
      return render(1.4, [&](double t, double total) {
        const double f = sweep(200.0, 1600.0, t / total);
        const double trem = 0.5 + 0.5 * lfo.step(lerp(8.0, 30.0, t / total), Wave::Sine);
        const double v = a.step(f, Wave::Sine) * 0.6 + b.step(f * 1.5, Wave::Triangle) * 0.25;
        return v * trem * 0.3 * std::min(1.0, t / 0.1) * std::min(1.0, (total - t) / 0.3);
      });
    }
    case Sfx::MenuMove:
    {
      Osc o;
      return render(0.06, [&](double t, double) { return o.step(880.0, Wave::Square, 0.5) * 0.12 * attack(t) * decay(t, 0.02); });
    }
    case Sfx::MenuSelect:
    {
      std::vector<float> b(std::size_t(samples(0.3)));
      addTone(b, 0.0, 0.1, midiFreq(76), Wave::Square, 0.14, 0.05);
      addTone(b, 0.08, 0.22, midiFreq(88), Wave::Square, 0.14, 0.08);
      return b;
    }
    case Sfx::TurboOn:
    {
      // A revving rise into a bright major arpeggio.
      Osc a, b;
      auto buf = render(0.9, [&](double t, double total) {
        const double f = sweep(110.0, 880.0, std::min(1.0, t / 0.5));
        const double v = a.step(f, Wave::Saw) * 0.35 + b.step(f * 1.005, Wave::Square, 0.3) * 0.2;
        return v * 0.35 * std::min(1.0, t / 0.02) * std::min(1.0, (total - t) / 0.3);
      });
      const int notes[4] = {72, 76, 79, 84};
      for (int i = 0; i < 4; ++i)
        addTone(buf, 0.45 + i * 0.07, 0.3, midiFreq(notes[i]), Wave::Square, 0.12, 0.12);
      return buf;
    }
    case Sfx::VirusOn:
    {
      // A sick, wobbling slide down with a gurgle of filtered noise.
      Osc a, b, lfo;
      Noise n(99);
      Svf f;
      return render(1.0, [&](double t, double total) {
        const double wob = 1.0 + 0.06 * lfo.step(7.0, Wave::Sine);
        const double fr = sweep(520.0, 90.0, t / total) * wob;
        const double tone = a.step(fr, Wave::Square, 0.25) * 0.3 + b.step(fr * 0.993 * 1.5, Wave::Saw) * 0.2;
        const double gurgle = f.band(n.next(), 300.0 + 200.0 * std::sin(t * 40.0), 0.7) * 0.5;
        return (tone + gurgle) * 0.32 * std::min(1.0, t / 0.01) * std::min(1.0, (total - t) / 0.25);
      });
    }
    case Sfx::EffectEnd:
    {
      std::vector<float> b(std::size_t(samples(0.35)));
      addTone(b, 0.0, 0.12, midiFreq(79), Wave::Triangle, 0.14, 0.06);
      addTone(b, 0.1, 0.25, midiFreq(72), Wave::Triangle, 0.14, 0.1);
      return b;
    }
    case Sfx::Count:
      break;
  }
  return {};
}

// --- Music -------------------------------------------------------------------

namespace
{

struct Song
{
  double bpm;
  int bars;
  std::vector<std::array<int, 3>> chords; // one triad (MIDI) per bar
  bool drums;
  bool lead;
  std::vector<int> melody; // one MIDI note (0 = rest, -1 = hold) per 8th note
  // Level 4's layered score: 0 is bass alone, then pad, drums, arpeggio and
  // lead come in one by one. A heartbeat thumps under it all.
  int layers = 9;
  bool heartbeat = false;
};

class Mixdown
{
public:
  Mixdown(double seconds) : left(std::size_t(samples(seconds)), 0.0f), right(left.size(), 0.0f) {}
  void add(int i, float v, float pan)
  {
    if (i < 0 || i >= int(left.size()))
      return;
    left[std::size_t(i)] += v * (1.0f - std::max(0.0f, pan));
    right[std::size_t(i)] += v * (1.0f + std::min(0.0f, pan));
  }
  std::vector<float> left, right;
};

void kick(Mixdown& m, int at, float vol)
{
  Osc o;
  for (int i = 0; i < samples(0.35); ++i)
  {
    const double t = double(i) / kRate;
    const double f = 45.0 + 110.0 * decay(t, 0.03);
    m.add(at + i, float(o.step(f, Wave::Sine) * decay(t, 0.12) * attack(t, 0.001)) * vol, 0.0f);
  }
}

void snare(Mixdown& m, int at, float vol, std::uint32_t seed)
{
  Noise n(seed);
  Svf bp;
  Osc o;
  for (int i = 0; i < samples(0.3); ++i)
  {
    const double t = double(i) / kRate;
    const double noise = bp.band(n.next(), 2400.0, 0.8) * decay(t, 0.07);
    const double body = o.step(190.0, Wave::Triangle) * decay(t, 0.04);
    m.add(at + i, float((noise * 0.9 + body * 0.5) * attack(t, 0.001)) * vol, 0.05f);
  }
}

void hat(Mixdown& m, int at, float vol, bool open, std::uint32_t seed)
{
  Noise n(seed);
  OnePole hp;
  for (int i = 0; i < samples(open ? 0.25 : 0.06); ++i)
  {
    const double t = double(i) / kRate;
    m.add(at + i, float(hp.highpass(n.next(), 7000.0) * decay(t, open ? 0.08 : 0.018)) * vol, -0.25f);
  }
}

Mixdown renderSong(const Song& s)
{
  const double beat = 60.0 / s.bpm;
  const double step = beat / 4.0; // 16th note
  const double total = s.bars * 4 * beat;
  Mixdown m(total + 0.01);
  const int n = int(m.left.size());
  std::uint32_t seed = 1234;

  // Drums.
  if (s.drums && s.layers >= 2)
  {
    for (int bar = 0; bar < s.bars; ++bar)
    {
      const bool fill = bar % 8 == 7;
      for (int st = 0; st < 16; ++st)
      {
        const int at = samples((bar * 16 + st) * step);
        if (st % 4 == 0)
          kick(m, at, 0.95f);
        if (st == 4 || st == 12 || (fill && (st == 14 || st == 15)))
          snare(m, at, st >= 14 ? 0.45f : 0.6f, ++seed);
        if (st % 2 == 0)
          hat(m, at, st % 4 == 2 ? 0.22f : 0.12f, st == 14 && bar % 2 == 1, ++seed);
        else
          hat(m, at, 0.06f, false, ++seed);
      }
    }
  }

  // Bass: driving octave eighths through a resonant filter.
  {
    Osc o1, o2;
    Svf f;
    for (int i = 0; i < n; ++i)
    {
      const double t = double(i) / kRate;
      const int bar = std::min(s.bars - 1, int(t / (4 * beat)));
      const double inStep = std::fmod(t, step * 2.0);
      const int eighth = int(t / (step * 2.0)) % 8;
      const int root = s.chords[std::size_t(bar % int(s.chords.size()))][0] - 24;
      const double note = root + (eighth % 2 == 1 ? 12 : 0);
      const double env = decay(inStep, 0.09) * std::min(1.0, inStep / 0.004) * std::min(1.0, (step * 2.0 - inStep) / 0.006);
      const double raw = o1.step(midiFreq(note), Wave::Saw) * 0.6 + o2.step(midiFreq(note) * 1.004, Wave::Square) * 0.4;
      const double cutoff = 220.0 + 1600.0 * env;
      const float v = f.low(float(raw), cutoff, 2.2) * float(0.35 * std::min(1.0, env * 3.0 + 0.15));
      m.add(i, v * (s.drums ? 1.0f : 0.6f), 0.0f);
    }
  }

  // The heartbeat: a soft lub-dub twice a bar.
  if (s.heartbeat)
    for (int b = 0; b < s.bars * 2; ++b)
    {
      const int at = samples(b * 2 * beat);
      for (int k = 0; k < 2; ++k)
      {
        Osc o;
        const int from = at + samples(k * 0.2), len = samples(0.25);
        for (int i = 0; i < len; ++i)
        {
          const double t = double(i) / kRate;
          const float v = o.step(48.0 * (1.0 + 0.8 * std::exp(-t * 30.0)), Wave::Sine) *
            float((k == 0 ? 0.5 : 0.35) * std::exp(-t * 14.0) * std::min(1.0, t / 0.004));
          m.add(from + i, v, 0.0f);
        }
      }
    }

  // Pad: detuned saws per chord, slowly swelling.
  if (s.layers >= 1)
  {
    std::array<Osc, 6> osc;
    OnePole lpL, lpR;
    for (int i = 0; i < n; ++i)
    {
      const double t = double(i) / kRate;
      const int bar = std::min(s.bars - 1, int(t / (4 * beat)));
      const double inBar = t - bar * 4 * beat;
      const auto& c = s.chords[std::size_t(bar % int(s.chords.size()))];
      double l = 0.0, r = 0.0;
      for (int k = 0; k < 3; ++k)
      {
        const double f = midiFreq(c[std::size_t(k)]);
        l += osc[std::size_t(k * 2)].step(f * 0.997, Wave::Saw);
        r += osc[std::size_t(k * 2 + 1)].step(f * 1.003, Wave::Saw);
      }
      const double env = std::min(1.0, inBar / 0.35) * std::min(1.0, (4 * beat - inBar) / 0.08);
      const double cutoff = 900.0 + 500.0 * std::sin(t * 0.7);
      const double vol = 0.055 * env;
      const float vl = lpL.lowpass(float(l), cutoff) * float(vol);
      const float vr = lpR.lowpass(float(r), cutoff) * float(vol);
      m.left[std::size_t(i)] += vl;
      m.right[std::size_t(i)] += vr;
    }
  }

  // Arpeggio with a ping-pong echo.
  if (s.layers >= 3)
  {
    std::vector<float> arp(std::size_t(n), 0.0f);
    Osc o;
    for (int i = 0; i < n; ++i)
    {
      const double t = double(i) / kRate;
      const int st = int(t / step);
      const int bar = std::min(s.bars - 1, st / 16);
      const auto& c = s.chords[std::size_t(bar % int(s.chords.size()))];
      static const int kPattern[8] = {0, 1, 2, 1, 0, 2, 1, 2};
      const int idx = kPattern[st % 8];
      const double note = c[std::size_t(idx)] + 12 + (st % 16 >= 12 ? 12 : 0);
      const double inStep = std::fmod(t, step);
      const double env = std::min(1.0, inStep / 0.003) * decay(inStep, 0.05);
      arp[std::size_t(i)] = o.step(midiFreq(note), Wave::Square, 0.3) * float(env) * 0.06f;
    }
    const int delay = samples(step * 3.0);
    for (int i = 0; i < n; ++i)
    {
      m.add(i, arp[std::size_t(i)], 0.0f);
      if (i >= delay)
        m.add(i, arp[std::size_t(i - delay)] * 0.45f, (i / delay) % 2 ? -0.7f : 0.7f);
      if (i >= delay * 2)
        m.add(i, arp[std::size_t(i - delay * 2)] * 0.2f, (i / delay) % 2 ? 0.7f : -0.7f);
    }
  }

  // Lead melody in the second half.
  if (s.lead && s.layers >= 4 && !s.melody.empty())
  {
    Osc a, b, vib;
    OnePole lp;
    double note = 0.0;
    double noteStart = 0.0;
    int lastEighth = -1;
    const int halfStart = s.bars / 2;
    for (int i = 0; i < n; ++i)
    {
      const double t = double(i) / kRate;
      const int eighth = int(t / (step * 2.0));
      if (eighth / 8 < halfStart)
        continue;
      if (eighth != lastEighth)
      {
        lastEighth = eighth;
        const int mel = s.melody[std::size_t((eighth - halfStart * 8) % int(s.melody.size()))];
        if (mel > 0)
        {
          note = mel;
          noteStart = t;
        }
        else if (mel == 0)
        {
          note = 0.0;
        }
      }
      if (note <= 0.0)
      {
        lp.lowpass(0.0f, 3000.0);
        continue;
      }
      const double age = t - noteStart;
      const double f = midiFreq(note) * (1.0 + 0.006 * vib.step(5.5, Wave::Sine) * std::min(1.0, age / 0.25));
      const double v = a.step(f, Wave::Saw) * 0.5 + b.step(f * 0.5, Wave::Square, 0.4) * 0.3;
      const double env = std::min(1.0, age / 0.01) * (0.7 + 0.3 * decay(age, 0.2));
      m.add(i, lp.lowpass(float(v), 2800.0) * float(env) * 0.09f, 0.1f);
    }
  }

  // Gentle bus compression: soft clip. Songs without drums get more gain so
  // the calm menu loop sits at a similar loudness to the level track.
  const float bus = s.drums ? 1.1f : 2.2f;
  for (int i = 0; i < n; ++i)
  {
    m.left[std::size_t(i)] = std::tanh(m.left[std::size_t(i)] * bus);
    m.right[std::size_t(i)] = std::tanh(m.right[std::size_t(i)] * bus);
  }
  return m;
}

} // namespace

MusicTrack makeMusic(Music m)
{
  // A minor synthwave: Am F C G | Am F Dm E
  const std::vector<std::array<int, 3>> chords = {
    {57, 60, 64}, {53, 57, 60}, {48, 52, 55}, {55, 59, 62},
    {57, 60, 64}, {53, 57, 60}, {50, 53, 57}, {52, 56, 59}};
  MusicTrack out;
  switch (m)
  {
    case Music::Menu:
    {
      Song s{96.0, 8, chords, false, false, {}};
      auto mix = renderSong(s);
      out.left = std::move(mix.left);
      out.right = std::move(mix.right);
      out.loops = true;
      break;
    }
    case Music::Level:
    {
      const std::vector<int> melody = {
        76, -1, 74, 72, -1, 71, 72, -1,   77, -1, 76, 74, -1, 72, 74, -1,
        72, -1, 71, 67, -1, 0, 67, 69,    71, -1, 74, -1, 71, -1, 67, -1,
        76, -1, 74, 72, -1, 71, 72, 76,   77, -1, 81, -1, 79, 77, 76, -1,
        74, -1, 72, 74, -1, 77, 76, -1,   76, -1, -1, -1, 0, 0, 0, 0};
      Song s{120.0, 16, chords, true, true, melody};
      auto mix = renderSong(s);
      out.left = std::move(mix.left);
      out.right = std::move(mix.right);
      out.loops = true;
      break;
    }
    case Music::Victory:
    {
      // A short fanfare: rising arpeggio into a held major chord.
      const double len = 4.0;
      out.left.assign(std::size_t(samples(len)), 0.0f);
      const int notes[8] = {60, 64, 67, 72, 64, 67, 72, 76};
      for (int i = 0; i < 8; ++i)
      {
        addTone(out.left, i * 0.11, 0.3, midiFreq(notes[i]), Wave::Square, 0.08, 0.12);
        addTone(out.left, i * 0.11, 0.3, midiFreq(notes[i] + 12), Wave::Triangle, 0.06, 0.12);
      }
      for (int n : {60, 64, 67, 72, 76})
      {
        addTone(out.left, 0.95, 2.9, midiFreq(n), Wave::Saw, 0.05, 1.4);
        addTone(out.left, 0.95, 2.9, midiFreq(n - 12), Wave::Triangle, 0.06, 1.6);
      }
      Mixdown drums(len);
      for (int i = 0; i < 4; ++i)
        snare(drums, samples(0.55 + i * 0.1), 0.3f + 0.1f * float(i), 900u + std::uint32_t(i));
      kick(drums, samples(0.95), 1.0f);
      OnePole lp;
      out.right = out.left;
      for (std::size_t i = 0; i < out.left.size(); ++i)
      {
        const float v = lp.lowpass(out.left[i], 5000.0);
        out.left[i] = std::tanh((v + drums.left[i]) * 1.1f);
        out.right[i] = std::tanh((v + drums.right[i]) * 1.1f);
      }
      out.loops = false;
      break;
    }
    case Music::None:
    case Music::Count:
      break;
  }
  return out;
}

} // namespace gr::synth

namespace gr::synth
{

namespace
{

std::uint32_t hashName(const std::string& s)
{
  std::uint32_t h = 2166136261u;
  for (char c : s)
    h = (h ^ std::uint32_t(static_cast<unsigned char>(c))) * 16777619u;
  return h | 1u;
}

bool contains(const std::string& s, const char* k) { return s.find(k) != std::string::npos; }

// A lead line made from the chord tones, so every track gets its own tune.
std::vector<int> makeMelody(const std::vector<std::array<int, 3>>& chords, std::uint32_t seed)
{
  Noise n(seed);
  std::vector<int> mel;
  for (std::size_t bar = 0; bar < 8; ++bar)
  {
    const auto& c = chords[bar % chords.size()];
    for (int e = 0; e < 8; ++e)
    {
      const float r = (n.next() + 1.0f) * 0.5f;
      if (e > 0 && r < 0.3f)
        mel.push_back(-1); // hold
      else if (r < 0.38f)
        mel.push_back(0); // rest
      else
        mel.push_back(c[std::size_t(int(r * 9.0f) % 3)] + 12 + (r > 0.85f ? 12 : 0));
    }
  }
  return mel;
}

} // namespace

MusicTrack makeNamedMusic(const std::string& id)
{
  if (id.empty() || id == "theme_synthwave")
    return makeMusic(Music::Level);
  if (id == "theme_diner" || id == "menu")
    return makeMusic(Music::Menu);
  if (id == "victory")
    return makeMusic(Music::Victory);

  using C = std::vector<std::array<int, 3>>;
  static const C kProgressions[] = {
    {{57, 60, 64}, {53, 57, 60}, {48, 52, 55}, {55, 59, 62}},                         // Am F C G
    {{50, 53, 57}, {58, 62, 65}, {53, 57, 60}, {48, 52, 55}},                         // Dm Bb F C
    {{52, 55, 59}, {48, 52, 55}, {55, 59, 62}, {50, 54, 57}},                         // Em C G D
    {{57, 60, 64}, {55, 59, 62}, {53, 57, 60}, {52, 56, 59}},                         // Am G F E
    {{48, 52, 55}, {57, 60, 64}, {53, 57, 60}, {55, 59, 62}},                         // C Am F G
    {{50, 53, 57}, {48, 52, 55}, {46, 50, 53}, {45, 49, 52}},                         // Dm C Bb A
    {{55, 58, 62}, {51, 55, 58}, {53, 57, 60}, {50, 54, 57}},                         // Gm Eb F D
  };
  // "track@N": the same track with only its first N layers (level 4).
  const auto at = id.find('@');
  const std::string base = id.substr(0, at);
  const std::uint32_t seed = hashName(base);
  const C& chords = kProgressions[seed % (sizeof(kProgressions) / sizeof(kProgressions[0]))];
  Song s{120.0, 16, chords, true, true, makeMelody(chords, seed)};
  if (contains(id, "episode_end") || contains(id, "lonely") || contains(id, "organ") || contains(id, "credits"))
  {
    s.bpm = 90.0;
    s.drums = false;
    s.bars = 8;
  }
  else if (contains(id, "ambient") || contains(id, "drone") || contains(id, "breathing") || contains(id, "bells") ||
           contains(id, "submerged") || contains(id, "cryo"))
  {
    s.drums = false;
  }
  else if (contains(id, "opening") || contains(id, "finale") || contains(id, "heroic") || contains(id, "medley"))
  {
    s.lead = true;
  }
  if (at != std::string::npos)
  {
    s.layers = std::atoi(id.c_str() + at + 1);
    s.lead = true;
  }
  s.heartbeat = contains(id, "heartbeat");
  auto mix = renderSong(s);
  if (contains(id, "clubhouse"))
  {
    // Level 3's phrase: the riser sweeps up through bars 15-16 and the drop
    // is three sub booms on beat 1 of bars 1-3 (world_club.cpp follows it).
    const double bar = 4.0 * 60.0 / s.bpm;
    for (int k = 0; k < 3; ++k)
    {
      kick(mix, samples(k * bar), 1.0f);
      Osc o;
      const int at = samples(k * bar), len = samples(0.7);
      for (int i = 0; i < len && at + i < int(mix.left.size()); ++i)
      {
        const double t = double(i) / kRate;
        const float v = o.step(55.0 * (1.0 + 2.0 * std::exp(-t * 18.0)), Wave::Sine) * float(0.55 * std::exp(-t * 4.0));
        mix.add(at + i, v, 0.0f);
      }
    }
    Osc saw;
    Noise noise(77u);
    const int from = samples(14 * bar), to = samples(16 * bar);
    for (int i = from; i < to && i < int(mix.left.size()); ++i)
    {
      const double u = double(i - from) / double(to - from);
      const float v = saw.step(180.0 * std::pow(10.0, u * 1.1), Wave::Saw) * float(0.10 * u) + noise.next() * float(0.07 * u * u);
      mix.add(i, v, float(std::sin(u * 20.0) * 0.4));
    }
  }
  MusicTrack out;
  out.left = std::move(mix.left);
  out.right = std::move(mix.right);
  out.loops = true;
  return out;
}

} // namespace gr::synth
