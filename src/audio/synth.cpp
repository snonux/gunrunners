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
      // A slow wooden groan: a low saw wobbling in pitch, rough with noise.
      Osc a;
      Noise n(81);
      return render(0.6, [&](double t, double total) {
        const double f = 90.0 + 30.0 * std::sin(t * 23.0) + 20.0 * std::sin(t * 7.0);
        return (a.step(f, Wave::Saw) * 0.12 + n.next() * 0.04) * std::min(1.0, t / 0.05) *
          std::min(1.0, (total - t) / 0.1);
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
      // The elevator's bell: two strikes of a small brass bell.
      Osc a, b;
      return render(0.9, [&](double t, double) {
        const double t2 = t < 0.18 ? t : t - 0.18;
        const double env = std::exp(-t2 * 6.0) * std::min(1.0, t2 / 0.003);
        return (a.step(1320.0, Wave::Sine) * 0.18 + b.step(1320.0 * 2.76, Wave::Sine) * 0.07) * env;
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
  if (id.empty() || id == "theme_synthwave")
    return makeMusic(Music::Level);
  if (id == "theme_diner" || id == "menu")
    return makeMusic(Music::Menu);
  if (id == "victory")
    return makeMusic(Music::Victory);
  return makeStyledMusic(id);
}

} // namespace gr::synth
