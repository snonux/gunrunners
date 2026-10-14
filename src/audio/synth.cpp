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

namespace
{

// A honky-tonk piano note: each key has three strings tuned a little apart
// (the beating is the saloon sound), struck hard, with a woody knock.
void addPiano(std::vector<float>& buf, double start, double len, double midi, double vol)
{
  const double f = midiFreq(midi);
  Osc s1, s2, s3, h1, h2;
  Noise n(std::uint32_t(midi * 97.0 + start * 1000.0) | 1u);
  OnePole lp;
  const int s0 = samples(start);
  const int count = samples(len);
  for (int i = 0; i < count && s0 + i < int(buf.size()); ++i)
  {
    const double t = double(i) / kRate;
    const double env = attack(t, 0.002) * (0.65 * decay(t, 0.18) + 0.35 * decay(t, 0.9)) * std::min(1.0, (len - t) / 0.02);
    const double strings = s1.step(f * 0.994, Wave::Triangle) + s2.step(f, Wave::Triangle) + s3.step(f * 1.007, Wave::Triangle);
    const double bright = (h1.step(f * 2.003, Wave::Sine) * 0.35 + h2.step(f * 3.01, Wave::Sine) * 0.18) * decay(t, 0.08);
    const double knock = lp.lowpass(n.next(), 1800.0) * decay(t, 0.006);
    buf[std::size_t(s0 + i)] += float((strings * 0.33 + bright + knock * 0.8) * env * vol);
  }
}

} // namespace

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
    case Sfx::Siren:
    {
      // Two slow up-and-down wails.
      Osc a;
      return render(2.2, [&](double t, double total) {
        const double fr = 420.0 + 260.0 * (0.5 - 0.5 * std::cos(t * 2.0 * 3.14159 / 1.1));
        return a.step(fr, Wave::Square, 0.4) * 0.16 * std::min(1.0, t / 0.05) * std::min(1.0, (total - t) / 0.3);
      });
    }
    case Sfx::Splash:
    {
      Noise n(311);
      Svf f;
      return render(0.45, [&](double t, double total) {
        const double cut = sweep(2400.0, 300.0, t / total);
        return f.low(n.next(), float(cut), 1.2) * 0.5 * std::exp(-t * 7.0) * std::min(1.0, t / 0.005);
      });
    }
    case Sfx::Bubble:
    {
      Osc a;
      return render(0.16, [&](double t, double total) {
        const double fr = sweep(300.0, 1300.0, t / total);
        return a.step(fr, Wave::Sine) * 0.35 * std::min(1.0, t / 0.004) * std::min(1.0, (total - t) / 0.03);
      });
    }
    case Sfx::Bloop:
    {
      Osc a;
      return render(0.35, [&](double t, double total) {
        const double fr = sweep(900.0, 110.0, t / total);
        return a.step(fr, Wave::Sine) * 0.4 * std::min(1.0, t / 0.004) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::Klaxon:
    {
      Osc a, b;
      return render(0.9, [&](double t, double total) {
        const bool on = std::fmod(t, 0.3) < 0.2;
        const double v = a.step(330.0, Wave::Saw) * 0.5 + b.step(392.0, Wave::Square, 0.5) * 0.4;
        return on ? v * 0.14 * std::min(1.0, (total - t) / 0.05) : 0.0;
      });
    }
    case Sfx::Rattle:
    {
      // Something scrabbling inside a metal pipe.
      Noise n(97);
      Svf f;
      return render(1.0, [&](double t, double total) {
        const double tick = std::fmod(t, 0.07) < 0.025 ? 1.0 : 0.25;
        return f.band(n.next(), float(900.0 + 400.0 * std::sin(t * 30.0)), 1.5) * 0.45 * tick *
          std::min(1.0, (total - t) / 0.1);
      });
    }
    case Sfx::Quack:
    {
      Osc a, b;
      return render(0.22, [&](double t, double total) {
        const double fr = sweep(620.0, 430.0, t / total);
        return (a.step(fr, Wave::Saw) * 0.5 + b.step(fr * 2.01, Wave::Square, 0.3) * 0.25) * 0.22 *
          std::min(1.0, t / 0.01) * std::min(1.0, (total - t) / 0.04);
      });
    }
    case Sfx::Warn:
    {
      Osc a;
      return render(0.12, [&](double t, double total) {
        return a.step(1180.0, Wave::Square, 0.5) * 0.12 * std::min(1.0, (total - t) / 0.02);
      });
    }
    case Sfx::Chime:
    {
      // The station's three-note chime.
      std::vector<float> b(std::size_t(samples(1.1)));
      addTone(b, 0.0, 0.5, midiFreq(76), Wave::Sine, 0.22, 0.4);
      addTone(b, 0.25, 0.5, midiFreq(72), Wave::Sine, 0.22, 0.4);
      addTone(b, 0.5, 0.6, midiFreq(67), Wave::Sine, 0.22, 0.5);
      return b;
    }
    case Sfx::Clunk:
    {
      Noise n(53);
      Svf f;
      Osc a;
      return render(0.25, [&](double t, double total) {
        (void)total;
        return (f.low(n.next(), 900.0f, 1.0) * 0.5 + a.step(sweep(160.0, 70.0, t / 0.25), Wave::Square, 0.5) * 0.3) *
          std::exp(-t * 18.0);
      });
    }
    case Sfx::Zap:
    {
      Noise n(29);
      Svf f;
      Osc a;
      return render(0.3, [&](double t, double total) {
        const double crackle = (n.next() > 0.6f ? 1.0 : -0.3) * 0.4;
        const double fr = 1800.0 + 900.0 * std::sin(t * 210.0);
        return (f.band(float(crackle), float(fr), 2.0) * 0.8 + a.step(sweep(2400.0, 300.0, t / total), Wave::Saw) * 0.15) *
          std::min(1.0, t / 0.003) * std::min(1.0, (total - t) / 0.08);
      });
    }
    case Sfx::Beep:
    {
      Osc a;
      return render(0.08, [&](double t, double total) {
        return a.step(1760.0, Wave::Square, 0.5) * 0.14 * std::min(1.0, (total - t) / 0.01);
      });
    }
    case Sfx::Whistle:
    {
      Osc a;
      return render(0.5, [&](double t, double total) {
        return a.step(sweep(2600.0, 900.0, t / total), Wave::Sine) * 0.16 * std::min(1.0, t / 0.05);
      });
    }
    case Sfx::Scream:
    {
      Osc a, b;
      Noise n(71);
      return render(1.4, [&](double t, double total) {
        const double f = sweep(220.0, 900.0, t / total);
        return (a.step(f, Wave::Saw) * 0.16 + b.step(f * 1.01, Wave::Square, 0.3) * 0.08 + n.next() * 0.05) *
          std::min(1.0, t / 0.2) * std::min(1.0, (total - t) / 0.1);
      });
    }
    case Sfx::Rev:
    {
      Osc a;
      return render(0.6, [&](double t, double total) {
        const double f = 70.0 + 140.0 * std::sin(std::min(1.0, t / total) * 3.1416);
        return a.step(f, Wave::Saw) * 0.2 * std::min(1.0, (total - t) / 0.08);
      });
    }
    case Sfx::Creak:
    {
      // Wood under strain: stick-slip - the boards catch and let go a
      // hundred-odd times a second, the rate sagging and picking up - each
      // slip ringing the timber's two hollow resonances, over a low groan.
      Svf body, knot;
      Osc groan;
      Noise n(81);
      double phase = 0.0;
      return render(0.6, [&](double t, double total) {
        const double u = t / total;
        const double rate = 70.0 + 120.0 * std::sin(kPi * u) + 25.0 * std::sin(t * 17.0);
        phase += rate / kRate;
        double slip = 0.0;
        if (phase >= 1.0)
        {
          phase -= 1.0;
          slip = 1.0 + 0.4 * double(n.next());
        }
        const double grit = double(n.next()) * 0.08;
        const double wood = body.band(float(slip + grit), 620.0 + 80.0 * std::sin(t * 9.0), 9.0) * 0.55 +
          knot.band(float(slip), 1450.0, 12.0) * 0.25;
        const double low = groan.step(rate * 0.5, Wave::Saw) * 0.04;
        return (wood * 1.8 + low) * std::min(1.0, t / 0.04) * std::min(1.0, (total - t) / 0.1);
      });
    }
    case Sfx::Chop:
    {
      Noise n(83);
      Osc a;
      return render(0.14, [&](double t, double total) {
        const double e = std::exp(-t * 40.0);
        return (n.next() * 0.35 + a.step(sweep(420.0, 180.0, t / total), Wave::Square, 0.5) * 0.12) * e;
      });
    }
    case Sfx::Hoot:
    {
      // Two monkey hoots, each sliding up.
      Osc a;
      return render(0.42, [&](double t, double) {
        const double k = std::fmod(t, 0.21) / 0.21;
        const double f = 380.0 + 420.0 * k;
        return a.step(f, Wave::Sine) * 0.22 * std::sin(k * 3.1416);
      });
    }
    case Sfx::Rustle:
    {
      Noise n(85);
      double lp = 0.0;
      return render(0.5, [&](double t, double total) {
        lp += (n.next() - lp) * 0.25;
        const double flutter = 0.5 + 0.5 * std::sin(t * 70.0);
        return lp * 0.5 * flutter * std::min(1.0, t / 0.05) * std::min(1.0, (total - t) / 0.1);
      });
    }
    case Sfx::Whoosh:
    {
      Noise n(87);
      double lp = 0.0;
      return render(0.3, [&](double t, double total) {
        lp += (n.next() - lp) * (0.1 + 0.3 * std::sin(t / total * 3.1416));
        return lp * 0.45 * std::sin(t / total * 3.1416);
      });
    }
    case Sfx::Yell:
    {
      // The jungle yell: a rising voice warble (three quick yodel steps).
      Osc a, b;
      return render(3.0, [&](double t, double total) {
        const double step = std::floor(t / 0.5);
        const double base = 260.0 * std::pow(1.26, std::fmod(step, 3.0));
        const double f = base * (1.0 + 0.06 * std::sin(t * 38.0));
        const double env = std::min(1.0, t / 0.08) * std::min(1.0, (total - t) / 0.4);
        return (a.step(f, Wave::Saw) * 0.08 + b.step(f * 2.0, Wave::Sine) * 0.1) * env;
      });
    }
    case Sfx::Boing:
    {
      Osc a;
      return render(0.25, [&](double t, double total) {
        const double f = 180.0 + 260.0 * (t / total) + 30.0 * std::sin(t * 90.0);
        return a.step(f, Wave::Sine) * 0.3 * std::exp(-t * 9.0);
      });
    }
    case Sfx::Rumble:
    {
      // Stone grinding on stone: low filtered noise with a slow wobble.
      Noise n(91);
      Svf f;
      return render(0.7, [&](double t, double total) {
        const double wob = 0.6 + 0.4 * std::sin(t * 31.0);
        return f.low(n.next(), 160.0f, 1.2) * 0.9 * wob * std::min(1.0, t / 0.1) * std::min(1.0, (total - t) / 0.15);
      });
    }
    case Sfx::Slice:
    {
      Noise n(93);
      Svf f;
      return render(0.22, [&](double t, double total) {
        return f.band(n.next(), float(sweep(5200.0, 1800.0, t / total)), 3.0) * 0.6 * std::sin(t / total * 3.1416);
      });
    }
    case Sfx::Click:
    {
      Osc a;
      Noise n(95);
      return render(0.09, [&](double t, double) {
        return (a.step(sweep(900.0, 300.0, t / 0.09), Wave::Square, 0.5) * 0.12 + n.next() * 0.1) * std::exp(-t * 50.0);
      });
    }
    case Sfx::Drum:
    {
      // A deep temple drum.
      Osc a;
      return render(0.45, [&](double t, double) {
        return a.step(sweep(140.0, 55.0, std::min(1.0, t / 0.2)), Wave::Sine) * 0.5 * std::exp(-t * 7.0);
      });
    }
    case Sfx::Dart:
    {
      Noise n(97);
      Svf f;
      return render(0.12, [&](double t, double total) {
        return f.band(n.next(), 3000.0f, 4.0) * 0.4 * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::Skitter:
    {
      Noise n(99);
      return render(0.4, [&](double t, double total) {
        const double tick = std::fmod(t, 0.035) < 0.006 ? 1.0 : 0.0;
        return n.next() * 0.25 * tick * std::min(1.0, (total - t) / 0.1);
      });
    }
    case Sfx::Bell:
    {
      // One strike of a bronze bell (the church steeple, the mine's shaft
      // bell): a clang, then the bell's own partials - the hum an octave
      // down, the prime, the minor-third tierce, the quint and the nominal -
      // each dying away at its own rate, the low ones longest.
      struct Partial
      {
        double ratio, vol, tau;
      };
      static constexpr Partial kParts[] = {{0.5, 0.20, 0.9},   {1.0, 0.24, 0.55}, {1.19, 0.16, 0.35},
                                           {1.5, 0.09, 0.28},  {2.0, 0.16, 0.22}, {2.51, 0.07, 0.12},
                                           {2.66, 0.05, 0.10}, {3.01, 0.05, 0.08}};
      const double base = 392.0;
      Osc o[8];
      Noise n(107);
      Svf bp;
      return render(1.8, [&](double t, double total) {
        double v = 0.0;
        for (std::size_t k = 0; k < 8; ++k)
          v += o[k].step(base * kParts[k].ratio * (1.0 + 0.0015 * std::sin(t * 9.0 + double(k))), Wave::Sine) *
            kParts[k].vol * decay(t, kParts[k].tau);
        const double clang = bp.band(n.next(), 2600.0, 2.0) * decay(t, 0.012);
        return (v * 0.9 + clang * 0.35) * attack(t, 0.0015) * std::min(1.0, (total - t) / 0.3);
      });
    }
    case Sfx::Fuse:
    {
      Noise n(101);
      Svf f;
      return render(0.35, [&](double t, double total) {
        const double crackle = std::fmod(t, 0.04) < 0.008 ? 1.6 : 0.6;
        return f.band(n.next(), 5000.0f, 1.5) * 0.25 * crackle * std::min(1.0, (total - t) / 0.08);
      });
    }
    case Sfx::Flap:
    {
      Noise n(103);
      Svf f;
      return render(0.5, [&](double t, double total) {
        const double beat = 0.5 + 0.5 * std::sin(t * 2.0 * 3.1416 * 14.0);
        return f.low(n.next(), 900.0f, 1.0) * 0.5 * beat * beat * std::min(1.0, (total - t) / 0.1);
      });
    }
    case Sfx::Dig:
    {
      Noise n(105);
      Svf f;
      return render(0.4, [&](double t, double total) {
        const double grit = std::fmod(t, 0.05) < 0.02 ? 1.0 : 0.4;
        return f.low(n.next(), 600.0f, 1.3) * 0.7 * grit * std::min(1.0, (total - t) / 0.1);
      });
    }
    case Sfx::Rail:
    {
      // Iron wheels landing on iron rails.
      Osc a;
      Noise n(107);
      return render(0.3, [&](double t, double) {
        return (a.step(sweep(420.0, 260.0, std::min(1.0, t / 0.3)), Wave::Square, 0.3) * 0.1 + n.next() * 0.15) *
          std::exp(-t * 14.0);
      });
    }
    case Sfx::Sizzle:
    {
      // Hot rock meeting something wet: a hiss that fades.
      Noise n(109);
      OnePole f;
      return render(0.5, [&](double t, double total) {
        return f.highpass(n.next(), 2800.0) * 0.45 * std::exp(-t * 5.0) * std::min(1.0, (total - t) / 0.08);
      });
    }
    case Sfx::Magma:
    {
      // A thick bubble: a low plop that bends up.
      Osc a;
      return render(0.28, [&](double t, double) {
        return a.step(sweep(70.0, 190.0, std::min(1.0, t / 0.12)), Wave::Sine, 0.0) * 0.6 * std::exp(-t * 11.0);
      });
    }
    case Sfx::Sink:
    {
      // Stone grinding down: a rumble with grit.
      Osc a;
      Noise n(111);
      Svf f;
      return render(0.45, [&](double t, double total) {
        const double grind = f.low(n.next(), 300.0f, 1.1) * 0.5;
        return (a.step(55.0, Wave::Saw, 0.0) * 0.18 + grind) * std::min(1.0, t / 0.05) *
          std::min(1.0, (total - t) / 0.15);
      });
    }
    case Sfx::Croak:
    {
      Osc a;
      return render(0.25, [&](double t, double) {
        const double wob = std::fmod(t, 0.04) < 0.02 ? 1.0 : 0.6;
        return a.step(sweep(140.0, 90.0, std::min(1.0, t / 0.25)), Wave::Square, 0.0) * 0.22 * wob *
          std::exp(-t * 6.0);
      });
    }
    case Sfx::Snap:
    {
      Noise n(113);
      Osc a;
      return render(0.12, [&](double t, double) {
        return (n.next() * 0.4 + a.step(900.0, Wave::Square, 0.0) * 0.15) * std::exp(-t * 40.0);
      });
    }
    case Sfx::Squelch:
    {
      // Wet goo: a low gulp bending down under filtered slop.
      Osc a;
      Noise n(115);
      Svf f;
      return render(0.22, [&](double t, double) {
        const double slop = f.band(n.next(), float(sweep(1400.0, 500.0, std::min(1.0, t / 0.2))), 2.5) * 0.35;
        return (a.step(sweep(260.0, 90.0, std::min(1.0, t / 0.18)), Wave::Sine, 0.0) * 0.4 + slop) *
          std::exp(-t * 12.0) * std::min(1.0, t / 0.004);
      });
    }
    case Sfx::Chitter:
    {
      // Three quick insect clicks.
      Noise n(117);
      Svf f;
      return render(0.2, [&](double t, double) {
        const double click = std::fmod(t, 0.05) < 0.008 ? 1.0 : 0.0;
        return f.band(n.next(), 3200.0f, 3.0) * 0.6 * click * (t < 0.15 ? 1.0 : 0.0);
      });
    }
    case Sfx::Spit:
    {
      // A puffed cheek letting go: a breathy pop that rises.
      Osc a;
      Noise n(119);
      OnePole f;
      return render(0.25, [&](double t, double) {
        return (a.step(sweep(180.0, 520.0, std::min(1.0, t / 0.08)), Wave::Sine, 0.0) * 0.3 +
                 f.highpass(n.next(), 1800.0) * 0.25) *
          std::exp(-t * 14.0);
      });
    }
    case Sfx::Crash:
    {
      // A ton of stone hitting the floor: a low boom and a spray of grit.
      Osc a;
      Noise n(115);
      Svf f;
      return render(0.7, [&](double t, double) {
        const double boom = a.step(sweep(90.0, 38.0, std::min(1.0, t / 0.3)), Wave::Sine, 0.0) * 0.8 * std::exp(-t * 5.0);
        return boom + f.low(n.next(), 900.0f, 0.9) * 0.45 * std::exp(-t * 9.0);
      });
    }
    case Sfx::Hiss:
    {
      // A snake in its hole: breathy noise that swells and stops.
      Noise n(117);
      OnePole f;
      return render(0.4, [&](double t, double total) {
        return f.highpass(n.next(), 4200.0) * 0.4 * std::min(1.0, t / 0.15) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::Gulp:
    {
      // A throat swallowing: a wet click, then a low bubble that drops.
      Osc a;
      Noise n(121);
      Svf f;
      return render(0.32, [&](double t, double) {
        const double click = t < 0.02 ? f.band(n.next(), 2400.0f, 3.0) * 0.6 : 0.0;
        const double gulp = a.step(sweep(320.0, 70.0, std::min(1.0, t / 0.28)), Wave::Sine, 0.0) * 0.55 *
          std::sin(std::min(1.0, t / 0.3) * 3.14159);
        return click + gulp;
      });
    }
    case Sfx::Inhale:
    {
      // A long breath in: filtered noise rising in pitch and loudness.
      Noise n(123);
      Svf f;
      return render(0.9, [&](double t, double total) {
        const double rise = std::min(1.0, t / total);
        return f.band(n.next(), float(400.0 + 1600.0 * rise), 1.2) * 0.5 * rise * std::min(1.0, (total - t) / 0.08);
      });
    }
    case Sfx::Screech:
    {
      // An insect queen's call: two warbling tones a fifth apart.
      Osc a, b;
      return render(0.5, [&](double t, double total) {
        const double wob = 1.0 + 0.04 * std::sin(t * 70.0);
        return (a.step(880.0 * wob, Wave::Saw, 0.0) * 0.18 + b.step(1320.0 * wob, Wave::Square, 0.5) * 0.12) *
          std::min(1.0, t / 0.03) * std::min(1.0, (total - t) / 0.1);
      });
    }
    case Sfx::Swap:
    {
      // A struck crystal: two bright bell partials, a glassy shimmer up.
      Osc a, b, c;
      return render(0.6, [&](double t, double) {
        const double bell = a.step(1568.0, Wave::Sine, 0.0) * 0.3 + b.step(2349.0 * 1.003, Wave::Sine, 0.0) * 0.18;
        const double up = c.step(sweep(800.0, 3200.0, std::min(1.0, t / 0.2)), Wave::Triangle, 0.0) * 0.12 *
          std::exp(-t * 10.0);
        return (bell * std::exp(-t * 5.0) + up) * attack(t, 0.003);
      });
    }
    case Sfx::Blink:
    {
      // A warp: a falling then rising whistle through noise.
      Osc a;
      Noise n(131);
      Svf f;
      return render(0.35, [&](double t, double total) {
        const double k = t / total;
        const double fr = k < 0.5 ? sweep(1800.0, 500.0, k * 2.0) : sweep(500.0, 2200.0, (k - 0.5) * 2.0);
        return (a.step(fr, Wave::Sine, 0.0) * 0.25 + f.band(n.next(), float(fr), 4.0) * 0.2) *
          std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::Zip:
    {
      // Hands on a taut line: a rising whine over the hiss of silk.
      Osc a;
      Noise n(137);
      Svf f;
      return render(0.45, [&](double t, double total) {
        const double fr = sweep(300.0, 900.0, t / total);
        return (a.step(fr, Wave::Triangle, 0.0) * 0.16 + f.band(n.next(), float(fr * 3.0), 2.0) * 0.22) *
          attack(t, 0.01) * std::min(1.0, (total - t) / 0.12);
      });
    }
    case Sfx::Crack:
    {
      // A whip: a swish rising into a sharp, bright crack.
      Noise n(149);
      Svf f;
      return render(0.3, [&](double t, double) {
        const double swish = f.band(n.next(), float(sweep(600.0, 3000.0, std::min(1.0, t / 0.08))), 1.5) * 0.25 *
          (t < 0.08 ? t / 0.08 : 0.0);
        const double crack = t >= 0.08 ? n.next() * 0.7 * std::exp(-(t - 0.08) * 70.0) : 0.0;
        return swish + crack;
      });
    }
    case Sfx::Snip:
    {
      // A plucked strand going slack: a low twang bending down, a snap.
      Osc a, b;
      Noise n(139);
      return render(0.5, [&](double t, double) {
        const double fr = 220.0 * std::exp(-t * 2.5) + 70.0;
        const double twang = (a.step(fr, Wave::Saw, 0.0) * 0.2 + b.step(fr * 2.01, Wave::Sine, 0.0) * 0.15) *
          std::exp(-t * 6.0);
        const double snap = n.next() * 0.35 * std::exp(-t * 60.0);
        return (twang + snap) * attack(t, 0.002);
      });
    }
    case Sfx::EngineOn:
    {
      // A starter motor catching, then the engine settling.
      Osc a, b;
      Noise n(127);
      return render(0.55, [&](double t, double) {
        const double f = t < 0.18 ? 40.0 + 30.0 * std::sin(t * 140.0) : sweep(90.0, 60.0, (t - 0.18) / 0.3);
        const double buzz = a.step(f, Wave::Saw) * 0.5 + b.step(f * 2.01, Wave::Square, 0.3) * 0.2;
        return (buzz + n.next() * 0.08) * 0.32 * attack(t, 0.01) * decay(t, 0.25);
      });
    }
    case Sfx::EngineOff:
    {
      Osc a;
      return render(0.35, [&](double t, double) {
        return a.step(sweep(80.0, 30.0, t / 0.35), Wave::Saw) * 0.25 * attack(t, 0.005) * decay(t, 0.12);
      });
    }
    case Sfx::Cannon:
    {
      Noise n(131);
      Svf lp;
      Osc o;
      return render(0.6, [&](double t, double) {
        const double boom = o.step(sweep(70.0, 28.0, t / 0.4), Wave::Sine) * decay(t, 0.16);
        const double blast = lp.low(n.next(), sweep(4000.0, 200.0, t / 0.3), 1.0) * decay(t, 0.07);
        return (boom * 0.8 + blast * 0.7) * attack(t, 0.001);
      });
    }
    case Sfx::Torpedo:
    {
      Noise n(137);
      Svf bp;
      Osc o;
      return render(0.5, [&](double t, double) {
        const double fizz = bp.band(n.next(), sweep(500.0, 1800.0, t / 0.5), 3.0) * decay(t, 0.2);
        const double thunk = o.step(sweep(160.0, 70.0, t / 0.1), Wave::Sine) * decay(t, 0.05);
        return (fizz * 0.5 + thunk * 0.5) * attack(t, 0.002);
      });
    }
    case Sfx::Stomp:
    {
      Noise n(139);
      Svf lp;
      Osc o;
      return render(0.5, [&](double t, double) {
        const double thud = o.step(sweep(60.0, 25.0, t / 0.3), Wave::Sine) * decay(t, 0.14);
        const double grit = lp.low(n.next(), 600.0, 0.8) * decay(t, 0.1);
        return (thud * 0.9 + grit * 0.6) * attack(t, 0.001);
      });
    }
    case Sfx::Crunch:
    {
      Noise n(149);
      Svf bp;
      return render(0.22, [&](double t, double) {
        const double c = bp.band(n.next(), 900.0 + 600.0 * std::sin(t * 200.0), 1.4);
        return c * 0.6 * attack(t, 0.001) * decay(t, 0.07);
      });
    }
    case Sfx::Freeze:
    {
      // Ice spreading: a crackle of high clicks over a cold rising shimmer.
      Noise n(151);
      Svf bp;
      Osc o;
      return render(0.45, [&](double t, double total) {
        const double crackle = (n.next() > 0.86f ? 1.0 : 0.0) * bp.band(n.next(), 5200.0f, 2.0) * 3.0;
        const double shimmer = o.step(sweep(1800.0, 3200.0, t / total), Wave::Sine) * 0.12;
        return (crackle * 0.35 + shimmer) * attack(t, 0.005) * decay(t, 0.18);
      });
    }
    case Sfx::Tink:
    {
      // Glass struck: two bright partials ringing out.
      Osc a, b;
      return render(0.35, [&](double t, double) {
        return (a.step(2960.0, Wave::Sine) * 0.3 + b.step(4430.0, Wave::Sine) * 0.15) * attack(t, 0.001) *
          decay(t, 0.08);
      });
    }
    case Sfx::Whine:
    {
      // A small motor spinning up.
      Osc a, b;
      return render(0.6, [&](double t, double total) {
        const double f = sweep(220.0, 1400.0, t / total);
        return (a.step(f, Wave::Saw) * 0.12 + b.step(f * 1.5, Wave::Square, 0.3) * 0.06) * attack(t, 0.05) *
          std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::Cheer:
    {
      // A crowd a long way off: band-limited noise that swells and fades.
      Noise n(157);
      Svf bp;
      return render(1.6, [&](double t, double total) {
        const double env = std::sin(std::min(1.0, t / total) * 3.14159);
        const double wob = 1.0 + 0.3 * std::sin(t * 23.0) * std::sin(t * 7.0);
        return bp.band(n.next(), 1100.0f, 0.7) * 0.32 * env * wob;
      });
    }
    case Sfx::Trim:
    {
      // Hedge-trimmer blades: a short, gritty buzz (three frames at 15 Hz)
      // that repeats seamlessly while the trigger is held.
      Osc a, b;
      Noise n(161);
      Svf bp;
      return render(0.2, [&](double t, double total) {
        const double chatter = 0.6 + 0.4 * (std::fmod(t, 1.0 / 60.0) < 1.0 / 120.0 ? 1.0 : 0.0);
        const double buzz = a.step(120.0, Wave::Saw) * 0.5 + b.step(241.0, Wave::Square, 0.3) * 0.25;
        const double grit = bp.band(n.next(), 2400.0f, 1.5) * 0.4;
        return (buzz + grit) * chatter * 0.3 * std::min(1.0, t / 0.01) * std::min(1.0, (total - t) / 0.02);
      });
    }
    case Sfx::Puff:
    {
      // A fungus pod letting go: a soft, breathy whump of air.
      Noise n(163);
      Svf bp;
      Osc o;
      return render(0.4, [&](double t, double total) {
        const double air = bp.band(n.next(), float(sweep(500.0, 1600.0, t / total)), 0.9) * 0.5;
        const double thump = o.step(sweep(150.0, 70.0, std::min(1.0, t / 0.12)), Wave::Sine) * std::exp(-t * 18.0) * 0.35;
        return (air * std::sin(3.14159 * std::min(1.0, t / total)) + thump) * attack(t, 0.02);
      });
    }
    case Sfx::Squish:
    {
      // A Glob landing or splitting: a wet slap with a wobbling gloop.
      Osc a, lfo;
      Noise n(167);
      Svf f;
      return render(0.3, [&](double t, double) {
        const double slap = f.band(n.next(), float(sweep(2200.0, 400.0, std::min(1.0, t / 0.1))), 2.0) *
          std::exp(-t * 30.0) * 0.5;
        const double gloop =
          a.step(sweep(320.0, 110.0, std::min(1.0, t / 0.25)) * (1.0 + 0.15 * lfo.step(28.0, Wave::Sine)), Wave::Sine) *
          std::exp(-t * 9.0) * 0.38;
        return (slap + gloop) * std::min(1.0, t / 0.003);
      });
    }
    case Sfx::LampOn:
    {
      // A grow lamp striking up: a ballast click, then a mains hum and buzz
      // flickering into steady light.
      Osc a, b;
      Noise n(169);
      return render(0.55, [&](double t, double total) {
        const double click = n.next() * std::exp(-t * 120.0) * 0.6;
        const double flick = t < 0.18 ? (std::fmod(t, 0.06) < 0.03 ? 1.0 : 0.25) : 1.0;
        const double hum = a.step(100.0, Wave::Saw) * 0.3 + b.step(200.0, Wave::Square, 0.5) * 0.12;
        return (click + hum * flick * 0.4) * std::min(1.0, (total - t) / 0.15);
      });
    }
    case Sfx::Grow:
    {
      // Growth Spurt: a rising, springy boing with a little whoosh.
      Osc a, b, lfo;
      Noise n(173);
      Svf bp;
      return render(0.45, [&](double t, double total) {
        const double f = sweep(180.0, 520.0, std::min(1.0, t / 0.3)) * (1.0 + 0.06 * lfo.step(22.0, Wave::Sine));
        const double boing = a.step(f, Wave::Triangle) * 0.35 + b.step(f * 2.0, Wave::Sine) * 0.1;
        const double air = bp.band(n.next(), float(sweep(600.0, 3000.0, t / total)), 1.4) * 0.15;
        return (boing + air) * attack(t, 0.005) * std::min(1.0, (total - t) / 0.12);
      });
    }
    case Sfx::Recoil:
    {
      // The Recoil Cannon: a deep thump with a whoosh of exhaust behind it.
      Osc o;
      Noise n(181);
      Svf bp;
      OnePole lp;
      return render(0.45, [&](double t, double total) {
        const double thump = o.step(sweep(140.0, 38.0, t / 0.18), Wave::Sine) * std::exp(-t * 11.0) * 0.8;
        const double crack = lp.lowpass(n.next(), 3000.0) * std::exp(-t * 60.0) * 0.5;
        const double whoosh = bp.band(n.next(), float(sweep(2400.0, 300.0, t / total)), 1.2) *
          std::min(1.0, t / 0.04) * std::exp(-t * 6.0) * 0.45;
        return (thump + crack + whoosh) * attack(t, 0.002);
      });
    }
    case Sfx::Thrust:
    {
      // An EVA Ram's thruster firing: a roaring rumble that swells and holds.
      Noise n(191);
      Svf lp;
      Osc o, lfo;
      return render(0.7, [&](double t, double total) {
        const double roar = lp.low(n.next(), 500.0 + 900.0 * std::min(1.0, t / 0.15), 1.6) * 0.7;
        const double hum = o.step(70.0 * (1.0 + 0.05 * lfo.step(18.0, Wave::Sine)), Wave::Saw) * 0.12;
        return (roar + hum) * std::min(1.0, t / 0.05) * std::min(1.0, (total - t) / 0.2);
      });
    }
    case Sfx::Spikes:
    {
      // A Space Barnacle's shell snapping open, the spikes shooting out.
      Osc a, b;
      Noise n(193);
      OnePole hp;
      return render(0.4, [&](double t, double) {
        const double snap = hp.highpass(n.next(), 1800.0) * std::exp(-t * 90.0) * 0.6;
        double zing = 0.0;
        for (int k = 0; k < 3; ++k)
        {
          const double tk = t - 0.02 * k;
          if (tk > 0.0)
            zing += (k == 0 ? a : b).step(sweep(2600.0 - k * 300.0, 900.0, tk / 0.25), Wave::Square, 0.2) *
              std::exp(-tk * 14.0) * 0.08;
        }
        return snap + zing;
      });
    }
    case Sfx::Rivet:
    {
      // A rivet working loose and popping out: a ratchet, then a metallic ping.
      Osc a, b, c;
      Noise n(197);
      Svf bp;
      return render(0.6, [&](double t, double) {
        const double ratchet = t < 0.18 && std::fmod(t, 0.03) < 0.006 ? bp.band(n.next(), 3200.0, 3.0) * 0.6 : 0.0;
        const double tp = t - 0.18;
        double ping = 0.0;
        if (tp > 0.0)
          ping = (a.step(1760.0, Wave::Sine) * 0.5 + b.step(2650.0, Wave::Sine) * 0.25 + c.step(4180.0, Wave::Sine) * 0.12) *
            std::exp(-tp * 9.0) * std::min(1.0, tp / 0.002) * 0.45;
        return ratchet + ping;
      });
    }
    case Sfx::Hop:
    {
      // A tiny chirp: a Rivet Mite (or the runner on a planetoid) leaping.
      Osc o;
      return render(0.12, [&](double t, double total) {
        return o.step(sweep(900.0, 2400.0, t / total), Wave::Triangle) * 0.25 * attack(t, 0.004) *
          std::min(1.0, (total - t) / 0.03);
      });
    }
    case Sfx::FlagUp:
    {
      // A flag planted: a flap of cloth, then a little three-note fanfare.
      Noise n(199);
      Svf bp;
      std::vector<float> out = render(1.1, [&](double t, double) {
        const double flap = t < 0.3 ? bp.band(n.next(), 700.0 + 500.0 * std::sin(t * 60.0), 1.5) *
            (0.5 + 0.5 * std::sin(t * 70.0)) * (1.0 - t / 0.3) * 0.5
                                    : 0.0;
        return flap;
      });
      const int notes[3] = {72, 76, 79};
      for (int i = 0; i < 3; ++i)
      {
        addTone(out, 0.3 + i * 0.12, i == 2 ? 0.7 : 0.14, midiFreq(notes[i]), Wave::Square, 0.07, i == 2 ? 0.3 : 0.08);
        addTone(out, 0.3 + i * 0.12, i == 2 ? 0.7 : 0.14, midiFreq(notes[i] + 12), Wave::Triangle, 0.05, 0.1);
      }
      return out;
    }
    case Sfx::GravFlip:
    {
      // A chamber turning over: air rushing up through a band-pass sweep,
      // with a short two-tone klaxon blip on top.
      Noise n(211);
      Svf bp;
      Osc k;
      return render(0.75, [&](double t, double total) {
        const double rush = bp.band(n.next(), float(sweep(250.0, 2600.0, t / total)), 1.4) *
          std::sin(kPi * std::clamp(t / total, 0.0, 1.0)) * 0.55;
        double blip = 0.0;
        if (t < 0.22)
        {
          const double f = t < 0.11 ? 660.0 : 880.0;
          blip = k.step(f, Wave::Square, 0.4) * 0.12 * std::min(1.0, std::fmod(t, 0.11) / 0.005) *
            std::min(1.0, (0.11 - std::fmod(t, 0.11)) / 0.01);
        }
        return rush + blip;
      });
    }
    case Sfx::Thud:
    {
      // Landing after a flip: a soft, low body thump with a little cloth.
      Osc o;
      Noise n(223);
      OnePole lp;
      return render(0.3, [&](double t, double) {
        const double thump = o.step(sweep(110.0, 45.0, t / 0.12), Wave::Sine) * std::exp(-t * 16.0) * 0.75;
        const double cloth = lp.lowpass(n.next(), 900.0) * std::exp(-t * 30.0) * 0.35;
        return (thump + cloth) * attack(t, 0.003);
      });
    }
    case Sfx::Vortex:
    {
      // The Grav Grenade's vortex: a deep hum that wobbles round and round,
      // a whistling swirl on top, swelling in and ebbing away.
      Osc a, b, lfo, wob;
      Noise n(227);
      Svf bp;
      return render(0.8, [&](double t, double total) {
        const double spin = lfo.step(6.0 + 6.0 * t / total, Wave::Sine);
        const double hum = a.step(55.0 * (1.0 + 0.04 * spin), Wave::Saw) * 0.25 + b.step(82.5, Wave::Sine) * 0.3;
        const double swirl = bp.band(n.next(), float(900.0 + 600.0 * wob.step(4.0, Wave::Sine)), 4.0) * 0.35;
        const double env = std::min(1.0, t / 0.12) * std::min(1.0, (total - t) / 0.3);
        return (hum * (0.8 + 0.2 * spin) + swirl) * env;
      });
    }
    case Sfx::ProbeShot:
    {
      // A Gravity Probe firing: a hollow electronic blip, pitch dropping
      // through a resonant ring.
      Osc a, b;
      return render(0.22, [&](double t, double total) {
        const double f = sweep(1400.0, 420.0, t / total);
        const double v = a.step(f, Wave::Triangle) * 0.5 + b.step(f * 1.5, Wave::Sine) * 0.25;
        return v * 0.45 * attack(t, 0.002) * std::exp(-t * 9.0);
      });
    }
    case Sfx::CoreHum:
    {
      // The core winding up for a pulse: a deep electrical hum whose pitch
      // and brightness climb over three seconds, a tremolo that speeds up
      // and a crackle of static coming in toward the end.
      Osc a, b, c, trem;
      Noise n(233);
      Svf lp;
      OnePole hp;
      return render(3.0, [&](double t, double total) {
        const double k = t / total;
        const double f = sweep(48.0, 110.0, k * k);
        const double body = a.step(f, Wave::Saw) * 0.45 + b.step(f * 2.003, Wave::Square, 0.4) * 0.2 +
          c.step(f * 3.01, Wave::Sine) * 0.25 * k;
        const double tone = lp.low(float(body), float(300.0 + 2600.0 * k * k), 1.6);
        const double wob = 0.75 + 0.25 * trem.step(4.0 + 18.0 * k, Wave::Sine);
        const double crackle = n.next() > 0.985f - 0.02f * float(k) ? hp.highpass(n.next(), 3000.0) * k : 0.0;
        const double env = std::min(1.0, t / 0.4) * (0.35 + 0.65 * k) * std::min(1.0, (total - t) / 0.03);
        return (tone * wob * 0.5 + crackle * 0.4) * env;
      });
    }
    case Sfx::CorePulse:
    {
      // A ring going off: a deep electric boom (a sub thump under crunchy
      // rumble) with a bright ringing tail that shimmers away.
      Noise n(239);
      Svf lp;
      Osc sub, ring1, ring2, lfo;
      double held = 0.0;
      int counter = 0;
      return render(1.8, [&](double t, double total) {
        if (counter-- <= 0)
        {
          held = n.next();
          counter = int(lerp(2.0, 18.0, t / total));
        }
        const double rumble = lp.low(float(held * 0.6 + n.next() * 0.4), sweep(4200.0, 140.0, t / 0.9), 1.2) * decay(t, 0.25);
        const double thump = sub.step(sweep(95.0, 30.0, t / 0.5), Wave::Sine) * decay(t, 0.3);
        const double shimmer = 1.0 + 0.3 * lfo.step(9.0, Wave::Sine);
        const double ring = (ring1.step(sweep(1560.0, 1320.0, t / total), Wave::Sine) * 0.6 +
                              ring2.step(sweep(2350.0, 1980.0, t / total), Wave::Sine) * 0.35) *
          decay(t, 0.55) * shimmer * std::min(1.0, t / 0.02);
        return (rumble * 0.75 + thump * 0.8 + ring * 0.22) * attack(t, 0.002);
      });
    }
    case Sfx::BracerUp:
    {
      // The Deflector Bracer's shield going up: a quick rising shimmer of
      // detuned tones over a soft energy hum.
      Osc a, b, hum, lfo;
      return render(0.35, [&](double t, double total) {
        const double f = sweep(300.0, 1200.0, std::min(1.0, t / 0.12));
        const double sh = 0.7 + 0.3 * lfo.step(32.0, Wave::Sine);
        const double v = (a.step(f, Wave::Triangle) * 0.5 + b.step(f * 1.51, Wave::Sine) * 0.3) * sh +
          hum.step(110.0, Wave::Saw) * 0.12;
        return v * 0.35 * attack(t, 0.004) * std::min(1.0, (total - t) / 0.15);
      });
    }
    case Sfx::Deflect:
    {
      // Something bouncing off a shield: a bright metallic ping with a
      // short electric zap on its front.
      Osc a, b, c, zap;
      Noise n(241);
      OnePole hp;
      return render(0.4, [&](double t, double) {
        const double ping = (a.step(2480.0, Wave::Sine) * 0.5 + b.step(3720.0 * 1.003, Wave::Sine) * 0.3 +
                              c.step(5410.0, Wave::Sine) * 0.15) *
          decay(t, 0.09);
        const double z = (zap.step(sweep(3000.0, 600.0, t / 0.05), Wave::Square, 0.3) * 0.4 +
                           hp.highpass(n.next(), 2500.0) * 0.5) *
          decay(t, 0.018);
        return (ping * 0.4 + z * 0.4) * attack(t, 0.001);
      });
    }
    case Sfx::ValveTurn:
    {
      // A valve wheel squeaking round a notch: a rubbing squeal that wavers,
      // with a gritty click at its start.
      Osc o, wob;
      Noise n(251);
      Svf bp;
      return render(0.26, [&](double t, double total) {
        const double f = 820.0 + 160.0 * std::sin(t * 31.0) + 60.0 * wob.step(47.0, Wave::Triangle);
        const double squeak = bp.band(float(o.step(f, Wave::Saw)), f * 1.5, 6.0) * std::sin(kPi * t / total);
        const double grit = n.next() * decay(t, 0.006);
        return (squeak * 0.3 + grit * 0.3) * attack(t, 0.002);
      });
    }
    case Sfx::ValveShut:
    {
      // A valve shut tight: a heavy metal clank, then the hiss of the line
      // losing its pressure.
      Osc a, b;
      Noise n(257), m(263);
      OnePole hp, lp;
      return render(1.2, [&](double t, double total) {
        const double clank = (a.step(sweep(190.0, 120.0, t / 0.08), Wave::Triangle) * 0.6 +
                               b.step(737.0, Wave::Square, 0.3) * 0.25) *
            decay(t, 0.07) +
          n.next() * decay(t, 0.008) * 0.6;
        const double hiss = t > 0.12 ? hp.highpass(lp.lowpass(m.next(), 8000.0), 1800.0) *
            std::min(1.0, (t - 0.12) / 0.05) * std::min(1.0, (total - t) / 0.6) * 0.35
                                     : 0.0;
        return (clank * 0.6 + hiss) * attack(t, 0.001);
      });
    }
    case Sfx::Rewind:
    {
      // DO NOT PRESS: a bright major-key victory fanfare, rendered forwards
      // and then played backwards (with a wobbly tape speed for good
      // measure), about six seconds.
      const double len = 6.0;
      std::vector<float> fwd(std::size_t(samples(len)), 0.0f);
      struct Note
      {
        double at, dur, note;
      };
      // C major: a rising call, a held answer and a last big chord.
      static const Note kTune[] = {
        {0.00, 0.18, 72}, {0.20, 0.18, 76}, {0.40, 0.18, 79}, {0.60, 0.50, 84}, {1.15, 0.18, 79}, {1.35, 0.70, 84},
        {2.15, 0.18, 81}, {2.35, 0.18, 83}, {2.55, 0.18, 84}, {2.75, 0.18, 86}, {2.95, 0.90, 88}, {4.00, 1.60, 84},
      };
      for (const Note& nt : kTune)
      {
        addTone(fwd, nt.at, nt.dur + 0.25, midiFreq(nt.note), Wave::Square, 0.08, nt.dur * 0.9);
        addTone(fwd, nt.at, nt.dur + 0.25, midiFreq(nt.note - 12.0), Wave::Saw, 0.05, nt.dur * 0.9);
      }
      // The closing chord and a timpani roll under it.
      for (const double ch : {60.0, 64.0, 67.0, 72.0})
        addTone(fwd, 4.0, 1.9, midiFreq(ch), Wave::Triangle, 0.09, 0.9);
      {
        Osc timp;
        for (int i = 0; i < samples(1.8); ++i)
        {
          const double t = double(i) / kRate;
          const std::size_t at = std::size_t(samples(4.0) + i);
          if (at < fwd.size())
            fwd[at] += float(timp.step(sweep(70.0, 55.0, t / 1.8), Wave::Sine) * 0.3 * decay(t, 0.6) *
              (0.8 + 0.2 * std::sin(t * 2.0 * kPi * 14.0)));
        }
      }
      // Backwards, through a slowly wavering tape.
      std::vector<float> out(fwd.size(), 0.0f);
      double pos = double(fwd.size() - 1);
      for (std::size_t i = 0; i < out.size() && pos > 1.0; ++i)
      {
        const double t = double(i) / kRate;
        const double speed = 0.94 + 0.06 * std::sin(t * 2.0 * kPi * 0.7);
        const auto k = std::size_t(pos);
        const double fr = pos - double(k);
        out[i] = float(fwd[k] * (1.0 - fr) + fwd[k - 1] * fr);
        pos -= speed;
      }
      // Fade in (the end of the fanfare's tail) and out.
      const std::size_t fade = std::size_t(samples(0.3));
      for (std::size_t i = 0; i < fade && i < out.size(); ++i)
      {
        out[i] *= float(i) / float(fade);
        out[out.size() - 1 - i] *= float(i) / float(fade);
      }
      return out;
    }
    case Sfx::Bulkhead:
    {
      // A bulkhead sliding: a heavy hydraulic grind that rises as it moves
      // and ends in a deep clunk as it seats.
      Osc motor, clunk;
      Noise n(271), m(277);
      Svf bp;
      OnePole lp;
      return render(1.1, [&](double t, double total) {
        const double move = std::min(1.0, t / 0.08) * (t < 0.85 ? 1.0 : std::max(0.0, 1.0 - (t - 0.85) / 0.05));
        const double grind = bp.band(n.next(), 260.0 + 140.0 * t, 3.0) * 0.6 +
          motor.step(55.0 + 18.0 * t, Wave::Saw) * 0.25 * (0.8 + 0.2 * std::sin(t * 70.0));
        const double seat = t > 0.86 ? (clunk.step(sweep(120.0, 45.0, (t - 0.86) / 0.15), Wave::Sine) * 0.9 +
                                          lp.lowpass(m.next(), 700.0) * 0.6) *
            decay(t - 0.86, 0.07)
                                     : 0.0;
        return (grind * move * 0.4 + seat * 0.6) * attack(t, 0.01) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::Monitor:
    {
      // A CRT switching channel: the high whine of the flyback, a pop and a
      // short burst of static.
      Osc whine, pop;
      Noise n(281);
      OnePole hp;
      return render(0.3, [&](double t, double) {
        const double w = whine.step(15700.0 * 0.5, Wave::Sine) * 0.08 * decay(t, 0.12);
        const double p = pop.step(sweep(900.0, 200.0, t / 0.03), Wave::Square, 0.4) * decay(t, 0.012);
        const double st = hp.highpass(n.next(), 1500.0) * decay(t, 0.06) * (t > 0.01 ? 1.0 : 0.0);
        return (w + p * 0.3 + st * 0.25) * attack(t, 0.001);
      });
    }
    case Sfx::PhaseShot:
    {
      // The Phase Rifle: a thin rising zip with a hollow, phasey ring after
      // it (two detuned voices beating against each other).
      Osc a, b, c;
      return render(0.32, [&](double t, double) {
        const double f = sweep(500.0, 3200.0, t / 0.09);
        const double zip = a.step(f, Wave::Square, 0.2) * decay(t, 0.05);
        const double hollow = (b.step(f * 0.5, Wave::Sine) - c.step(f * 0.5 * 1.012, Wave::Sine)) * decay(t, 0.12);
        return (zip * 0.22 + hollow * 0.3) * attack(t, 0.001);
      });
    }
    case Sfx::Glint:
    {
      // A fake wall or a hidden pocket glinting: a tiny high two-note chime.
      std::vector<float> b(std::size_t(samples(0.4)));
      addTone(b, 0.0, 0.3, midiFreq(98), Wave::Sine, 0.12, 0.06);
      addTone(b, 0.07, 0.33, midiFreq(103), Wave::Sine, 0.14, 0.09);
      addTone(b, 0.07, 0.33, midiFreq(115), Wave::Sine, 0.04, 0.05);
      return b;
    }
    case Sfx::Rebuild:
    {
      // A Repair Swarm finishing a rebuild: a ratchet clicking faster and
      // higher, then a bright ping.
      Osc tick, ping, ping2;
      Noise n(283);
      Svf bp;
      return render(0.7, [&](double t, double) {
        double out = 0.0;
        if (t < 0.45)
        {
          const double rate = lerp(14.0, 40.0, t / 0.45);
          const double ph = std::fmod(t * rate, 1.0);
          out += bp.band(n.next(), lerp(1200.0, 3000.0, t / 0.45), 4.0) * std::exp(-ph * 18.0) * 0.6;
          out += tick.step(lerp(300.0, 700.0, t / 0.45), Wave::Square, 0.3) * std::exp(-ph * 30.0) * 0.12;
        }
        else
        {
          const double u = t - 0.45;
          out += (ping.step(1760.0, Wave::Sine) * 0.4 + ping2.step(2640.0, Wave::Sine) * 0.2) * decay(u, 0.08);
        }
        return out * 0.6 * attack(t, 0.002);
      });
    }
    case Sfx::EchoIn:
    {
      // An Echo stepping out of its pad: a shimmer swelling up as if played
      // backwards, cut off sharply.
      Osc a, b, lfo;
      Noise n(293);
      OnePole hp;
      return render(0.6, [&](double t, double total) {
        const double swell = std::pow(t / total, 2.5);
        const double f = sweep(400.0, 1400.0, t / total);
        const double sh = 0.6 + 0.4 * lfo.step(24.0, Wave::Sine);
        const double v = (a.step(f, Wave::Triangle) * 0.5 + b.step(f * 1.5, Wave::Sine) * 0.3) * sh +
          hp.highpass(n.next(), 4000.0) * 0.25;
        return v * swell * 0.4 * std::min(1.0, (total - t) / 0.01);
      });
    }
    case Sfx::EyeCharge:
    {
      // ZERO's eye charging: a whine rising for a second and a half, with a
      // tremolo that speeds up as it gets there.
      Osc a, b, lfo;
      return render(1.5, [&](double t, double total) {
        const double u = t / total;
        const double f = sweep(140.0, 1800.0, u);
        const double trem = 0.6 + 0.4 * lfo.step(lerp(4.0, 30.0, u), Wave::Sine);
        const double v = a.step(f, Wave::Saw) * 0.25 + b.step(f * 2.003, Wave::Sine) * 0.2;
        return v * trem * (0.2 + 0.8 * u) * 0.35 * attack(t, 0.05) * std::min(1.0, (total - t) / 0.02);
      });
    }
    case Sfx::EyeBeam:
    {
      // The beam hitting the grille: a fat mains buzz with crackling
      // sparks over it.
      Osc a, b;
      Noise n(307), m(311);
      OnePole hp;
      Svf lp;
      return render(0.9, [&](double t, double total) {
        const double buzz = lp.low(float(a.step(100.0, Wave::Saw) * 0.6 + b.step(150.3, Wave::Square, 0.3) * 0.4),
                              1400.0, 1.5);
        const double crack = (n.next() > 0.97f ? 1.0 : 0.0) * hp.highpass(m.next(), 2500.0) * 3.0;
        return (buzz * 0.35 + crack * 0.15) * attack(t, 0.01) * std::min(1.0, (total - t) / 0.2);
      });
    }
    case Sfx::Shutter:
    {
      // A camera iris snapping: a quick double clack of blades with a short
      // mechanical whirr between.
      Noise n(313), m(317);
      Svf bp, bp2;
      Osc whirr;
      return render(0.22, [&](double t, double) {
        const double c1 = bp.band(n.next(), 3200.0, 3.0) * decay(t, 0.006);
        const double c2 = t > 0.12 ? bp2.band(m.next(), 2400.0, 3.0) * decay(t - 0.12, 0.008) : 0.0;
        const double w = t < 0.12 ? whirr.step(sweep(600.0, 1100.0, t / 0.12), Wave::Square, 0.3) * 0.05 : 0.0;
        return (c1 * 0.9 + c2 * 0.8 + w) * attack(t, 0.0005);
      });
    }
    case Sfx::WallFall:
    {
      // The painted flat tipping over: a long wooden creak, then a huge flat
      // slap and the boom of the stage under it.
      Osc creak, boom;
      Noise n(331), m(337);
      Svf bp;
      OnePole lp;
      return render(2.4, [&](double t, double total) {
        double out = 0.0;
        if (t < 1.0)
        {
          const double f = 140.0 + 50.0 * std::sin(t * 9.0) + 30.0 * t;
          out += bp.band(float(creak.step(f, Wave::Saw)), f * 3.0, 5.0) * std::sin(kPi * t) * 0.35;
        }
        else
        {
          const double u = t - 1.0;
          out += lp.lowpass(m.next(), sweep(5000.0, 300.0, u / 0.4)) * decay(u, 0.12) * 1.2;
          out += boom.step(sweep(70.0, 28.0, u / 0.6), Wave::Sine) * decay(u, 0.35) * 0.9;
          out += n.next() * decay(u, 0.01) * 0.4;
        }
        return out * 0.6 * attack(t, 0.02) * std::min(1.0, (total - t) / 0.3);
      });
    }
    case Sfx::LightClunk:
    {
      // A bank of studio lights switching on: a heavy relay clunk and the
      // low electric hum of the lamps coming up.
      Osc thud, hum, hum2;
      Noise n(347);
      OnePole lp;
      return render(1.0, [&](double t, double total) {
        const double c = (thud.step(sweep(160.0, 60.0, t / 0.06), Wave::Triangle) * 0.8 + lp.lowpass(n.next(), 1600.0) * 0.6) *
          decay(t, 0.04);
        const double h = (hum.step(100.0, Wave::Saw) * 0.5 + hum2.step(200.0, Wave::Sine) * 0.3) *
          std::min(1.0, t / 0.15) * std::min(1.0, (total - t) / 0.4) * 0.12;
        return (c * 0.7 + h) * attack(t, 0.001);
      });
    }
    case Sfx::Applause:
    {
      // A studio audience cheering: many hand claps (short filtered noise
      // bursts at random times) over a crowd's "woo".
      const double len = 3.0;
      std::vector<float> b(std::size_t(samples(len)), 0.0f);
      std::uint32_t seed = 3571;
      auto rnd = [&]() {
        seed = seed * 1664525u + 1013904223u;
        return double(seed >> 8) / double(1u << 24);
      };
      Noise n(349);
      for (int k = 0; k < 900; ++k)
      {
        const double at = rnd() * (len - 0.1);
        const double vol = 0.25 + 0.5 * rnd();
        const double cut = 900.0 + 2200.0 * rnd();
        Svf bp;
        const int start = samples(at);
        for (int i = 0; i < samples(0.04) && start + i < int(b.size()); ++i)
        {
          const double t = double(i) / kRate;
          b[std::size_t(start + i)] += float(bp.band(n.next(), cut, 1.4) * vol * decay(t, 0.008));
        }
      }
      // The "woo": a few voices on a vowel-ish band, sliding up.
      Noise v(353);
      Svf f1, f2;
      Osc o1, o2, o3;
      for (std::size_t i = 0; i < b.size(); ++i)
      {
        const double t = double(i) / kRate;
        const double env = std::min(1.0, t / 0.3) * std::min(1.0, (len - t) / 1.2);
        const double voice = o1.step(sweep(330.0, 440.0, t / 0.6), Wave::Saw) * 0.3 +
          o2.step(sweep(392.0, 523.0, t / 0.7), Wave::Saw) * 0.25 + o3.step(sweep(262.0, 349.0, t / 0.5), Wave::Saw) * 0.25;
        const double formant = f1.band(float(voice + v.next() * 0.2), 700.0, 4.0) * 0.6 + f2.band(float(voice), 1200.0, 5.0) * 0.3;
        b[i] = float(b[i] * 0.5 * std::min(1.0, (len - t) / 1.5) + formant * env * 0.12);
      }
      return b;
    }
    case Sfx::FuseLit:
    {
      // A match struck on the sole of a boot, then the fuse catching and
      // fizzing away.
      Noise n(461);
      Svf strike, fizz;
      return render(0.55, [&](double t, double total) {
        const double scratch = strike.band(n.next(), sweep(1800.0, 4200.0, t / 0.07), 1.6) * (t < 0.08 ? 1.0 : decay(t - 0.08, 0.02));
        const double flare = n.next() * 0.5 * (t > 0.06 ? decay(t - 0.06, 0.05) : 0.0);
        const double crackle = std::fmod(t * 1000.0 + 7.0 * std::sin(t * 61.0), 23.0) < 3.0 ? 1.7 : 0.6;
        const double hiss = fizz.band(n.next(), 6200.0, 1.3) * crackle * std::min(1.0, std::max(0.0, t - 0.1) / 0.05);
        return (scratch * 0.55 + flare * 0.25 + hiss * 0.24) * attack(t, 0.002) * std::min(1.0, (total - t) / 0.12);
      });
    }
    case Sfx::BarrelHiss:
    {
      // The fuse reaching the powder: a sharp hiss that climbs and gets
      // louder up to the bang.
      Noise n(463);
      OnePole hp;
      Svf bp;
      return render(0.3, [&](double t, double total) {
        const double h = hp.highpass(n.next(), 2500.0) * 0.6 + bp.band(n.next(), sweep(3000.0, 7500.0, t / total), 3.0) * 0.5;
        return h * (0.3 + 0.7 * t / total) * 0.42 * attack(t, 0.01) * std::min(1.0, (total - t) / 0.02);
      });
    }
    case Sfx::Ricochet:
    {
      // A bullet glancing off iron: a crack, then the whining "pt-twang"
      // falling away.
      Osc a, b, lfo;
      Noise n(467);
      OnePole hp;
      return render(0.55, [&](double t, double total) {
        const double f = sweep(3400.0, 1100.0, t / 0.45) * (1.0 + 0.03 * lfo.step(31.0, Wave::Sine));
        const double whine = (a.step(f, Wave::Sine) * 0.7 + b.step(f * 1.5, Wave::Sine) * 0.2) * decay(t, 0.16);
        const double crack = hp.highpass(n.next(), 3000.0) * decay(t, 0.006);
        return (whine * 0.34 + crack * 0.5) * attack(t, 0.0008) * std::min(1.0, (total - t) / 0.08);
      });
    }
    case Sfx::Reload:
    {
      // The cylinder spun: a run of ratchet clicks slowing down, then the
      // gate snapping shut.
      std::vector<float> b(std::size_t(samples(0.32)), 0.0f);
      Noise n(479);
      double at = 0.0, gap = 0.022;
      for (int k = 0; k < 7; ++k)
      {
        Svf bp;
        const int s0 = samples(at);
        for (int i = 0; i < samples(0.012) && s0 + i < int(b.size()); ++i)
        {
          const double t = double(i) / kRate;
          b[std::size_t(s0 + i)] += float(bp.band(n.next(), 3800.0 + 300.0 * double(k % 2), 4.0) * 0.5 * decay(t, 0.0025));
        }
        at += gap;
        gap *= 1.12;
      }
      Svf snap;
      Osc o;
      const int s0 = samples(at + 0.03);
      for (int i = 0; s0 + i < int(b.size()); ++i)
      {
        const double t = double(i) / kRate;
        b[std::size_t(s0 + i)] += float((snap.band(n.next(), 2400.0, 3.0) * 0.6 + o.step(900.0, Wave::Square, 0.3) * 0.12) * decay(t, 0.008));
      }
      return b;
    }
    case Sfx::Piano1:
    case Sfx::Piano2:
    case Sfx::Piano3:
    case Sfx::Piano4:
    {
      // The saloon piano's four keys, up a C major chord: C, E, G, high C,
      // each with a fifth or third under it the way a saloon player would
      // hit it.
      static constexpr double kNotes[4] = {72.0, 76.0, 79.0, 84.0};
      const int k = int(id) - int(Sfx::Piano1);
      std::vector<float> b(std::size_t(samples(0.8)), 0.0f);
      addPiano(b, 0.0, 0.8, kNotes[k], 0.32);
      addPiano(b, 0.0, 0.8, kNotes[k] - (k % 2 == 0 ? 5.0 : 4.0), 0.14);
      return b;
    }
    case Sfx::HonkyTonk:
    {
      // One bar of ragtime (160 bpm, 1.5 s): the left hand's oom-pah - bass
      // note, chord, bass, chord - under a syncopated right-hand tune.
      const double beat = 60.0 / 160.0, six = beat / 4.0;
      std::vector<float> b(std::size_t(samples(beat * 4.0 + 0.6)), 0.0f);
      // Left hand: C2 / C-E-G, G1 / C-E-G.
      addPiano(b, 0.0, beat * 0.9, 48.0, 0.2);
      addPiano(b, 0.0, beat * 0.9, 36.0, 0.1);
      for (const double n : {60.0, 64.0, 67.0})
        addPiano(b, beat, beat * 0.6, n, 0.08);
      addPiano(b, beat * 2.0, beat * 0.9, 43.0, 0.2);
      addPiano(b, beat * 2.0, beat * 0.9, 31.0, 0.1);
      for (const double n : {60.0, 64.0, 67.0})
        addPiano(b, beat * 3.0, beat * 1.2, n, 0.08);
      // Right hand, in sixteenths: off the beat, then home.
      struct Note
      {
        int at;
        double midi, len;
      };
      static constexpr Note kTune[] = {{0, 79.0, 1.0},  {1, 76.0, 1.0},  {3, 79.0, 1.0},  {4, 81.0, 2.0},
                                       {6, 79.0, 1.0},  {7, 76.0, 1.0},  {8, 72.0, 2.0},  {10, 74.0, 1.0},
                                       {11, 76.0, 3.0}, {14, 72.0, 1.0}, {15, 76.0, 1.0}, {16, 84.0, 4.0}};
      for (const auto& n : kTune)
      {
        const double len = n.len * six + (n.at == 16 ? 0.5 : 0.06);
        addPiano(b, double(n.at) * six, len, n.midi, 0.16);
        if (n.at % 4 == 0)
          addPiano(b, double(n.at) * six, len, n.midi - 12.0, 0.07); // octaves on the beat
      }
      return b;
    }
    case Sfx::PosterSpin:
    {
      // A poster whirling round its nail: paper flapping, fast and then
      // slowing (two turns), and a creak of the nail.
      Noise n(487);
      Svf bp;
      Osc creak;
      return render(0.75, [&](double t, double total) {
        const double u = t / total;
        const double turns = 2.0 * (2.0 * u - u * u); // the level's ease: 720 * (2u - u^2)
        const double flap = 0.5 + 0.5 * std::cos(turns * 2.0 * kPi * 2.0);
        const double paper = bp.band(n.next(), 1400.0 + 1600.0 * flap, 1.2) * (0.25 + 0.75 * flap * flap);
        const double nail = creak.step(620.0 + 120.0 * std::sin(t * 40.0), Wave::Saw) * 0.05 * (1.0 - u);
        return (paper * 0.42 + nail) * attack(t, 0.005) * std::min(1.0, (total - t) / 0.15);
      });
    }
    case Sfx::Gust:
    {
      // Wind down the mine tunnel: a hollow, swelling howl of filtered
      // noise with a breathy whistle on top.
      Noise n(491);
      Svf low, howl;
      Osc w;
      return render(1.4, [&](double t, double total) {
        const double u = t / total;
        const double swell = std::sin(kPi * std::min(1.0, u * 1.15)) * (0.85 + 0.15 * std::sin(t * 11.0));
        const double body = low.low(n.next(), 500.0 + 500.0 * swell, 0.8);
        const double hollow = howl.band(n.next(), 300.0 + 260.0 * swell + 40.0 * std::sin(t * 5.0), 7.0);
        const double whistle = w.step(880.0 + 140.0 * swell, Wave::Sine) * 0.025 * swell;
        return (body * 0.35 + hollow * 0.3 + whistle) * std::max(0.0, swell) * std::min(1.0, (total - t) / 0.2);
      });
    }
    case Sfx::DuelDraw:
    {
      // A gun leaving its holster: leather slap, a hand on wood, and the
      // hammer cocked (two quick clicks).
      Noise n(499);
      Svf slap, c1, c2;
      Osc thump;
      return render(0.32, [&](double t, double total) {
        const double leather = slap.band(n.next(), 900.0, 0.9) * decay(t, 0.025);
        const double body = thump.step(sweep(160.0, 70.0, t / 0.06), Wave::Sine) * decay(t, 0.03);
        const double t1 = t - 0.12, t2 = t - 0.17;
        const double click1 = t1 > 0 ? c1.band(n.next(), 3400.0, 5.0) * decay(t1, 0.004) : 0.0;
        const double click2 = t2 > 0 ? c2.band(n.next(), 2700.0, 5.0) * decay(t2, 0.005) : 0.0;
        return (leather * 0.55 + body * 0.35 + click1 * 0.7 + click2 * 0.8) * attack(t, 0.001) *
          std::min(1.0, (total - t) / 0.03);
      });
    }
    case Sfx::MirrorFlip:
    {
      // Stepping through a mirror: a glass chime played backwards (the
      // partials swell in and cut off), a shimmer of tremolo over it and a
      // soft low whoomp as the world turns over.
      Osc a, b, c, d, lfo, low;
      Noise n(503);
      Svf air;
      return render(0.75, [&](double t, double total) {
        const double u = std::clamp(t / 0.4, 0.0, 1.0);
        const double swell = u * u * (t < 0.4 ? 1.0 : decay(t - 0.4, 0.025));
        const double trem = 0.7 + 0.3 * lfo.step(sweep(6.0, 22.0, u), Wave::Sine);
        const double f = midiFreq(84.0) * (1.0 + 0.004 * std::sin(t * 40.0));
        const double glass = a.step(f, Wave::Sine) * 0.45 + b.step(f * 2.76, Wave::Sine) * 0.25 +
          c.step(f * 5.4, Wave::Sine) * 0.14 + d.step(f * 0.5, Wave::Sine) * 0.2;
        const double hiss = air.band(n.next(), sweep(3000.0, 8000.0, u), 2.0) * 0.18;
        const double whoomp = t > 0.38 ? low.step(sweep(140.0, 50.0, (t - 0.38) / 0.25), Wave::Sine) * decay(t - 0.38, 0.12) : 0.0;
        return (glass * trem + hiss) * swell * 0.32 + whoomp * 0.4 * std::min(1.0, (total - t) / 0.03);
      });
    }
    case Sfx::Lever:
    {
      // A heavy iron lever: a rusty creak as it moves, then a deep clunk
      // and the rattle of the mechanism behind the wall.
      Osc saw, thump;
      Noise n(509);
      Svf creak, clank, ratl;
      return render(0.75, [&](double t, double total) {
        const double c = t < 0.34 ? creak.band(float(saw.step(70.0 + 50.0 * std::sin(t * 23.0) + 30.0 * t, Wave::Saw)),
                                      900.0 + 500.0 * std::sin(t * 9.0), 6.0) *
            std::sin(kPi * t / 0.34)
                                  : 0.0;
        const double tc = t - 0.34;
        const double k = tc > 0 ? thump.step(sweep(110.0, 45.0, tc / 0.15), Wave::Sine) * decay(tc, 0.09) : 0.0;
        const double hit = tc > 0 ? clank.band(n.next(), 1400.0, 3.0) * decay(tc, 0.02) : 0.0;
        const double rat = tc > 0.08 ? ratl.band(n.next(), 2200.0, 4.0) * (std::fmod(tc, 0.05) < 0.012 ? 1.0 : 0.15) *
            decay(tc - 0.08, 0.12)
                                     : 0.0;
        return (c * 0.5 + k * 0.75 + hit * 0.6 + rat * 0.35) * 0.78 * std::min(1.0, (total - t) / 0.04);
      });
    }
    case Sfx::GhostHiss:
    {
      // A cold hiss: breath through a long "haaah" formant sliding down,
      // and a thin wavering whistle under it.
      Noise n(521);
      Svf f1, f2;
      OnePole hp;
      Osc whistle, vib;
      return render(0.8, [&](double t, double total) {
        const double env = std::min(1.0, t / 0.06) * std::min(1.0, (total - t) / 0.35);
        const double u = t / total;
        const float x = n.next();
        const double breath = f1.band(x, sweep(2600.0, 900.0, u), 3.0) * 0.6 + f2.band(x, sweep(5200.0, 3000.0, u), 4.0) * 0.4 +
          hp.highpass(x, 6000.0) * 0.25;
        const double w = whistle.step(sweep(950.0, 620.0, u) * (1.0 + 0.02 * vib.step(7.0, Wave::Sine)), Wave::Sine) * 0.12;
        return (breath * 0.55 + w) * env;
      });
    }
    case Sfx::ArmorClank:
    {
      // A suit of armour waking: a cascade of plate on plate (inharmonic
      // metal hits) and a heavy step.
      static const double kHits[6] = {0.0, 0.06, 0.11, 0.19, 0.24, 0.33};
      std::vector<float> out(std::size_t(samples(0.75)), 0.0f);
      for (int h = 0; h < 6; ++h)
      {
        const double base = 420.0 + 90.0 * double((h * 37) % 5);
        addTone(out, kHits[h], 0.35, base, Wave::Sine, 0.16, 0.05);
        addTone(out, kHits[h], 0.3, base * 2.63, Wave::Sine, 0.1, 0.035);
        addTone(out, kHits[h], 0.25, base * 4.17, Wave::Sine, 0.07, 0.02);
      }
      Noise n(523);
      Osc o;
      for (int i = 0; i < int(out.size()); ++i)
      {
        const double t = double(i) / kRate - 0.36;
        if (t > 0)
          out[std::size_t(i)] += float((o.step(sweep(95.0, 50.0, t / 0.2), Wave::Sine) * 0.5 + n.next() * 0.25) * decay(t, 0.07));
      }
      return out;
    }
    case Sfx::Swoosh:
    {
      // A halberd cutting the air: a band of noise sweeping up and down.
      Noise n(541);
      Svf bp;
      return render(0.38, [&](double t, double total) {
        const double u = t / total;
        const double f = u < 0.45 ? sweep(350.0, 2600.0, u / 0.45) : sweep(2600.0, 500.0, (u - 0.45) / 0.55);
        return bp.band(n.next(), f, 2.4) * std::pow(std::sin(kPi * u), 1.5) * 0.6;
      });
    }
    case Sfx::Throw:
    {
      // A Poltergeist flinging something: a short whoosh and an eerie
      // rising whistle, like a breath drawn in.
      Noise n(547);
      Svf bp;
      Osc w;
      return render(0.4, [&](double t, double total) {
        const double u = t / total;
        const double whoosh = bp.band(n.next(), sweep(500.0, 1800.0, u), 1.8) * std::sin(kPi * u);
        const double whistle = w.step(sweep(500.0, 1100.0, u), Wave::Sine) * std::sin(kPi * u) * 0.15;
        return whoosh * 0.5 + whistle;
      });
    }
    case Sfx::BoltThunk:
    {
      // A crossbow bolt driving home: a dull thunk and the shaft's twang.
      Osc body, twang, tw2;
      Noise n(557);
      OnePole lp;
      return render(0.45, [&](double t, double) {
        const double k = body.step(sweep(160.0, 70.0, t / 0.06), Wave::Sine) * decay(t, 0.05);
        const double click = lp.lowpass(n.next(), 3000.0) * decay(t, 0.006);
        const double f = 210.0 * (1.0 + 0.03 * decay(t, 0.05));
        const double tw = (twang.step(f, Wave::Triangle) * 0.6 + tw2.step(f * 2.01, Wave::Sine) * 0.25) * decay(t, 0.12) *
          (0.75 + 0.25 * std::sin(t * 2.0 * kPi * 18.0));
        return (k * 0.8 + click * 0.6 + tw * 0.3) * 0.8 * attack(t, 0.001);
      });
    }
    case Sfx::GlassChime:
    {
      // The broken mirror shot: a crack, then a glass chime gone sour
      // (pairs of partials a little apart, beating).
      std::vector<float> out(std::size_t(samples(1.4)), 0.0f);
      const double f = midiFreq(81.0);
      for (const double m : {1.0, 2.76, 5.4})
      {
        addTone(out, 0.02, 1.3, f * m, Wave::Sine, 0.13 / m, 0.5 / m);
        addTone(out, 0.02, 1.3, f * m * 1.013, Wave::Sine, 0.11 / m, 0.45 / m);
      }
      Noise n(563);
      Svf bp;
      for (int i = 0; i < samples(0.12); ++i)
      {
        const double t = double(i) / kRate;
        out[std::size_t(i)] += float(bp.band(n.next(), 4500.0, 1.5) * decay(t, 0.02) * 0.7);
      }
      return out;
    }
    case Sfx::CoffinSlam:
    {
      // Shooting the coffin: the lid creaks up, something breathes in the
      // dark inside, and two and a half seconds later the lid slams.
      Osc saw, low, slamO;
      Noise n(569), m(571);
      Svf creak, breath;
      OnePole lp;
      return render(2.9, [&](double t, double total) {
        double v = 0.0;
        if (t < 0.45)
          v += creak.band(float(saw.step(55.0 + 40.0 * std::sin(t * 17.0) + 60.0 * t, Wave::Saw)), 700.0 + 300.0 * t, 7.0) *
            std::sin(kPi * t / 0.45) * 0.55;
        if (t > 0.7 && t < 1.9)
        {
          const double u = (t - 0.7) / 1.2;
          v += breath.band(m.next(), sweep(500.0, 300.0, u), 3.0) * std::sin(kPi * u) * 0.35;
          v += low.step(55.0, Wave::Saw) * std::sin(kPi * u) * 0.05;
        }
        const double ts = t - 2.35;
        if (ts > 0)
        {
          v += slamO.step(sweep(120.0, 38.0, ts / 0.3), Wave::Sine) * decay(ts, 0.14) * 0.9;
          v += lp.lowpass(n.next(), sweep(5000.0, 300.0, ts / 0.2)) * decay(ts, 0.06) * 0.9;
        }
        return v * 0.62 * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::Thunder:
    {
      // Lightning over the manor: a sharp crack, then a long roll that
      // grumbles away (noise held in random steps through a falling filter).
      Noise n(577), m(587);
      Svf lp;
      OnePole hp, lp2;
      double held = 0.0;
      int counter = 0;
      return render(3.2, [&](double t, double total) {
        if (counter-- <= 0)
        {
          held = m.next();
          counter = int(lerp(3.0, 40.0, t / total));
        }
        const double crack = hp.highpass(n.next(), 1500.0) * decay(t, 0.05) * 1.2;
        const double roll = lp.low(float(held * 0.7 + n.next() * 0.3), sweep(1800.0, 90.0, t / total), 1.3) *
          (0.55 + 0.45 * std::sin(t * 7.0 + std::sin(t * 2.3) * 3.0));
        const double body = lp2.lowpass(n.next(), 120.0) * 2.0;
        const double env = attack(t, 0.004) * (0.35 * decay(t, 0.2) + 0.65 * decay(t, 1.1)) * std::min(1.0, (total - t) / 0.3);
        return (crack * 0.35 + roll * 0.75 + body * 0.4) * env;
      });
    }
    case Sfx::BellDing:
    {
      // A chef's brass bell: a bright strike and its inharmonic partials
      // ringing out, the top ones dying first, a slow shimmer between two
      // near-unison strings of it.
      std::vector<float> out(std::size_t(samples(1.6)), 0.0f);
      const double f = midiFreq(86.0);
      static const double kPartials[5][3] = {
        {1.0, 0.2, 1.1}, {2.0, 0.09, 0.7}, {2.76, 0.11, 0.55}, {5.4, 0.06, 0.25}, {8.93, 0.035, 0.12}};
      for (const auto& pt : kPartials)
      {
        addTone(out, 0.0, 1.6, f * pt[0], Wave::Sine, pt[1], pt[2]);
        addTone(out, 0.0, 1.6, f * pt[0] * 1.004, Wave::Sine, pt[1] * 0.6, pt[2]);
      }
      Noise n(601);
      Svf bp;
      for (int i = 0; i < samples(0.03); ++i)
      {
        const double t = double(i) / kRate;
        out[std::size_t(i)] += float(bp.band(n.next(), 5200.0, 2.0) * decay(t, 0.006) * 0.6);
      }
      return out;
    }
    case Sfx::Sneeze:
    {
      // A cartoon sneeze: "ah... ah..." (a voiced vowel rising twice), then
      // "CHOO!" (a burst of breath noise through a closing formant).
      Osc voice, vib;
      Noise n(607);
      Svf f1, f2, b1, b2;
      return render(1.05, [&](double t, double total) {
        double v = 0.0;
        auto ah = [&](double t0, double len, double p0, double p1) {
          const double u = (t - t0) / len;
          if (u < 0.0 || u > 1.0)
            return 0.0;
          const double pitch = sweep(p0, p1, u) * (1.0 + 0.02 * vib.step(6.0, Wave::Sine));
          const float x = voice.step(pitch, Wave::Saw);
          const double vowel = f1.band(x, 800.0, 6.0) * 0.6 + f2.band(x, 1250.0, 7.0) * 0.4;
          return vowel * std::sin(kPi * u) * (0.6 + 0.4 * u);
        };
        v += ah(0.0, 0.26, 330.0, 420.0);
        v += ah(0.32, 0.3, 360.0, 520.0) * 1.2;
        const double tc = t - 0.7;
        if (tc > 0)
        {
          const double u = tc / (total - 0.7);
          const float x = n.next();
          const double ch = b1.band(x, sweep(3400.0, 1400.0, u), 1.6) * 0.8 + b2.band(x, sweep(1200.0, 500.0, u), 2.5) * 0.5;
          v += ch * attack(tc, 0.004) * decay(tc, 0.12) * 1.6;
        }
        return v * 0.55 * std::min(1.0, (total - t) / 0.03);
      });
    }
    case Sfx::EggRing:
    {
      // A wind-up kitchen timer's bell: the little hammer chattering on it
      // (25 strikes a second), its partials ringing between strikes.
      Osc a, b, c;
      Noise n(613);
      OnePole hp;
      return render(0.26, [&](double t, double total) {
        const double ph = std::fmod(t * 25.0, 1.0);
        const double strike = std::exp(-ph * 9.0);
        const double f = 2350.0;
        const double bell = a.step(f, Wave::Sine) * 0.5 + b.step(f * 2.71, Wave::Sine) * 0.22 + c.step(f * 0.5, Wave::Sine) * 0.2;
        const double tick = hp.highpass(n.next(), 4000.0) * std::exp(-ph * 40.0);
        return (bell * (0.35 + 0.65 * strike) + tick * 0.4) * 0.42 * std::min(1.0, (total - t) / 0.03) * attack(t, 0.002);
      });
    }
    case Sfx::GrinderWhir:
    {
      // The Pepper Grinder spinning up: a motor whine climbing over a second
      // and the crackle of peppercorns cracking in the burr.
      Osc motor, sub;
      Noise n(617), m(619);
      Svf whine, crack;
      return render(1.05, [&](double t, double total) {
        const double u = std::min(1.0, t / 0.95);
        const double f = sweep(70.0, 240.0, u * (2.0 - u));
        const float saw = motor.step(f, Wave::Saw);
        const double hum = whine.band(saw, f * 6.0, 3.0) * 0.55 + sub.step(f * 0.5, Wave::Square, 0.3) * 0.12;
        const double rate = lerp(0.992, 0.94, u);
        const double cr = n.next() > rate ? crack.band(m.next(), 3200.0, 2.5) * 2.4 : crack.band(0.0f, 3200.0, 2.5);
        return (hum * (0.4 + 0.6 * u) + cr * 0.4) * 0.5 * attack(t, 0.04) * std::min(1.0, (total - t) / 0.08);
      });
    }
    case Sfx::Toaster:
    {
      // The toast popping: the latch clacks, the spring twangs (a bent
      // pitch with a wobble), the bread thumps up.
      Osc spring, wob;
      Noise n(631);
      Svf clack;
      OnePole lp;
      return render(0.6, [&](double t, double total) {
        const double c = clack.band(n.next(), 2600.0, 3.0) * decay(t, 0.008) * 1.4;
        const double f = 180.0 + 260.0 * decay(t, 0.05) + 18.0 * wob.step(24.0, Wave::Sine) * decay(t, 0.2);
        const double tw = spring.step(f, Wave::Triangle) * decay(t, 0.16) * 0.45;
        const double th = t > 0.02 ? lp.lowpass(n.next(), 300.0) * decay(t - 0.02, 0.04) * 1.6 : 0.0;
        return (c + tw + th) * 0.6 * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::CleaverThunk:
    {
      // A cleaver biting into the wall: a heavy woody thunk and the blade
      // left quivering (a metal ring with a fast wobble).
      Osc body, r1, r2, wob;
      Noise n(641);
      Svf wood;
      return render(0.7, [&](double t, double total) {
        const double k = body.step(sweep(150.0, 60.0, t / 0.08), Wave::Sine) * decay(t, 0.06);
        const double knock = wood.band(n.next(), 520.0, 2.0) * decay(t, 0.03);
        const double q = 0.6 + 0.4 * wob.step(14.0, Wave::Sine);
        const double ring = (r1.step(930.0, Wave::Sine) * 0.5 + r2.step(2410.0, Wave::Sine) * 0.25) * decay(t, 0.22) * q;
        return (k * 0.9 + knock * 0.8 + ring * 0.3) * 0.75 * attack(t, 0.001) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::Mist:
    {
      // The Chrono Mister: the pump's soft click, then an airy spray hiss
      // (noise through a bright, slowly wandering band) easing off.
      Noise n(653), m(659);
      Svf spray, body;
      Osc drift;
      return render(0.55, [&](double t, double total) {
        const double c = body.band(m.next(), 1500.0, 4.0) * decay(t, 0.006) * 1.2;
        const double f = 5200.0 + 900.0 * drift.step(3.0, Wave::Sine);
        const float x = n.next();
        const double h = spray.band(x, f, 0.9) * 0.75 + x * 0.06;
        const double env = attack(t, 0.03) * (0.55 + 0.45 * decay(t, 0.18)) * std::min(1.0, (total - t) / 0.22);
        return (c + h * env) * 0.42;
      });
    }
    case Sfx::AntAlarm:
    {
      // A Soldier Ant column alarmed: a rattle of tiny chitinous clicks, fast
      // and uneven, under a thin stridulating buzz that rises.
      Noise n(661), jit(673);
      Svf click, rasp;
      Osc buzz;
      double next = 0.0, last = -1.0;
      return render(0.5, [&](double t, double total) {
        if (t >= next)
        {
          last = t;
          next = t + 0.018 + 0.016 * (double(jit.next()) * 0.5 + 0.5);
        }
        const double since = t - last;
        const double c = click.band(n.next(), 3900.0, 5.0) * std::exp(-since / 0.0025) * 1.8;
        const float saw = buzz.step(sweep(180.0, 260.0, t / total), Wave::Saw);
        const double b = rasp.band(saw, 2600.0, 3.0) * (0.5 + 0.5 * std::sin(t * 2.0 * kPi * 34.0));
        return (c * 0.8 + b * 0.22) * 0.34 * attack(t, 0.004) * std::min(1.0, (total - t) / 0.08);
      });
    }
    case Sfx::BeetleJet:
    {
      // A Bombardier Beetle firing: a boiling pop (a low thump and a
      // flurry of bubble blips) and the scalding hiss of the spray.
      Osc thump, bub;
      Noise n(677), b(683);
      Svf hiss;
      OnePole lp;
      double next = 0.0, last = -1.0, pitch = 600.0;
      return render(0.6, [&](double t, double total) {
        const double k = thump.step(sweep(160.0, 50.0, t / 0.07), Wave::Sine) * decay(t, 0.05) * 1.1;
        const double crack = lp.lowpass(n.next(), 2400.0) * decay(t, 0.012) * 1.2;
        if (t >= next && t < 0.32)
        {
          last = t;
          next = t + 0.02 + 0.03 * (double(b.next()) * 0.5 + 0.5);
          pitch = 380.0 + 520.0 * (double(b.next()) * 0.5 + 0.5);
        }
        const double since = t - last;
        const double blip = since < 0.03 ? bub.step(pitch * (1.0 + since * 30.0), Wave::Sine) * std::exp(-since / 0.008) * 0.7 : 0.0;
        const double h = hiss.band(n.next(), sweep(4200.0, 2600.0, t / total), 1.2) * attack(t, 0.02) * decay(t, 0.22);
        return (k + crack + blip + h * 0.9) * 0.55 * attack(t, 0.001) * std::min(1.0, (total - t) / 0.08);
      });
    }
    case Sfx::Narrator:
    {
      // A hushed documentary voice, wordless: three or four breathy blips
      // around 200 Hz, each a soft voiced vowel buried in breath.
      Osc voice, vib;
      Noise n(691);
      Svf f1, f2, breath;
      static const double kSyl[4][4] = {
        // start, length, pitch, vowel formant
        {0.00, 0.09, 205.0, 650.0},
        {0.12, 0.07, 190.0, 900.0},
        {0.22, 0.11, 214.0, 560.0},
        {0.37, 0.12, 178.0, 760.0}};
      return render(0.55, [&](double t, double total) {
        double v = 0.0;
        for (const auto& s : kSyl)
        {
          const double u = (t - s[0]) / s[1];
          if (u < 0.0 || u > 1.0)
            continue;
          const double env = std::sin(kPi * std::sqrt(u));
          const double p = s[2] * (1.0 - 0.06 * u) * (1.0 + 0.012 * vib.step(5.5, Wave::Sine));
          const float x = voice.step(p, Wave::Saw);
          const double vowel = f1.band(x, s[3], 5.0) * 0.6 + f2.band(x, 2300.0, 6.0) * 0.18;
          const double air = breath.band(n.next(), 1800.0, 0.8) * 0.5;
          v += (vowel + air) * env;
        }
        return v * 0.42 * std::min(1.0, (total - t) / 0.03);
      });
    }
    case Sfx::Roar:
    {
      // Gorgazilla's roar: a man growling into a megaphone off camera. A
      // rough voice (a saw with a jittering pitch) through two vowel
      // formants that open from "oo" to "ah", band-limited like a cheap horn
      // and driven into its distortion; the pitch climbs, then cracks and
      // falls away.
      Osc voice, sub, vib;
      Noise n(701), jit(709);
      Svf f1, f2, horn, breath;
      OnePole hp;
      double wob = 0.0;
      return render(1.4, [&](double t, double total) {
        const double u = t / total;
        wob = wob * 0.995 + double(jit.next()) * 0.005;
        double p = u < 0.55 ? sweep(82.0, 128.0, u / 0.55) : sweep(150.0, 70.0, (u - 0.55) / 0.45);
        p *= 1.0 + 0.03 * vib.step(7.0, Wave::Sine) + 0.6 * wob;
        const float x = voice.step(p, Wave::Saw);
        const double open = std::min(1.0, u / 0.3);
        const double vowel = f1.band(x, lerp(380.0, 760.0, open), 4.0) * 0.8 + f2.band(x, lerp(900.0, 1250.0, open), 5.0) * 0.45;
        const double growl = sub.step(p * 0.5, Wave::Square, 0.3) * 0.25 * (0.5 + 0.5 * std::sin(t * 2.0 * kPi * 31.0));
        const double air = breath.band(n.next(), 1700.0, 0.7) * 0.35;
        const double megaphone = hp.highpass(horn.band(float(vowel + growl + air), 1100.0, 0.8), 260.0);
        const double env = attack(t, 0.08) * std::min(1.0, (total - t) / 0.35);
        return std::tanh(megaphone * 3.2) * 0.42 * env;
      });
    }
    case Sfx::BuildingCrumble:
    {
      // A model building giving way: the balsa frame crunching (a burst of
      // crackling snaps that thins out), a soft thump onto the plywood and
      // a rattle of little bits bouncing after.
      Noise n(719), m(727), r(733);
      Svf snap, bits;
      Osc body;
      OnePole lp;
      double held = 0.0;
      int counter = 0;
      return render(0.95, [&](double t, double total) {
        if (counter-- <= 0)
        {
          held = double(m.next());
          counter = 6 + int(t * 60.0);
        }
        const double crunch = lp.lowpass(float(held), 1400.0) * decay(t, 0.18) * 0.9;
        const double rate = lerp(0.9, 0.995, std::min(1.0, t / 0.5));
        const double s = n.next() > rate ? snap.band(r.next(), 2400.0, 3.0) * 3.0 : snap.band(0.0f, 2400.0, 3.0);
        const double thump = t > 0.06 ? body.step(sweep(120.0, 48.0, (t - 0.06) / 0.12), Wave::Sine) * decay(t - 0.06, 0.09) : 0.0;
        const double rattle = t > 0.25 && n.next() > 0.985 ? bits.band(r.next(), 3600.0, 4.0) * 2.0 : bits.band(0.0f, 3600.0, 4.0);
        return (crunch + s * 0.5 + thump * 0.8 + rattle * 0.3) * 0.5 * attack(t, 0.002) * std::min(1.0, (total - t) / 0.1);
      });
    }
    case Sfx::BreathBeam:
    {
      // The Atomic Breath: a rising electric whoosh (noise through a band
      // sweeping up), a buzzing core at the mains hum's harmonics and the
      // crackle of sparks along the beam.
      Noise n(739), c(743);
      Svf rush, zap;
      Osc buzz, core;
      OnePole lp;
      return render(1.1, [&](double t, double total) {
        const double u = t / total;
        const double whoosh = rush.band(n.next(), sweep(500.0, 2600.0, std::min(1.0, u * 1.4)), 1.4);
        const double hum = lp.lowpass(buzz.step(110.0 + 6.0 * std::sin(t * 40.0), Wave::Saw), 900.0) * 0.5;
        const double hot = core.step(sweep(220.0, 440.0, u), Wave::Sine) * 0.25;
        const double sp = c.next() > 0.985 ? zap.band(n.next(), 4200.0, 2.0) * 3.0 : zap.band(0.0f, 4200.0, 2.0);
        const double env = attack(t, 0.12) * std::min(1.0, (total - t) / 0.25);
        return (whoosh * 0.6 + hum + hot + sp * 0.35) * 0.3 * env;
      });
    }
    case Sfx::MaserHum:
    {
      // A Maser dish charging: a heavy transformer hum climbing in pitch,
      // pulsing faster and faster, with a thin whine riding on top.
      Osc hum, whine;
      OnePole lp;
      double ph = 0.0;
      return render(0.85, [&](double t, double total) {
        const double u = t / total;
        const double f = sweep(70.0, 190.0, u);
        ph += sweep(5.0, 22.0, u) / kRate;
        const double pulse = 0.55 + 0.45 * std::sin(2.0 * kPi * ph);
        const double h = lp.lowpass(hum.step(f, Wave::Square, 0.5), 700.0) * pulse;
        const double w = whine.step(sweep(1100.0, 2600.0, u), Wave::Sine) * 0.12 * u;
        return (h * 0.55 + w) * 0.32 * attack(t, 0.03) * std::min(1.0, (total - t) / 0.06);
      });
    }
    case Sfx::SlideWhistle:
    {
      // Level 28's fall: a slide whistle gliding down two octaves, with the
      // player's breathy vibrato and a little air on top.
      Osc o, vib;
      Noise n(751);
      Svf air;
      return render(0.75, [&](double t, double total) {
        const double u = t / total;
        const double f = sweep(1500.0, 360.0, u * u * 0.4 + u * 0.6) * (1.0 + 0.012 * vib.step(9.0, Wave::Sine));
        const double tone = o.step(f, Wave::Sine);
        const double breath = air.band(n.next(), f * 1.5, 2.0) * 0.25;
        return (tone + breath) * 0.3 * attack(t, 0.02) * std::min(1.0, (total - t) / 0.12);
      });
    }
    case Sfx::Bonk:
    {
      // Running into painted rock: a hollow wood block knock, then a spring
      // boinging out of it, wobbling down.
      Osc block, spring, wob;
      Noise n(757);
      Svf knock;
      return render(0.6, [&](double t, double total) {
        const double k = (block.step(sweep(820.0, 560.0, t / 0.03), Wave::Sine) * 0.8 + knock.band(n.next(), 1600.0, 6.0) * 0.6) *
          decay(t, 0.025);
        const double ts = std::max(0.0, t - 0.03);
        const double f = 220.0 + 140.0 * std::sin(2.0 * kPi * 11.0 * ts) * decay(ts, 0.2);
        const double boing = t > 0.03 ? spring.step(f * (1.0 + 0.02 * wob.step(30.0, Wave::Sine)), Wave::Triangle) * decay(ts, 0.18) : 0.0;
        return (k + boing * 0.5) * 0.42 * attack(t, 0.001) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::HoleGulp:
    {
      // A Portable Hole opening: a cartoon gulp. A deep hollow bloop that
      // drops in pitch through a throaty vowel, then a little click of a
      // swallow.
      Osc o, sub;
      Svf throat;
      Noise n(761);
      return render(0.45, [&](double t, double) {
        const double f = sweep(330.0, 70.0, std::min(1.0, t / 0.28));
        const double v = throat.band(o.step(f, Wave::Saw), lerp(900.0, 380.0, std::min(1.0, t / 0.28)), 3.0);
        const double body = sub.step(f * 0.5, Wave::Sine) * 0.6;
        const double click = t > 0.3 ? n.next() * decay(t - 0.3, 0.006) * 0.5 : 0.0;
        return (v * 0.9 + body) * 0.38 * attack(t, 0.01) * decay(t, 0.16) + click * 0.3;
      });
    }
    case Sfx::PianoCrash:
    {
      // A piano landing: a wooden smash, a low thud through the floor and
      // every string jangling at once, out of tune, ringing down.
      std::vector<float> out(std::size_t(samples(1.4)), 0.0f);
      for (int k = 0; k < 9; ++k)
        addPiano(out, 0.005 * double(k), 1.3, 33.0 + double(k) * 5.0 + double(k % 3) * 0.4, 0.13);
      Noise n(769);
      Svf wood;
      Osc thud;
      for (int i = 0; i < samples(0.4); ++i)
      {
        const double t = double(i) / kRate;
        const double smash = wood.band(n.next(), sweep(2200.0, 500.0, t / 0.3), 1.2) * decay(t, 0.08);
        const double boom = thud.step(sweep(95.0, 38.0, t / 0.2), Wave::Sine) * decay(t, 0.1);
        out[std::size_t(i)] += float((smash * 0.7 + boom * 0.6) * attack(t, 0.001));
      }
      for (auto& v : out)
        v = float(std::tanh(double(v) * 1.2) * 0.55);
      return out;
    }
    case Sfx::PieSplat:
    {
      // A cream pie in the face: a wet splat (noise squeezed through a
      // falling band) and a soft low plop under it.
      Noise n(773);
      Svf wet;
      Osc plop;
      OnePole lp;
      return render(0.4, [&](double t, double total) {
        const double s = wet.band(n.next(), sweep(2600.0, 400.0, t / 0.2), 0.9) * decay(t, 0.07);
        const double drip = lp.lowpass(n.next(), 600.0) * decay(t, 0.15) * 0.4;
        const double p = plop.step(sweep(260.0, 90.0, t / 0.08), Wave::Sine) * decay(t, 0.05);
        return (s * 0.8 + drip + p * 0.6) * 0.45 * attack(t, 0.001) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::PencilScratch:
    {
      // The giant pencil drawing: dry graphite on paper, a scratchy noise
      // band that rasps in strokes.
      Noise n(787), g(797);
      Svf lead;
      OnePole hp;
      return render(0.7, [&](double t, double total) {
        const double stroke = 0.5 + 0.5 * std::sin(2.0 * kPi * 7.0 * t);
        const double grit = g.next() > 0.6f ? 1.0 : 0.35;
        const double x = hp.highpass(lead.band(n.next(), 3200.0 + 900.0 * stroke, 1.5), 900.0);
        return x * grit * (0.35 + 0.65 * stroke) * 0.4 * attack(t, 0.02) * std::min(1.0, (total - t) / 0.1);
      });
    }
    case Sfx::EraserRub:
    {
      // The eraser on the floor: a squeaking rubber scrub, back and forth.
      Osc squeak;
      Noise n(809);
      Svf rub;
      return render(0.75, [&](double t, double total) {
        const double ph = std::sin(2.0 * kPi * 6.0 * t);
        const double f = 700.0 + 260.0 * ph;
        const double sq = squeak.step(f, Wave::Square, 0.3) * 0.2 * std::max(0.0, std::abs(ph) - 0.3);
        const double scrub = rub.band(n.next(), 1300.0 + 500.0 * ph, 1.1) * 0.6;
        return (sq + scrub) * 0.42 * attack(t, 0.02) * std::min(1.0, (total - t) / 0.1);
      });
    }
    case Sfx::Honk:
    {
      // Buddy Boom hit: a squeeze-bulb horn, a reedy double honk.
      Osc a, b;
      Svf reed;
      return render(0.36, [&](double t, double total) {
        const double on = t < 0.14 ? 1.0 : (t > 0.18 ? 1.0 : 0.0);
        const double f = t < 0.16 ? 420.0 : 380.0;
        const double x = a.step(f, Wave::Saw) + b.step(f * 1.01, Wave::Square, 0.4) * 0.5;
        const double env = t < 0.16 ? attack(t, 0.01) * decay(t, 0.2) : attack(t - 0.18, 0.01) * decay(t - 0.18, 0.12);
        return reed.band(float(x), 1100.0, 1.5) * on * env * 0.36 * std::min(1.0, (total - t) / 0.03);
      });
    }
    case Sfx::RubberBoing:
    {
      // A flat Rubber Goon bouncing a runner up: a long twanging boing, the
      // pitch wobbling wide and settling up high.
      Osc o, sub;
      return render(0.8, [&](double t, double total) {
        const double f = sweep(120.0, 420.0, std::min(1.0, t / 0.4)) * (1.0 + 0.25 * std::sin(2.0 * kPi * 13.0 * t) * decay(t, 0.25));
        const double v = o.step(f, Wave::Triangle) + 0.4 * sub.step(f * 0.5, Wave::Sine);
        return v * 0.34 * attack(t, 0.004) * decay(t, 0.35) * std::min(1.0, (total - t) / 0.08);
      });
    }
    case Sfx::DialTurn:
    {
      // An RGB dial clicking round a notch: a hard plastic click, a blip of
      // static as the picture starts to change, and two rising notes.
      Noise n(401), m(409);
      Svf bp;
      OnePole hp;
      Osc a;
      return render(0.32, [&](double t, double total) {
        const double c = bp.band(n.next(), 2600.0, 3.0) * decay(t, 0.005) * 0.45;
        const double st = t > 0.015 ? hp.highpass(m.next(), 1800.0) * decay(t - 0.015, 0.04) * 0.22 : 0.0;
        const double u = t - 0.05;
        const double tone = u > 0.0 ? a.step(u < 0.08 ? 660.0 : 990.0, Wave::Sine) * 0.15 * attack(u, 0.004) *
            (u < 0.08 ? 1.0 : decay(u - 0.08, 0.07)) : 0.0;
        return (c + st + tone) * std::min(1.0, (total - t) / 0.03);
      });
    }
    case Sfx::ChannelFlip:
    {
      // The change landing: a picture tube's thump as every bar swaps, the
      // mains hum swelling and dying under it, a crackle across the top.
      Osc thump, hum;
      Noise n(419);
      OnePole hp;
      Svf lp;
      return render(0.34, [&](double t, double total) {
        const double th = thump.step(sweep(110.0, 45.0, t / 0.12), Wave::Sine) * decay(t, 0.06) * 0.36;
        const double h = lp.low(float(hum.step(120.0, Wave::Saw)), 700.0, 1.0) * decay(t, 0.1) * 0.12;
        const double cr = (n.next() > 0.88f ? 1.0 : 0.0) * hp.highpass(n.next(), 3000.0) * decay(t, 0.05) * 0.28;
        return (th + h + cr) * attack(t, 0.002) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::TintTurn:
    {
      // The Tri-Tone's selector: a tight detent and a short pure blip.
      Noise n(421);
      Svf bp;
      Osc a;
      return render(0.12, [&](double t, double total) {
        const double c = bp.band(n.next(), 3400.0, 4.0) * decay(t, 0.003) * 0.35;
        const double b = t > 0.012 ? a.step(1320.0, Wave::Sine) * 0.14 * attack(t - 0.012, 0.003) * decay(t - 0.012, 0.035) : 0.0;
        return (c + b) * std::min(1.0, (total - t) / 0.02);
      });
    }
    case Sfx::SnowHiss:
    {
      // A Static Swarm winding up: TV snow swelling, its band climbing.
      Noise n(431);
      Svf bp;
      return render(0.2, [&](double t, double total) {
        const double u = t / total;
        return bp.band(n.next(), sweep(1800.0, 6500.0, u), 0.9) * 0.3 * u * u * std::min(1.0, (total - t) / 0.02);
      });
    }
    case Sfx::SnowLatch:
    {
      // A Static Swarm latching on: a burst of static clamping down, the
      // crackle held and stuttering, and a buzzy chirp.
      Noise n(433), m(439);
      OnePole hp;
      Osc chirp;
      double held = 0.0;
      int hold = 0;
      return render(0.36, [&](double t, double total) {
        if (hold-- <= 0)
        {
          held = m.next();
          hold = 6 + int(t * 60.0);
        }
        const double st = hp.highpass(n.next(), 900.0) * 0.5 + held * 0.25;
        const double gate = std::sin(t * 2.0 * kPi * 22.0) > -0.3 ? 1.0 : 0.35;
        const double ch = t < 0.08 ? chirp.step(sweep(900.0, 2400.0, t / 0.08), Wave::Square, 0.3) * 0.08 : 0.0;
        return (st * gate * 0.45 + ch) * attack(t, 0.004) * decay(t, 0.16) * std::min(1.0, (total - t) / 0.04);
      });
    }
    case Sfx::TestTone:
    {
      // The Test Card's tell: the 1 kHz test tone, a quarter second, with
      // the faintest hum under it.
      Osc a, b;
      return render(0.25, [&](double t, double total) {
        const double tone = a.step(1000.0, Wave::Sine) + b.step(3000.0, Wave::Sine) * 0.04;
        return tone * 0.12 * attack(t, 0.01) * std::min(1.0, (total - t) / 0.02);
      });
    }
    case Sfx::CardBeam:
    {
      // The Test Card's beam: a hard bright buzz, the sweep falling away,
      // fizzing at the edges.
      Osc a, b;
      Noise n(443);
      OnePole hp;
      Svf f;
      return render(0.4, [&](double t, double total) {
        const double fr = sweep(1400.0, 360.0, t / total);
        const double buzz = f.low(float(a.step(fr, Wave::Saw) * 0.6 + b.step(fr * 1.5, Wave::Square, 0.3) * 0.4), 3200.0, 1.2);
        const double fizz = hp.highpass(n.next(), 4000.0) * 0.5;
        return (buzz * 0.24 + fizz * 0.1) * attack(t, 0.004) * decay(t, 0.18) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::TuneIn:
    {
      // Tuned in: the static narrows to a clean tone, then the tone opens
      // into a bright major chord.
      std::vector<float> b(std::size_t(samples(1.6)), 0.0f);
      {
        Noise n(449);
        Svf bp;
        Osc tone;
        for (std::size_t i = 0; i < b.size(); ++i)
        {
          const double t = double(i) / kRate, u = std::min(1.0, t / 0.7);
          const double hiss = u < 1.0 ? bp.band(n.next(), 880.0, lerp(0.7, 12.0, u * u)) * (1.0 - u) * 0.3 : 0.0;
          const double pure = tone.step(880.0, Wave::Sine) * 0.12 * u * (t < 0.7 ? 1.0 : decay(t - 0.7, 0.3));
          b[i] = float((hiss + pure) * attack(t, 0.02));
        }
      }
      for (const double note : {69.0, 73.0, 76.0, 81.0})
        addTone(b, 0.62, 0.95, midiFreq(note), Wave::Triangle, 0.1, 0.5);
      addTone(b, 0.62, 0.95, midiFreq(57.0), Wave::Sine, 0.12, 0.6);
      return b;
    }
    case Sfx::SwimStroke:
    {
      // Level 30: a swim stroke: a soft muffled whoosh of water pushed back
      // and a burble of small bubbles rising after it.
      Noise n(503);
      Svf lp;
      Osc b;
      return render(0.34, [&](double t, double total) {
        const double env = attack(t, 0.03) * decay(t, 0.08);
        const double wh = lp.low(n.next(), sweep(500.0, 1400.0, std::min(1.0, t / 0.08)), 1.4) * env * 0.8;
        const double u = t - 0.07;
        double bub = 0.0;
        if (u > 0.0)
        {
          const double k = std::fmod(u, 0.055) / 0.055;
          const int i = int(u / 0.055);
          bub = b.step(sweep(500.0 + 90.0 * i, 1100.0 + 140.0 * i, k), Wave::Sine) * 0.12 * (1.0 - k) * decay(u, 0.1);
        }
        return (wh + bub) * std::min(1.0, (total - t) / 0.04);
      });
    }
    case Sfx::AbyssSplash:
    {
      // Spat back out of the abyss: a deep muffled boom, as if far below,
      // and a rush of bubbles climbing after it.
      Osc boom;
      Noise n(509), m(521);
      Svf lp, bp;
      Osc b;
      return render(0.8, [&](double t, double total) {
        const double bm = boom.step(sweep(90.0, 38.0, t / 0.4), Wave::Sine) * decay(t, 0.18) * 0.3;
        const double rush = lp.low(n.next(), sweep(300.0, 1800.0, t / total), 1.0) * attack(t, 0.12) * decay(t, 0.25) * 0.42;
        const double k = std::fmod(t, 0.07) / 0.07;
        const double bub = t > 0.12 ? b.step(sweep(380.0, 1300.0, k), Wave::Sine) * 0.09 * (1.0 - k) * decay(t - 0.12, 0.25) : 0.0;
        const double fizz = bp.band(m.next(), 2400.0, 2.0) * 0.06 * decay(t, 0.3);
        return (bm + rush + bub + fizz) * attack(t, 0.004) * std::min(1.0, (total - t) / 0.08);
      });
    }
    case Sfx::HarpoonPlant:
    {
      // A harpoon driven into the floor: a dull thud through the water and
      // a short muffled ring of its shaft.
      Noise n(523);
      Svf lp;
      Osc a, ring;
      return render(0.32, [&](double t, double total) {
        const double th = lp.low(n.next(), 600.0, 1.0) * decay(t, 0.03) * 0.5;
        const double body = a.step(sweep(140.0, 70.0, t / 0.12), Wave::Sine) * decay(t, 0.06) * 0.3;
        const double rg = ring.step(720.0, Wave::Triangle) * decay(t, 0.09) * 0.08 * (1.0 + 0.3 * std::sin(t * 2.0 * kPi * 9.0));
        return (th + body + rg) * attack(t, 0.002) * std::min(1.0, (total - t) / 0.04);
      });
    }
    case Sfx::HarpoonStick:
    {
      // A harpoon biting into rock or an anchor ring: a sharp thunk and a
      // bright clang with a short wobble.
      Noise n(541);
      Svf bp;
      Osc a, b, c;
      return render(0.4, [&](double t, double total) {
        const double tk = bp.band(n.next(), 1800.0, 1.5) * decay(t, 0.012) * 0.3;
        const double body = a.step(sweep(220.0, 110.0, t / 0.08), Wave::Sine) * decay(t, 0.04) * 0.2;
        const double wob = 1.0 + 0.004 * std::sin(t * 2.0 * kPi * 7.0);
        const double cl = (b.step(1180.0 * wob, Wave::Sine) + c.step(1740.0 * wob, Wave::Sine) * 0.6) * decay(t, 0.12) * 0.1;
        return (tk + body + cl) * attack(t, 0.001) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::LineTaut:
    {
      // The harpoon line snapping taut: a rope's creak and a low twang that
      // bends up as it settles.
      Noise n(547);
      Svf bp;
      Osc tw, tw2;
      return render(0.42, [&](double t, double total) {
        const double creak = bp.band(n.next(), 900.0, 6.0) * (std::fmod(t, 0.018) < 0.006 ? 1.0 : 0.3) * decay(t, 0.05) * 0.35;
        const double fr = sweep(70.0, 98.0, std::min(1.0, t / 0.06));
        const double twang = (tw.step(fr, Wave::Saw) * 0.5 + tw2.step(fr * 2.0, Wave::Triangle) * 0.5) * decay(t, 0.14) * 0.26;
        return (creak + twang) * attack(t, 0.003) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::EelSpark:
    {
      // A Cable Eel's box waking: crackling arcs over a hum that climbs.
      Noise n(557), m(563);
      OnePole hp;
      Osc hum;
      return render(0.5, [&](double t, double total) {
        const double u = t / total;
        const double arc = (m.next() > 0.8f ? 1.0 : 0.15) * hp.highpass(n.next(), 2500.0) * 0.34;
        const double h = hum.step(sweep(100.0, 240.0, u), Wave::Saw) * 0.12 * (0.6 + 0.4 * std::sin(t * 2.0 * kPi * 30.0));
        return (arc + h) * attack(t, 0.01) * (0.5 + 0.5 * u) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::EelSnap:
    {
      // A Cable Eel lunging out: a zap down its length, then a wet snap of
      // its jaws.
      Noise n(569), m(571);
      OnePole hp;
      Svf bp;
      Osc z;
      return render(0.36, [&](double t, double total) {
        const double zp = t < 0.14 ? (z.step(sweep(1600.0, 240.0, t / 0.14), Wave::Saw) * 0.16 + hp.highpass(n.next(), 3000.0) * 0.18) *
            decay(t, 0.07) : 0.0;
        const double u = t - 0.1;
        const double snap = u > 0.0 ? bp.band(m.next(), 1400.0, 2.0) * decay(u, 0.018) * 0.55 : 0.0;
        return (zp + snap) * attack(t, 0.002) * std::min(1.0, (total - t) / 0.04);
      });
    }
    case Sfx::AnglerLunge:
    {
      // An Anglerfish lunging: a rushing gulp of water and a clap of teeth.
      Noise n(577), m(587);
      Svf lp, bp;
      Osc g;
      return render(0.42, [&](double t, double total) {
        const double rush = lp.low(n.next(), sweep(250.0, 1200.0, std::min(1.0, t / 0.2)), 1.2) * attack(t, 0.06) * decay(t, 0.12) * 0.45;
        const double gulp = g.step(sweep(180.0, 60.0, t / 0.25), Wave::Sine) * decay(t, 0.1) * 0.22;
        const double u = t - 0.16;
        const double clap = u > 0.0 ? bp.band(m.next(), 2200.0, 1.6) * decay(u, 0.012) * 0.34 : 0.0;
        return (rush + gulp + clap) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::JellyPulse:
    {
      // A Box Jelly's bell contracting: a soft glassy pulse that swells and
      // sinks, like a finger round a wet glass.
      Osc a, b;
      return render(0.4, [&](double t, double total) {
        const double fr = sweep(520.0, 760.0, t / total);
        const double v = a.step(fr, Wave::Sine) + b.step(fr * 2.01, Wave::Sine) * 0.3;
        return v * 0.2 * attack(t, 0.06) * decay(t, 0.2) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::BubblePop:
    {
      // The Bubble Up bubble popping: a bright pop and a fine spray.
      Osc a;
      Noise n(593);
      OnePole hp;
      return render(0.24, [&](double t, double total) {
        const double pop = a.step(sweep(1500.0, 420.0, std::min(1.0, t / 0.05)), Wave::Sine) * decay(t, 0.03) * 0.36;
        const double spray = hp.highpass(n.next(), 3500.0) * decay(t, 0.06) * 0.16;
        return (pop + spray) * attack(t, 0.001) * std::min(1.0, (total - t) / 0.04);
      });
    }
    case Sfx::RingPass:
    {
      // A Bubble Up ring passed: two quick bubble blips and a rising bell.
      std::vector<float> b(std::size_t(samples(0.7)), 0.0f);
      {
        Osc a;
        for (std::size_t i = 0; i < b.size(); ++i)
        {
          const double t = double(i) / kRate;
          const double k = std::fmod(t, 0.06) / 0.06;
          b[i] = t < 0.12 ? float(a.step(sweep(500.0, 1400.0, k), Wave::Sine) * 0.14 * (1.0 - k)) : 0.0f;
        }
      }
      addTone(b, 0.1, 0.5, midiFreq(79), Wave::Sine, 0.16, 0.18);
      addTone(b, 0.18, 0.5, midiFreq(84), Wave::Sine, 0.14, 0.22);
      addTone(b, 0.18, 0.5, midiFreq(91), Wave::Sine, 0.04, 0.12);
      return b;
    }
    case Sfx::StormStrike:
    {
      // Level 31: lightning close by: a tearing crack (bright noise in fast
      // ragged bursts), the boom of the air closing behind it and a rumble
      // rolling away down the cliff.
      Noise n(601), m(607), q(613);
      OnePole hp, lp2;
      Svf lp;
      Osc boom;
      double held = 0.0;
      int counter = 0;
      return render(2.6, [&](double t, double total) {
        if (counter-- <= 0)
        {
          held = m.next();
          counter = int(lerp(2.0, 30.0, t / total));
        }
        const double rip = t < 0.12 ? hp.highpass(n.next(), 2200.0) * (q.next() > -0.2f ? 1.0 : 0.25) * decay(t, 0.04) : 0.0;
        const double bm = boom.step(sweep(70.0, 30.0, t / 0.6), Wave::Sine) * attack(t, 0.01) * decay(t, 0.35);
        const double roll = lp.low(float(held * 0.7 + n.next() * 0.3), sweep(1400.0, 80.0, t / total), 1.2) *
          (0.6 + 0.4 * std::sin(t * 9.0 + std::sin(t * 3.1) * 2.0)) * attack(t, 0.05) * decay(t, 0.9);
        const double body = lp2.lowpass(n.next(), 110.0) * 2.4 * decay(t, 0.6);
        return (rip * 0.5 + bm * 0.3 + roll * 0.5 + body * 0.35) * 0.68 * std::min(1.0, (total - t) / 0.3);
      });
    }
    case Sfx::StormGlow:
    {
      // The metal charging before a strike: a buzz of static that climbs
      // and swells, crackles coming faster and harder toward the end.
      Noise n(617), m(619);
      OnePole hp;
      Osc buzz, buzz2;
      return render(0.4, [&](double t, double total) {
        const double u = t / total;
        const double b = (buzz.step(sweep(90.0, 240.0, u), Wave::Saw) + buzz2.step(sweep(180.0, 486.0, u), Wave::Saw) * 0.4) *
          (0.5 + 0.5 * std::sin(t * 2.0 * kPi * 60.0)) * 0.12 * (0.3 + 0.7 * u);
        const double crack = (m.next() > float(0.94 - 0.12 * u) ? 1.0 : 0.0) * hp.highpass(n.next(), 3000.0) * (0.35 + 0.5 * u);
        return (b + crack) * 0.56 * attack(t, 0.02) * std::min(1.0, (total - t) / 0.02);
      });
    }
    case Sfx::RodThunk:
    {
      // A Tesla rod biting into rock: a hard thunk, a short copper ring and
      // a little electric zing off its bulb.
      Noise n(631);
      Svf bp;
      Osc a, b, c, z;
      return render(0.42, [&](double t, double total) {
        const double tk = bp.band(n.next(), 1500.0, 1.4) * decay(t, 0.012) * 0.45;
        const double body = a.step(sweep(200.0, 90.0, t / 0.08), Wave::Sine) * decay(t, 0.05) * 0.3;
        const double ring = (b.step(1320.0, Wave::Sine) + c.step(2150.0, Wave::Sine) * 0.5) * decay(t, 0.12) * 0.09;
        const double u = t - 0.04;
        const double zing = u > 0.0 ? z.step(sweep(2400.0, 5200.0, u / 0.15), Wave::Square, 0.3) * decay(u, 0.05) * 0.05 : 0.0;
        return (tk + body + ring + zing) * 0.84 * attack(t, 0.001) * std::min(1.0, (total - t) / 0.04);
      });
    }
    case Sfx::RodArc:
    {
      // A struck rod's arcs: a sharp zap leaping out and a sizzle after.
      Noise n(641), m(643);
      OnePole hp;
      Osc z, z2;
      return render(0.4, [&](double t, double total) {
        const double zap = (z.step(sweep(2600.0, 300.0, t / 0.12), Wave::Saw) * 0.5 + z2.step(sweep(1300.0, 160.0, t / 0.12), Wave::Square) * 0.3) *
          decay(t, 0.05);
        const double sz = (m.next() > 0.6f ? 1.0 : 0.2) * hp.highpass(n.next(), 4000.0) * decay(t, 0.12);
        return (zap * 0.36 + sz * 0.3) * attack(t, 0.001) * std::min(1.0, (total - t) / 0.04);
      });
    }
    case Sfx::MachinePower:
    {
      // A dead machine waking: a relay's clack, then a motor spinning up
      // from a growl to a steady electric hum.
      Noise n(653);
      Svf bp;
      Osc mo, mo2, hum;
      return render(1.1, [&](double t, double total) {
        const double clack = bp.band(n.next(), 1800.0, 2.0) * decay(t, 0.01) * 0.6;
        const double u = std::min(1.0, t / 0.7);
        const double f = sweep(40.0, 180.0, u * u * (3.0 - 2.0 * u));
        const double motor = (mo.step(f, Wave::Saw) * 0.6 + mo2.step(f * 2.0, Wave::Triangle) * 0.4) * 0.16 * attack(t, 0.08);
        const double h = hum.step(120.0, Wave::Sine) * 0.08 * std::max(0.0, (t - 0.5) / 0.5);
        return (clack + motor + h) * std::min(1.0, (total - t) / 0.2);
      });
    }
    case Sfx::GullCry:
    {
      // A herring gull's cry: a harsh "kee-yow", pitch flung up then
      // falling, its rasp in the overtones; a second shorter one after.
      Osc a, b, c;
      Noise n(659);
      Svf bp;
      return render(0.62, [&](double t, double total) {
        auto call = [&](double u, double len) {
          if (u < 0.0 || u > len)
            return 0.0;
          const double k = u / len;
          const double f = k < 0.25 ? sweep(1100.0, 1900.0, k / 0.25) : sweep(1900.0, 850.0, (k - 0.25) / 0.75);
          const double rasp = 1.0 + 0.35 * std::sin(u * 2.0 * kPi * 70.0);
          const double v = (a.step(f, Wave::Saw) * 0.5 + b.step(f * 1.5, Wave::Triangle) * 0.3 + c.step(f * 0.5, Wave::Sine) * 0.2) * rasp;
          return bp.band(float(v + n.next() * 0.15), f * 1.4, 1.2) * std::min(1.0, u / 0.015) * std::min(1.0, (len - u) / 0.05);
        };
        return (call(t, 0.34) + call(t - 0.38, 0.2) * 0.7) * 0.42 * std::min(1.0, (total - t) / 0.02);
      });
    }
    case Sfx::GullDive:
    {
      // A Storm Gull diving: wind rushing past its folded wings, rising.
      Noise n(661);
      Svf bp;
      return render(0.5, [&](double t, double total) {
        const double u = t / total;
        return bp.band(n.next(), sweep(500.0, 2600.0, u), 1.6) * attack(t, 0.15) * decay(t, 0.3) * 0.9 *
          std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::KiteWhine:
    {
      // A Kite Drone turning at you: its rotors' whine climbing, a buzz in
      // the blades, a wobble of the wind on them.
      Osc a, b, lfo;
      Noise n(673);
      OnePole hp;
      return render(0.55, [&](double t, double total) {
        const double u = t / total;
        const double f = sweep(380.0, 1500.0, std::min(1.0, u * 1.3));
        const double chop = 0.65 + 0.35 * std::sin(t * 2.0 * kPi * sweep(30.0, 90.0, u));
        const double v = (a.step(f, Wave::Saw) * 0.45 + b.step(f * 2.0, Wave::Sine) * 0.35) * chop;
        const double air = hp.highpass(n.next(), 2500.0) * 0.12;
        return (v * 0.22 + air) * attack(t, 0.03) * std::min(1.0, (total - t) / 0.08);
      });
    }
    case Sfx::FogWheeze:
    {
      // A Foghorn's bellows filling: a breathy inhale rising, a reed
      // wheezing in it.
      Noise n(677);
      Svf bp, bp2;
      Osc reed;
      return render(0.4, [&](double t, double total) {
        const double u = t / total;
        const double breath = bp.band(n.next(), sweep(500.0, 1400.0, u), 1.3) * 0.5;
        const double wz = bp2.band(float(reed.step(sweep(300.0, 460.0, u), Wave::Saw)), 900.0, 3.0) * 0.25 * u;
        return (breath + wz) * 0.86 * attack(t, 0.12) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::FogBlast:
    {
      // A Foghorn's blast: a deep brassy diaphone, the "BEEE-oh" of a
      // lighthouse horn, its grunt at the end, the air rumbling with it.
      Osc a, b, c, d;
      Svf lp;
      Noise n(683);
      OnePole lp2;
      return render(1.0, [&](double t, double total) {
        const double fall = t > 0.7 ? sweep(1.0, 0.82, (t - 0.7) / 0.3) : 1.0;
        const double f = 98.0 * fall * (1.0 + 0.004 * std::sin(t * 2.0 * kPi * 5.0));
        const double v = a.step(f, Wave::Saw) * 0.55 + b.step(f * 2.0, Wave::Saw) * 0.3 + c.step(f * 3.0, Wave::Square) * 0.12 +
          d.step(f * 0.5, Wave::Sine) * 0.4;
        const double brass = lp.low(float(v), sweep(600.0, 1500.0, std::min(1.0, t / 0.15)), 1.6);
        const double air = lp2.lowpass(n.next(), 300.0) * 0.6;
        return (brass * 0.4 + air * 0.3) * 0.9 * attack(t, 0.04) * std::min(1.0, (total - t) / 0.12);
      });
    }
    case Sfx::BungeeStretch:
    {
      // Bungee's rope stretching: a rubbery creak, slow ticks of friction.
      Noise n(691);
      Svf bp;
      Osc o;
      return render(0.45, [&](double t, double total) {
        const double tick = std::fmod(t, 0.035) < 0.012 ? 1.0 : 0.25;
        const double creak = bp.band(n.next(), sweep(300.0, 650.0, t / total), 5.0) * tick * 0.7;
        const double groan = o.step(sweep(90.0, 140.0, t / total), Wave::Triangle) * 0.08;
        return (creak + groan) * 0.64 * attack(t, 0.03) * std::min(1.0, (total - t) / 0.06);
      });
    }
    case Sfx::BungeeTwang:
    {
      // Let go: a deep elastic twang that bends up, its wobble dying away.
      Osc a, b;
      Noise n(701);
      Svf bp;
      return render(0.7, [&](double t, double total) {
        const double f = sweep(55.0, 140.0, std::min(1.0, t / 0.25)) * (1.0 + 0.06 * std::sin(t * 2.0 * kPi * 9.0) * decay(t, 0.2));
        const double v = (a.step(f, Wave::Saw) * 0.6 + b.step(f * 2.0, Wave::Triangle) * 0.4) * decay(t, 0.22);
        const double slap = bp.band(n.next(), 900.0, 1.5) * decay(t, 0.02);
        return (v * 0.32 + slap * 0.4) * attack(t, 0.002) * std::min(1.0, (total - t) / 0.08);
      });
    }
    case Sfx::StormRing:
    {
      // A storm ring taken: a crackle, then a bright electric chime.
      std::vector<float> b(std::size_t(samples(0.7)), 0.0f);
      {
        Noise n(709), m(719);
        OnePole hp;
        for (std::size_t i = 0; i < b.size(); ++i)
        {
          const double t = double(i) / kRate;
          b[i] = t < 0.1 ? float((m.next() > 0.5f ? 1.0 : 0.15) * hp.highpass(n.next(), 3500.0) * decay(t, 0.03) * 0.3) : 0.0f;
        }
      }
      addTone(b, 0.03, 0.55, midiFreq(81), Wave::Sine, 0.15, 0.2);
      addTone(b, 0.08, 0.55, midiFreq(88), Wave::Sine, 0.13, 0.22);
      addTone(b, 0.08, 0.45, midiFreq(93), Wave::Triangle, 0.05, 0.12);
      return b;
    }
    case Sfx::DishGroan:
    {
      // Level 32: a big dish about to turn: a deep steel groan bending
      // under its own weight, the bearings creaking in slow ticks, a
      // shudder through the frame.
      Osc a, b, c;
      Noise n(727), m(733);
      Svf bp, lp;
      return render(1.7, [&](double t, double total) {
        const double u = t / total;
        const double f = 46.0 * (1.0 + 0.08 * std::sin(t * 2.0 * kPi * 1.3) + 0.04 * std::sin(t * 2.0 * kPi * 3.7)) * sweep(1.0, 0.86, u);
        const double steel = lp.low(float(a.step(f, Wave::Saw) * 0.6 + b.step(f * 2.02, Wave::Saw) * 0.3 + c.step(f * 3.01, Wave::Square) * 0.12),
          sweep(380.0, 620.0, std::sin(u * kPi)), 2.4);
        const double tick = std::fmod(t, 0.11) < 0.03 ? 1.0 : 0.15;
        const double creak = bp.band(n.next(), sweep(700.0, 420.0, u), 6.0) * tick * (0.6 + 0.4 * (m.next() > 0.0f ? 1.0 : 0.5));
        const double shudder = 0.75 + 0.25 * std::sin(t * 2.0 * kPi * 7.0);
        return (steel * 0.25 * shudder + creak * 0.19) * attack(t, 0.25) * std::min(1.0, (total - t) / 0.4);
      });
    }
    case Sfx::DishTurn:
    {
      // A dish turning: its drive motors spinning up to a hum, the gears
      // ticking under it.
      Osc mo, mo2, hum;
      Noise n(739);
      Svf bp;
      return render(1.3, [&](double t, double total) {
        const double u = std::min(1.0, t / 0.6);
        const double f = sweep(30.0, 110.0, u * u * (3.0 - 2.0 * u));
        const double motor = (mo.step(f, Wave::Saw) * 0.6 + mo2.step(f * 3.0, Wave::Triangle) * 0.3) * 0.18;
        const double h = hum.step(220.0, Wave::Sine) * 0.05 * u;
        const double gear = bp.band(n.next(), 2400.0, 3.0) * (std::fmod(t, 0.045) < 0.006 ? 1.0 : 0.0) * 0.2;
        return (motor + h + gear) * 0.8 * attack(t, 0.08) * std::min(1.0, (total - t) / 0.35);
      });
    }
    case Sfx::JamBurst:
    {
      // The Signal Jammer: a warbling burst of radio noise, a carrier
      // wobbling fast and falling, chopped static on it.
      Osc car, lfo;
      Noise n(743), m(751);
      Svf bp;
      return render(0.4, [&](double t, double total) {
        const double wob = std::sin(t * 2.0 * kPi * 38.0);
        const double f = sweep(1400.0, 500.0, t / total) * (1.0 + 0.25 * wob);
        const double tone = car.step(f, Wave::Square, 0.4) * 0.14;
        const double chop = m.next() > 0.1f ? 1.0 : 0.2;
        const double hiss = bp.band(n.next(), sweep(3200.0, 1200.0, t / total), 1.0) * chop * 0.45;
        return (tone + hiss) * attack(t, 0.004) * decay(t, 0.18) * 0.95 * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::RobotJammed:
    {
      // A robot jammed: garbled little bleeps tumbling down, each a random
      // pitch, and a fizz under them.
      Osc a;
      Noise n(757), pick(761);
      OnePole hp;
      double f = 1200.0;
      int left = 0;
      return render(0.55, [&](double t, double total) {
        if (left-- <= 0)
        {
          f = (500.0 + 650.0 * (double(pick.next()) + 1.0)) * sweep(1.0, 0.45, t / total);
          left = int(0.04 * kRate);
        }
        const double bleep = a.step(f, Wave::Square, 0.5) * 0.13 * (std::fmod(t, 0.04) < 0.03 ? 1.0 : 0.0);
        const double fizz = hp.highpass(n.next(), 4500.0) * 0.12 * decay(t, 0.25);
        return (bleep + fizz) * 1.4 * attack(t, 0.003) * std::min(1.0, (total - t) / 0.08);
      });
    }
    case Sfx::SandBulge:
    {
      // A Sand Shark about to breach: a low rumble swelling under the
      // sand, the grains hissing up over it.
      Noise n(769), m(773);
      OnePole lp;
      Svf bp;
      Osc o;
      return render(0.8, [&](double t, double total) {
        const double u = t / total;
        const double rum = lp.lowpass(n.next(), 140.0) * 2.6 + o.step(sweep(42.0, 60.0, u), Wave::Sine) * 0.4;
        const double hiss = bp.band(m.next(), sweep(1800.0, 4200.0, u), 1.2) * u * 0.5;
        return (rum * 0.43 + hiss * 0.48) * attack(t, 0.3) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::SharkBreach:
    {
      // A Sand Shark breaching: a whoosh of sand spraying up and the snap
      // of its jaws.
      Noise n(787), m(797);
      Svf bp, bp2;
      Osc thud;
      return render(0.5, [&](double t, double total) {
        const double spray = bp.band(n.next(), sweep(900.0, 3000.0, std::min(1.0, t / 0.2)), 0.9) * attack(t, 0.02) * decay(t, 0.16) * 0.6;
        const double u = t - 0.14;
        const double snap = u > 0.0 ? bp2.band(m.next(), 1600.0, 2.5) * decay(u, 0.015) * 0.6 : 0.0;
        const double body = u > 0.0 ? thud.step(sweep(180.0, 70.0, u / 0.06), Wave::Sine) * decay(u, 0.05) * 0.4 : 0.0;
        return (spray + snap + body) * 0.5 * std::min(1.0, (total - t) / 0.06);
      });
    }
    case Sfx::ScrapDrop:
    {
      // Scrap let go: a falling whistle, the air rushing past it.
      Osc a;
      Noise n(809);
      Svf bp;
      return render(0.7, [&](double t, double total) {
        const double u = t / total;
        const double f = sweep(1900.0, 600.0, u);
        const double w = a.step(f, Wave::Sine) * 0.16;
        const double air = bp.band(n.next(), f * 1.2, 2.0) * 0.18;
        return (w + air) * 1.2 * attack(t, 0.04) * std::min(1.0, (total - t) / 0.06);
      });
    }
    case Sfx::ScrapClank:
    {
      // Scrap landing: a tinny clank, two odd metal partials, a rattle.
      Osc a, b, c;
      Noise n(811), m(821);
      Svf bp;
      return render(0.42, [&](double t, double total) {
        const double hit = bp.band(n.next(), 2100.0, 1.6) * decay(t, 0.012) * 0.7;
        const double ring = (a.step(940.0, Wave::Sine) + b.step(1530.0, Wave::Sine) * 0.7 + c.step(2470.0, Wave::Sine) * 0.4) * decay(t, 0.07) * 0.1;
        const double rattle = (std::fmod(t, 0.05) < 0.008 && t > 0.06 ? 1.0 : 0.0) * m.next() * decay(t, 0.12) * 0.35;
        return (hit + ring + rattle) * 0.72 * attack(t, 0.001) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::MechAim:
    {
      // A Scorpion Mech aiming: a capacitor's whine charging up, a servo
      // whirr as the stinger tracks, a tick as it locks.
      Osc a, b, s;
      Noise n(823);
      Svf bp;
      return render(1.3, [&](double t, double total) {
        const double u = t / total;
        const double f = sweep(300.0, 2200.0, u * u);
        const double whine = (a.step(f, Wave::Sine) * 0.6 + b.step(f * 1.5, Wave::Triangle) * 0.25) * (0.3 + 0.7 * u) * 0.2;
        const double servo = bp.band(float(s.step(sweep(140.0, 210.0, u), Wave::Saw)), 700.0, 2.0) * 0.12 * (1.0 - u);
        const double lock = t > total - 0.18 ? bp.band(n.next(), 3000.0, 3.0) * decay(t - (total - 0.18), 0.01) * 0.3 : 0.0;
        return (whine + servo + lock) * attack(t, 0.05) * std::min(1.0, (total - t) / 0.03);
      });
    }
    case Sfx::MechBeam:
    {
      // A Scorpion Mech's laser: a searing zap and the beam's hum, a
      // crackle where it burns.
      Osc a, b, c;
      Noise n(827), m(829);
      OnePole hp;
      return render(0.6, [&](double t, double total) {
        const double zap = a.step(sweep(3200.0, 900.0, std::min(1.0, t / 0.08)), Wave::Saw) * decay(t, 0.05) * 0.3;
        const double hum = (b.step(180.0, Wave::Saw) * 0.5 + c.step(362.0, Wave::Square) * 0.25) * 0.16 * (0.7 + 0.3 * std::sin(t * 2.0 * kPi * 30.0));
        const double burn = (m.next() > 0.7f ? 1.0 : 0.15) * hp.highpass(n.next(), 3500.0) * 0.16;
        return (zap + hum + burn) * 1.1 * attack(t, 0.002) * std::min(1.0, (total - t) / 0.15);
      });
    }
    case Sfx::MiniDishTurn:
    {
      // A minidish knocked round: a little servo whirr and a click home.
      Osc a;
      Noise n(839);
      Svf bp;
      return render(0.3, [&](double t, double total) {
        const double w = a.step(sweep(500.0, 900.0, std::min(1.0, t / 0.2)), Wave::Saw) * (t < 0.22 ? 1.0 : 0.0) * 0.1;
        const double u = t - 0.22;
        const double click = u > 0.0 ? bp.band(n.next(), 2600.0, 3.0) * decay(u, 0.01) * 0.5 : 0.0;
        return (w + click) * attack(t, 0.01) * std::min(1.0, (total - t) / 0.03);
      });
    }
    case Sfx::CactusHat:
    {
      // The cactus's hat popping up: a cork pop and a slide whistle going
      // up and wobbling.
      Osc a, b;
      Noise n(853);
      Svf bp;
      return render(0.5, [&](double t, double total) {
        const double pop = bp.band(n.next(), 1200.0, 2.0) * decay(t, 0.01) * 0.7 + b.step(sweep(500.0, 200.0, t / 0.05), Wave::Sine) * decay(t, 0.03) * 0.3;
        const double u = t - 0.05;
        const double f = sweep(500.0, 1500.0, std::min(1.0, u / 0.25)) * (1.0 + 0.03 * std::sin(u * 2.0 * kPi * 12.0));
        const double slide = u > 0.0 ? a.step(f, Wave::Sine) * 0.2 * std::min(1.0, u / 0.03) : 0.0;
        return (pop + slide) * 0.8 * std::min(1.0, (total - t) / 0.1);
      });
    }
    case Sfx::SwapWarn:
    {
      // Heatstroke: left and right about to swap: a wobbling two-tone
      // siren, the tones trading places.
      Osc a, b;
      return render(1.4, [&](double t, double total) {
        const bool hi = std::fmod(t, 0.35) < 0.175;
        const double wob = 1.0 + 0.02 * std::sin(t * 2.0 * kPi * 9.0);
        const double f1 = (hi ? 880.0 : 660.0) * wob, f2 = (hi ? 660.0 : 880.0) * wob * 0.5;
        const double v = a.step(f1, Wave::Triangle) * 0.6 + b.step(f2, Wave::Square, 0.3) * 0.15;
        return v * 0.45 * attack(t, 0.02) * std::min(1.0, (total - t) / 0.1);
      });
    }
    case Sfx::EngineDrone:
    {
      // Level 34: the fleet's engines: a low chugging drone, two propellers
      // beating slightly out of step through it, swelling in and out.
      Osc a, b, c;
      Noise n(3401);
      OnePole lp;
      return render(2.4, [&](double t, double total) {
        const double chug = 0.6 + 0.4 * std::sin(t * 2.0 * kPi * 7.0);
        const double tone = (a.step(55.0, Wave::Saw) * 0.5 + b.step(82.5, Wave::Triangle) * 0.4) * chug;
        const double beat = (0.5 + 0.5 * std::sin(t * 2.0 * kPi * 13.0)) * (0.5 + 0.5 * std::sin(t * 2.0 * kPi * 11.3));
        const double prop = lp.lowpass(n.next(), 400.0) * 2.2 * beat;
        const double hum = c.step(110.0, Wave::Sine) * 0.15;
        const double env = std::min(1.0, t / 0.6) * std::min(1.0, (total - t) / 0.8);
        return (tone * 0.5 + prop * 0.6 + hum) * 0.5 * env;
      });
    }
    case Sfx::GrappleFire:
    {
      // The Grapple Gun: a pneumatic thunk, then the line whizzing off its
      // drum, a ratchet ticking under it.
      Osc o, tick;
      Noise n(3403);
      Svf bp;
      std::vector<float> v = render(0.45, [&](double t, double) {
        const double thunk = o.step(sweep(180.0, 60.0, t / 0.06), Wave::Sine) * decay(t, 0.03) +
          bp.band(n.next(), sweep(3000.0, 800.0, t / 0.05), 1.2) * decay(t, 0.02) * 0.8;
        const double whiz = bp.band(n.next(), sweep(1400.0, 3800.0, t / 0.4), 4.0) * 0.8 * std::min(1.0, t / 0.05) * decay(t, 0.25);
        const double ratchet = std::fmod(t, 0.022) < 0.003 ? tick.step(2200.0, Wave::Square, 0.3) * 0.25 * decay(t, 0.2) : 0.0;
        return (thunk * 0.8 + whiz * 0.5 + ratchet) * 0.6 * attack(t, 0.001);
      });
      for (float& x : v)
        x *= 0.53f; // to a peak near 0.4
      return v;
    }
    case Sfx::GrappleLatch:
    {
      // The hook catching a ring: a brass clank, the ring's bright chime
      // ringing on.
      std::vector<float> b = render(0.7, [](double, double) { return 0.0; });
      Noise n(3405);
      Svf bp;
      for (std::size_t i = 0; i < std::size_t(samples(0.05)) && i < b.size(); ++i)
      {
        const double t = double(i) / kRate;
        b[i] += float(bp.band(n.next(), 2400.0, 3.0) * decay(t, 0.012) * 0.9);
      }
      addTone(b, 0.0, 0.7, 1318.5, Wave::Sine, 0.22, 0.25);
      addTone(b, 0.0, 0.7, 1975.5, Wave::Sine, 0.12, 0.18);
      addTone(b, 0.0, 0.3, 3135.0, Wave::Sine, 0.06, 0.08);
      addTone(b, 0.01, 0.12, 420.0, Wave::Square, 0.08, 0.03);
      for (float& x : b)
        x *= 0.4f; // to a peak near 0.4
      return b;
    }
    case Sfx::RopeCreak:
    {
      // A line taking the weight: a hemp creak, a short rasp rising.
      Osc o;
      Svf bp;
      std::vector<float> v = render(0.35, [&](double t, double total) {
        const double f = 140.0 + 60.0 * t / total + 20.0 * std::sin(t * 2.0 * kPi * 31.0);
        const double rasp = bp.band(float(o.step(f, Wave::Saw)), f * 3.0, 5.0);
        return rasp * 0.6 * std::min(1.0, t / 0.03) * std::min(1.0, (total - t) / 0.1);
      });
      for (float& x : v)
        x *= 0.3f; // to a peak near 0.4
      return v;
    }
    case Sfx::GrappleYank:
    {
      // A light enemy yanked in: the line cracking taut, the reel whirring.
      Osc o, reel;
      Noise n(3407);
      Svf bp;
      std::vector<float> v = render(0.5, [&](double t, double total) {
        const double crack = bp.band(n.next(), 2600.0, 1.5) * decay(t, 0.008) * 1.4 + o.step(sweep(500.0, 220.0, t / 0.04), Wave::Triangle) * decay(t, 0.03) * 0.5;
        const double whirr = reel.step(sweep(300.0, 700.0, t / total) * (1.0 + 0.1 * std::sin(t * 2.0 * kPi * 40.0)), Wave::Square, 0.3) *
          0.12 * std::min(1.0, t / 0.05) * std::min(1.0, (total - t) / 0.1);
        return (crack + whirr) * 0.6;
      });
      for (float& x : v)
        x *= 0.45f; // to a peak near 0.4
      return v;
    }
    case Sfx::PirateJet:
    {
      // A Sky Pirate jetting in: a jet pack's roar, sputtering, its pitch
      // dropping as he lands.
      Noise n(3409);
      Svf bp;
      OnePole lp;
      Osc o;
      std::vector<float> v = render(1.1, [&](double t, double total) {
        const double u = t / total;
        const double roar = bp.band(n.next(), sweep(1600.0, 500.0, u), 0.7) * 0.7 + lp.lowpass(n.next(), 300.0) * 1.2;
        const double sputter = 0.65 + 0.35 * (std::sin(t * 2.0 * kPi * 23.0) > -0.3 ? 1.0 : 0.0);
        const double whine = o.step(sweep(900.0, 400.0, u), Wave::Triangle) * 0.12;
        return (roar * sputter + whine) * 0.55 * std::min(1.0, t / 0.08) * std::min(1.0, (total - t) / 0.3);
      });
      for (float& x : v)
        x *= 1.25f; // to a peak near 0.4
      return v;
    }
    case Sfx::CutlassDraw:
    {
      // The cutlass raised: a steel shing, a ring after it.
      Noise n(3411);
      Svf bp;
      std::vector<float> b = render(0.5, [&](double t, double) {
        return bp.band(n.next(), sweep(3000.0, 7000.0, t / 0.2), 6.0) * 0.9 * std::min(1.0, t / 0.03) * decay(t, 0.09);
      });
      addTone(b, 0.12, 0.38, 2793.0, Wave::Sine, 0.1, 0.12);
      addTone(b, 0.12, 0.38, 4186.0, Wave::Sine, 0.05, 0.08);
      for (float& x : b)
        x *= 0.37f; // to a peak near 0.4
      return b;
    }
    case Sfx::CutlassSlash:
    {
      // The cutlass swung: a heavy swish, a short ring of steel.
      Noise n(3413);
      Svf bp;
      std::vector<float> b = render(0.4, [&](double t, double total) {
        const double f = sweep(500.0, 2400.0, t / 0.12);
        return bp.band(n.next(), f, 1.8) * 1.2 * std::sin(std::min(1.0, t / 0.18) * kPi) * std::min(1.0, (total - t) / 0.05);
      });
      addTone(b, 0.14, 0.26, 2349.0, Wave::Sine, 0.08, 0.06);
      for (float& x : b)
        x *= 0.35f; // to a peak near 0.4
      return b;
    }
    case Sfx::FlintCock:
    {
      // The flintlock's hammer coming back: two hard clicks.
      std::vector<float> b(std::size_t(samples(0.25)));
      Noise n(3415);
      Svf bp;
      for (const double at : {0.0, 0.09})
        for (int i = 0; i < samples(0.03); ++i)
        {
          const std::size_t k = std::size_t(samples(at) + i);
          if (k < b.size())
            b[k] += float(bp.band(n.next(), at > 0.0 ? 3200.0 : 2200.0, 4.0) * decay(double(i) / kRate, 0.004) * 1.6);
        }
      addTone(b, 0.09, 0.05, 1800.0, Wave::Square, 0.05, 0.01);
      for (float& x : b)
        x *= 0.18f; // to a peak near 0.4
      return b;
    }
    case Sfx::FlintShot:
    {
      // The flintlock fired: the pan's fizz, a crack and a puff of smoke.
      Noise n(3417);
      OnePole hp, lp;
      Osc o;
      std::vector<float> v = render(0.6, [&](double t, double) {
        const double pan = t < 0.04 ? hp.highpass(n.next(), 5000.0) * 0.4 : 0.0;
        const double u = t - 0.04;
        if (u < 0.0)
          return pan;
        const double crack = lp.lowpass(n.next(), sweep(8000.0, 600.0, u / 0.2)) * decay(u, 0.05) * 1.6;
        const double boom = o.step(sweep(160.0, 50.0, u / 0.15), Wave::Sine) * decay(u, 0.08) * 0.8;
        return (pan + crack + boom) * 0.6;
      });
      for (float& x : v)
        x *= 0.36f; // to a peak near 0.4
      return v;
    }
    case Sfx::ParrotSquawk:
    {
      // A Peck Parrot shooed off: two harsh rising squawks, buzzy and nasal.
      Osc a, b;
      Svf bp;
      std::vector<float> v = render(0.55, [&](double t, double) {
        const double ph = std::fmod(t, 0.26);
        if (ph > 0.2 || t > 0.5)
          return 0.0;
        const double f = sweep(700.0, 1400.0, ph / 0.12) * (1.0 + 0.04 * std::sin(t * 2.0 * kPi * 60.0));
        const double v = a.step(f, Wave::Saw) * 0.6 + b.step(f * 1.5, Wave::Square, 0.3) * 0.3;
        return bp.band(float(v), 2200.0, 1.5) * 0.9 * attack(ph, 0.01) * std::min(1.0, (0.2 - ph) / 0.04);
      });
      for (float& x : v)
        x *= 0.42f; // to a peak near 0.4
      return v;
    }
    case Sfx::ParrotDive:
    {
      // A Peck Parrot diving: a falling whistle of air, a shriek at the end.
      Osc o, s;
      Noise n(3419);
      Svf bp;
      std::vector<float> v = render(0.7, [&](double t, double total) {
        const double air = bp.band(n.next(), sweep(2600.0, 900.0, t / total), 3.0) * 0.6;
        const double whistle = o.step(sweep(1800.0, 900.0, t / 0.5), Wave::Sine) * 0.2;
        const double u = t - 0.45;
        const double shriek = u > 0.0 ? s.step(sweep(1500.0, 2200.0, u / 0.1), Wave::Saw) * 0.12 * decay(u, 0.08) : 0.0;
        return (air + whistle + shriek) * 0.75 * std::min(1.0, t / 0.05) * std::min(1.0, (total - t) / 0.05);
      });
      for (float& x : v)
        x *= 0.52f; // to a peak near 0.4
      return v;
    }
    case Sfx::ParrotHop:
    {
      // The carrier hopping: a flap of wings, a chirp.
      Noise n(3421);
      OnePole lp;
      Osc o;
      std::vector<float> v = render(0.25, [&](double t, double) {
        const double flap = lp.lowpass(n.next(), 1200.0) * (decay(t, 0.02) + (t > 0.06 ? decay(t - 0.06, 0.02) : 0.0)) * 1.8;
        const double u = t - 0.1;
        const double chirp = u > 0.0 && u < 0.08 ? o.step(sweep(2000.0, 3000.0, u / 0.08), Wave::Sine) * 0.3 * std::sin(u / 0.08 * kPi) : 0.0;
        return (flap + chirp) * 0.7;
      });
      for (float& x : v)
        x *= 0.62f; // to a peak near 0.4
      return v;
    }
    case Sfx::BalloonHiss:
    {
      // A parrot pecking its balloon down: two quick taps on taut canvas,
      // air hissing out under them.
      Noise n(3423), m(3427);
      OnePole hp;
      Svf bp;
      std::vector<float> v = render(0.6, [&](double t, double total) {
        double tap = 0.0;
        for (const double at : {0.0, 0.11})
          if (t >= at)
            tap += bp.band(n.next(), 900.0, 3.0) * decay(t - at, 0.01) * 1.2;
        const double hiss = hp.highpass(m.next(), 4000.0) * 0.35 * std::min(1.0, t / 0.1) * std::min(1.0, (total - t) / 0.3);
        return (tap + hiss) * 0.6;
      });
      for (float& x : v)
        x *= 0.8f; // to a peak near 0.4
      return v;
    }
    case Sfx::MineFuse:
    {
      // A Balloon Mine lit: quick ticking beeps over a fizzing fuse.
      Osc o;
      Noise n(3429);
      OnePole hp;
      return render(0.4, [&](double t, double total) {
        const double beep = std::fmod(t, 0.067) < 0.03 ? o.step(1760.0, Wave::Square, 0.5) * 0.25 : 0.0;
        const double fizz = hp.highpass(n.next(), 3000.0) * 0.3;
        return (beep + fizz) * 0.7 * std::min(1.0, (total - t) / 0.03);
      });
    }
    case Sfx::MinePop:
    {
      // A Balloon Mine bursting: the balloon's sharp bang, then shrapnel
      // rattling and pinging away.
      Noise n(3431);
      OnePole lp;
      Svf bp;
      std::vector<float> b = render(0.8, [&](double t, double) {
        const double bang = lp.lowpass(n.next(), sweep(9000.0, 400.0, t / 0.1)) * decay(t, 0.03) * 1.8;
        const double rattle = (std::fmod(t * 37.0 + std::sin(t * 9.0), 1.0) < 0.15 ? 1.0 : 0.0) * bp.band(n.next(), 3500.0, 4.0) *
          decay(t, 0.25) * 0.7;
        return (bang + rattle) * 0.6;
      });
      for (int k = 0; k < 4; ++k)
        addTone(b, 0.08 + 0.07 * k, 0.2, 2600.0 + 600.0 * double(k % 3), Wave::Sine, 0.06, 0.04);
      for (float& x : b)
        x *= 0.27f; // to a peak near 0.4
      return b;
    }
    case Sfx::CannonBoom:
    {
      // Human Cannonball: a circus cannon's deep boom, a puff, and a slide
      // whistle going up with the flight.
      Noise n(3433);
      OnePole lp;
      Osc o, w;
      std::vector<float> v = render(1.3, [&](double t, double) {
        const double boom = o.step(sweep(110.0, 32.0, t / 0.5), Wave::Sine) * decay(t, 0.3) * 1.2 +
          lp.lowpass(n.next(), sweep(3000.0, 200.0, t / 0.3)) * decay(t, 0.12) * 1.5;
        const double u = t - 0.15;
        const double slide = u > 0.0 && u < 0.9 ? w.step(sweep(500.0, 1500.0, u / 0.9), Wave::Sine) * 0.18 * std::min(1.0, u / 0.05) *
          std::min(1.0, (0.9 - u) / 0.15) : 0.0;
        return (boom + slide) * 0.55 * attack(t, 0.001);
      });
      for (float& x : v)
        x *= 0.41f; // to a peak near 0.4
      return v;
    }
    case Sfx::NetBoing:
    {
      // Landing in a net: a springy boing, wobbling down.
      Osc o;
      std::vector<float> v = render(0.6, [&](double t, double) {
        const double f = 180.0 * (1.0 + 0.35 * std::sin(t * 2.0 * kPi * 9.0) * decay(t, 0.2)) * sweep(1.6, 1.0, t / 0.08);
        return o.step(f, Wave::Triangle) * 0.6 * decay(t, 0.22) * attack(t, 0.004);
      });
      for (float& x : v)
        x *= 0.66f; // to a peak near 0.4
      return v;
    }
    case Sfx::LooterSink:
    {
      // The Looter going down: a groaning hull, its engine coughing out.
      Osc g, e;
      Svf bp;
      Noise n(3435);
      std::vector<float> v = render(2.0, [&](double t, double total) {
        const double f = 70.0 + 30.0 * std::sin(t * 2.3) - 15.0 * t;
        const double groan = bp.band(float(g.step(f, Wave::Saw)), f * 3.0, 4.0) * 0.6;
        const double cough = std::fmod(t, 0.31) < 0.09 && t < 1.3 ? (e.step(45.0, Wave::Square, 0.4) * 0.3 + n.next() * 0.2) *
          (1.0 - t / 1.3) : 0.0;
        return (groan + cough) * 0.7 * std::min(1.0, t / 0.2) * std::min(1.0, (total - t) / 0.4);
      });
      for (float& x : v)
        x *= 0.42f; // to a peak near 0.4
      return v;
    }
    case Sfx::BayHatch:
    {
      // The bomb bay's hatch opening: a wooden creak, then a heavy clunk.
      Osc o, k;
      Svf bp;
      Noise n(3437);
      std::vector<float> v = render(0.8, [&](double t, double) {
        const double creak = t < 0.45 ? bp.band(float(o.step(120.0 + 160.0 * t, Wave::Saw)), 500.0 + 600.0 * t, 5.0) * 0.6 *
          std::sin(t / 0.45 * kPi) : 0.0;
        const double u = t - 0.45;
        const double clunk = u > 0.0 ? (k.step(sweep(140.0, 70.0, u / 0.1), Wave::Sine) * 0.9 + bp.band(n.next(), 900.0, 2.0) * 0.6) *
          decay(u, 0.06) : 0.0;
        return (creak + clunk) * 0.7;
      });
      for (float& x : v)
        x *= 0.5f; // to a peak near 0.4
      return v;
    }
    case Sfx::DeckDip:
    {
      // A small deck dipping under Rocco: a wooden thump, the rigging creaking.
      Osc o, c;
      Svf bp;
      Noise n(3439);
      std::vector<float> v = render(0.7, [&](double t, double) {
        const double thump = o.step(sweep(120.0, 50.0, t / 0.1), Wave::Sine) * decay(t, 0.08) * 1.2 + bp.band(n.next(), 700.0, 1.5) * decay(t, 0.03);
        const double u = t - 0.12;
        const double creak = u > 0.0 ? bp.band(float(c.step(160.0 + 40.0 * std::sin(u * 20.0), Wave::Saw)), 700.0, 5.0) * 0.4 * decay(u, 0.2) : 0.0;
        return (thump + creak) * 0.6;
      });
      for (float& x : v)
        x *= 0.47f; // to a peak near 0.4
      return v;
    }
    case Sfx::CurtainSwish:
    {
      // Level 37: a velvet curtain drawn along its brass rail: a heavy,
      // soft swish of cloth and the rings clattering along the pole.
      Noise n(37001), r(37003);
      Svf bp, ring;
      OnePole lp;
      auto v = render(0.45, [&](double t, double total) {
        const double u = t / total;
        const double f = u < 0.4 ? sweep(500.0, 2200.0, u / 0.4) : sweep(2200.0, 900.0, (u - 0.4) / 0.6);
        const double cloth = (bp.band(n.next(), f, 1.2) * 0.8 + lp.lowpass(n.next(), 500.0) * 0.6) * std::pow(std::sin(kPi * u), 1.2);
        // The rings: a burst of little metal clicks while it runs.
        const double click = r.next() > (u < 0.6 ? 0.93f : 0.995f) ? 1.0 : 0.0;
        const double rings = ring.band(float(click), 3600.0, 9.0) * (u < 0.7 ? 1.0 : 0.3);
        return (cloth + rings * 0.5) * 0.62 * attack(t, 0.01);
      });
      for (float& x : v)
        x *= 0.77f; // to a peak near 0.4
      return v;
    }
    case Sfx::BoothSwap:
    {
      // Out of a changing booth as someone else: the curtain whipped open
      // and a two-note brass "ta-da" over a brush on a cymbal.
      Noise n(37011), c(37013);
      Svf bp;
      OnePole lp1, lp2, hp;
      Osc a1, a2, b1, b2;
      auto v = render(0.9, [&](double t, double total) {
        const double whip = t < 0.14 ? bp.band(n.next(), sweep(900.0, 3200.0, t / 0.14), 1.5) * std::sin(kPi * t / 0.14) : 0.0;
        const double u1 = t - 0.08, u2 = t - 0.26;
        const double ta = u1 > 0.0 && u1 < 0.2
          ? lp1.lowpass(float(a1.step(midiFreq(67), Wave::Saw) + a2.step(midiFreq(67) * 1.005, Wave::Saw)), 2200.0) * attack(u1, 0.015) *
              std::min(1.0, (0.2 - u1) / 0.04)
          : 0.0;
        const double da = u2 > 0.0
          ? lp2.lowpass(float(b1.step(midiFreq(72), Wave::Saw) + b2.step(midiFreq(76), Wave::Saw) * 0.7), 2600.0) * attack(u2, 0.015) *
              decay(u2, 0.35)
          : 0.0;
        const double cym = u2 > 0.0 ? hp.highpass(c.next(), 6000.0) * decay(u2, 0.25) : 0.0;
        return (whip * 0.8 + ta * 0.32 + da * 0.32 + cym * 0.5) * 0.7 * std::min(1.0, (total - t) / 0.05);
      });
      for (float& x : v)
        x *= 0.62f; // to a peak near 0.4
      return v;
    }
    case Sfx::PlateClunk:
    {
      // A heavy plate going down under Rocco: a deep thump, a brass clank
      // and the latch catching.
      Osc thump, clank, ring;
      Noise n(37021);
      OnePole lp;
      Svf bp;
      auto v = render(0.6, [&](double t, double total) {
        const double th = (thump.step(sweep(110.0, 45.0, t / 0.15), Wave::Sine) + lp.lowpass(n.next(), 400.0) * 1.5) * decay(t, 0.09);
        const double cl = (clank.step(310.0, Wave::Square, 0.3) * 0.4 + ring.step(640.0, Wave::Sine) * 0.5) * decay(t, 0.12);
        const double u = t - 0.16;
        const double latch = u > 0.0 ? bp.band(n.next(), 2800.0, 4.0) * decay(u, 0.015) * 2.0 : 0.0;
        return (th + cl + latch) * 0.42 * attack(t) * std::min(1.0, (total - t) / 0.05);
      });
      for (float& x : v)
        x *= 0.31f; // to a peak near 0.4
      return v;
    }
    case Sfx::ShutterRoll:
    {
      // A rolling shutter going up: its steel slats clacking round the
      // drum, fast then slowing, over a little motor's whine.
      Noise n(37031);
      Svf slat;
      Osc motor;
      OnePole lp;
      double phase = 0.0;
      auto v = render(1.1, [&](double t, double total) {
        const double u = t / total;
        const double rate = 26.0 * std::sin(kPi * std::min(1.0, u * 1.1)) + 4.0;
        phase += rate / kRate;
        double hit = 0.0;
        if (phase >= 1.0)
        {
          phase -= 1.0;
          hit = 1.0;
        }
        const double clack = slat.band(float(hit * 6.0 + n.next() * 0.05), 1900.0, 6.0);
        const double whine = lp.lowpass(motor.step(110.0 + 50.0 * u, Wave::Saw), 700.0) * 0.25 * std::sin(kPi * u);
        return (clack * 0.8 + whine) * 0.85 * attack(t, 0.02) * std::min(1.0, (total - t) / 0.1);
      });
      for (float& x : v)
        x *= 0.34f; // to a peak near 0.4
      return v;
    }
    case Sfx::TableLower:
    {
      // A cutting table let down on its chains: links clinking through the
      // pulleys, a winch's pawl ticking, a low motor.
      Noise n(37041), m(37043);
      Svf chain, pawl;
      Osc motor;
      OnePole lp;
      double phase = 0.0;
      auto v = render(1.4, [&](double t, double total) {
        const double clink = chain.band(m.next() > 0.985f ? 1.0f : 0.0f, 3200.0, 8.0) * 3.0;
        phase += 14.0 / kRate;
        double tick = 0.0;
        if (phase >= 1.0)
        {
          phase -= 1.0;
          tick = 1.0;
        }
        const double pw = pawl.band(float(tick * 5.0 + n.next() * 0.02), 1300.0, 5.0);
        const double hum = lp.lowpass(motor.step(70.0, Wave::Saw), 300.0) * 0.4;
        return (clink + pw + hum) * 0.5 * std::min(1.0, t / 0.05) * std::min(1.0, (total - t) / 0.3);
      });
      for (float& x : v)
        x *= 0.29f; // to a peak near 0.4
      return v;
    }
    case Sfx::TableLand:
    {
      // The table coming down flush: a big wooden thump that shakes the
      // floor, the scissors and pins on it jumping.
      Osc body;
      Noise n(37051), m(37053);
      OnePole lp;
      Svf wood, tin;
      auto v = render(0.7, [&](double t, double total) {
        const double th = (body.step(sweep(90.0, 40.0, t / 0.2), Wave::Sine) * 1.2 + lp.lowpass(n.next(), 300.0) * 2.0) * decay(t, 0.12);
        const double knock = wood.band(n.next(), 520.0, 3.0) * decay(t, 0.04) * 1.5;
        const double u = t - 0.05;
        const double jump = u > 0.0 ? tin.band(m.next() > 0.996f ? 1.0f : 0.0f, 4200.0, 10.0) * 4.0 * decay(u, 0.2) : 0.0;
        return (th + knock + jump) * 0.38 * attack(t) * std::min(1.0, (total - t) / 0.05);
      });
      for (float& x : v)
        x *= 0.45f; // to a peak near 0.4
      return v;
    }
    case Sfx::SpringBoing:
    {
      // A spring rack throwing the runner up: a big coil's boing, its pitch
      // wobbling up, a twang of steel in it.
      Osc a, b, c;
      auto v = render(0.7, [&](double t, double total) {
        const double u = t / total;
        const double f = sweep(150.0, 420.0, std::min(1.0, u * 1.6)) * (1.0 + 0.12 * std::sin(t * 2.0 * kPi * 22.0) * decay(t, 0.25));
        const double boing = a.step(f, Wave::Sine) + b.step(f * 2.01, Wave::Triangle) * 0.3;
        const double twang = c.step(f * 3.98, Wave::Saw) * 0.15 * decay(t, 0.06);
        return (boing + twang) * 0.42 * decay(t, 0.28) * attack(t, 0.004);
      });
      for (float& x : v)
        x *= 0.69f; // to a peak near 0.4
      return v;
    }
    case Sfx::SpikeTumble:
    {
      // Into the ceiling's needles: a sharp tink of steel, then a wobbling
      // whoosh tumbling down.
      Osc tink, tink2;
      Noise n(37061);
      Svf bp;
      auto v = render(0.8, [&](double t, double total) {
        const double tk = (tink.step(2600.0, Wave::Sine) + tink2.step(3900.0, Wave::Sine) * 0.5) * decay(t, 0.05);
        const double u = t / total;
        const double f = sweep(1800.0, 300.0, u) * (1.0 + 0.3 * std::sin(t * 2.0 * kPi * 9.0));
        const double fall = bp.band(n.next(), f, 2.0) * std::sin(kPi * std::min(1.0, u * 1.05)) * 0.9;
        return (tk * 0.6 + fall) * 0.6 * attack(t) * std::min(1.0, (total - t) / 0.05);
      });
      for (float& x : v)
        x *= 0.83f; // to a peak near 0.4
      return v;
    }
    case Sfx::PushRackScrape:
    {
      // The archive's rack shoved a block: heavy casters grinding, one
      // squeaking, the wood creaking.
      Noise n(37071);
      OnePole lp;
      Svf bp;
      Osc squeak;
      auto v = render(0.5, [&](double t, double total) {
        const double u = t / total;
        const double grind = (lp.lowpass(n.next(), 260.0) * 2.5 + bp.band(n.next(), 700.0, 2.0) * 0.4) * std::sin(kPi * u);
        const double sq = squeak.step(950.0 + 120.0 * std::sin(t * 2.0 * kPi * 7.0), Wave::Triangle) * 0.25 * std::pow(std::sin(kPi * u), 2.0);
        return (grind + sq) * 0.62 * attack(t, 0.01);
      });
      for (float& x : v)
        x *= 0.76f; // to a peak near 0.4
      return v;
    }
    case Sfx::VaultDoor:
    {
      // Through the vault door: its wheel spun (a ratchet), the heavy steel
      // swinging on its hinges with a groan, a deep boom as it shuts.
      Noise n(37081);
      Svf rat, groan;
      Osc g, boom;
      OnePole lp;
      double phase = 0.0;
      auto v = render(1.2, [&](double t, double total) {
        double tick = 0.0;
        if (t < 0.3)
        {
          phase += 30.0 / kRate;
          if (phase >= 1.0)
          {
            phase -= 1.0;
            tick = 1.0;
          }
        }
        const double r = rat.band(float(tick * 5.0), 2400.0, 6.0);
        const double gr = t > 0.25 && t < 0.85
          ? groan.band(float(g.step(65.0 + 20.0 * std::sin(t * 9.0), Wave::Saw)), 380.0, 3.0) * std::sin(kPi * (t - 0.25) / 0.6) * 0.8
          : 0.0;
        const double u = t - 0.82;
        const double bm = u > 0.0 ? (boom.step(sweep(80.0, 38.0, u / 0.3), Wave::Sine) * 1.4 + lp.lowpass(n.next(), 500.0) * 1.5) * decay(u, 0.12) : 0.0;
        return (r + gr + bm) * 0.5 * attack(t) * std::min(1.0, (total - t) / 0.05);
      });
      for (float& x : v)
        x *= 0.46f; // to a peak near 0.4
      return v;
    }
    case Sfx::DoorBlocked:
    {
      // The vault door won't open: two dull knocks on the rack in front.
      Osc a;
      Noise n(37091);
      Svf bp;
      auto v = render(0.4, [&](double t, double total) {
        double out = 0.0;
        for (const double s : {0.0, 0.16})
        {
          const double u = t - s;
          if (u > 0.0)
            out += (a.step(sweep(150.0, 90.0, u / 0.06), Wave::Sine) + bp.band(n.next(), 420.0, 2.5)) * decay(u, 0.035);
        }
        return out * 0.5 * attack(t) * std::min(1.0, (total - t) / 0.03);
      });
      for (float& x : v)
        x *= 0.70f; // to a peak near 0.4
      return v;
    }
    case Sfx::DecoyLaunch:
    {
      // The Decoy Launcher: a pneumatic thoonk, a puff of air after the
      // mannequin and a spring's twang.
      Osc pop, tw;
      Noise n(37101);
      Svf bp;
      auto v = render(0.55, [&](double t, double total) {
        const double p = pop.step(sweep(240.0, 70.0, t / 0.08), Wave::Sine) * decay(t, 0.05) * 1.4;
        const double air = bp.band(n.next(), sweep(600.0, 2600.0, t / total), 1.5) * std::sin(kPi * t / total) * 0.7;
        const double twang = tw.step(330.0 * (1.0 + 0.05 * std::sin(t * 2.0 * kPi * 30.0)), Wave::Triangle) * decay(t, 0.12) * 0.3;
        return (p + air + twang) * 0.5 * attack(t) * std::min(1.0, (total - t) / 0.05);
      });
      for (float& x : v)
        x *= 0.52f; // to a peak near 0.4
      return v;
    }
    case Sfx::DecoyLand:
    {
      // The decoy landing upright: a clatter of wooden limbs, a knock.
      Noise n(37111);
      Svf a, b, c;
      auto v = render(0.45, [&](double t, double total) {
        double out = 0.0;
        const double hits[3][2] = {{0.0, 900.0}, {0.045, 1350.0}, {0.1, 700.0}};
        Svf* f[3] = {&a, &b, &c};
        for (int k = 0; k < 3; ++k)
        {
          const double u = t - hits[k][0];
          out += f[k]->band(n.next(), hits[k][1], 5.0) * (u > 0.0 ? decay(u, 0.03) : 0.0) * 2.0;
        }
        return out * 0.45 * attack(t) * std::min(1.0, (total - t) / 0.03);
      });
      for (float& x : v)
        x *= 0.53f; // to a peak near 0.4
      return v;
    }
    case Sfx::DecoyBonk:
    {
      // Something walloping the decoy: a hollow wooden bonk.
      Osc a, b;
      Noise n(37121);
      Svf bp;
      auto v = render(0.3, [&](double t, double total) {
        const double f = sweep(560.0, 380.0, t / 0.08);
        const double tone = (a.step(f, Wave::Sine) + b.step(f * 2.7, Wave::Sine) * 0.3) * decay(t, 0.06);
        const double knock = bp.band(n.next(), 1200.0, 3.0) * decay(t, 0.01) * 1.5;
        return (tone + knock) * 0.5 * attack(t, 0.001) * std::min(1.0, (total - t) / 0.03);
      });
      for (float& x : v)
        x *= 0.51f; // to a peak near 0.4
      return v;
    }
    case Sfx::DecoyPop:
    {
      // The decoy giving up: a pop, a poof of sawdust and its wooden parts
      // clattering to the floor.
      Noise n(37131), m(37133);
      OnePole hp, lp;
      Svf wood;
      auto v = render(0.8, [&](double t, double total) {
        const double pop = hp.highpass(n.next(), 1200.0) * decay(t, 0.02) * 1.6;
        const double poof = lp.lowpass(n.next(), 900.0) * decay(t, 0.15) * 1.2;
        const double u = t - 0.12;
        const double clat = u > 0.0 ? wood.band(m.next() > 0.996f ? 1.0f : 0.0f, 950.0, 6.0) * 5.0 * decay(u, 0.25) : 0.0;
        return (pop + poof + clat) * 0.42 * attack(t, 0.001) * std::min(1.0, (total - t) / 0.05);
      });
      for (float& x : v)
        x *= 0.51f; // to a peak near 0.4
      return v;
    }
    case Sfx::MannequinCreak:
    {
      // A Mannequin about to move: its wooden joints creaking, stick-slip
      // through two hollow resonances, a sharp crick at the end.
      Svf body, knot, crick;
      Noise n(37141);
      double phase = 0.0;
      auto v = render(0.5, [&](double t, double total) {
        const double u = t / total;
        const double rate = 55.0 - 30.0 * u + 12.0 * std::sin(t * 23.0);
        phase += rate / kRate;
        double slip = 0.0;
        if (phase >= 1.0)
        {
          phase -= 1.0;
          slip = 1.0;
        }
        const double ex = slip * 4.0 + n.next() * 0.02;
        const double c = body.band(float(ex), 640.0, 8.0) + knot.band(float(ex), 1380.0, 10.0) * 0.6;
        const double v = t > 0.4 ? crick.band(n.next(), 2600.0, 4.0) * decay(t - 0.4, 0.012) * 3.0 : 0.0;
        return (c * std::sin(kPi * std::min(1.0, u * 1.2)) + v) * 0.5 * attack(t, 0.01);
      });
      for (float& x : v)
        x *= 0.25f; // to a peak near 0.4
      return v;
    }
    case Sfx::PufferSqueeze:
    {
      // A Powder Puffer squeezing its bulb: a rubbery squeak and a hiss of
      // air drawn in.
      Osc sq;
      Noise n(37151);
      OnePole lp;
      Svf bp;
      auto v = render(0.4, [&](double t, double total) {
        const double u = t / total;
        const double f = sweep(520.0, 880.0, u) * (1.0 + 0.04 * std::sin(t * 2.0 * kPi * 18.0));
        const double squeak = lp.lowpass(sq.step(f, Wave::Square, 0.3), 2200.0) * std::sin(kPi * u) * 0.5;
        const double hiss = bp.band(n.next(), 3000.0, 1.0) * std::sin(kPi * u) * 0.3;
        return (squeak + hiss) * 0.62 * attack(t, 0.01);
      });
      for (float& x : v)
        x *= 0.87f; // to a peak near 0.4
      return v;
    }
    case Sfx::PufferPuff:
    {
      // The puff: a soft powdery pfff and a low whump of air.
      Noise n(37161);
      Svf bp;
      OnePole lp;
      Osc whump;
      auto v = render(0.7, [&](double t, double total) {
        const double pf = (bp.band(n.next(), sweep(2200.0, 900.0, t / total), 0.9) + lp.lowpass(n.next(), 1500.0) * 0.6) *
                          attack(t, 0.01) * decay(t, 0.18);
        const double wh = whump.step(sweep(120.0, 60.0, t / 0.2), Wave::Sine) * decay(t, 0.08) * 0.8;
        return (pf + wh) * 0.6 * std::min(1.0, (total - t) / 0.05);
      });
      for (float& x : v)
        x *= 0.56f; // to a peak near 0.4
      return v;
    }
    case Sfx::RackRattle:
    {
      // A Rack Runner's tell: every hanger on it rattling on the rail.
      Noise n(37171), m(37173);
      Svf a, b;
      auto v = render(0.6, [&](double t, double total) {
        const double u = t / total;
        const float hit = m.next() > 0.975f ? 1.0f : 0.0f;
        const double jangle = a.band(hit, 3800.0, 12.0) * 2.0 + b.band(hit * 0.7f + n.next() * 0.01f, 5200.0, 14.0) * 1.5;
        return jangle * std::sin(kPi * u) * 0.75 * attack(t, 0.01);
      });
      for (float& x : v)
        x *= 0.20f; // to a peak near 0.4
      return v;
    }
    case Sfx::RackRoll:
    {
      // A Rack Runner setting off down its aisle: casters rumbling faster
      // and faster, the hangers swinging.
      Noise n(37181), m(37183);
      OnePole lp;
      Svf caster, hang;
      double phase = 0.0;
      auto v = render(0.9, [&](double t, double total) {
        const double u = t / total;
        phase += (8.0 + 30.0 * u) / kRate;
        double tick = 0.0;
        if (phase >= 1.0)
        {
          phase -= 1.0;
          tick = 1.0;
        }
        const double rum = lp.lowpass(n.next(), 180.0 + 200.0 * u) * 2.2;
        const double cl = caster.band(float(tick * 3.0), 900.0, 6.0);
        const double hg = hang.band(m.next() > 0.99f ? 1.0f : 0.0f, 4200.0, 12.0) * 1.5;
        return (rum + cl + hg) * 0.48 * std::min(1.0, t / 0.08) * std::min(1.0, (total - t) / 0.25);
      });
      for (float& x : v)
        x *= 0.48f; // to a peak near 0.4
      return v;
    }
    case Sfx::RackRam:
    {
      // Rammed by a Rack Runner: a crash of chrome, a clank and the
      // hangers jangling.
      Noise n(37191), m(37193);
      Svf crash, jang;
      Osc clank;
      auto v = render(0.7, [&](double t, double total) {
        const double cr = crash.band(n.next(), 2100.0, 1.2) * decay(t, 0.09) * 1.3;
        const double ck = clank.step(sweep(420.0, 260.0, t / 0.1), Wave::Square, 0.35) * decay(t, 0.06) * 0.5;
        const double jg = jang.band(m.next() > 0.985f ? 1.0f : 0.0f, 4600.0, 12.0) * 3.0 * decay(t, 0.25);
        return (cr + ck + jg) * 0.45 * attack(t, 0.001) * std::min(1.0, (total - t) / 0.05);
      });
      for (float& x : v)
        x *= 0.23f; // to a peak near 0.4
      return v;
    }
    case Sfx::MirrorSit:
    {
      // Sitting down at the bulb mirror: the stool's creak, then the
      // powder puff's three soft pats.
      Svf creak;
      Noise n(37201);
      OnePole lp;
      double phase = 0.0;
      auto v = render(1.0, [&](double t, double total) {
        double out = 0.0;
        if (t < 0.25)
        {
          phase += (90.0 - 200.0 * t) / kRate;
          double slip = 0.0;
          if (phase >= 1.0)
          {
            phase -= 1.0;
            slip = 1.0;
          }
          out += creak.band(float(slip * 4.0), 820.0, 8.0) * std::sin(kPi * t / 0.25);
        }
        for (const double s : {0.4, 0.6, 0.8})
        {
          const double u = t - s;
          if (u > 0.0)
            out += lp.lowpass(n.next(), 1100.0) * decay(u, 0.035) * 2.0;
        }
        return out * 0.55 * attack(t, 0.01) * std::min(1.0, (total - t) / 0.05);
      });
      for (float& x : v)
        x *= 0.80f; // to a peak near 0.4
      return v;
    }
    case Sfx::Mustache:
    {
      // The mustache on: a twirling slide up and a sparkling ding.
      Osc w, d1, d2;
      auto v = render(0.9, [&](double t, double total) {
        const double sl = t < 0.25 ? w.step(sweep(500.0, 1400.0, t / 0.25) * (1.0 + 0.03 * std::sin(t * 2.0 * kPi * 25.0)), Wave::Sine) *
                                       std::sin(kPi * t / 0.25) * 0.5
                                 : 0.0;
        const double u = t - 0.22;
        const double ding = u > 0.0 ? (d1.step(1760.0, Wave::Sine) + d2.step(2640.0, Wave::Sine) * 0.5) * decay(u, 0.22) : 0.0;
        return (sl + ding * 0.6) * 0.62 * attack(t, 0.005) * std::min(1.0, (total - t) / 0.05);
      });
      for (float& x : v)
        x *= 0.65f; // to a peak near 0.4
      return v;
    }
    case Sfx::SuitTouch:
    {
      // The Gorgazilla suit grabbed off its rack: a rubbery squeak and its
      // zip run up.
      Osc sq;
      Noise n(37211);
      Svf zip;
      double phase = 0.0;
      auto v = render(0.6, [&](double t, double total) {
        const double s = t < 0.2 ? sq.step(760.0 + 200.0 * std::sin(t * 30.0), Wave::Triangle) * std::sin(kPi * t / 0.2) * 0.6 : 0.0;
        double tooth = 0.0;
        if (t > 0.22 && t < 0.52)
        {
          phase += sweep(60.0, 220.0, (t - 0.22) / 0.3) / kRate;
          if (phase >= 1.0)
          {
            phase -= 1.0;
            tooth = 1.0;
          }
        }
        const double z = zip.band(float(tooth * 4.0 + n.next() * 0.03), 3400.0, 3.0);
        return (s + z) * 0.6 * attack(t, 0.005) * std::min(1.0, (total - t) / 0.05);
      });
      for (float& x : v)
        x *= 0.37f; // to a peak near 0.4
      return v;
    }
    case Sfx::SuitRoar:
    {
      // Gorgazilla steps out of the booth: a hammy, rubbery monster roar,
      // muffled by the suit.
      Osc a, b;
      Noise n(37221);
      Svf f1, f2;
      auto v = render(1.2, [&](double t, double total) {
        const double u = t / total;
        const double pitch = 85.0 + 30.0 * std::sin(kPi * u) + 6.0 * std::sin(t * 2.0 * kPi * 11.0);
        const double src = a.step(pitch, Wave::Saw) + b.step(pitch * 1.5, Wave::Saw) * 0.4 + n.next() * 0.4;
        const double vox = f1.band(float(src), 500.0 + 300.0 * std::sin(kPi * u), 3.0) + f2.band(float(src), 1100.0, 4.0) * 0.5;
        return vox * 0.55 * std::min(1.0, t / 0.08) * std::pow(std::max(0.0, 1.0 - u), 0.6);
      });
      for (float& x : v)
        x *= 0.25f; // to a peak near 0.4
      return v;
    }
    case Sfx::TotemThrow:
    {
      // The Totem throwing its top runner: a "hup" whoosh up.
      Noise n(37231);
      Svf bp;
      Osc up;
      auto v = render(0.4, [&](double t, double total) {
        const double u = t / total;
        const double wh = bp.band(n.next(), sweep(500.0, 3000.0, u), 1.6) * std::sin(kPi * u);
        const double tone = up.step(sweep(300.0, 900.0, u), Wave::Sine) * std::sin(kPi * u) * 0.3;
        return (wh + tone) * 0.62 * attack(t, 0.01);
      });
      for (float& x : v)
        x *= 0.68f; // to a peak near 0.4
      return v;
    }
    case Sfx::TotemStack:
    {
      // Back on the stack: three quick bops climbing, one per runner.
      std::vector<float> b(std::size_t(samples(0.6)));
      const double notes[3] = {72.0, 76.0, 79.0};
      for (int k = 0; k < 3; ++k)
      {
        addTone(b, k * 0.07, 0.35, midiFreq(notes[k]), Wave::Sine, 0.32, 0.09);
        addTone(b, k * 0.07, 0.2, midiFreq(notes[k] + 12), Wave::Triangle, 0.1, 0.04);
      }
      for (float& x : b)
        x *= 0.73f; // to a peak near 0.4
      return b;
    }
    case Sfx::PenStrike:
    {
      // Level 38: a red marker struck through a word: a fast felt swipe
      // across the paper, the nib squeaking as it drags, a tick as it lifts.
      Noise n(38001), m(38003);
      Svf felt, lift;
      Osc sq;
      auto v = render(0.36, [&](double t, double) {
        const double u = std::min(1.0, t / 0.3);
        const double env = t < 0.3 ? std::pow(std::sin(kPi * u), 0.6) : 0.0;
        const double swipe = felt.band(n.next(), sweep(1400.0, 3800.0, u), 1.6) * env;
        const double squeak =
          sq.step(2300.0 + 400.0 * u + 70.0 * std::sin(t * 190.0), Wave::Triangle) * env * 0.16 * (0.6 + 0.4 * std::sin(t * 95.0));
        const double tick = t > 0.3 ? lift.band(m.next(), 5200.0, 4.0) * decay(t - 0.3, 0.006) * 1.6 : 0.0;
        return (swipe + squeak + tick) * attack(t, 0.004);
      });
      for (float& x : v)
        x *= 0.147f; // to a peak near 0.4
      return v;
    }
    case Sfx::WordFade:
    {
      // The struck thing fading out of the script: a soft airy whoosh going
      // dull, paper rustling apart in it.
      Noise n(38011), r(38013);
      Svf air, rustle;
      auto v = render(0.55, [&](double t, double total) {
        const double u = t / total;
        const double whoosh = air.band(n.next(), sweep(2600.0, 500.0, u), 0.9) * std::sin(kPi * std::pow(u, 0.6));
        const double crackle = rustle.band(r.next() > 0.9f ? r.next() : 0.0f, 3200.0, 2.0) * (1.0 - u) * 2.5;
        return (whoosh + crackle) * attack(t, 0.02);
      });
      for (float& x : v)
        x *= 0.246f; // to a peak near 0.36
      return v;
    }
    case Sfx::WordBack:
    {
      // A word written back and its thing back in place: a pencil's quick
      // flourish and a soft wooden two-note knock up.
      Noise n(38021);
      Svf graph;
      std::vector<float> v = render(0.5, [&](double t, double) {
        const double u = t / 0.16;
        return t < 0.16 ? graph.band(n.next(), sweep(1800.0, 3400.0, u), 2.5) * std::sin(kPi * u) * 0.8 : 0.0;
      });
      addTone(v, 0.12, 0.3, midiFreq(76), Wave::Sine, 0.45, 0.06);
      addTone(v, 0.2, 0.3, midiFreq(83), Wave::Sine, 0.45, 0.08);
      addTone(v, 0.2, 0.2, midiFreq(95), Wave::Triangle, 0.08, 0.03);
      for (float& x : v)
        x *= 0.386f; // to a peak near 0.38
      return v;
    }
    case Sfx::Scribble:
    {
      // A Typo scribbling: graphite zigzagging fast over paper, short.
      Noise n(38031);
      Svf graph;
      auto v = render(0.14, [&](double t, double total) {
        const double zig = std::abs(std::sin(t * 2.0 * kPi * 26.0));
        const double f = 2200.0 + 1400.0 * zig;
        return graph.band(n.next(), f, 2.2) * (0.35 + 0.65 * zig) * std::sin(kPi * t / total);
      });
      for (float& x : v)
        x *= 0.269f; // to a peak near 0.34
      return v;
    }
    case Sfx::PencilSketch:
    {
      // A pencil sketching a thing back: five long strokes of graphite, each
      // rising in pitch as the hand speeds up, a tap between.
      Noise n(38041), m(38043);
      Svf graph, tap;
      auto v = render(0.75, [&](double t, double) {
        const double stroke = 0.14;
        const int k = int(t / stroke);
        const double u = (t - double(k) * stroke) / stroke;
        const double s = k < 5 ? graph.band(n.next(), sweep(1500.0, 3000.0 + 300.0 * (k % 2), u), 2.0) * std::pow(std::sin(kPi * u), 0.8) : 0.0;
        const double tp = u < 0.05 && k < 5 ? tap.band(m.next(), 900.0, 2.0) * 1.2 : 0.0;
        return (s + tp) * attack(t, 0.005);
      });
      for (float& x : v)
        x *= 0.323f; // to a peak near 0.36
      return v;
    }
    case Sfx::TypoSqueak:
    {
      // A Typo springing up: a rubbery little "eek!" and a wet blip of ink.
      Osc eek, blip;
      auto v = render(0.24, [&](double t, double) {
        const double f = t < 0.05 ? sweep(1500.0, 2700.0, t / 0.05) : sweep(2700.0, 2100.0, (t - 0.05) / 0.12);
        const double e = eek.step(f * (1.0 + 0.02 * std::sin(t * 2.0 * kPi * 38.0)), Wave::Triangle) * attack(t, 0.01) * decay(t, 0.08);
        const double b = blip.step(sweep(300.0, 900.0, t / 0.04), Wave::Sine) * decay(t, 0.035) * 0.8;
        return e + b;
      });
      for (float& x : v)
        x *= 0.271f; // to a peak near 0.38
      return v;
    }
    case Sfx::KeyClack:
    {
      // A typewriter key: the key's sharp click going down, then the type
      // bar slapping the paper against the roller.
      Noise n(38051), m(38053);
      Svf click, slap;
      OnePole lp;
      Osc body;
      auto v = render(0.2, [&](double t, double) {
        const double c = click.band(n.next(), 3200.0, 3.0) * decay(t, 0.004) * 1.4;
        const double u = t - 0.028;
        const double s = u > 0.0
          ? (slap.band(m.next(), 1700.0, 1.5) * 1.3 + lp.lowpass(m.next(), 500.0) * 1.5 + body.step(190.0, Wave::Triangle) * 0.5) * decay(u, 0.018)
          : 0.0;
        return (c + s) * attack(t, 0.0005);
      });
      for (float& x : v)
        x *= 0.29f; // to a peak near 0.4
      return v;
    }
    case Sfx::KeyWrong:
    {
      // The wrong key: two type bars jamming together, a dull clunk and a
      // tinny rattle, nothing typed.
      Noise n(38061);
      Osc a, b;
      Svf rattle;
      OnePole lp;
      auto v = render(0.32, [&](double t, double total) {
        const double clunk = (a.step(sweep(210.0, 150.0, t / 0.1), Wave::Square, 0.3) * 0.4 + lp.lowpass(n.next(), 600.0) * 1.2) * decay(t, 0.05);
        const double u = t - 0.04;
        const double r = u > 0.0 ? (rattle.band(n.next(), 2400.0, 6.0) * 0.8 + b.step(1180.0, Wave::Sine) * 0.15) * decay(u, 0.06) *
              (0.5 + 0.5 * std::sin(u * 2.0 * kPi * 45.0))
                                 : 0.0;
        return (clunk + r) * attack(t, 0.001) * std::min(1.0, (total - t) / 0.03);
      });
      for (float& x : v)
        x *= 0.331f; // to a peak near 0.4
      return v;
    }
    case Sfx::CarriageBell:
    {
      // The end of the line: the typewriter's bell, a bright "ding", then
      // the carriage thrown back, its ratchet zipping and a knock home.
      Osc b1, b2, b3;
      Noise n(38071), m(38073);
      Svf ratchet, knock;
      double phase = 0.0;
      auto v = render(1.2, [&](double t, double total) {
        const double ding = (b1.step(2093.0, Wave::Sine) + b2.step(2093.0 * 2.76, Wave::Sine) * 0.4 + b3.step(2093.0 * 5.4, Wave::Sine) * 0.15) *
          attack(t, 0.002) * decay(t, 0.45) * 0.55;
        const double u = t - 0.42;
        double r = 0.0;
        if (u > 0.0 && u < 0.36)
        {
          phase += (30.0 + 70.0 * u / 0.36) / kRate;
          double hit = 0.0;
          if (phase >= 1.0)
          {
            phase -= 1.0;
            hit = 1.0;
          }
          r = ratchet.band(float(hit * 8.0 + n.next() * 0.04), 2600.0, 7.0) * 0.6;
        }
        const double w = t - 0.8;
        const double home = w > 0.0 ? knock.band(m.next(), 700.0, 1.5) * decay(w, 0.03) * 1.6 : 0.0;
        return (ding + r + home) * std::min(1.0, (total - t) / 0.05);
      });
      for (float& x : v)
        x *= 0.224f; // to a peak near 0.42
      return v;
    }
    case Sfx::CrusherCreak:
    {
      // The press about to drop: a heavy steel groan as its piston takes
      // the strain, a hiss of air under it.
      Osc g, g2;
      Noise n(38081);
      Svf res;
      OnePole hp;
      auto v = render(0.7, [&](double t, double total) {
        const double f = 62.0 + 10.0 * std::sin(t * 9.0) + 6.0 * std::sin(t * 23.0);
        const double groan = res.band(float(g.step(f, Wave::Saw) + g2.step(f * 1.51, Wave::Saw) * 0.5), 650.0 + 120.0 * std::sin(t * 7.0), 6.0);
        const double hiss = hp.highpass(n.next(), 3000.0) * 0.12 * std::min(1.0, t / 0.3);
        return (groan + hiss) * std::sin(kPi * t / total);
      });
      for (float& x : v)
        x *= 0.196f; // to a peak near 0.4
      return v;
    }
    case Sfx::CrusherSlam:
    {
      // The rubber stamp press slamming down: a huge thump, the rubber
      // slapping the page, the steel frame clanking and ringing out.
      Osc boom, c1, c2;
      Noise n(38091), m(38093);
      OnePole lp;
      Svf slap;
      auto v = render(0.8, [&](double t, double total) {
        const double th = boom.step(sweep(120.0, 38.0, t / 0.25), Wave::Sine) * decay(t, 0.16) * 1.3;
        const double sl = (slap.band(n.next(), 900.0, 1.0) * 1.2 + lp.lowpass(m.next(), 300.0) * 1.6) * decay(t, 0.04);
        const double cl = (c1.step(330.0, Wave::Square, 0.3) * 0.25 + c2.step(787.0, Wave::Sine) * 0.3) * decay(t, 0.2);
        return (th + sl + cl) * attack(t, 0.001) * std::min(1.0, (total - t) / 0.08);
      });
      for (float& x : v)
        x *= 0.222f; // to a peak near 0.45
      return v;
    }
    case Sfx::DeadlineTick:
    {
      // A Deadline's minute hand: a small dry clock tick.
      Noise n(38101);
      Svf tick;
      Osc tock;
      auto v = render(0.07, [&](double t, double) {
        return (tick.band(n.next(), 4200.0, 4.0) * 1.4 + tock.step(1900.0, Wave::Sine) * 0.4) * decay(t, 0.008) * attack(t, 0.0005);
      });
      for (float& x : v)
        x *= 0.204f; // to a peak near 0.33
      return v;
    }
    case Sfx::DeadlineBell:
    {
      // A Deadline's alarm: the hammer rattling between its two bells, the
      // whole clock buzzing with it.
      Osc a1, a2, b1, b2, buzz;
      auto v = render(0.95, [&](double t, double total) {
        const int k = int(t * 26.0);
        const double u = t - double(k) / 26.0;
        const bool left = k % 2 == 0;
        const double strike = attack(u, 0.001) * decay(u, 0.03);
        const double bellA = (a1.step(2350.0, Wave::Sine) + a2.step(2350.0 * 2.7, Wave::Sine) * 0.3) * (left ? strike : strike * 0.25);
        const double bellB = (b1.step(2790.0, Wave::Sine) + b2.step(2790.0 * 2.7, Wave::Sine) * 0.3) * (left ? strike * 0.25 : strike);
        const double z = buzz.step(52.0, Wave::Square, 0.2) * 0.06;
        return (bellA + bellB + z) * std::min(1.0, t / 0.01) * std::min(1.0, (total - t) / 0.15);
      });
      for (float& x : v)
        x *= 0.258f; // to a peak near 0.4
      return v;
    }
    case Sfx::BlockScrape:
    {
      // A Writer's Block about to slide: concrete grinding on the floor,
      // grit popping under it.
      Noise n(38111), m(38113), g(38117);
      OnePole lp;
      Svf grind, grit;
      auto v = render(0.65, [&](double t, double total) {
        const double rough = 0.6 + 0.4 * std::sin(t * 2.0 * kPi * 17.0 + std::sin(t * 31.0) * 2.0);
        const double low = lp.lowpass(n.next(), 280.0) * 1.8;
        const double mid = grind.band(m.next(), 950.0, 1.2) * 0.7 * rough;
        const double pops = grit.band(g.next() > 0.96f ? 1.0f : 0.0f, 2600.0, 3.0) * 2.0;
        return (low + mid + pops) * std::sin(kPi * std::min(1.0, t / total * 1.1)) * attack(t, 0.02);
      });
      for (float& x : v)
        x *= 0.264f; // to a peak near 0.4
      return v;
    }
    case Sfx::BlockThud:
    {
      // A Writer's Block coming to rest (or shoving the runner): a deep
      // concrete thud, a crumble of grit after.
      Osc o;
      Noise n(38121), g(38123);
      OnePole lp;
      Svf grit;
      auto v = render(0.55, [&](double t, double total) {
        const double th = (o.step(sweep(90.0, 40.0, t / 0.15), Wave::Sine) * 1.2 + lp.lowpass(n.next(), 260.0) * 2.0) * decay(t, 0.09);
        const double u = t - 0.05;
        const double cr = u > 0.0 ? grit.band(g.next() > 0.93f ? 1.0f : 0.0f, 3000.0, 3.0) * decay(u, 0.12) * 1.6 : 0.0;
        return (th + cr) * attack(t, 0.001) * std::min(1.0, (total - t) / 0.05);
      });
      for (float& x : v)
        x *= 0.308f; // to a peak near 0.44
      return v;
    }
    case Sfx::BlockTink:
    {
      // A shot glancing off a Writer's Block: a hard ping and a flick of grit.
      Osc a, b;
      Noise n(38131);
      Svf grit;
      auto v = render(0.22, [&](double t, double) {
        const double ping = (a.step(3150.0, Wave::Sine) + b.step(4730.0, Wave::Sine) * 0.5) * decay(t, 0.045);
        const double g = grit.band(n.next(), 5000.0, 2.0) * decay(t, 0.01);
        return (ping + g) * attack(t, 0.0005);
      });
      for (float& x : v)
        x *= 0.164f; // to a peak near 0.36
      return v;
    }
    case Sfx::CardSpin:
    {
      // A pitch card spinning on its pin: paper flapping round, fast then
      // slowing, the pin squeaking.
      Noise n(38141);
      Svf flap;
      Osc pin;
      double phase = 0.0;
      auto v = render(0.7, [&](double t, double total) {
        const double u = t / total;
        phase += sweep(22.0, 5.0, u) / kRate;
        const double f = phase - std::floor(phase);
        const double env = std::exp(-f * 9.0);
        const double p = flap.band(n.next(), 1900.0, 1.3) * env;
        const double s = pin.step(3400.0 + 200.0 * std::sin(t * 40.0), Wave::Sine) * 0.05 * (1.0 - u);
        return (p + s) * attack(t, 0.003) * std::min(1.0, (total - t) / 0.1);
      });
      for (float& x : v)
        x *= 0.508f; // to a peak near 0.36
      return v;
    }
    case Sfx::EggCrack:
    {
      // The Easter egg hatching: the shell crackling in three little bursts,
      // a pop as it splits.
      Noise n(38151), m(38153);
      Svf crack, pop;
      auto v = render(0.55, [&](double t, double total) {
        double c = 0.0;
        for (const double at : {0.0, 0.12, 0.22})
          if (t > at && t < at + 0.05)
            c += 1.0;
        const double crackle = crack.band(n.next() > 0.6f ? n.next() : 0.0f, 4200.0, 2.0) * c * 2.4;
        const double u = t - 0.32;
        const double pp = u > 0.0 ? pop.band(m.next(), 1200.0, 2.0) * decay(u, 0.02) * 2.0 : 0.0;
        return (crackle + pp) * std::min(1.0, (total - t) / 0.05);
      });
      for (float& x : v)
        x *= 0.142f; // to a peak near 0.38
      return v;
    }
    case Sfx::EggCheep:
    {
      // The chick: two quick high cheeps, each dropping in pitch.
      Osc o;
      auto v = render(0.26, [&](double t, double) {
        const double u = t < 0.11 ? t : t - 0.13;
        const bool on = t < 0.11 || (t > 0.13 && t < 0.24);
        const double f = sweep(4300.0, 3000.0, u / 0.09);
        return on ? o.step(f, Wave::Sine) * attack(u, 0.006) * decay(u, 0.045) : 0.0;
      });
      for (float& x : v)
        x *= 0.412f; // to a peak near 0.36
      return v;
    }
    case Sfx::PaddleBoing:
    {
      // Breakout: the paper ball off the runner's ruler, a ruler's twang.
      Osc o, h;
      Noise n(38171);
      Svf click;
      auto v = render(0.38, [&](double t, double) {
        const double f = 190.0 * (1.0 + 0.06 * std::sin(t * 2.0 * kPi * 22.0) * decay(t, 0.15));
        const double tw = (o.step(f, Wave::Triangle) + h.step(f * 3.02, Wave::Sine) * 0.25) * decay(t, 0.12);
        const double c = click.band(n.next(), 1800.0, 2.0) * decay(t, 0.006) * 1.2;
        return (tw + c) * attack(t, 0.001);
      });
      for (float& x : v)
        x *= 0.326f; // to a peak near 0.4
      return v;
    }
    case Sfx::BrickBreak:
    {
      // A letter brick knocked out: a crisp card crunch and a typed clack,
      // with a little wooden note.
      Noise n(38181), m(38183);
      Svf crunch, clack;
      auto v = render(0.3, [&](double t, double total) {
        const double cr = crunch.band(n.next(), sweep(2600.0, 1200.0, t / 0.12), 1.1) * decay(t, 0.05) * 1.2;
        const double cl = clack.band(m.next(), 3300.0, 3.0) * decay(t, 0.004) * 1.4;
        return (cr + cl) * attack(t, 0.0005) * std::min(1.0, (total - t) / 0.04);
      });
      addTone(v, 0.0, 0.25, midiFreq(79), Wave::Sine, 0.3, 0.05);
      for (float& x : v)
        x *= 0.239f; // to a peak near 0.4
      return v;
    }
    case Sfx::BrickCrack:
    {
      // A red-ink brick cracking without breaking: a dry snap.
      Noise n(38191);
      Svf snap;
      Osc o;
      auto v = render(0.14, [&](double t, double) {
        return (snap.band(n.next(), 2400.0, 1.8) * 1.4 + o.step(sweep(900.0, 500.0, t / 0.05), Wave::Triangle) * 0.3) * decay(t, 0.018) *
          attack(t, 0.0005);
      });
      for (float& x : v)
        x *= 0.278f; // to a peak near 0.36
      return v;
    }
    case Sfx::BallLost:
    {
      // Breakout: the ball past the ruler: a sad muted horn, two notes down.
      Osc a, b, c, d;
      OnePole lp1, lp2;
      auto v = render(0.85, [&](double t, double total) {
        const double u = t - 0.28;
        const double n1 = t < 0.26 ? lp1.lowpass(float(a.step(midiFreq(62), Wave::Saw) + b.step(midiFreq(62) * 1.004, Wave::Saw)), 1100.0) *
            attack(t, 0.02) * std::min(1.0, (0.26 - t) / 0.03)
                                   : 0.0;
        const double bend = u > 0.0 ? midiFreq(57) * (1.0 - 0.03 * std::min(1.0, u / 0.5)) * (1.0 + 0.012 * std::sin(u * 2.0 * kPi * 6.0)) : 0.0;
        const double n2 = u > 0.0 ? lp2.lowpass(float(c.step(bend, Wave::Saw) + d.step(bend * 1.004, Wave::Saw)), 900.0) * attack(u, 0.02) : 0.0;
        return (n1 + n2) * std::min(1.0, (total - t) / 0.2);
      });
      for (float& x : v)
        x *= 0.223f; // to a peak near 0.4
      return v;
    }
    case Sfx::InkPlop:
    {
      // A Typo crawling up out of its inkwell: a deep liquid plop, a bubble.
      Osc o, b;
      auto v = render(0.32, [&](double t, double) {
        const double plop = o.step(sweep(260.0, 900.0, t / 0.035), Wave::Sine) * decay(t, 0.04);
        const double u = t - 0.13;
        const double bub = u > 0.0 ? b.step(sweep(600.0, 1300.0, u / 0.03), Wave::Sine) * decay(u, 0.025) * 0.5 : 0.0;
        return (plop + bub) * attack(t, 0.002);
      });
      for (float& x : v)
        x *= 0.385f; // to a peak near 0.36
      return v;
    }
    case Sfx::MonitorHop:
    {
      // Level 39: through a monitor: a burst of TV snow swelling and
      // cutting off, the CRT's degauss thunk, its high whine coming up.
      Noise n(39001);
      Svf hiss;
      OnePole lp;
      Osc thunk, whine;
      auto v = render(0.5, [&](double t, double total) {
        const double u = t / total;
        const double gate = (std::fmod(t, 0.05) < 0.04 ? 1.0 : 0.55) * std::sin(kPi * std::min(1.0, u * 1.15));
        const double snow = (hiss.band(n.next(), 5200.0, 0.7) * 0.8 + lp.lowpass(n.next(), 2200.0) * 0.6) * gate;
        const double th = thunk.step(sweep(140.0, 50.0, t / 0.12), Wave::Sine) * decay(t, 0.06) * 0.9;
        const double wh = whine.step(15734.0 * 0.5, Wave::Sine) * 0.06 * std::min(1.0, t / 0.2);
        return (snow + th + wh) * attack(t, 0.004);
      });
      for (float& x : v)
        x *= 0.384f; // to a peak near 0.4
      return v;
    }
    case Sfx::EchoRepeat:
    {
      // An Echo Mic shot's echo: the shot again a beat later, fainter and
      // darker, a slapback off the studio walls tailing away.
      Osc o, s;
      Noise n(39003);
      Svf room;
      auto v = render(0.5, [&](double t, double) {
        double out = 0.0;
        for (int k = 0; k < 3; ++k)
        {
          const double u = t - double(k) * 0.11;
          if (u < 0.0)
            continue;
          const double g = std::pow(0.45, double(k));
          out += g * (o.step(sweep(880.0, 420.0, u / 0.12), Wave::Triangle) * decay(u, 0.05) * 0.7 +
            room.band(n.next(), 1400.0, 2.0) * decay(u, 0.03) * 0.5);
        }
        return (out + s.step(220.0, Wave::Sine) * decay(t, 0.08) * 0.2) * attack(t, 0.003);
      });
      for (float& x : v)
        x *= 0.397f; // to a peak near 0.33, an echo is fainter
      return v;
    }
    case Sfx::FloorBeep:
    {
      // A Floor Manager counting down: a walkie's short beep with a little
      // squelch of static behind it.
      Osc o;
      Noise n(39005);
      Svf bp;
      auto v = render(0.16, [&](double t, double total) {
        const double env = std::min(1.0, (total - t) / 0.02) * attack(t, 0.004);
        const double beep = t < 0.11 ? o.step(1320.0, Wave::Square, 0.5) * 0.5 : 0.0;
        const double sq = bp.band(n.next(), 2600.0, 1.2) * (t > 0.1 ? 0.6 : 0.08);
        return (beep + sq) * env;
      });
      for (float& x : v)
        x *= 0.338f; // to a peak near 0.34
      return v;
    }
    case Sfx::CountOne:
    {
      // "One": a clapperboard's clack, then the whole room letting fly, a
      // volley of cards flapping out.
      Noise n(39007), m(39009);
      Svf clack, flap;
      Osc low;
      auto v = render(0.6, [&](double t, double) {
        const double c = clack.band(n.next(), 2200.0, 3.0) * decay(t, 0.012) * 1.1 + low.step(sweep(180.0, 90.0, t / 0.05), Wave::Sine) * decay(t, 0.03);
        double cards = 0.0;
        for (int k = 0; k < 5; ++k)
        {
          const double u = t - 0.05 - double(k) * 0.045;
          if (u > 0.0)
            cards += decay(u, 0.05) * (0.6 + 0.4 * std::sin(u * 2.0 * kPi * (40.0 + 7.0 * k)));
        }
        return (c + flap.band(m.next(), 3200.0, 1.0) * cards * 1.6) * attack(t, 0.001);
      });
      for (float& x : v)
        x *= 0.234f; // to a peak near 0.42
      return v;
    }
    case Sfx::CableWhip:
    {
      // A Cable Wrangler yanking his cable: a rubbery swish building to a
      // whip crack, the slack slapping the floor.
      Noise n(39011), m(39013);
      Svf sw, crack;
      Osc slap;
      auto v = render(0.42, [&](double t, double) {
        const double u = std::min(1.0, t / 0.16);
        const double swish = sw.band(n.next(), sweep(400.0, 3600.0, u), 1.4) * u * u * (t < 0.17 ? 1.0 : decay(t - 0.17, 0.02));
        const double ct = t - 0.16;
        const double cr = ct > 0.0 ? crack.band(m.next(), 4200.0, 0.8) * decay(ct, 0.008) * 2.5 : 0.0;
        const double st = t - 0.24;
        const double sl = st > 0.0 ? slap.step(sweep(160.0, 70.0, st / 0.06), Wave::Sine) * decay(st, 0.035) * 0.8 : 0.0;
        return (swish * 0.8 + cr + sl) * attack(t, 0.01);
      });
      for (float& x : v)
        x *= 0.213f; // to a peak near 0.4
      return v;
    }
    case Sfx::TripFall:
    {
      // Tripped over a cable: a scuffed stumble, the body's thump on the
      // studio floor, the cable twanging.
      Noise n(39015), m(39017);
      Svf scuff;
      OnePole lp;
      Osc body, twang;
      auto v = render(0.55, [&](double t, double) {
        const double sc = t < 0.12 ? scuff.band(n.next(), 1600.0, 1.0) * std::sin(kPi * t / 0.12) * 0.7 : 0.0;
        const double bt = t - 0.11;
        const double th = bt > 0.0 ? (body.step(sweep(120.0, 45.0, bt / 0.1), Wave::Sine) * decay(bt, 0.08) * 1.2 +
                                        lp.lowpass(m.next(), 700.0) * decay(bt, 0.04) * 2.0)
                                   : 0.0;
        const double tw = bt > 0.0 ? twang.step(196.0 * (1.0 + 0.04 * decay(bt, 0.05)), Wave::Saw) * decay(bt, 0.12) * 0.18 : 0.0;
        return (sc + th + tw) * attack(t, 0.004);
      });
      for (float& x : v)
        x *= 0.252f; // to a peak near 0.42
      return v;
    }
    case Sfx::BoomWhoosh:
    {
      // A Boom Op's pass: a long airy swoosh going over, low and furry,
      // the dead-cat windshield's rumble under it.
      Noise n(39019), m(39021);
      Svf air;
      OnePole fur;
      auto v = render(0.75, [&](double t, double total) {
        const double u = t / total;
        const double env = std::sin(kPi * std::pow(u, 0.8));
        const double a = air.band(n.next(), sweep(500.0, 1800.0, std::sin(kPi * u)), 1.1);
        const double f = fur.lowpass(m.next(), 160.0) * 3.0;
        return (a * 0.9 + f) * env;
      });
      for (float& x : v)
        x *= 0.406f; // to a peak near 0.38
      return v;
    }
    case Sfx::PressHiss:
    {
      // A studio press about to drop: a hydraulic groan, air hissing out of
      // its valve.
      Noise n(39023);
      Svf hiss;
      Osc g1, g2;
      auto v = render(0.5, [&](double t, double total) {
        const double u = t / total;
        const double groan = (g1.step(70.0 + 18.0 * std::sin(t * 2.0 * kPi * 3.0), Wave::Saw) * 0.5 +
                                g2.step(105.0 + 10.0 * std::sin(t * 2.0 * kPi * 4.3), Wave::Saw) * 0.3) *
          std::sin(kPi * u);
        const double h = hiss.band(n.next(), 5200.0, 0.6) * std::min(1.0, u * 3.0) * (1.0 - u);
        return (groan * 0.6 + h * 0.9) * attack(t, 0.02);
      });
      for (float& x : v)
        x *= 0.354f; // to a peak near 0.36
      return v;
    }
    case Sfx::PressSlam:
    {
      // The press slamming: a heavy steel slam, a clank ringing off, a hiss
      // of relief as it lets go.
      Noise n(39025), m(39027);
      OnePole lp;
      Svf ring, hiss;
      Osc sub;
      auto v = render(0.7, [&](double t, double) {
        const double slam = sub.step(sweep(110.0, 38.0, t / 0.15), Wave::Sine) * decay(t, 0.12) * 1.3 + lp.lowpass(n.next(), 900.0) * decay(t, 0.03) * 3.0;
        const double cl = ring.band(m.next(), 2350.0, 14.0) * decay(t, 0.12) * 2.0;
        const double ht = t - 0.3;
        const double h = ht > 0.0 ? hiss.band(n.next(), 4800.0, 0.7) * std::sin(kPi * std::min(1.0, ht / 0.4)) * 0.5 : 0.0;
        return (slam + cl + h) * attack(t, 0.001);
      });
      for (float& x : v)
        x *= 0.0994f; // to a peak near 0.45
      return v;
    }
    case Sfx::VhsRewind:
    {
      // The tape rewinding: the deck's clunk, the spools whirring up in
      // pitch, the heads chattering against the tape.
      Noise n(39029), m(39031);
      Svf head;
      OnePole lp;
      Osc spool, motor;
      auto v = render(1.3, [&](double t, double total) {
        const double cl = lp.lowpass(n.next(), 1200.0) * decay(t, 0.02) * 3.0 + motor.step(sweep(160.0, 60.0, t / 0.05), Wave::Sine) * decay(t, 0.03);
        const double rt = t - 0.08;
        double whirr = 0.0;
        if (rt > 0.0)
        {
          const double u = rt / (total - 0.08);
          const double f = sweep(180.0, 1400.0, std::min(1.0, u * 1.2));
          const double chatter = 0.6 + 0.4 * std::sin(rt * 2.0 * kPi * (30.0 + 50.0 * u));
          whirr = (spool.step(f, Wave::Saw) * 0.25 + head.band(m.next(), f * 3.0, 3.0) * 0.8 * chatter) * std::min(1.0, rt / 0.1) *
            std::min(1.0, (total - t) / 0.12);
        }
        return cl + whirr;
      });
      for (float& x : v)
        x *= 0.229f; // to a peak near 0.4
      return v;
    }
    case Sfx::LavaRumble:
    {
      // Level 33: the lava rising: a deep rumble swelling up from below, a
      // sub tone wavering under it, fat bubbles glugging up and bursting.
      Noise n(907), m(911);
      OnePole lp;
      Svf bp;
      Osc sub, glug;
      double bubbleF = 90.0;
      double bubbleT = -1.0;
      return render(1.8, [&](double t, double total) {
        const double u = t / total;
        const double rum = lp.lowpass(n.next(), 120.0) * 3.0 * (0.7 + 0.3 * std::sin(t * 2.0 * kPi * 2.3));
        const double s = sub.step(38.0 + 4.0 * std::sin(t * 2.0 * kPi * 0.7), Wave::Sine) * 0.35;
        // A bubble every ~0.27 s: a falling glug and the pop of its skin.
        if (std::fmod(t, 0.27) < 1.0 / kRate)
        {
          bubbleT = t;
          bubbleF = 70.0 + 60.0 * (double(m.next()) + 1.0);
        }
        double bub = 0.0;
        if (bubbleT >= 0.0)
        {
          const double bt = t - bubbleT;
          bub = glug.step(sweep(bubbleF, bubbleF * 0.55, bt / 0.12), Wave::Sine) * decay(bt, 0.05) * 0.5 +
            bp.band(n.next(), 900.0, 2.0) * decay(bt, 0.01) * 0.4;
        }
        return (rum * 0.42 + s + bub * 0.55) * 0.55 * std::min(1.0, u / 0.25) * std::min(1.0, (total - t) / 0.5);
      });
    }
    case Sfx::SteamBurst:
    {
      // A coolant purge venting: a blast of steam bursting from the nozzle,
      // a hissing roar that tails off, a valve's clank at the start.
      Noise n(919), m(929);
      Svf bp, bp2;
      OnePole hp;
      Osc clank;
      return render(1.5, [&](double t, double total) {
        const double u = t / total;
        const double blast = bp.band(n.next(), sweep(1800.0, 5200.0, std::min(1.0, t / 0.15)), 0.7) * attack(t, 0.02) * decay(t, 0.5);
        const double hiss = hp.highpass(m.next(), 5000.0) * (0.6 + 0.4 * std::sin(t * 2.0 * kPi * 11.0)) * (1.0 - u);
        const double roar = bp2.band(n.next(), 500.0, 0.8) * decay(t, 0.3) * 0.6;
        const double k = clank.step(sweep(420.0, 260.0, t / 0.05), Wave::Square, 0.3) * decay(t, 0.025) * 0.25;
        return (blast * 0.6 + hiss * 0.45 + roar + k) * 0.57 * std::min(1.0, (total - t) / 0.3);
      });
    }
    case Sfx::PurgeSiren:
    {
      // The purge running out: a klaxon whooping up twice, a rasp on it.
      Osc a, b;
      return render(1.4, [&](double t, double total) {
        const double ph = std::fmod(t, 0.7) / 0.7;
        const double f = sweep(300.0, 900.0, ph * 1.2);
        const double v = a.step(f, Wave::Square, 0.45) * 0.5 + b.step(f * 1.01, Wave::Saw) * 0.3;
        const double gate = ph < 0.85 ? 1.0 : 0.0;
        return v * 0.49 * gate * attack(std::fmod(t, 0.7), 0.01) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::LavaPop:
    {
      // Popped out of the lava: a searing sizzle, then a fat bubble's pop
      // and a little rising whistle as you fly up and out.
      Noise n(937);
      OnePole hp;
      Svf bp;
      Osc pop, w;
      return render(0.7, [&](double t, double total) {
        const double sizzle = hp.highpass(n.next(), 3800.0) * (n.next() > 0.3f ? 1.0 : 0.3) * decay(t, 0.18) * 0.7;
        const double u = t - 0.06;
        const double p = u > 0.0 ? (pop.step(sweep(160.0, 520.0, u / 0.05), Wave::Sine) * decay(u, 0.04) * 0.9 +
                                      bp.band(n.next(), 1400.0, 2.0) * decay(u, 0.012) * 0.6)
                                 : 0.0;
        const double v = u > 0.05 ? w.step(sweep(600.0, 1500.0, (u - 0.05) / 0.4), Wave::Sine) * 0.18 * decay(u - 0.05, 0.25) : 0.0;
        return (sizzle + p + v) * 0.31 * attack(t, 0.003) * std::min(1.0, (total - t) / 0.08);
      });
    }
    case Sfx::GutterSizzle:
    {
      // Wading in the magma gutter: a short searing sizzle.
      Noise n(941), m(947);
      OnePole hp;
      Svf bp;
      return render(0.32, [&](double t, double total) {
        const double s = hp.highpass(n.next(), 3000.0) * (m.next() > 0.0f ? 1.0 : 0.35);
        const double body = bp.band(n.next(), 700.0, 1.0) * 0.5;
        return (s * 0.8 + body) * 0.5 * attack(t, 0.005) * decay(t, 0.12) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::SinkSuck:
    {
      // The Heat Sink held: a vacuum's whoosh pulling in, its motor
      // whirring up under it.
      Noise n(953);
      Svf bp;
      Osc mo;
      return render(0.75, [&](double t, double total) {
        const double u = t / total;
        const double air = bp.band(n.next(), sweep(2600.0, 600.0, u), 1.4) * (0.4 + 0.6 * u);
        const double motor = mo.step(sweep(120.0, 360.0, std::min(1.0, u * 1.5)), Wave::Saw) * 0.12;
        return (air * 0.7 + motor) * 0.9 * attack(t, 0.04) * std::min(1.0, (total - t) / 0.12);
      });
    }
    case Sfx::SinkGulp:
    {
      // A shot swallowed: a deep glug, then a bright ping (one more stored).
      Osc a, b;
      Noise n(967);
      Svf bp;
      return render(0.36, [&](double t, double total) {
        const double glug = a.step(sweep(240.0, 80.0, std::min(1.0, t / 0.12)), Wave::Sine) * decay(t, 0.06) * 0.7 +
          bp.band(n.next(), 600.0, 2.0) * decay(t, 0.02) * 0.4;
        const double u = t - 0.1;
        const double ping = u > 0.0 ? b.step(1760.0, Wave::Triangle) * decay(u, 0.08) * 0.35 : 0.0;
        return (glug + ping) * 0.55 * attack(t, 0.003) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::SinkThrow:
    {
      // The Heat Sink's fireball thrown: a whump of air and a rolling roar
      // of flame going away.
      Noise n(971), m(977);
      Svf bp;
      OnePole lp;
      Osc thump;
      return render(0.75, [&](double t, double total) {
        const double whump = thump.step(sweep(140.0, 45.0, t / 0.15), Wave::Sine) * decay(t, 0.08);
        const double roar = bp.band(n.next(), 500.0 + 250.0 * std::sin(t * 2.0 * kPi * 7.0), 0.9) * decay(t, 0.3);
        const double crackle = (m.next() > 0.85f ? 1.0 : 0.0) * lp.lowpass(n.next(), 3000.0) * decay(t, 0.25);
        return (whump * 0.75 + roar * 0.8 + crackle * 0.5) * 0.48 * attack(t, 0.004) * std::min(1.0, (total - t) / 0.15);
      });
    }
    case Sfx::FirewallSputter:
    {
      // A Firewall about to dip: its flames sputtering, a glitchy buzz
      // chopping in and out like a failing signal.
      Noise n(983), m(991);
      Svf bp;
      Osc buzz;
      return render(0.55, [&](double t, double total) {
        const double chop = std::fmod(t, 0.06) < 0.035 ? 1.0 : 0.15;
        const double flame = bp.band(n.next(), 900.0 + 400.0 * std::sin(t * 2.0 * kPi * 13.0), 1.0) * chop;
        const double b = buzz.step(sweep(220.0, 110.0, t / total), Wave::Square, 0.3) * (m.next() > 0.0f ? 0.2 : 0.05);
        return (flame * 0.75 + b) * 0.6 * attack(t, 0.01) * std::min(1.0, (total - t) / 0.06);
      });
    }
    case Sfx::FirewallRoar:
    {
      // A Firewall flaring back up: a roaring whoosh rising.
      Noise n(997), m(1009);
      Svf bp;
      OnePole lp;
      return render(0.7, [&](double t, double total) {
        const double u = t / total;
        const double whoosh = bp.band(n.next(), sweep(250.0, 1400.0, std::min(1.0, u * 2.0)), 0.8) * std::sin(std::min(1.0, u * 1.4) * kPi * 0.5);
        const double body = lp.lowpass(m.next(), 300.0) * 2.0;
        return (whoosh * 0.7 + body * 0.35) * 0.81 * attack(t, 0.02) * std::min(1.0, (total - t) / 0.25);
      });
    }
    case Sfx::DaemonWindup:
    {
      // A Lava Daemon winding up: a crackling charge rising, the fireball
      // in its fist roaring louder.
      Noise n(1013), m(1019);
      Svf bp;
      Osc a;
      return render(0.8, [&](double t, double total) {
        const double u = t / total;
        const double crackle = (m.next() > 0.8f - 0.25f * float(u) ? 1.0 : 0.0) * bp.band(n.next(), 2400.0, 2.0) * 1.4;
        const double rise = a.step(sweep(120.0, 480.0, u), Wave::Saw) * 0.12 * u;
        const double roar = bp.band(n.next(), sweep(300.0, 900.0, u), 1.0) * 0.4 * u;
        return (crackle * 0.4 + rise + roar) * 0.48 * attack(t, 0.03) * std::min(1.0, (total - t) / 0.03);
      });
    }
    case Sfx::DaemonSpit:
    {
      // A Lava Daemon's fireball lobbed: a fwoomp and a hiss trailing off.
      Noise n(1021);
      Svf bp;
      OnePole hp;
      Osc o;
      return render(0.45, [&](double t, double total) {
        const double f = o.step(sweep(260.0, 90.0, t / 0.12), Wave::Sine) * decay(t, 0.06) * 0.8;
        const double flame = bp.band(n.next(), sweep(600.0, 1600.0, t / total), 1.0) * decay(t, 0.12) * 0.7;
        const double hiss = hp.highpass(n.next(), 4500.0) * decay(t, 0.2) * 0.3;
        return (f + flame + hiss) * 0.42 * attack(t, 0.004) * std::min(1.0, (total - t) / 0.06);
      });
    }
    case Sfx::DaemonHop:
    {
      // A Lava Daemon leaping: a rocky scrape and a little sizzle.
      Noise n(1031), m(1033);
      Svf bp;
      OnePole hp;
      return render(0.3, [&](double t, double total) {
        const double scrape = bp.band(n.next(), 1100.0, 1.6) * (m.next() > -0.2f ? 1.0 : 0.2) * decay(t, 0.05);
        const double sizzle = hp.highpass(n.next(), 4000.0) * decay(t, 0.1) * 0.4;
        return (scrape + sizzle) * 0.55 * attack(t, 0.002) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::FanWhine:
    {
      // A Fan Bot spinning up: a turbine whine climbing, the blades'
      // chop speeding up under it.
      Osc a, b;
      Noise n(1039);
      Svf bp;
      return render(1.0, [&](double t, double total) {
        const double u = t / total;
        const double f = sweep(180.0, 1400.0, u * u * (3.0 - 2.0 * u));
        const double whine = a.step(f, Wave::Sine) * 0.5 + b.step(f * 1.5, Wave::Triangle) * 0.2;
        const double chopRate = sweep(6.0, 60.0, u);
        const double chop = bp.band(n.next(), 500.0, 1.0) * (0.5 + 0.5 * std::sin(t * 2.0 * kPi * chopRate * (1.0 + u)));
        return (whine * 0.3 * (0.3 + 0.7 * u) + chop * 0.35) * 1.6 * attack(t, 0.05) * std::min(1.0, (total - t) / 0.04);
      });
    }
    case Sfx::FanGust:
    {
      // A Fan Bot's gust: a rush of wind swelling and buffeting.
      Noise n(1049), m(1051);
      Svf bp, bp2;
      return render(1.6, [&](double t, double total) {
        const double u = t / total;
        const double buffet = 0.7 + 0.3 * std::sin(t * 2.0 * kPi * 5.0) * std::sin(t * 2.0 * kPi * 1.3);
        const double wind = bp.band(n.next(), 700.0 + 500.0 * std::sin(u * kPi), 0.6) * buffet;
        const double whistle = bp2.band(m.next(), 2200.0 + 300.0 * std::sin(t * 3.0), 8.0) * 0.6;
        return (wind * 0.8 + whistle * 0.3) * 0.75 * std::min(1.0, t / 0.15) * std::min(1.0, (total - t) / 0.6);
      });
    }
    case Sfx::RackGlitch:
    {
      // The corrupted rack spinning up: digital chatter (bit-crushed
      // bleeps at random pitches) over a sick, warbling fan whine.
      Osc a, b;
      Noise pick(1061), n(1063);
      double f = 800.0;
      int left = 0;
      return render(1.0, [&](double t, double total) {
        if (left-- <= 0)
        {
          f = 300.0 + 1700.0 * (double(pick.next()) + 1.0) * 0.5;
          left = int(0.03 * kRate);
        }
        const double chatter = std::round(a.step(f, Wave::Square, 0.5) * 3.0) / 3.0 * (std::fmod(t, 0.03) < 0.02 ? 0.15 : 0.0);
        const double u = t / total;
        const double whine = b.step(sweep(200.0, 900.0, u) * (1.0 + 0.06 * std::sin(t * 2.0 * kPi * 7.0)), Wave::Saw) * 0.12;
        const double fizz = n.next() * 0.05 * u;
        return (chatter + whine + fizz) * 1.26 * attack(t, 0.02) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::RebootChime:
    {
      // The DO NOT REBOOT rack shot: fans and drives whirring down to
      // silence, a click, then a warm startup chime.
      Osc down;
      Noise n(1069);
      Svf bp;
      std::vector<float> b = render(2.2, [&](double t, double total) {
        (void)total;
        if (t > 0.9)
          return 0.0;
        const double u = t / 0.9;
        const double whir = down.step(sweep(900.0, 60.0, u), Wave::Saw) * 0.16 * (1.0 - u);
        const double air = bp.band(n.next(), sweep(1800.0, 200.0, u), 1.0) * 0.3 * (1.0 - u);
        return (whir + air) * attack(t, 0.01);
      });
      addTone(b, 1.05, 0.03, 2400.0, Wave::Square, 0.12, 0.01);
      for (const auto& note : {std::array<double, 2>{60, 0}, {64, 0.12}, {67, 0.24}, {72, 0.36}})
      {
        addTone(b, 1.25 + note[1], 0.9 - note[1], midiFreq(note[0]), Wave::Triangle, 0.11, 0.35);
        addTone(b, 1.25 + note[1], 0.9 - note[1], midiFreq(note[0] + 12), Wave::Sine, 0.04, 0.25);
      }
      for (auto& x : b)
        x *= 1.3f;
      return b;
    }
    case Sfx::RatingsDing:
    {
      // The RATINGS server shot: a quick rising arpeggio (the ratings going
      // through the roof) and a till's bright ding.
      std::vector<float> b(std::size_t(samples(1.1)));
      static constexpr std::array<double, 6> kUp{67, 71, 74, 79, 83, 86};
      for (std::size_t i = 0; i < kUp.size(); ++i)
        addTone(b, 0.05 * double(i), 0.18, midiFreq(kUp[i]), Wave::Square, 0.07, 0.07);
      addTone(b, 0.34, 0.75, midiFreq(96), Wave::Sine, 0.22, 0.3);
      addTone(b, 0.34, 0.75, midiFreq(103), Wave::Sine, 0.09, 0.2);
      for (auto& x : b)
        x *= 1.25f;
      return b;
    }
    case Sfx::CraneBell:
    {
      // Level 36: a crane's warning bell: an electric bell's clapper
      // hammering away, the bell's partials ringing out when it stops.
      Osc a, b, c;
      auto out = render(0.6, [&](double t, double) {
        const double strike = decay(std::fmod(t, 0.042), 0.018);
        const double env = t < 0.42 ? 1.0 : decay(t - 0.42, 0.07);
        const double ring = a.step(1720.0, Wave::Sine) * 0.6 + b.step(1720.0 * 2.76, Wave::Sine) * 0.25 + c.step(1720.0 * 5.4, Wave::Sine) * 0.1;
        return ring * (0.35 + 0.65 * strike) * env * attack(t);
      });
      for (auto& x : out)
        x *= float(0.429);
      return out;
    }
    case Sfx::MagnetClamp:
    {
      // The magnet coming down on steel: a heavy clank, then its hum taking
      // hold of the load.
      Osc k, ring, h1, h2;
      Noise n(36001);
      OnePole lp;
      auto out = render(0.7, [&](double t, double total) {
        const double clank = k.step(sweep(240.0, 110.0, t / 0.08), Wave::Square, 0.35) * decay(t, 0.05) * 0.6 +
          lp.lowpass(n.next(), 1800.0) * decay(t, 0.03) * 1.2 + ring.step(640.0, Wave::Triangle) * decay(t, 0.12) * 0.3;
        const double hum = (h1.step(60.0, Wave::Saw) * 0.5 + h2.step(120.0, Wave::Sine) * 0.4) * std::min(1.0, t / 0.15) * 0.35;
        return (clank + hum) * attack(t) * std::min(1.0, (total - t) / 0.1);
      });
      for (auto& x : out)
        x *= float(0.372);
      return out;
    }
    case Sfx::MagnetTap:
    {
      // The magnet down on nothing it can hold: a hollow clonk.
      Osc a, b;
      Noise n(36003);
      Svf bp;
      auto out = render(0.35, [&](double t, double) {
        return (a.step(330.0, Wave::Triangle) * decay(t, 0.06) + b.step(710.0, Wave::Sine) * decay(t, 0.03) * 0.5 +
                 bp.band(n.next(), 1500.0, 2.0) * decay(t, 0.008)) * attack(t);
      });
      for (auto& x : out)
        x *= float(0.270);
      return out;
    }
    case Sfx::MagnetRelease:
    {
      // A load let go: the hum cut off with a clack of the release, a thunk.
      Osc h1, h2, th;
      Noise n(36005);
      Svf bp;
      auto out = render(0.5, [&](double t, double) {
        const double hum = (h1.step(60.0, Wave::Saw) * 0.5 + h2.step(120.0, Wave::Sine) * 0.4) * decay(t, 0.03) * 0.5;
        const double u = t - 0.02;
        const double clack = u > 0.0 ? bp.band(n.next(), 2200.0, 2.5) * decay(u, 0.012) * 1.5 : 0.0;
        const double thunk = u > 0.0 ? th.step(sweep(180.0, 90.0, u / 0.1), Wave::Sine) * decay(u, 0.08) : 0.0;
        return (hum + clack + thunk) * attack(t);
      });
      for (auto& x : out)
        x *= float(0.199);
      return out;
    }
    case Sfx::CraneStop:
    {
      // A ride crane reaching the end of its rail: the buffer's clank and a
      // spring's boing as it settles.
      Osc k, ring, boing;
      auto out = render(0.7, [&](double t, double total) {
        const double clank = k.step(sweep(200.0, 120.0, t / 0.06), Wave::Square, 0.3) * decay(t, 0.05) * 0.6 + ring.step(950.0, Wave::Sine) * decay(t, 0.2) * 0.3;
        const double b = boing.step(150.0 * (1.0 + 0.25 * std::sin(2.0 * kPi * 16.0 * t) * decay(t, 0.2)), Wave::Sine) * decay(t, 0.16) * 0.7;
        return (clank + b) * attack(t) * std::min(1.0, (total - t) / 0.05);
      });
      for (auto& x : out)
        x *= float(0.269);
      return out;
    }
    case Sfx::CrateScrape:
    {
      // A metal crate shoved along the floor: steel grating on concrete,
      // juddering, a low rumble under it.
      Noise n(36007), m(36009);
      Svf bp;
      OnePole lp;
      double rough = 0.0;
      int hold = 0;
      auto out = render(0.6, [&](double t, double total) {
        if (hold-- <= 0)
        {
          rough = 0.5 + 0.5 * (m.next() * 0.5 + 0.5);
          hold = samples(0.012);
        }
        const double grate = bp.band(n.next(), 1100.0 + 300.0 * std::sin(t * 40.0), 3.0) * rough;
        const double rumble = lp.lowpass(m.next(), 200.0) * 2.0;
        return (grate + rumble * 0.4) * std::min(1.0, t / 0.02) * std::min(1.0, (total - t) / 0.25);
      });
      for (auto& x : out)
        x *= float(0.333);
      return out;
    }
    case Sfx::CrateLand:
    {
      // A metal crate landing: a big steel boom and the box rattling.
      Osc boom, ring;
      Noise n(36011), m(36013);
      OnePole lp;
      Svf bp;
      auto out = render(0.9, [&](double t, double total) {
        const double b = boom.step(sweep(95.0, 38.0, t / 0.4), Wave::Sine) * decay(t, 0.2);
        const double crash = lp.lowpass(n.next(), sweep(3500.0, 250.0, t / 0.3)) * decay(t, 0.09) * 1.4;
        const double rattle = t > 0.06 && t < 0.45 && m.next() > 0.93f ? bp.band(1.0f, 2600.0, 4.0) * 0.6 : bp.band(0.0f, 2600.0, 4.0) * 0.6;
        const double r = ring.step(430.0, Wave::Triangle) * decay(t, 0.15) * 0.25;
        return (b + crash + rattle + r) * attack(t, 0.001) * std::min(1.0, (total - t) / 0.1);
      });
      for (auto& x : out)
        x *= float(0.208);
      return out;
    }
    case Sfx::CrateBump:
    {
      // A sliding crate coming to rest: a dull clunk.
      Osc a, b;
      Noise n(36015);
      OnePole lp;
      auto out = render(0.35, [&](double t, double) {
        return (a.step(110.0, Wave::Sine) * decay(t, 0.08) + lp.lowpass(n.next(), 1200.0) * decay(t, 0.03) * 1.2 +
                 b.step(600.0, Wave::Triangle) * decay(t, 0.04) * 0.3) * attack(t);
      });
      for (auto& x : out)
        x *= float(0.320);
      return out;
    }
    case Sfx::CrateBack:
    {
      // A crate back at its start: a winch's whir climbing, then a clunk.
      Osc w, c;
      OnePole lp;
      Noise n(36017);
      auto out = render(0.65, [&](double t, double total) {
        const double whir = t < 0.38 ? lp.lowpass(float(w.step(sweep(180.0, 420.0, t / 0.38), Wave::Saw)), 1400.0) * 0.4 * std::sin(t / 0.38 * kPi) : 0.0;
        const double u = t - 0.38;
        const double clunk = u > 0.0 ? (c.step(130.0, Wave::Sine) * decay(u, 0.07) + n.next() * decay(u, 0.01) * 0.5) : 0.0;
        return (whir + clunk) * attack(t) * std::min(1.0, (total - t) / 0.05);
      });
      for (auto& x : out)
        x *= float(0.293);
      return out;
    }
    case Sfx::CrateCrunch:
    {
      // A crate hitting an enemy: a metal bonk with a ring.
      Osc a, ring;
      Noise n(36019);
      Svf bp;
      auto out = render(0.4, [&](double t, double) {
        return (a.step(sweep(420.0, 200.0, t / 0.1), Wave::Square, 0.3) * decay(t, 0.05) * 0.6 + ring.step(1300.0, Wave::Sine) * decay(t, 0.1) * 0.3 +
                 bp.band(n.next(), 1800.0, 1.5) * decay(t, 0.02)) * attack(t);
      });
      for (auto& x : out)
        x *= float(0.331);
      return out;
    }
    case Sfx::FoamFlump:
    {
      // A crate into the foam pit: a soft, muffled whump.
      Osc o;
      Noise n(36021);
      OnePole lp;
      auto out = render(0.5, [&](double t, double) {
        return (lp.lowpass(n.next(), 380.0) * decay(t, 0.12) * 2.0 + o.step(sweep(80.0, 50.0, t / 0.2), Wave::Sine) * decay(t, 0.1)) *
          attack(t, 0.01);
      });
      for (auto& x : out)
        x *= float(0.378);
      return out;
    }
    case Sfx::LeverThrow:
    {
      // A crane lever thrown: the handle's travel, a knife switch's heavy
      // clack, a crackle of spark and a relay's thunk.
      Osc k, th;
      Noise n(36023), sp(36025);
      Svf bp;
      OnePole hp;
      auto out = render(0.55, [&](double t, double total) {
        const double travel = t < 0.1 ? bp.band(n.next(), 900.0, 1.0) * 0.25 * (t / 0.1) : 0.0;
        const double u = t - 0.1;
        double clack = 0.0, spark = 0.0, relay = 0.0;
        if (u > 0.0)
        {
          clack = (k.step(260.0, Wave::Square, 0.3) * decay(u, 0.03) * 0.6 + n.next() * decay(u, 0.006));
          spark = sp.next() > 0.8f ? hp.highpass(sp.next(), 3000.0) * decay(u, 0.12) * 1.2 : 0.0;
          relay = th.step(95.0, Wave::Sine) * decay(u, 0.06) * 0.7;
        }
        return (travel + clack + spark + relay) * attack(t) * std::min(1.0, (total - t) / 0.05);
      });
      for (auto& x : out)
        x *= float(0.177);
      return out;
    }
    case Sfx::ForkRev:
    {
      // A Forklift Bot's tell: its engine revving twice, hard, and the forks'
      // hydraulics whining down.
      Osc e1, e2, wh;
      OnePole lp;
      Noise n(36027);
      auto out = render(0.9, [&](double t, double total) {
        const double rev = 0.5 + 0.5 * std::pow(std::sin(std::min(1.0, t / 0.8) * kPi * 2.0 - kPi * 0.5) * 0.5 + 0.5, 2.0);
        const double f = 48.0 + 70.0 * rev;
        const double engine = lp.lowpass(float(e1.step(f, Wave::Saw) * 0.6 + e2.step(f * 0.5, Wave::Square, 0.3) * 0.4 + n.next() * 0.3), 700.0 + 900.0 * rev);
        const double whine = wh.step(sweep(950.0, 480.0, t / total), Wave::Sine) * 0.12;
        return (engine * (0.6 + 0.4 * rev) + whine) * std::min(1.0, t / 0.03) * std::min(1.0, (total - t) / 0.1);
      });
      for (auto& x : out)
        x *= float(0.363);
      return out;
    }
    case Sfx::ForkCharge:
    {
      // A Forklift Bot charging: its tyres squealing, the engine roaring.
      Osc sq, e1;
      Noise n(36029);
      Svf bp;
      OnePole lp;
      auto out = render(0.8, [&](double t, double total) {
        const double squeal = t < 0.35 ? (sq.step(2300.0 + 120.0 * std::sin(t * 70.0), Wave::Sine) * 0.3 + bp.band(n.next(), 2600.0, 6.0) * 0.4) * decay(t, 0.15) : 0.0;
        const double engine = lp.lowpass(float(e1.step(sweep(90.0, 140.0, t / total), Wave::Saw) + n.next() * 0.3), 1200.0) * 0.6;
        return (squeal + engine) * std::min(1.0, t / 0.02) * std::min(1.0, (total - t) / 0.2);
      });
      for (auto& x : out)
        x *= float(0.334);
      return out;
    }
    case Sfx::ForkLift:
    {
      // The runner lifted on the forks: the hydraulics hissing up, the pump
      // whining, a clank at the top.
      Osc pump, k;
      Noise n(36031);
      OnePole hp;
      auto out = render(0.7, [&](double t, double total) {
        const double up = std::min(1.0, t / 0.5);
        const double hiss = t < 0.5 ? hp.highpass(n.next(), 2500.0) * 0.4 * std::sin(up * kPi) : 0.0;
        const double whine = t < 0.5 ? pump.step(sweep(400.0, 900.0, up), Wave::Triangle) * 0.25 : 0.0;
        const double u = t - 0.5;
        const double clank = u > 0.0 ? k.step(sweep(300.0, 160.0, u / 0.05), Wave::Square, 0.3) * decay(u, 0.04) * 0.6 : 0.0;
        return (hiss + whine + clank) * std::min(1.0, t / 0.02) * std::min(1.0, (total - t) / 0.05);
      });
      for (auto& x : out)
        x *= float(0.601);
      return out;
    }
    case Sfx::ForkDrop:
    {
      // Set down off the forks: the hydraulics letting go with a sigh, a
      // clunk.
      Osc k;
      Noise n(36033);
      OnePole hp;
      auto out = render(0.5, [&](double t, double total) {
        const double hiss = hp.highpass(n.next(), 1800.0) * 0.4 * decay(t, 0.15);
        const double u = t - 0.15;
        const double clunk = u > 0.0 ? k.step(120.0, Wave::Sine) * decay(u, 0.07) : 0.0;
        return (hiss + clunk) * attack(t, 0.01) * std::min(1.0, (total - t) / 0.05);
      });
      for (auto& x : out)
        x *= float(0.374);
      return out;
    }
    case Sfx::ForkBeep:
    {
      // A Forklift Bot turning round: its reversing beeper, two beeps.
      std::vector<float> out(std::size_t(samples(0.7)));
      for (const double at : {0.0, 0.38})
      {
        addTone(out, at, 0.24, 1150.0, Wave::Triangle, 0.4, 1.0);
        addTone(out, at, 0.24, 2300.0, Wave::Sine, 0.08, 1.0);
      }
      for (auto& x : out)
        x *= float(0.909);
      return out;
    }
    case Sfx::FlatDown:
    {
      // A scenery flat set down: a wooden slap, the stage weight's thud and
      // the brace rattling.
      Osc o, c1;
      Noise n(36035);
      OnePole lp;
      auto out = render(0.5, [&](double t, double) {
        const double slap = lp.lowpass(n.next(), 2500.0) * decay(t, 0.03) * 1.2;
        const double thud = o.step(sweep(110.0, 70.0, t / 0.15), Wave::Sine) * decay(t, 0.1);
        const double u = t - 0.09;
        const double rattle = u > 0.0 ? c1.step(520.0, Wave::Triangle) * decay(std::fmod(u, 0.06), 0.01) * decay(u, 0.08) * 0.4 : 0.0;
        return (slap + thud + rattle) * attack(t);
      });
      for (auto& x : out)
        x *= float(0.305);
      return out;
    }
    case Sfx::FlatHit:
    {
      // A shot into a flat or a wooden crate: a hollow knock.
      Osc a;
      Noise n(36037);
      Svf bp;
      auto out = render(0.3, [&](double t, double) {
        return (a.step(240.0, Wave::Triangle) * decay(t, 0.05) + bp.band(n.next(), 900.0, 2.0) * decay(t, 0.02) * 1.5) * attack(t);
      });
      for (auto& x : out)
        x *= float(0.363);
      return out;
    }
    case Sfx::FlatBreak:
    {
      // A flat torn down: timber splintering, the canvas ripping, the
      // pieces clattering down.
      Noise n(36039), m(36041), r(36043);
      Svf bp, rip;
      Osc o;
      auto out = render(0.85, [&](double t, double total) {
        const double snap = n.next() > 0.96f ? bp.band(m.next(), 2400.0, 2.0) * 3.0 * decay(t, 0.3) : bp.band(0.0f, 2400.0, 2.0) * 3.0 * decay(t, 0.3);
        const double tear = t < 0.4 ? rip.band(r.next(), sweep(3200.0, 900.0, t / 0.4), 1.5) * (0.6 + 0.4 * std::sin(t * 300.0)) * 0.7 : 0.0;
        const double u = t - 0.45;
        const double clatter = u > 0.0 ? o.step(380.0 + 120.0 * std::floor(std::fmod(u * 20.0, 3.0)), Wave::Triangle) * decay(std::fmod(u, 0.08), 0.012) * decay(u, 0.15) * 0.5 : 0.0;
        return (snap + tear + clatter) * attack(t) * std::min(1.0, (total - t) / 0.05);
      });
      for (auto& x : out)
        x *= float(0.344);
      return out;
    }
    case Sfx::TapeThrow:
    {
      // A Stagehand's throw: a whoosh with the tape roll fluttering in it.
      Noise n(36045);
      Svf bp;
      auto out = render(0.4, [&](double t, double total) {
        const double u = t / total;
        return bp.band(n.next(), sweep(600.0, 2400.0, u), 1.4) * std::sin(u * kPi) * (0.65 + 0.35 * std::sin(t * 2.0 * kPi * 28.0));
      });
      for (auto& x : out)
        x *= float(0.689);
      return out;
    }
    case Sfx::MimicCreak:
    {
      // A Prop Mimic's lid opening: a hinge creaking, a wet growl under it.
      Osc c, g;
      Svf bp;
      Noise n(36047);
      OnePole lp;
      auto out = render(0.45, [&](double t, double total) {
        const double f = 190.0 + 70.0 * std::sin(t * 23.0) + 40.0 * std::sin(t * 61.0);
        const double creak = bp.band(float(c.step(f, Wave::Saw)), f * 3.0, 5.0) * 0.6;
        const double growl = lp.lowpass(float(g.step(78.0, Wave::Saw) * (0.6 + 0.4 * n.next())), 500.0) * 0.8 * (t / total);
        return (creak + growl) * std::min(1.0, t / 0.03) * std::min(1.0, (total - t) / 0.05);
      });
      for (auto& x : out)
        x *= float(0.266);
      return out;
    }
    case Sfx::MimicSnap:
    {
      // A Prop Mimic's bite: the lid slammed shut on its teeth.
      Osc o;
      Noise n(36049), m(36051);
      OnePole lp;
      Svf bp;
      auto out = render(0.35, [&](double t, double) {
        const double slam = lp.lowpass(n.next(), 2000.0) * decay(t, 0.025) * 1.3 + o.step(sweep(150.0, 90.0, t / 0.1), Wave::Sine) * decay(t, 0.06);
        const double clack = (bp.band(m.next(), 3200.0, 3.0) * (decay(t, 0.008) + (t > 0.03 ? decay(t - 0.03, 0.008) : 0.0))) * 1.5;
        return (slam + clack) * attack(t, 0.001);
      });
      for (auto& x : out)
        x *= float(0.124);
      return out;
    }
    case Sfx::MagnetWhine:
    {
      // The Magnet Gun pulling: an electric hum swelling, a warbling tone
      // rising through it.
      Osc h, w;
      OnePole lp;
      auto out = render(0.8, [&](double t, double total) {
        const double u = t / total;
        const double hum = lp.lowpass(float(h.step(120.0, Wave::Saw)), 900.0) * 0.5;
        const double tone = w.step(sweep(300.0, 1100.0, u) * (1.0 + 0.03 * std::sin(t * 2.0 * kPi * 9.0)), Wave::Triangle) * 0.35;
        return (hum + tone) * std::min(1.0, t / 0.08) * std::min(1.0, (total - t) / 0.15);
      });
      for (auto& x : out)
        x *= float(0.424);
      return out;
    }
    case Sfx::MagnetPulse:
    {
      // The Magnet Gun's push: a deep thoom, a zap and a crackle.
      Osc th, z;
      Noise n(36053);
      OnePole hp;
      auto out = render(0.5, [&](double t, double total) {
        const double thoom = th.step(sweep(170.0, 48.0, t / 0.25), Wave::Sine) * decay(t, 0.12);
        const double zap = z.step(sweep(1300.0, 280.0, t / 0.12), Wave::Square, 0.4) * decay(t, 0.05) * 0.3;
        const double crackle = n.next() > 0.85f ? hp.highpass(n.next(), 2500.0) * decay(t, 0.12) : 0.0;
        return (thoom + zap + crackle * 0.8) * attack(t, 0.001) * std::min(1.0, (total - t) / 0.05);
      });
      for (auto& x : out)
        x *= float(0.233);
      return out;
    }
    case Sfx::LiftOverhead:
    {
      // Throw Everything: something lifted overhead: a scrape, a heave up.
      Noise n(36055), m(36057);
      Svf bp, up;
      auto out = render(0.4, [&](double t, double total) {
        const double scrape = bp.band(n.next(), 700.0, 2.0) * decay(t, 0.05);
        const double heave = up.band(m.next(), sweep(400.0, 1500.0, t / total), 1.5) * std::sin(t / total * kPi) * 0.6;
        return (scrape + heave) * attack(t);
      });
      for (auto& x : out)
        x *= float(0.909);
      return out;
    }
    case Sfx::ThrowToss:
    {
      // Something thrown: a whoosh, tumbling.
      Noise n(36059);
      Svf bp;
      auto out = render(0.45, [&](double t, double total) {
        const double u = t / total;
        return bp.band(n.next(), sweep(1800.0, 600.0, u), 1.6) * std::sin(u * kPi) * (0.5 + 0.5 * std::abs(std::sin(t * 2.0 * kPi * 7.0)));
      });
      for (auto& x : out)
        x *= float(0.617);
      return out;
    }
    case Sfx::ThrowLand:
    {
      // It lands as a block: a crate's wooden crash, the planks rattling.
      Osc o, c;
      Noise n(36061);
      OnePole lp;
      auto out = render(0.5, [&](double t, double) {
        const double crash = lp.lowpass(n.next(), 3000.0) * decay(t, 0.06) * 1.3;
        const double thud = o.step(sweep(120.0, 70.0, t / 0.2), Wave::Sine) * decay(t, 0.12);
        const double rattle = c.step(440.0, Wave::Triangle) * decay(std::fmod(t, 0.05), 0.01) * decay(t, 0.12) * 0.4;
        return (crash + thud + rattle) * attack(t, 0.001);
      });
      for (auto& x : out)
        x *= float(0.241);
      return out;
    }
    case Sfx::OldTitle:
    {
      // The TURBODUDES crate shot open: the old prototype's title jingle, a
      // chirpy square-wave fanfare over a triangle bass, all 8-bit.
      std::vector<float> out(std::size_t(samples(1.7)));
      static constexpr std::array<std::array<double, 2>, 12> kTune{{{72, 0.0}, {76, 0.1}, {79, 0.2}, {84, 0.3}, {83, 0.45},
        {79, 0.55}, {81, 0.65}, {84, 0.8}, {88, 0.95}, {86, 1.05}, {88, 1.15}, {91, 1.25}}};
      for (std::size_t i = 0; i < kTune.size(); ++i)
      {
        const double len = i + 1 < kTune.size() ? kTune[i + 1][1] - kTune[i][1] : 0.4;
        addTone(out, kTune[i][1], len, midiFreq(kTune[i][0]), Wave::Square, 0.13, 0.4);
      }
      static constexpr std::array<std::array<double, 2>, 4> kBass{{{48, 0.0}, {43, 0.3}, {45, 0.65}, {48, 0.95}}};
      for (const auto& b : kBass)
        addTone(out, b[1], 0.35, midiFreq(b[0]), Wave::Triangle, 0.2, 0.5);
      for (auto& x : out)
        x *= float(1.022);
      return out;
    }
    case Sfx::MastWind:
    {
      // Level 35: the wind round the mast: gusts swelling and dying away,
      // a whistle singing in the lattice on the strongest of them.
      Noise n(35001), m(35003);
      Svf bp, wh;
      OnePole lp;
      return render(3.2, [&](double t, double total) {
        const double u = t / total;
        const double gust = 0.45 + 0.55 * std::pow(std::sin(u * kPi), 1.5) * (0.75 + 0.25 * std::sin(t * 2.0 * kPi * 0.9));
        const double air = bp.band(n.next(), 380.0 + 520.0 * gust, 0.7) * gust;
        const double rumble = lp.lowpass(m.next(), 160.0) * 2.2 * gust;
        const double whistle = wh.band(n.next(), 1500.0 + 500.0 * gust + 80.0 * std::sin(t * 6.0), 12.0) * std::max(0.0, gust - 0.6) * 2.0;
        return (air * 0.8 + rumble * 0.5 + whistle * 0.6) * 0.221 * std::min(1.0, t / 0.6) * std::min(1.0, (total - t) / 0.8);
      });
    }
    case Sfx::HatchClank:
    {
      // Through a hatch: the latch thrown, the steel door's creak, a clank
      // as it shuts behind you.
      Osc latch, creak, clank, ring;
      Noise n(35011);
      Svf bp;
      return render(0.8, [&](double t, double total) {
        const double l = latch.step(1100.0, Wave::Square, 0.3) * decay(t, 0.012) * 0.5 + bp.band(n.next(), 2600.0, 2.0) * decay(t, 0.01);
        const double c = t > 0.08 && t < 0.5 ? creak.step(160.0 + 60.0 * std::sin(t * 30.0), Wave::Saw) * 0.12 * std::sin((t - 0.08) / 0.42 * kPi) : 0.0;
        const double u = t - 0.5;
        const double k = u > 0.0 ? (clank.step(sweep(300.0, 140.0, u / 0.08), Wave::Square, 0.35) * decay(u, 0.03) * 0.6 +
                                     ring.step(830.0, Wave::Sine) * decay(u, 0.12) * 0.3)
                                 : 0.0;
        return (l + c + k) * 0.448 * attack(t) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::ClimberSetOff:
    {
      // A Mast Climber setting off up a ladder: boots ringing on steel
      // rungs, four quick steps.
      std::vector<float> b(std::size_t(samples(0.9)));
      Noise n(35021);
      for (int k = 0; k < 4; ++k)
      {
        const double at = 0.05 + k * 0.19;
        addTone(b, at, 0.25, 520.0 + (k % 2) * 90.0, Wave::Triangle, 0.35, 0.05);
        addTone(b, at, 0.25, 1310.0 + (k % 2) * 140.0, Wave::Sine, 0.18, 0.08);
        for (int i = 0; i < samples(0.03); ++i)
          b[std::size_t(samples(at) + i)] += float(n.next() * decay(double(i) / kRate, 0.006) * 0.5);
      }
      for (auto& x : b)
        x *= float(0.584);
      return b;
    }
    case Sfx::WrenchRaise:
    {
      // A Mast Climber's wrench going up: a ratchet clicking, then a bright
      // steel shing as it catches the light.
      Noise n(35031);
      Svf bp;
      Osc s1, s2;
      return render(0.6, [&](double t, double total) {
        const double tick = std::fmod(t, 0.045);
        const double ratchet = t < 0.3 ? bp.band(n.next(), 3200.0, 3.0) * decay(tick, 0.004) * 1.4 : 0.0;
        const double u = t - 0.28;
        const double shing = u > 0.0 ? (s1.step(3520.0, Wave::Sine) * 0.5 + s2.step(5280.0 * (1.0 + 0.003 * std::sin(u * 40.0)), Wave::Sine) * 0.3) * decay(u, 0.12) : 0.0;
        return (ratchet + shing) * 0.205 * attack(t) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::WrenchSwing:
    {
      // The wrench swung: a heavy swish and a dull clang at the end.
      Noise n(35041);
      Svf bp;
      Osc c1, c2;
      return render(0.5, [&](double t, double total) {
        const double sw = bp.band(n.next(), sweep(400.0, 1800.0, std::min(1.0, t / 0.16)), 1.2) * std::sin(std::min(1.0, t / 0.18) * kPi);
        const double u = t - 0.15;
        const double clang = u > 0.0 ? (c1.step(310.0, Wave::Triangle) * 0.6 + c2.step(797.0, Wave::Sine) * 0.4) * decay(u, 0.09) : 0.0;
        return (sw * 0.9 + clang) * 0.412 * attack(t) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::RotorWhine:
    {
      // A Relay Drone coming round the mast: its rotors' whine rising out of
      // the wind, the blades' chop under it.
      Osc a, b;
      Noise n(35051);
      Svf bp;
      return render(2.0, [&](double t, double total) {
        const double u = t / total;
        const double f = sweep(420.0, 760.0, u);
        const double whine = a.step(f, Wave::Saw) * 0.25 + b.step(f * 2.01, Wave::Sine) * 0.2;
        const double chop = bp.band(n.next(), 700.0, 1.0) * (0.5 + 0.5 * std::sin(t * 2.0 * kPi * (38.0 + 14.0 * u)));
        return (whine + chop * 0.5) * 0.749 * (0.15 + 0.85 * u * u) * std::min(1.0, (total - t) / 0.1);
      });
    }
    case Sfx::DroneCharge:
    {
      // A Relay Drone's muzzle charging: two quick beeps over a rising
      // whine.
      Osc w;
      std::vector<float> b = render(0.65, [&](double t, double total) {
        return w.step(sweep(600.0, 2400.0, t / total), Wave::Sine) * 0.18 * (t / total) * std::min(1.0, (total - t) / 0.03);
      });
      addTone(b, 0.0, 0.08, 1760.0, Wave::Square, 0.18, 0.1);
      addTone(b, 0.16, 0.08, 1760.0, Wave::Square, 0.18, 0.1);
      for (auto& x : b)
        x *= float(1.852);
      return b;
    }
    case Sfx::DroneShot:
    {
      // A Relay Drone's shot: a buzzy zap dropping in pitch.
      Osc a, b;
      Noise n(35061);
      OnePole hp;
      return render(0.3, [&](double t, double total) {
        const double f = sweep(1800.0, 300.0, t / 0.22);
        const double zap = a.step(f, Wave::Square, 0.3) * 0.5 + b.step(f * 1.5, Wave::Saw) * 0.3;
        const double fizz = hp.highpass(n.next(), 4000.0) * decay(t, 0.05) * 0.5;
        return (zap * decay(t, 0.09) + fizz) * 0.356 * attack(t) * std::min(1.0, (total - t) / 0.04);
      });
    }
    case Sfx::NodeHum:
    {
      // A Feedback Node charging: a mains hum swelling, a howl of feedback
      // creeping up through it.
      Osc h1, h2, howl;
      return render(1.5, [&](double t, double total) {
        const double u = t / total;
        const double hum = h1.step(110.0, Wave::Saw) * 0.25 + h2.step(220.0, Wave::Sine) * 0.2;
        const double fb = howl.step(sweep(900.0, 2200.0, u) * (1.0 + 0.01 * std::sin(t * 2.0 * kPi * 6.0)), Wave::Sine) * u * u * 0.4;
        return (hum * (0.4 + 0.6 * u) + fb) * 0.538 * std::min(1.0, t / 0.1) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::NodePulse:
    {
      // A Feedback Node's ring going out: a feedback shriek cut short, a
      // whump of air pushing out.
      Osc s, o;
      Noise n(35071);
      Svf bp;
      return render(0.9, [&](double t, double total) {
        const double shriek = s.step(2600.0 + 300.0 * std::sin(t * 90.0), Wave::Square, 0.4) * decay(t, 0.07) * 0.35;
        const double whump = o.step(sweep(110.0, 40.0, t / 0.4), Wave::Sine) * decay(t, 0.18);
        const double air = bp.band(n.next(), sweep(2000.0, 300.0, t / 0.6), 0.7) * decay(t, 0.25);
        return (shriek + whump * 0.9 + air * 0.8) * 0.296 * attack(t) * std::min(1.0, (total - t) / 0.1);
      });
    }
    case Sfx::SwarmVolley:
    {
      // The Satellite Swarm firing: three quick rising chirps, one per drone.
      std::vector<float> b(std::size_t(samples(0.3)));
      for (int k = 0; k < 3; ++k)
      {
        Osc o;
        const int s0 = samples(0.04 * k);
        for (int i = 0; i < samples(0.12) && s0 + i < int(b.size()); ++i)
        {
          const double t = double(i) / kRate;
          b[std::size_t(s0 + i)] += float(o.step(sweep(1400.0 + 300.0 * k, 3200.0 + 400.0 * k, t / 0.08), Wave::Triangle) *
            decay(t, 0.035) * attack(t, 0.002) * 0.6);
        }
      }
      for (auto& x : b)
        x *= float(0.48);
      return b;
    }
    case Sfx::SwarmBlock:
    {
      // A swarm drone soaking up a shot: a crackling zap, then its power
      // winding down.
      Noise n(35081);
      Svf bp;
      Osc d;
      return render(0.7, [&](double t, double total) {
        const double zap = bp.band(n.next(), 3000.0, 1.5) * decay(t, 0.04) * 1.2;
        const double down = d.step(sweep(1200.0, 120.0, t / total), Wave::Square, 0.3) * 0.25 * (1.0 - t / total);
        return (zap + down) * 0.329 * attack(t) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::AirshipDrone:
    {
      // The cargo airship: big engines droning, slightly out of tune with
      // each other, propellers beating through it.
      Osc e1, e2, e3;
      Noise n(35091);
      OnePole lp;
      return render(3.0, [&](double t, double total) {
        const double beat = 0.6 + 0.4 * std::sin(t * 2.0 * kPi * 7.5);
        const double eng = e1.step(55.0, Wave::Saw) * 0.35 + e2.step(56.3, Wave::Saw) * 0.3 + e3.step(82.5, Wave::Square, 0.4) * 0.12;
        const double prop = lp.lowpass(n.next(), 400.0) * 2.0 * beat;
        return (eng * 0.6 * beat + prop * 0.4) * 0.701 * std::min(1.0, t / 0.8) *
          std::min(1.0, (total - t) / 0.8);
      });
    }
    case Sfx::HypeSting:
    {
      // A Halcyon holo banner: a bright, cheesy corporate fanfare sting and
      // a shimmer of hologram.
      std::vector<float> b(std::size_t(samples(1.0)));
      static constexpr std::array<std::array<double, 2>, 4> kNotes{{{72, 0.0}, {76, 0.09}, {79, 0.18}, {84, 0.3}}};
      for (const auto& nt : kNotes)
      {
        addTone(b, nt[1], 0.65 - nt[1], midiFreq(nt[0]), Wave::Square, 0.09, 0.3);
        addTone(b, nt[1], 0.65 - nt[1], midiFreq(nt[0] + 12), Wave::Sine, 0.08, 0.25);
      }
      Osc sh;
      for (int i = samples(0.3); i < int(b.size()); ++i)
      {
        const double t = double(i) / kRate - 0.3;
        b[std::size_t(i)] += float(sh.step(3000.0 + 800.0 * std::sin(t * 50.0), Wave::Sine) * 0.06 * decay(t, 0.25));
      }
      for (auto& x : b)
        x *= float(0.939);
      return b;
    }
    case Sfx::TitanStomp:
    {
      // The Titan's stomp: a huge metal boom, the deck ringing, the
      // shockwave rushing off along it.
      Osc o, r1, r2;
      Noise n(35101), m(35103);
      OnePole lp;
      Svf bp;
      return render(1.4, [&](double t, double total) {
        const double boom = o.step(sweep(90.0, 30.0, t / 0.5), Wave::Sine) * decay(t, 0.25);
        const double hit = lp.lowpass(n.next(), sweep(5000.0, 300.0, t / 0.2)) * decay(t, 0.06) * 1.5;
        const double ring = (r1.step(180.0, Wave::Triangle) * 0.5 + r2.step(463.0, Wave::Sine) * 0.3) * decay(t, 0.35) * 0.4;
        const double rush = bp.band(m.next(), sweep(300.0, 2400.0, t / total), 0.8) * std::sin(std::min(1.0, t / total) * kPi) * 0.6;
        return (boom + hit + ring + rush) * 0.222 * attack(t, 0.002) * std::min(1.0, (total - t) / 0.2);
      });
    }
    case Sfx::TitanLaser:
    {
      // Its laser sweep: a searing beam, a high whine with a growl in it,
      // crackling.
      Osc a, b, c;
      Noise n(35111);
      OnePole hp;
      return render(0.9, [&](double t, double total) {
        const double beam = a.step(1400.0 + 40.0 * std::sin(t * 70.0), Wave::Saw) * 0.25 + b.step(70.0, Wave::Square, 0.5) * 0.2 +
          c.step(2800.0, Wave::Sine) * 0.15;
        const double crack = hp.highpass(n.next(), 5000.0) * (n.next() > 0.6f ? 1.0 : 0.2) * 0.4;
        return (beam + crack) * 0.495 * attack(t, 0.01) * std::min(1.0, (total - t) / 0.2);
      });
    }
    case Sfx::TitanFlame:
    {
      // Its flamethrower: a gas whoosh lighting into a roar, crackling.
      Noise n(35121), m(35123);
      Svf bp;
      OnePole lp;
      return render(3.0, [&](double t, double total) {
        const double whoosh = t < 0.3 ? bp.band(n.next(), sweep(300.0, 1600.0, t / 0.3), 0.8) * (1.0 - t / 0.3) : 0.0;
        const double roar = lp.lowpass(n.next(), 700.0 + 300.0 * std::sin(t * 2.0 * kPi * 3.0)) * 2.0;
        const double crackle = (m.next() > 0.9f ? 1.0 : 0.0) * 0.6;
        return (whoosh + roar * std::min(1.0, t / 0.2) + crackle) * 0.261 * std::min(1.0, (total - t) / 0.4);
      });
    }
    case Sfx::MissileLock:
    {
      // Crosshairs locking on: beeps quickening into a steady tone.
      std::vector<float> b(std::size_t(samples(1.4)));
      double at = 0.0, gap = 0.22;
      while (at < 1.05)
      {
        addTone(b, at, 0.06, 2093.0, Wave::Square, 0.15, 0.2);
        at += gap;
        gap = std::max(0.07, gap * 0.78);
      }
      addTone(b, 1.05, 0.33, 2093.0, Wave::Square, 0.15, 1.0);
      for (auto& x : b)
        x *= float(2.685);
      return b;
    }
    case Sfx::MissileRain:
    {
      // The missiles coming down: whistles falling, then booms.
      std::vector<float> b(std::size_t(samples(1.6)));
      for (int k = 0; k < 4; ++k)
      {
        Osc w;
        const double s0 = 0.03 * k;
        for (int i = 0; i < samples(0.3); ++i)
        {
          const double t = double(i) / kRate;
          b[std::size_t(samples(s0) + i)] += float(w.step(sweep(2200.0 - k * 150.0, 700.0, t / 0.3), Wave::Sine) * 0.12 * attack(t, 0.05));
        }
      }
      Noise n(35131);
      OnePole lp;
      Osc o;
      for (int i = samples(0.33); i < int(b.size()); ++i)
      {
        const double t = double(i) / kRate - 0.33;
        b[std::size_t(i)] += float((lp.lowpass(n.next(), sweep(4000.0, 200.0, t / 0.6)) * 1.4 + o.step(sweep(80.0, 30.0, t / 0.6), Wave::Sine) * 0.7) *
          decay(t, 0.3) * attack(t, 0.002));
      }
      for (auto& x : b)
        x *= float(0.29);
      return b;
    }
    case Sfx::CoreOpen:
    {
      // Its power core opening: a pneumatic hiss as the shutter slides, a
      // humming rise behind it.
      Noise n(35141);
      OnePole hp;
      Osc h;
      return render(0.8, [&](double t, double total) {
        const double hiss = hp.highpass(n.next(), 3500.0) * decay(t, 0.15) * 0.8;
        const double hum = h.step(sweep(150.0, 300.0, std::min(1.0, t / 0.4)), Wave::Triangle) * 0.35 * std::min(1.0, t / 0.2);
        return (hiss + hum) * 0.544 * attack(t) * std::min(1.0, (total - t) / 0.2);
      });
    }
    case Sfx::LatticeShot:
    {
      // A lattice shot coming round the mast: a ringing metallic zing, the
      // girders singing as it passes.
      Osc a, b;
      Noise n(35151);
      Svf bp;
      return render(0.7, [&](double t, double total) {
        const double z = a.step(sweep(3000.0, 900.0, t / total), Wave::Saw) * 0.2 * decay(t, 0.3);
        const double sing = b.step(1245.0 * (1.0 + 0.02 * std::sin(t * 2.0 * kPi * 12.0)), Wave::Sine) * 0.4 * decay(t, 0.25);
        const double rattle = bp.band(n.next(), 2000.0, 3.0) * (std::fmod(t, 0.05) < 0.01 ? 1.0 : 0.1) * 0.6;
        return (z + sing + rattle) * 0.379 * attack(t) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::VentHiss:
    {
      // A vent panel about to open: a hiss rising, a latch's clunk.
      Noise n(35161);
      Svf bp;
      Osc k;
      return render(1.0, [&](double t, double total) {
        const double u = t / total;
        const double hiss = bp.band(n.next(), sweep(2500.0, 6000.0, u), 0.9) * std::min(1.0, u * 1.6);
        const double v = t - 0.85;
        const double clunk = v > 0.0 ? k.step(140.0, Wave::Square, 0.4) * decay(v, 0.03) * 0.5 : 0.0;
        return (hiss * 0.8 + clunk) * 0.314 * std::min(1.0, (total - t) / 0.03);
      });
    }
    case Sfx::TitanTopple:
    {
      // The Titan toppling: a long groan of bending metal, then the
      // whistle of something huge falling away.
      Osc g1, g2, w;
      Noise n(35171);
      Svf bp;
      return render(3.2, [&](double t, double total) {
        const double groan = t < 1.8 ? bp.band(float(g1.step(70.0 + 25.0 * std::sin(t * 3.0), Wave::Saw) + n.next() * 0.2),
                                         180.0 + 120.0 * std::sin(t * 1.7), 4.0) * 0.8 +
                                         g2.step(52.0 + 10.0 * std::sin(t * 2.3), Wave::Saw) * 0.15
                                     : 0.0;
        const double u = t - 1.4;
        const double fall = u > 0.0 ? w.step(sweep(1300.0, 260.0, u / 1.8), Wave::Sine) * 0.35 * std::min(1.0, u / 0.2) : 0.0;
        return (groan * std::min(1.0, std::max(0.0, (1.8 - t) / 0.3)) + fall) * 0.293 * std::min(1.0, t / 0.2) *
          std::min(1.0, (total - t) / 0.3);
      });
    }
    case Sfx::BrochureJingle:
    {
      // The sales brochure found: a cheesy ad jingle, a page flipping.
      std::vector<float> b(std::size_t(samples(1.3)));
      static constexpr std::array<std::array<double, 2>, 6> kNotes{{{67, 0.0}, {72, 0.12}, {76, 0.24}, {74, 0.36}, {79, 0.5}, {84, 0.68}}};
      for (const auto& nt : kNotes)
      {
        addTone(b, nt[1], 0.5, midiFreq(nt[0]), Wave::Triangle, 0.22, 0.2);
        addTone(b, nt[1], 0.5, midiFreq(nt[0] + 12), Wave::Sine, 0.08, 0.15);
      }
      Noise n(35181);
      OnePole hp;
      for (int i = 0; i < samples(0.1); ++i)
        b[std::size_t(i)] += float(hp.highpass(n.next(), 2500.0) * 0.3 * std::sin(double(i) / samples(0.1) * kPi));
      for (auto& x : b)
        x *= float(0.911);
      return b;
    }
    case Sfx::AlienAnthem:
    {
      // The alien's anthem: four warbling theremin notes, the last held.
      std::vector<float> b(std::size_t(samples(2.4)));
      static constexpr std::array<std::array<double, 3>, 4> kNotes{{{69, 0.0, 0.4}, {74, 0.42, 0.4}, {71, 0.84, 0.4}, {78, 1.26, 1.1}}};
      for (const auto& nt : kNotes)
      {
        Osc o;
        const int s0 = samples(nt[1]);
        for (int i = 0; i < samples(nt[2]) && s0 + i < int(b.size()); ++i)
        {
          const double t = double(i) / kRate;
          const double f = midiFreq(nt[0]) * (1.0 + 0.012 * std::sin(t * 2.0 * kPi * 6.0));
          b[std::size_t(s0 + i)] += float(o.step(f, Wave::Sine) * 0.4 * std::min(1.0, t / 0.05) * std::min(1.0, (nt[2] - t) / 0.08));
        }
      }
      for (auto& x : b)
        x *= float(1.0);
      return b;
    }
    case Sfx::DummyBump:
    {
      // Copycat: a dummy bumping the runner: a rubbery boing.
      Osc o;
      return render(0.4, [&](double t, double total) {
        const double f = 180.0 * (1.0 + 0.5 * std::sin(t * 2.0 * kPi * 14.0) * decay(t, 0.12));
        return o.step(f, Wave::Triangle) * decay(t, 0.12) * 0.356 * attack(t) * std::min(1.0, (total - t) / 0.05);
      });
    }
    case Sfx::DummyDrop:
    {
      // A dummy dropping into a pit: a slide whistle down and a thud.
      Osc w, o;
      Noise n(35191);
      OnePole lp;
      return render(0.8, [&](double t, double total) {
        const double wh = t < 0.5 ? w.step(sweep(1600.0, 300.0, t / 0.5), Wave::Sine) * 0.4 : 0.0;
        const double u = t - 0.5;
        const double thud = u > 0.0 ? (o.step(sweep(120.0, 50.0, u / 0.1), Wave::Sine) + lp.lowpass(n.next(), 800.0) * 1.5) * decay(u, 0.06) : 0.0;
        return (wh + thud) * 0.277 * attack(t) * std::min(1.0, (total - t) / 0.05);
      });
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

MusicTrack makeNamedMusic(const std::string& id)
{
  // "track+fast": the track 1.5 times as fast and a fifth higher (Level 33's
  // Overclock bonus), resampled.
  if (const auto plus = id.rfind("+fast"); plus != std::string::npos && plus + 5 == id.size() && plus > 0)
  {
    MusicTrack t = makeNamedMusic(id.substr(0, plus));
    auto speed = [](std::vector<float>& v) {
      if (v.size() < 2)
        return;
      std::vector<float> out((v.size() * 2) / 3);
      for (std::size_t i = 0; i < out.size(); ++i)
      {
        const std::size_t at = (i * 3) / 2;
        const float f = (i % 2) ? 0.5f : 0.0f;
        out[i] = at + 1 < v.size() ? v[at] + (v[at + 1] - v[at]) * f : v[at];
      }
      v = std::move(out);
    };
    speed(t.left);
    speed(t.right);
    return t;
  }
  if (id.empty() || id == "theme_synthwave")
    return makeMusic(Music::Level);
  if (id == "theme_diner" || id == "menu")
    return makeMusic(Music::Menu);
  if (id == "victory")
    return makeMusic(Music::Victory);
  return makeStyledMusic(id);
}

} // namespace gr::synth
