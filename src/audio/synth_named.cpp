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
  // Clicks and mechanisms.
  if (has(id, "click") || has(id, "clunk") || has(id, "clamp") || has(id, "switch") || has(id, "plate") ||
      has(id, "arrow") || has(id, "trapdoor") || has(id, "door") || has(id, "slam") || has(id, "bump"))
    return click(has(id, "slam") ? 120.0 : 500.0 + double(seed % 400u), has(id, "slam") ? 0.5 : 0.25, 0.5, seed);
  // Air: wind, whooshes, steam, hiss.
  if (has(id, "wind"))
    return whoosh(3.0, seed, has(id, "high") ? 0.5 : 0.4, 300.0, 1400.0);
  if (has(id, "whoosh") || has(id, "swish") || has(id, "beam"))
    return whoosh(0.7, seed, 0.5, 400.0, 3000.0);
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
