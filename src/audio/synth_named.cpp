// Cutscene sounds and character voices. The cutscene scripts name about a
// hundred sounds (city_hum, phone_chime_dash, big_explosion...); each one is
// synthesized from the family its name belongs to, seeded by the name, so
// every id gets its own consistent sound without a sample in sight.

#include "audio/dsp.hpp"
#include "audio/synth.hpp"

#include <functional>
#include <string>

namespace gr::synth
{

namespace
{

bool has(const std::string& s, const char* key) { return s.find(key) != std::string::npos; }

std::uint32_t seedOf(const std::string& s)
{
  std::uint32_t h = 2166136261u;
  for (char c : s)
    h = (h ^ std::uint32_t(static_cast<unsigned char>(c))) * 16777619u;
  return h | 1u;
}

std::vector<float> noiseBed(double len, std::uint32_t seed, double lowCut, double highCut, double vol, double fadeIn)
{
  Noise n(seed);
  OnePole lp, hp;
  return render(len, [&](double t, double total) {
    const double env = std::min(1.0, t / std::max(0.01, fadeIn)) * std::min(1.0, (total - t) / 0.3);
    return hp.highpass(lp.lowpass(n.next(), highCut), lowCut) * vol * env;
  });
}

std::vector<float> boom(double len, std::uint32_t seed, double vol)
{
  Noise n(seed);
  Svf lp;
  Osc o;
  double held = 0.0;
  int counter = 0;
  return render(len, [&](double t, double total) {
    if (counter-- <= 0)
    {
      held = n.next();
      counter = int(lerp(2.0, 16.0, t / total));
    }
    const double rumble = lp.low(float(held * 0.6 + n.next() * 0.4), sweep(2600.0, 120.0, t / total), 1.1);
    const double thump = o.step(sweep(70.0, 28.0, t / 0.5), Wave::Sine) * decay(t, 0.18);
    return (rumble * 0.7 + thump * 0.7) * attack(t, 0.002) * decay(t, len * 0.3) * vol;
  });
}

std::vector<float> bell(double freq, double len, double vol)
{
  std::vector<float> out(std::size_t(samples(len)), 0.0f);
  addTone(out, 0.0, len, freq, Wave::Sine, vol, len * 0.35);
  addTone(out, 0.0, len, freq * 2.76, Wave::Sine, vol * 0.4, len * 0.15);
  addTone(out, 0.0, len, freq * 5.4, Wave::Sine, vol * 0.2, len * 0.08);
  return out;
}

std::vector<float> hum(double freq, double len, double vol, std::uint32_t seed)
{
  Osc a, b, lfo;
  Noise n(seed);
  OnePole lp;
  return render(len, [&](double t, double total) {
    const double env = std::min(1.0, t / 0.3) * std::min(1.0, (total - t) / 0.3);
    const double v = a.step(freq, Wave::Saw) * 0.4 + b.step(freq * 2.003, Wave::Triangle) * 0.3 +
      lp.lowpass(n.next(), 400.0) * 0.4;
    return lp.lowpass(float(v), 900.0) * vol * env * (0.85 + 0.15 * lfo.step(0.7, Wave::Sine));
  });
}

std::vector<float> click(double freq, double len, double vol, std::uint32_t seed)
{
  Noise n(seed);
  Osc o;
  return render(len, [&](double t, double) {
    return (n.next() * decay(t, 0.004) * 0.6 + o.step(freq, Wave::Square, 0.3) * decay(t, len * 0.3) * 0.4) * vol;
  });
}

std::vector<float> whoosh(double len, std::uint32_t seed, double vol, double from, double to)
{
  Noise n(seed);
  Svf bp;
  return render(len, [&](double t, double total) {
    const double env = std::sin(kPi * std::clamp(t / total, 0.0, 1.0));
    return bp.band(n.next(), sweep(from, to, t / total), 1.6) * env * vol;
  });
}

std::vector<float> crowd(double len, std::uint32_t seed, double vol, bool cheer)
{
  Noise n(seed), m(seed * 3u);
  Svf bp1, bp2;
  Osc lfo;
  return render(len, [&](double t, double total) {
    const double env = std::min(1.0, t / (cheer ? 0.15 : 0.6)) * std::min(1.0, (total - t) / 0.5);
    const double babble = bp1.band(n.next(), 600.0 + 250.0 * lfo.step(3.1, Wave::Sine), 1.3) +
      bp2.band(m.next(), cheer ? 1800.0 : 1100.0, 2.0) * 0.6;
    return babble * env * vol;
  });
}

std::vector<float> chime(const std::vector<double>& notes, double step, double vol, Wave w)
{
  std::vector<float> out(std::size_t(samples(step * double(notes.size()) + 0.6)), 0.0f);
  for (std::size_t i = 0; i < notes.size(); ++i)
    addTone(out, step * double(i), 0.6, midiFreq(notes[i]), w, vol, 0.22);
  return out;
}

std::vector<float> rain(double len, std::uint32_t seed, double vol)
{
  Noise n(seed), drops(seed + 7u);
  OnePole lp, hp;
  return render(len, [&](double t, double total) {
    const double env = std::min(1.0, t / 0.4) * std::min(1.0, (total - t) / 0.4);
    const double hiss = hp.highpass(lp.lowpass(n.next(), 5000.0), 800.0) * 0.5;
    const double drip = drops.next() > 0.995f ? 0.8 : 0.0;
    return (hiss + drip) * env * vol;
  });
}

} // namespace

std::vector<float> makeNamedSfx(const std::string& id)
{
  const std::uint32_t seed = seedOf(id);
  const bool loop = has(id, "loop");
  const double len = loop ? 6.0 : 1.0;
  if (id == "rain")
  {
    // Storm Relay (brief_31): rain driving in on the wind at the foot of the
    // cliff: a dense hiss with big drops spattering, the wind gusting, and
    // the surf breaking on the rocks below in slow swells.
    Noise n(seed), drops(seed + 7u), w(seed + 13u), sea(seed + 29u);
    OnePole lp, hp, slp;
    Svf wind, surf;
    return render(6.0, [&](double t, double total) {
      const double env = std::min(1.0, t / 0.5) * std::min(1.0, (total - t) / 0.6);
      const double hiss = hp.highpass(lp.lowpass(n.next(), 6000.0), 900.0) * 0.4;
      const double drop = slp.lowpass(drops.next() > 0.993f ? 1.0f : 0.0f, 3000.0) * 1.6;
      const double gust = 0.55 + 0.45 * std::sin(t * 1.3 + std::sin(t * 0.7) * 1.5);
      const double wd = wind.band(w.next(), 380.0 + 260.0 * gust, 2.0) * 0.35 * gust;
      const double swell = 0.5 + 0.5 * std::sin(t * 2.0 * 3.14159265 / 3.2);
      const double sf = surf.low(sea.next(), 500.0 + 900.0 * swell, 0.8) * 0.5 * swell * swell;
      return (hiss + drop + wd + sf) * env * 0.42;
    });
  }
  if (id == "sonar_ping")
  {
    // Fiber Trench (brief_30): a submarine's sonar ping, a pure high tone
    // struck hard, ringing out, then its echo coming back off the sea floor
    // fainter and duller, over a low swell of the deep.
    Osc a, b, e, deep;
    Svf lp;
    Noise n(seed);
    return render(2.6, [&](double t, double total) {
      const double ping = (a.step(1480.0, Wave::Sine) + b.step(2960.0, Wave::Sine) * 0.08) * attack(t, 0.004) * decay(t, 0.35) * 0.3;
      const double u = t - 0.95;
      const double echo = u > 0.0 ? e.step(1476.0, Wave::Sine) * attack(u, 0.03) * decay(u, 0.4) * 0.09 : 0.0;
      const double swell = lp.low(n.next(), 180.0, 0.8) * 0.18 + deep.step(55.0, Wave::Sine) * 0.03;
      return (ping + echo + swell * std::min(1.0, t / 0.4)) * std::min(1.0, (total - t) / 0.5);
    });
  }
  if (id == "bubbles")
  {
    // Fiber Trench (brief_30): the runners' breath bubbling up past the
    // camera: bursts of little rising bloops, thick at first and thinning,
    // over the muffled wash of the water.
    Noise n(seed), m(seed + 7u);
    Svf lp;
    Osc o;
    double fr = 0.0, age = 1.0, life = 0.05;
    return render(3.0, [&](double t, double total) {
      const double perSecond = 26.0 * (1.0 - 0.6 * std::min(1.0, t / total));
      if (age >= life && m.next() * 0.5 + 0.5 < perSecond / kRate)
      {
        fr = 380.0 + 700.0 * (m.next() * 0.5 + 0.5);
        life = 0.03 + 0.04 * (m.next() * 0.5 + 0.5);
        age = 0.0;
      }
      double bloop = 0.0;
      if (age < life)
      {
        const double k = age / life;
        bloop = o.step(fr * (1.0 + 1.6 * k), Wave::Sine) * (1.0 - k) * std::min(1.0, k / 0.1) * 0.24;
        age += 1.0 / kRate;
      }
      const double wash = lp.low(n.next(), 400.0, 0.7) * 0.2;
      return (bloop + wash) * std::min(1.0, t / 0.08) * std::min(1.0, (total - t) / 0.6);
    });
  }
  // The end of Episode 3: the back wall cracking from end to end and
  // landing, the studio's lights and its audience, the TV in the dark and
  // ZERO's servers humming.
  if (id == "crack_long")
  {
    // A long rending crack: splintering snaps that come faster, over a
    // groaning creak.
    Noise n(seed), m(seed + 3u);
    Svf bp, creak;
    Osc o;
    return render(2.6, [&](double t, double total) {
      const double u = t / total;
      const double rate = lerp(0.985, 0.94, u);
      const double snap = n.next() > rate ? bp.band(m.next(), 1800.0 + 1600.0 * u, 2.0) * 3.0 : 0.0;
      const double f = 90.0 + 40.0 * std::sin(t * 5.0) + 60.0 * u;
      const double groan = creak.band(float(o.step(f, Wave::Saw)), f * 2.5, 4.0) * 0.5;
      return (snap * 0.35 + groan * 0.4) * std::min(1.0, t / 0.2) * std::min(1.0, (total - t) / 0.25);
    });
  }
  if (id == "big_thud")
  {
    // The flat landing: a flat slap and a deep boom of the stage.
    Noise n(seed);
    OnePole lp;
    Osc o;
    return render(1.8, [&](double t, double) {
      const double slap = lp.lowpass(n.next(), sweep(6000.0, 250.0, t / 0.35)) * decay(t, 0.1);
      const double b = o.step(sweep(65.0, 26.0, t / 0.7), Wave::Sine) * decay(t, 0.45);
      return (slap * 1.1 + b * 0.9) * 0.6 * attack(t, 0.001);
    });
  }
  // Level 34's briefing: the fleet's engines at sunset.
  if (id == "propellers")
    return makeSfx(Sfx::EngineDrone);
  // Level 32's briefing: a big dish groaning round.
  if (id == "dish_groan")
    return makeSfx(Sfx::DishGroan);
  // Level 33's briefing: the lava welling up, a coolant vent bursting.
  if (id == "lava_rumble")
    return makeSfx(Sfx::LavaRumble);
  if (id == "steam_burst")
    return makeSfx(Sfx::SteamBurst);
  // Level 37's briefing: a changing booth's curtain.
  if (id == "curtain_swish")
    return makeSfx(Sfx::CurtainSwish);
  // Level 38's briefing: the writers' room through its door, a typewriter
  // going at it (it loops, see Audio::playNamed): keys in bursts, the space
  // bar's deeper thunk between words, a low room tone. Every key rings out
  // inside the 3 s, so the loop joins without a seam. Then the carriage
  // bell at the end of the line.
  if (id == "typewriter_loop")
  {
    std::vector<float> out(std::size_t(samples(3.0)), 0.0f);
    Noise pick(seed);
    double at = 0.05;
    int letters = 0;
    std::uint32_t k = 0;
    while (at < 2.8)
    {
      const bool space = letters > 2 && pick.next() > 0.45f;
      const double vol = 0.75 + 0.25 * double(pick.next());
      const double bodyF = space ? 120.0 : 170.0 + 40.0 * double(pick.next());
      Noise n(seed + 101u * ++k), m(seed + 211u * k);
      Svf click, slap;
      OnePole lp;
      Osc body;
      const int s0 = samples(at);
      for (int i = 0; i < samples(0.12) && s0 + i < int(out.size()); ++i)
      {
        const double t = double(i) / kRate;
        const double c = click.band(n.next(), space ? 1800.0 : 3200.0, 3.0) * decay(t, 0.004) * 1.4;
        const double u = t - 0.026;
        const double sl = u > 0.0
          ? (slap.band(m.next(), 1700.0, 1.5) * 1.3 + lp.lowpass(m.next(), 500.0) * 1.5 + body.step(bodyF, Wave::Triangle) * 0.5) *
              decay(u, space ? 0.03 : 0.016)
          : 0.0;
        out[std::size_t(s0 + i)] += float((c + sl) * vol * 0.22);
      }
      if (space)
      {
        letters = 0;
        at += 0.2 + 0.1 * double(pick.next());
      }
      else
      {
        ++letters;
        at += 0.1 + 0.035 * double(pick.next());
      }
    }
    Noise r(seed + 11u);
    OnePole room;
    Osc hum;
    for (auto& x : out)
      x += float(room.lowpass(r.next(), 400.0) * 0.05 + hum.step(60.0, Wave::Sine) * 0.012);
    return out;
  }
  if (id == "bell_ding")
    return makeSfx(Sfx::CarriageBell);
  // Level 39's briefing: the security desk's walkies over the control
  // room's hum: a squelch, a garbled voice in bursts, the squelch tail;
  // twice, the second voice answering the first.
  if (id == "radio_chatter")
  {
    Noise n(seed), m(seed + 3u);
    Svf voice, f1, f2, hiss;
    OnePole room;
    Osc buzz, hum, syl;
    std::vector<float> out = render(3.6, [&](double t, double total) {
      // Two transmissions: 0.15-1.5 s and 1.9-3.3 s.
      const bool second = t >= 1.75;
      const double t0 = second ? 1.9 : 0.15, t1 = second ? 3.3 : 1.5;
      double v = 0.0;
      if (t >= t0 - 0.06 && t < t1 + 0.12)
      {
        const double u = t - t0;
        const double sq = (u < 0.0 ? 1.0 : t >= t1 ? 1.0 : 0.0) * hiss.band(n.next(), 3400.0, 0.8) * 0.9;
        double talk = 0.0;
        if (u >= 0.0 && t < t1)
        {
          // Syllables: a buzzy voice (a low saw) through two moving
          // formants, gated on and off, the pitch wandering.
          const double rate = second ? 6.2 : 7.4;
          const double ph = std::fmod(u * rate, 1.0);
          const double gate = ph < 0.7 ? std::sin(kPi * ph / 0.7) : 0.0;
          const double pitch = (second ? 105.0 : 140.0) * (1.0 + 0.12 * std::sin(u * 2.0 * kPi * 1.3) + 0.05 * syl.step(rate * 0.5, Wave::Sine));
          const double src = buzz.step(pitch, Wave::Saw);
          const double a = 500.0 + 350.0 * std::sin(u * 2.0 * kPi * rate * 0.37), b = 1500.0 + 700.0 * std::sin(u * 2.0 * kPi * rate * 0.23 + 1.0);
          talk = (f1.band(float(src), a, 4.0) + f2.band(float(src), b, 5.0) * 0.7) * gate * (u < 0.05 ? u / 0.05 : 1.0);
          talk = voice.band(float(talk + m.next() * 0.15), 1300.0, 0.6) * 2.2; // the walkie's tiny speaker
        }
        v = sq + talk;
      }
      const double bed = room.lowpass(m.next(), 300.0) * 0.08 + hum.step(50.0, Wave::Sine) * 0.02;
      return (v + bed) * std::min(1.0, t / 0.05) * std::min(1.0, (total - t) / 0.2);
    });
    float peak = 0.0f;
    for (const float x : out)
      peak = std::max(peak, std::abs(x));
    if (peak > 0.0f)
      for (float& x : out)
        x *= 0.4f / peak;
    return out;
  }
  // Maxine grabbed: feet scuffing on the studio floor, grunts, a thump
  // against the desk, her headset clattering down.
  if (id == "scuffle")
  {
    std::vector<float> out(std::size_t(samples(1.7)), 0.0f);
    Noise pick(seed);
    auto add = [&](double at, const std::vector<float>& v, double vol) {
      const int s0 = samples(at);
      for (std::size_t i = 0; i < v.size() && s0 + int(i) < int(out.size()); ++i)
        out[std::size_t(s0) + i] += float(v[i] * vol);
    };
    // Feet: short scrapes of shoe on floor.
    for (int k = 0; k < 7; ++k)
    {
      Noise n(seed + 31u * std::uint32_t(k + 1));
      Svf bp;
      const double len = 0.08 + 0.08 * double(pick.next() * 0.5f + 0.5f);
      const double f = 900.0 + 900.0 * double(pick.next() * 0.5f + 0.5f);
      add(0.02 + 0.2 * double(k) + 0.06 * double(pick.next()), render(len, [&](double t, double total) {
        return bp.band(n.next(), f, 1.2) * std::sin(kPi * t / total);
      }), 0.7);
    }
    // Grunts: a low voice pushed out through a formant, falling.
    for (int k = 0; k < 3; ++k)
    {
      Osc o;
      Svf f1, f2;
      Noise breath(seed + 77u * std::uint32_t(k + 1));
      const double p0 = k == 1 ? 240.0 : 120.0 + 20.0 * double(k);
      add(0.15 + 0.45 * double(k), render(0.22, [&](double t, double total) {
        const double src = o.step(sweep(p0, p0 * 0.75, t / total), Wave::Saw) + breath.next() * 0.3;
        const double env = std::min(1.0, t / 0.02) * std::pow(1.0 - t / total, 1.5);
        return (f1.band(float(src), k == 1 ? 800.0 : 550.0, 5.0) + f2.band(float(src), k == 1 ? 1500.0 : 1100.0, 6.0) * 0.6) * env;
      }), 0.9);
    }
    // A body bumping the desk, and the headset hitting the floor.
    {
      Osc o;
      Noise n(seed + 5u);
      OnePole lp;
      add(0.8, render(0.3, [&](double t, double) {
        return o.step(sweep(110.0, 50.0, t / 0.1), Wave::Sine) * decay(t, 0.07) + lp.lowpass(n.next(), 800.0) * decay(t, 0.03) * 2.5;
      }), 1.0);
      Noise c(seed + 9u);
      Svf r1, r2;
      add(1.18, render(0.35, [&](double t, double) {
        const double hit = std::fmod(t, 0.09) < 0.004 ? 1.0 : 0.0;
        return (r1.band(c.next(), 3100.0, 6.0) + r2.band(c.next(), 1900.0, 5.0) * 0.6) * decay(std::fmod(t, 0.09), 0.012) * decay(t, 0.15) * 2.0 +
          hit * 0.2;
      }), 0.8);
    }
    float peak = 0.0f;
    for (const float x : out)
      peak = std::max(peak, std::abs(x));
    if (peak > 0.0f)
      for (float& x : out)
        x *= 0.42f / peak;
    return out;
  }
  // Level 35's briefing: the wind round the mast, the cargo airship.
  if (id == "wind")
    return makeSfx(Sfx::MastWind);
  if (id == "engine_drone")
    return makeSfx(Sfx::AirshipDrone);
  // Level 36's briefing: the loading dock at night, Maxine's headset, the
  // roll-up door.
  if (id == "dock_hum")
  {
    // The dock's work lights: a mains hum with its buzz on top, crickets
    // out on the lot, and a car going past somewhere far off.
    Osc h1, h2, h3, cr;
    Noise n(seed), m(seed + 5u);
    Svf road, buzz;
    return render(3.2, [&](double t, double total) {
      const double hum = h1.step(60.0, Wave::Sine) * 0.5 + h2.step(120.0, Wave::Sine) * 0.3 + h3.step(180.0, Wave::Sine) * 0.12;
      const double bz = buzz.band(float(h3.step(120.0, Wave::Saw)), 2400.0, 3.0) * 0.05;
      const double chirp = std::fmod(t, 0.9) < 0.18 && std::fmod(t * 30.0, 1.0) < 0.5 ? cr.step(4300.0, Wave::Sine) * 0.06 : 0.0;
      const double car = road.low(n.next(), 300.0, 0.7) * 0.25 * std::exp(-std::pow((t - 1.9) / 0.6, 2.0));
      const double air = m.next() * 0.015;
      return (hum * 0.32 + bz + chirp + car + air) * std::min(1.0, t / 0.4) * std::min(1.0, (total - t) / 0.5);
    });
  }
  if (id == "headset_click")
  {
    // The talk button: a sharp plastic click, a burst of squelch, then a
    // little static under it.
    Noise n(seed), m(seed + 9u);
    Svf bp, hiss;
    Osc o;
    return render(0.55, [&](double t, double total) {
      const double click = bp.band(n.next(), 3200.0, 2.5) * decay(t, 0.006) * 2.2 + o.step(1900.0, Wave::Square, 0.5) * decay(t, 0.012) * 0.3;
      const double sq = t > 0.03 && t < 0.2 ? hiss.band(m.next(), 1800.0, 0.8) * 0.5 * (0.6 + 0.4 * std::sin(t * 260.0)) : 0.0;
      const double tail = t >= 0.2 ? hiss.band(m.next(), 2400.0, 0.9) * 0.12 : 0.0;
      return (click + sq + tail) * 0.172 * std::min(1.0, (total - t) / 0.08);
    });
  }
  if (id == "roll_door")
  {
    // The roll-up door going up: its motor whining, the steel slats
    // clattering over the drum, and the clank at the top.
    Osc motor, drum;
    Noise n(seed), m(seed + 13u);
    Svf slat, rumble;
    double age = 1.0;
    return render(2.2, [&](double t, double total) {
      const double run = std::min(1.0, t / 0.25) * (t < 1.75 ? 1.0 : 0.0);
      const double f = 95.0 + 25.0 * std::min(1.0, t / 0.6);
      const double whine = (motor.step(f, Wave::Saw) * 0.25 + drum.step(f * 0.5, Wave::Sine) * 0.4) * run;
      if (run > 0.0 && std::fmod(t, 0.075) < 1.0 / kRate)
        age = 0.0;
      const double tick = age < 0.03 ? slat.band(n.next(), 1400.0 + 400.0 * std::sin(t * 7.0), 3.0) * (1.0 - age / 0.03) * 1.6 : 0.0;
      age += 1.0 / kRate;
      const double rattle = rumble.low(m.next(), 250.0, 0.9) * 0.35 * run;
      const double u = t - 1.78;
      const double clank = u > 0.0 ? (rumble.band(m.next(), 700.0, 4.0) * 2.0 + drum.step(140.0, Wave::Sine)) * decay(u, 0.12) * 0.6 : 0.0;
      return (whine + tick + rattle + clank) * 0.224 * std::min(1.0, (total - t) / 0.15);
    });
  }
  // Level 23's briefing: the storm over the manor.
  if (id == "thunder")
    return makeSfx(Sfx::Thunder);
  if (id == "light_clunk")
    return makeSfx(Sfx::LightClunk);
  if (id == "crowd_cheer")
    return makeSfx(Sfx::Applause);
  // The end of Episode 5: the Titan going over, the runners riding the
  // signal (big_explosion is the boom family's).
  if (id == "metal_groan")
    return makeSfx(Sfx::TitanTopple);
  if (id == "beam_whoosh")
  {
    // A rising rush of air with a shimmering carrier tone climbing in it,
    // the beam taking the runners and streaking away.
    Noise n(seed);
    Svf bp;
    Osc a, b;
    return render(2.4, [&](double t, double total) {
      const double u = t / total;
      const double rush = bp.band(n.next(), sweep(300.0, 3200.0, std::min(1.0, u * 1.4)), 0.8) * std::sin(std::min(1.0, u * 1.1) * kPi);
      const double f = sweep(220.0, 1760.0, std::min(1.0, u * 1.3));
      const double tone = (a.step(f, Wave::Sine) + b.step(f * 1.505, Wave::Triangle) * 0.4) * 0.25 * std::sin(u * kPi);
      return (rush * 0.7 + tone) * 0.58 * std::min(1.0, t / 0.05) * std::min(1.0, (total - t) / 0.4);
    });
  }
  // Level 24's briefing: Lance taps his spoon on the pot (a bright, short
  // ding of a steel spoon on a stockpot's rim, with a little wobble).
  if (id == "spoon_ding")
  {
    std::vector<float> out(std::size_t(samples(1.2)), 0.0f);
    const double f = midiFreq(84.0);
    addTone(out, 0.0, 1.2, f, Wave::Sine, 0.22, 0.6);
    addTone(out, 0.0, 1.2, f * 1.006, Wave::Sine, 0.12, 0.55);
    addTone(out, 0.0, 0.8, f * 2.42, Wave::Sine, 0.1, 0.25);
    addTone(out, 0.0, 0.5, f * 4.1, Wave::Sine, 0.06, 0.09);
    Noise n(seed);
    OnePole hp;
    for (int i = 0; i < samples(0.02); ++i)
      out[std::size_t(i)] += float(hp.highpass(n.next(), 3000.0) * decay(double(i) / kRate, 0.004) * 0.5);
    return out;
  }
  if (id == "tv_on")
  {
    // An old TV coming on: the power thunk, the flyback whine and a
    // fading wash of static.
    Noise n(seed);
    OnePole hp, lp;
    Osc thunk, whine;
    return render(1.4, [&](double t, double total) {
      const double k = thunk.step(sweep(140.0, 60.0, t / 0.05), Wave::Triangle) * decay(t, 0.03);
      const double w = whine.step(7850.0, Wave::Sine) * 0.05 * std::min(1.0, t / 0.2);
      const double st = hp.highpass(lp.lowpass(n.next(), 7000.0), 400.0) * std::min(1.0, t / 0.08) * decay(t, 0.45);
      return (k * 0.6 + w + st * 0.3) * std::min(1.0, (total - t) / 0.2);
    });
  }
  if (id == "tv_static")
  {
    // A dead channel's snow, held for as long as the picture is (it loops,
    // see Audio::playNamed): steady hiss with the set's faint line whine
    // and a slow wash in its loudness. Every part repeats a whole number of
    // times in the 2 s, so the loop joins without a seam.
    Noise n(seed);
    OnePole lp, hp;
    Osc whine, buzz;
    return render(2.0, [&](double t, double total) {
      const double wash = 0.88 + 0.12 * std::sin(2.0 * kPi * t * 1.5);
      const double hiss = hp.highpass(lp.lowpass(n.next(), 8000.0), 250.0);
      const double env = std::min(1.0, t / 0.01) * std::min(1.0, (total - t) / 0.01);
      return (hiss * 0.34 * wash + whine.step(15625.0, Wave::Sine) * 0.012 + buzz.step(60.0, Wave::Saw) * 0.015) * env;
    });
  }
  if (id == "channel_click")
  {
    // A channel knob turning one notch: a hard plastic click and a
    // blip of static as the picture changes.
    Noise n(seed), m(seed + 5u);
    Svf bp;
    OnePole hp;
    return render(0.35, [&](double t, double) {
      const double c = bp.band(n.next(), 2800.0, 3.0) * decay(t, 0.005) * 1.5;
      const double st = t > 0.03 ? hp.highpass(m.next(), 1200.0) * decay(t - 0.03, 0.07) * 0.3 : 0.0;
      return (c + st) * attack(t, 0.0005);
    });
  }
  if (id == "server_hum")
  {
    // A room of servers: fans' airy whoosh, a mains hum and a faint
    // high whine, steady enough to loop.
    Noise n(seed);
    OnePole lp, hp;
    Osc a, b, c;
    return render(6.0, [&](double t, double total) {
      const double fans = hp.highpass(lp.lowpass(n.next(), 3000.0), 200.0) * 0.25;
      const double mains = a.step(60.0, Wave::Sine) * 0.25 + b.step(120.0, Wave::Triangle) * 0.12;
      const double whine = c.step(4400.0, Wave::Sine) * 0.015;
      return (fans + mains + whine) * 0.6 * std::min(1.0, t / 0.4) * std::min(1.0, (total - t) / 0.4);
    });
  }
  // Phones, chimes and dings: little melodies, one per runner.
  if (has(id, "phone_chime"))
  {
    if (has(id, "dash"))
      return chime({76, 79, 84}, 0.09, 0.22, Wave::Square);
    if (has(id, "rocco"))
      return chime({55, 58, 62}, 0.12, 0.25, Wave::Triangle);
    return chime({84, 88, 91, 96}, 0.07, 0.18, Wave::Sine);
  }
  if (has(id, "jingle_break"))
    return chime({72, 76, 79, 84, 79, 84}, 0.11, 0.2, Wave::Square);
  if (has(id, "jingle_back"))
    return chime({84, 79, 76, 79, 84, 88}, 0.1, 0.2, Wave::Square);
  if (has(id, "jazz_sting"))
    return chime({62, 65, 69, 72, 75}, 0.08, 0.2, Wave::Triangle);
  if (has(id, "channel_ident"))
    return chime({67, 72, 76, 79}, 0.12, 0.2, Wave::Sine);
  if (has(id, "intercom") || has(id, "bell") || has(id, "ding") || has(id, "chime"))
    return bell(midiFreq(76.0 + double(seed % 8u)), 1.4, 0.3);
  if (has(id, "sonar") || has(id, "ping"))
    return bell(midiFreq(81.0), 1.8, 0.25);
  if (has(id, "choir"))
    return hum(midiFreq(57.0), 3.5, 0.35, seed);
  // Booms and impacts.
  if (has(id, "explosion") || has(id, "boom"))
    return boom(2.2, seed, 1.0);
  if (has(id, "rumble") || has(id, "thud") || has(id, "falls") || has(id, "bass_thump"))
    return boom(has(id, "thud") || has(id, "thump") ? 0.6 : 2.0, seed, 0.8);
  if (has(id, "crack") || has(id, "clang") || has(id, "groan") || has(id, "grind"))
  {
    const auto b = boom(0.6, seed, 0.4);
    auto c = click(has(id, "clang") ? 900.0 : 300.0, has(id, "groan") || has(id, "grind") ? 1.5 : 0.5, 0.4, seed);
    for (std::size_t i = 0; i < c.size() && i < b.size(); ++i)
      c[i] += b[i];
    return c;
  }
  // Level 19's lab: a run of computer bleeps, a two-tone klaxon blip, the
  // big wall arrow clunking round onto its new heading.
  if (has(id, "lab_bleeps"))
  {
    std::vector<float> out(std::size_t(samples(1.6)), 0.0f);
    Noise n(seed);
    for (int k = 0; k < 9; ++k)
    {
      const double note = 84.0 + std::floor((n.next() + 1.0f) * 6.0) * 2.0;
      addTone(out, 0.05 + 0.16 * k + (k % 3 == 2 ? 0.05 : 0.0), 0.07, midiFreq(note), k % 2 ? Wave::Sine : Wave::Square,
        k % 2 ? 0.14 : 0.06, 0.03);
    }
    return out;
  }
  if (has(id, "klaxon_short"))
  {
    Osc o;
    return render(0.5, [&](double t, double) {
      const double f = t < 0.22 ? 620.0 : 465.0;
      const double seg = std::fmod(t, 0.25);
      return o.step(f, Wave::Square, 0.4) * 0.14 * std::min(1.0, seg / 0.006) * std::min(1.0, std::max(0.0, 0.22 - seg) / 0.02);
    });
  }
  if (has(id, "arrow_clunk"))
  {
    auto c = click(260.0, 0.3, 0.45, seed);
    const auto b = boom(0.35, seed, 0.5);
    for (std::size_t i = 0; i < c.size() && i < b.size(); ++i)
      c[i] += b[i];
    return c;
  }
  // Level 20's core: the hum winding up before a pulse, and the ring going
  // off (the game's own sounds).
  if (has(id, "core_hum_rise"))
    return makeSfx(Sfx::CoreHum);
  if (has(id, "core_pulse"))
    return makeSfx(Sfx::CorePulse);
  // Clicks and mechanisms.
  // Level 42's briefing: the crowd settling before the show, the ON AIR
  // sign's transformer coming up, the house going up when Dash steps out.
  if (id == "crowd_hush")
  {
    // A full house murmuring, a floor manager's long "shhh" and the room
    // settling to an expectant hiss.
    Noise n(seed), m(seed * 3u), s(seed + 5u);
    Svf bp1, bp2, sh;
    Osc lfo;
    return render(2.6, [&](double t, double total) {
      const double settle = 1.0 - 0.8 * std::min(1.0, std::max(0.0, t - 0.6) / 1.2);
      const double babble = (bp1.band(n.next(), 650.0 + 220.0 * lfo.step(2.7, Wave::Sine), 1.4) + bp2.band(m.next(), 1150.0, 2.2) * 0.5) * settle;
      const double u = t - 0.45;
      const double shh = u > 0.0 && u < 1.1 ? sh.band(s.next(), 4200.0, 1.6) * std::sin(kPi * u / 1.1) * 0.7 : 0.0;
      return (babble * 0.45 + shh) * 0.6 * std::min(1.0, t / 0.4) * std::min(1.0, (total - t) / 0.5);
    });
  }
  if (id == "onair_hum")
  {
    // The ON AIR box lighting: a relay's clack, a 50 Hz transformer hum
    // swelling under the red glass, and the neon's high buzz.
    Noise n(seed);
    Svf clk;
    Osc a, b, c, buzz;
    return render(2.0, [&](double t, double total) {
      const double clack = t < 0.04 ? clk.band(n.next(), 2400.0, 3.0) * decay(t, 0.008) * 2.0 : 0.0;
      const double sw = std::min(1.0, t / 0.18);
      const double hum = (a.step(50.0, Wave::Sine) * 0.5 + b.step(100.0, Wave::Saw) * 0.22 + c.step(150.0, Wave::Sine) * 0.14) * sw;
      const double z = buzz.step(2400.0, Wave::Square, 0.5) * 0.02 * sw * (0.7 + 0.3 * std::sin(t * 2.0 * kPi * 7.0));
      return (clack + hum + z) * 0.4 * std::min(1.0, (total - t) / 0.6);
    });
  }
  if (id == "crowd_roar")
  {
    // A stadium on its feet: a wide wash of cheering, whistles darting over
    // it and a stamping beat underneath.
    Noise n(seed), m(seed * 3u), w(seed + 9u);
    Svf bp1, bp2, lo;
    Osc wh1, wh2, lfo;
    return render(3.2, [&](double t, double total) {
      const double env = std::min(1.0, t / 0.12) * std::min(1.0, (total - t) / 0.8);
      const double wash = bp1.band(n.next(), 900.0 + 300.0 * lfo.step(1.3, Wave::Sine), 0.9) + bp2.band(m.next(), 2100.0, 1.6) * 0.7;
      const double w1 = std::fmod(t, 0.9) < 0.45 ? wh1.step(sweep(1800.0, 2900.0, std::fmod(t, 0.9) / 0.45), Wave::Sine) * 0.07 : 0.0;
      const double w2 = std::fmod(t + 0.3, 1.3) < 0.35 ? wh2.step(sweep(3100.0, 2200.0, std::fmod(t + 0.3, 1.3) / 0.35), Wave::Sine) * 0.06 : 0.0;
      const double ph = std::fmod(t, 0.42);
      const double stamp = lo.low(w.next(), 160.0, 0.9) * decay(ph, 0.05) * 1.4;
      return (wash * 0.42 + w1 + w2 + stamp) * env;
    });
  }
  // The finale: the master switch thrown, the ON AIR sign dying, the
  // network's feed falling to snow, an empty studio.
  if (id == "switch_big")
  {
    // A heavy knife switch: the handle's iron creak, the blades leaving the
    // jaws in a snap of arc, and the clunk of the handle hitting its stop.
    Noise n(seed), m(seed + 3u);
    Svf crk, arc, body;
    Osc thud;
    return render(1.3, [&](double t, double total) {
      const double creak = t < 0.3 ? crk.band(n.next(), 520.0 + 180.0 * std::sin(t * 40.0), 6.0) * std::sin(kPi * t / 0.3) * 0.5 : 0.0;
      const double u = t - 0.3;
      const double spark = u > 0.0 ? arc.band(m.next(), 5200.0, 0.8) * decay(u, 0.06) * (m.next() > 0.3f ? 1.2 : 0.4) : 0.0;
      const double v = t - 0.36;
      const double clunk = v > 0.0 ? (thud.step(sweep(110.0, 55.0, std::min(1.0, v / 0.2)), Wave::Sine) * 1.1 + body.band(n.next(), 380.0, 3.0) * 0.8) * decay(v, 0.16) : 0.0;
      return (creak + spark + clunk) * 0.42 * std::min(1.0, (total - t) / 0.2);
    });
  }
  if (id == "neon_fizz")
  {
    // A neon tube dying: its buzz stuttering, a couple of crackles and a
    // falling whine as the gas goes out.
    Noise n(seed);
    Svf cr;
    Osc buzz, whine;
    return render(1.4, [&](double t, double total) {
      const double flick = std::sin(t * 2.0 * kPi * 9.0) > -0.2 + 1.2 * std::min(1.0, t / 0.9) ? 1.0 : 0.0;
      const double b = buzz.step(120.0, Wave::Saw) * 0.35 * flick * (1.0 - std::min(1.0, t / 1.0));
      const double c = cr.band(n.next(), 3400.0, 1.2) * (n.next() > 0.82f ? 1.0 : 0.08) * decay(t, 0.5);
      const double wh = whine.step(sweep(9000.0, 2000.0, std::min(1.0, t / 1.1)), Wave::Sine) * 0.05 * decay(t, 0.6);
      return (b + c * 0.6 + wh) * 0.7 * std::min(1.0, (total - t) / 0.2);
    });
  }
  if (id == "static_burst")
  {
    // Every monitor dropping to snow at once: a thump on the line, a
    // blast of bright static and the dead channel's hiss settling.
    Noise n(seed);
    OnePole hp, lp;
    Osc thump;
    return render(1.8, [&](double t, double total) {
      const double k = thump.step(sweep(90.0, 40.0, std::min(1.0, t / 0.1)), Wave::Sine) * decay(t, 0.08);
      const double st = hp.highpass(lp.lowpass(n.next(), 9000.0), 350.0) * (0.35 + 0.65 * decay(t, 0.25));
      return (k * 0.7 + st * 0.6) * 0.6 * std::min(1.0, t / 0.005) * std::min(1.0, (total - t) / 0.4);
    });
  }
  if (id == "room_tone")
  {
    // An empty room: a low air-handling rumble, a fluorescent's faint hum
    // and nothing else. Loops.
    Noise n(seed);
    OnePole lp;
    Osc hum;
    return render(4.0, [&](double t, double total) {
      const double air = lp.lowpass(n.next(), 220.0) * 0.6;
      const double h = hum.step(120.0, Wave::Sine) * 0.02;
      return (air + h) * 0.5 * std::min(1.0, t / 0.5) * std::min(1.0, (total - t) / 0.5);
    });
  }
  if (has(id, "click") || has(id, "clunk") || has(id, "clamp") || has(id, "switch") || has(id, "plate") ||
      has(id, "arrow") || has(id, "trapdoor") || has(id, "door") || has(id, "slam") || has(id, "bump"))
    return click(has(id, "slam") ? 120.0 : 500.0 + double(seed % 400u), has(id, "slam") ? 0.5 : 0.25, 0.5, seed);
  // Air: wind, whooshes, steam, hiss.
  if (has(id, "wind"))
    return whoosh(3.0, seed, has(id, "high") ? 0.5 : 0.4, 300.0, 1400.0);
  if (has(id, "whoosh") || has(id, "swish") || has(id, "beam"))
    return whoosh(0.7, seed, 0.5, 400.0, 3000.0);
  // Level 18: the airlock venting, a long pressurised hiss (it loops).
  if (has(id, "airlock_hiss"))
  {
    Noise n(seed);
    OnePole lp, hp;
    Osc lfo;
    return render(2.4, [&](double t, double total) {
      const double flutter = 0.85 + 0.15 * lfo.step(7.0, Wave::Sine);
      const double hiss = hp.highpass(lp.lowpass(n.next(), 7000.0), 1500.0);
      // Short fades at both ends so the loop joins without a click.
      const double env = std::min(1.0, t / 0.05) * std::min(1.0, (total - t) / 0.05);
      return hiss * 0.35 * flutter * env;
    });
  }
  if (id == "deflate")
  {
    // A cartoon balloon letting go (end_e4, Buddy Boom): the neck's rubbery
    // squeal fluttering and climbing as the air runs out, a rush of air
    // under it, and the last flap of the empty skin.
    Osc reed, flap;
    Noise n(seed);
    Svf body, air;
    return render(1.9, [&](double t, double total) {
      const double u = t / 1.6;
      const double wob = 1.0 + 0.08 * std::sin(t * 2.0 * kPi * 17.0) + 0.05 * std::sin(t * 2.0 * kPi * 5.3);
      const double f = sweep(260.0, 720.0, u) * wob;
      const double squeal = body.band(float(reed.step(f, Wave::Square, 0.3)), f * 2.2, 3.0) * 0.2;
      const double rush = air.band(n.next(), 1800.0 + 900.0 * std::sin(t * 9.0), 0.8) * 0.12;
      const double held = attack(t, 0.03) * std::clamp((1.6 - t) / 0.08, 0.0, 1.0) * (0.75 + 0.25 * std::sin(t * 2.0 * kPi * 3.0));
      const double end = t >= 1.6 ? flap.step(90.0, Wave::Triangle) * decay(t - 1.6, 0.05) * 0.4 : 0.0;
      return (squeal + rush) * held + end * std::min(1.0, (total - t) / 0.05);
    });
  }
  if (id == "static_hiss")
  {
    // Dead air (brief_29): the snow between channels for as long as S
    // talks through it (the cutscene stops it). Steady hiss with the set's
    // mains buzz, crackles popping in it, a slow wash in its loudness, and
    // toward the end the hiss thinning out as the runners start to tune.
    Noise n(seed), m(seed + 11u);
    OnePole lp, hp;
    Osc buzz;
    double crackle = 0.0;
    return render(12.0, [&](double t, double total) {
      const double thin = std::clamp((t - 7.0) / 4.0, 0.0, 1.0);
      const double wash = 0.82 + 0.1 * std::sin(2.0 * kPi * t * 0.37) + 0.08 * std::sin(2.0 * kPi * t * 1.9);
      const double hiss = hp.highpass(lp.lowpass(n.next(), 8000.0 - 4500.0 * thin), 250.0 + 900.0 * thin);
      if (m.next() > 0.9996f)
        crackle = 1.0;
      crackle *= 0.995;
      const double env = std::min(1.0, t / 0.3) * std::min(1.0, (total - t) / 0.8);
      return (hiss * 0.3 * wash * (1.0 - 0.45 * thin) + crackle * m.next() * 0.2 + buzz.step(60.0, Wave::Saw) * 0.012) * env;
    });
  }
  if (id == "pa_feedback")
  {
    // The Fly Tower (brief_40): the house PA keyed open: a thump and a
    // crackle in the speaker, then the howl of feedback creeping up and
    // squealing until someone pulls the fader down.
    Noise n(seed), m(seed + 5u);
    OnePole lp;
    Svf howl, cone;
    Osc tone, tone2, mains;
    return render(1.6, [&](double t, double total) {
      const double key = lp.lowpass(n.next(), 300.0) * decay(t, 0.05) * 2.0;
      const double crackle = (m.next() > 0.97f ? m.next() : 0.0f) * decay(t, 0.35) * 0.6;
      const double u = std::clamp((t - 0.15) / 1.1, 0.0, 1.0);
      const double f = 1180.0 + 40.0 * std::sin(t * 2.0 * kPi * 5.0) * u;
      const double ring = (tone.step(f, Wave::Sine) * 0.8 + tone2.step(f * 2.0, Wave::Sine) * 0.25 * u + howl.band(n.next(), f, 12.0) * 0.4) *
        u * u * std::min(1.0, (total - t) / 0.12);
      const double buzz = mains.step(60.0, Wave::Saw) * 0.06 * std::min(1.0, t / 0.05) + cone.band(m.next(), 900.0, 2.0) * 0.08;
      return (key + crackle + ring * 0.5 + buzz) * 0.45;
    });
  }
  if (id == "crowd_murmur")
  {
    // Sweeps Week (brief_41): a studio audience filling the banks: a warm
    // murmur of voices, seats thumping down, a laugh here and there.
    Noise n(seed), m(seed + 3u), s(seed + 11u);
    Svf v1, v2, v3;
    OnePole lp;
    Osc thump;
    double seatT = -1.0;
    return render(4.0, [&](double t, double total) {
      const double env = std::min(1.0, t / 0.8) * std::min(1.0, (total - t) / 0.8);
      const double babble = v1.band(n.next(), 420.0 + 120.0 * std::sin(t * 3.1), 2.5) * 0.7 +
        v2.band(m.next(), 900.0 + 200.0 * std::sin(t * 4.7 + 1.0), 3.0) * 0.45;
      if (s.next() > 0.99985f)
        seatT = t;
      double seat = 0.0;
      if (seatT >= 0.0 && t - seatT < 0.12)
        seat = thump.step(sweep(140.0, 60.0, (t - seatT) / 0.1), Wave::Sine) * decay(t - seatT, 0.04) * 0.5 + lp.lowpass(n.next(), 800.0) * decay(t - seatT, 0.02) * 0.6;
      const double laugh = t > 1.6 && t < 2.2 ? v3.band(m.next(), 1100.0, 6.0) * (0.5 + 0.5 * std::sin(t * 2.0 * kPi * 7.0)) * 0.4 *
          std::sin(kPi * (t - 1.6) / 0.6)
                                             : 0.0;
      return (babble * env + seat + laugh) * 0.5;
    });
  }
  if (id == "marker_squeak")
  {
    // Sweeps Week (brief_41): a fat marker struck across a cue card: two
    // squeaky strokes over the card's fibre.
    Noise n(seed);
    Svf fibre;
    Osc squeak;
    return render(0.6, [&](double t, double) {
      const bool second = t > 0.3;
      const double st = second ? t - 0.3 : t, len = second ? 0.2 : 0.25;
      if (st > len)
        return 0.0;
      const double u = st / len;
      const double f = (second ? 1700.0 : 1400.0) + 400.0 * std::sin(kPi * u) + 120.0 * std::sin(st * 2.0 * kPi * 35.0);
      const double env = std::sin(kPi * u);
      return (squeak.step(f, Wave::Triangle) * 0.3 + fibre.band(n.next(), 3500.0, 2.0) * 0.5) * env * 0.6;
    });
  }
  if (has(id, "steam") || has(id, "hiss") || has(id, "airlock") || has(id, "sizzle") || has(id, "deflate"))
    return noiseBed(has(id, "static_hiss") ? 3.0 : 1.2, seed, 2000.0, 9000.0, 0.35, 0.05);
  if (has(id, "static") || has(id, "snow") || has(id, "tv_"))
    return noiseBed(has(id, "burst") ? 0.5 : 1.5, seed, 300.0, 9000.0, 0.4, 0.01);
  // Weather and water.
  if (has(id, "rain") || has(id, "drip") || has(id, "bubbles"))
    return rain(loop || has(id, "rain") ? 6.0 : 1.5, seed, 0.35);
  if (has(id, "thunder"))
    return boom(3.0, seed, 1.0);
  // People.
  if (has(id, "crowd") || has(id, "laugh") || has(id, "scuffle"))
    return crowd(3.0, seed, has(id, "hush") ? 0.2 : 0.45, has(id, "cheer") || has(id, "roar"));
  // Hums, drones and engines: sustained tones.
  if (has(id, "hum") || has(id, "drone") || has(id, "tone") || has(id, "engine") || has(id, "rotor") ||
      has(id, "propeller") || has(id, "thruster") || has(id, "hover") || has(id, "maglev") || has(id, "lava") ||
      has(id, "magma") || has(id, "core") || has(id, "server") || has(id, "power"))
  {
    const double f = has(id, "rotor") || has(id, "propeller") ? 30.0 : 55.0 + double(seed % 60u);
    auto h = hum(f, std::max(len, 3.0), 0.35, seed);
    if (has(id, "rotor") || has(id, "propeller"))
      for (std::size_t i = 0; i < h.size(); ++i)
        h[i] *= float(0.5 + 0.5 * std::sin(double(i) / kRate * 2.0 * kPi * 9.0));
    if (has(id, "power_down"))
      for (std::size_t i = 0; i < h.size(); ++i)
        h[i] *= float(1.0 - double(i) / double(h.size()));
    return h;
  }
  // Holograms, glitches, bleeps: sci-fi sweeps.
  if (has(id, "holo") || has(id, "glitch") || has(id, "bleep") || has(id, "hologram"))
  {
    Osc o, lfo;
    const bool off = has(id, "off");
    return render(has(id, "rise") ? 1.2 : 0.6, [&](double t, double total) {
      const double f = off ? sweep(1800.0, 200.0, t / total) : sweep(200.0, 1800.0, t / total);
      return o.step(f * (1.0 + 0.05 * lfo.step(30.0, Wave::Square)), Wave::Triangle) * 0.25 * attack(t) *
        std::min(1.0, (total - t) / 0.1);
    });
  }
  if (has(id, "name_slam"))
    return boom(0.7, seed, 0.9);
  if (has(id, "slide_whistle"))
  {
    Osc o;
    return render(0.9, [&](double t, double total) {
      return o.step(sweep(400.0, 1600.0, t / total), Wave::Sine) * 0.3 * attack(t) * std::min(1.0, (total - t) / 0.1);
    });
  }
  if (has(id, "squeak") || has(id, "rubber"))
  {
    Osc o;
    return render(0.25, [&](double t, double total) {
      return o.step(1200.0 + 500.0 * std::sin(t * 60.0), Wave::Square, 0.3) * 0.15 * std::sin(kPi * t / total);
    });
  }
  if (has(id, "typewriter"))
  {
    std::vector<float> out(std::size_t(samples(4.0)), 0.0f);
    Noise n(seed);
    for (int k = 0; k < 28; ++k)
    {
      const auto c = click(2000.0, 0.05, 0.4, seed + std::uint32_t(k));
      const int at = samples(0.05 + 0.14 * double(k) + (n.next() + 1.0f) * 0.03);
      for (std::size_t i = 0; i < c.size() && at + int(i) < int(out.size()); ++i)
        out[std::size_t(at) + i] += c[i];
    }
    return out;
  }
  if (has(id, "tape_stop"))
  {
    Osc o;
    return render(1.0, [&](double t, double total) {
      return o.step(sweep(440.0, 30.0, t / total), Wave::Saw) * 0.25 * (1.0 - t / total);
    });
  }
  if (has(id, "klaxon") || has(id, "siren") || has(id, "feedback"))
  {
    Osc o, lfo;
    return render(1.5, [&](double t, double total) {
      const double f = has(id, "feedback") ? 2200.0 : 500.0 + 200.0 * lfo.step(1.5, Wave::Triangle);
      return o.step(f, Wave::Square, 0.4) * 0.15 * std::min(1.0, t / 0.05) * std::min(1.0, (total - t) / 0.2);
    });
  }
  if (has(id, "jump"))
    return makeSfx(Sfx::Jump);
  // Level 17's greenhouse: grow lamps buzzing on, leaves rustling.
  if (has(id, "lamp_buzz"))
    return makeSfx(Sfx::LampOn);
  if (has(id, "rustle"))
    return makeSfx(Sfx::Rustle);
  // Anything else: a soft generic room tone.
  return noiseBed(1.5, seed, 100.0, 1200.0, 0.2, 0.3);
}

std::vector<float> makeVoiceBlip(const std::string& speaker)
{
  // Square-wave blips, 40 ms each, pitched per speaker (SPEC 8.2).
  double f = 200.0;
  Wave w = Wave::Square;
  int mod = 0; // 1 ring, 2 vibrato, 3 noise, 4 breathy
  if (speaker == "DASH")
    f = 220.0;
  else if (speaker == "ROCCO")
    f = 150.0;
  else if (speaker == "NOVA")
    f = 330.0;
  else if (speaker == "MAX")
  {
    f = 260.0;
    mod = 1;
  }
  else if (speaker == "ZERO")
  {
    f = 180.0;
    w = Wave::Sine;
  }
  else if (speaker == "LANCE")
  {
    f = 280.0;
    mod = 2;
  }
  else if (speaker == "MAXINE")
    f = 300.0;
  else if (speaker == "S")
  {
    f = 300.0;
    mod = 3;
  }
  else
    mod = 4;
  Osc o, ring, vib;
  Noise n(seedOf(speaker));
  OnePole lp;
  return render(0.04, [&](double t, double total) {
    double ff = f;
    if (mod == 2)
      ff *= 1.0 + 0.04 * vib.step(14.0, Wave::Sine);
    double v = o.step(ff, w, 0.4);
    if (mod == 1)
      v *= ring.step(ff * 1.5, Wave::Sine);
    if (mod == 3)
      v = v * 0.7 + n.next() * 0.3;
    if (mod == 4)
      v = lp.lowpass(float(v * 0.5 + n.next() * 0.5), 1500.0);
    return v * 0.16 * std::min(1.0, t / 0.004) * std::min(1.0, (total - t) / 0.006);
  });
}

} // namespace gr::synth
