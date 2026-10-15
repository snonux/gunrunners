// The runners' spoken lines (voices/lines.txt and its recordings):
//   - every campaign level has three lines, every bonus and extra level one
//     or more (the Silent Picture none: it has no sound), and no section
//     names a level that doesn't exist;
//   - every `see` names an enemy and every `at` / `reach` a block of that
//     level's map;
//   - every event and every runner has its lines;
//   - every line is recorded in every voice it needs, in its voice's bank,
//     and decodes;
//   - a custom runner keeps its voice through runners.txt, and one saved
//     before voices gets one by its body.
// Usage: voices_test DATA_DIR

#include "audio/audio.hpp"
#include "audio/voice_clip.hpp"
#include "data/characters.hpp"
#include "data/level.hpp"
#include "frontend/voices.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

using namespace gr;

namespace
{

int gFailures = 0;

void check(bool ok, const std::string& what)
{
  if (!ok)
  {
    ++gFailures;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
  }
}

} // namespace

int main(int argc, char** argv)
{
  const std::string data = argc > 1 ? argv[1] : ".";
  VoiceBank bank;
  check(bank.load(data + "/voices"), "voices/lines.txt loads");

  // Levels and their lines.
  int levels = 0;
  for (const auto& entry : std::filesystem::directory_iterator(data + "/levels"))
  {
    const std::string file = entry.path().filename().string();
    if (entry.path().extension() != ".txt" || file == "level1.txt" || file == "README.md")
      continue;
    const Level level = Level::loadFile(entry.path().string());
    const auto* lines = bank.section("level " + level.slug);
    if (level.slug == "27_bonus_silent_picture")
    {
      check(!lines, "the Silent Picture says nothing");
      continue;
    }
    ++levels;
    const bool bonus = level.slug.find("_bonus_") != std::string::npos || level.slug.rfind("extra_", 0) == 0;
    const std::size_t want = bonus ? 1 : 3;
    check(lines && lines->size() >= want, level.slug + ": at least " + std::to_string(want) + " lines");
    if (!lines)
      continue;
    for (const auto& l : *lines)
    {
      if (l.trigger == VoiceTrigger::See)
        check(l.enemy >= 0, l.id + ": see " + l.enemyKey + " is an enemy");
      if (l.trigger == VoiceTrigger::At || l.trigger == VoiceTrigger::Reach)
        check(l.x >= 0 && l.y >= 0 && l.x < level.width && l.y < level.height, l.id + ": the spot is on the map");
      if (l.trigger == VoiceTrigger::Clear)
        check(!bonus, l.id + ": bonus levels have no exit line");
    }
  }
  check(levels >= 90, "the levels were found");
  for (const auto& [key, lines] : bank.sections())
  {
    if (key.rfind("level ", 0) != 0)
      continue;
    const std::string slug = key.substr(6);
    check(std::filesystem::exists(data + "/levels/" + slug + ".txt"), key + " is a level");
  }

  for (const char* e : {"turbo", "virus", "weapon", "letters", "vehicle", "respawn", "lowhp"})
  {
    const auto* lines = bank.section(std::string("event ") + e);
    check(lines && !lines->empty(), std::string("event ") + e + " has lines");
  }
  for (int v = 0; v < kVoiceCount; ++v)
  {
    const auto* lines = bank.section(std::string("runner ") + voiceId(v));
    check(lines && lines->size() >= 3, std::string("runner ") + voiceId(v) + " has catchphrases");
  }

  // Recordings.
  int clips = 0;
  for (const auto& [key, lines] : bank.sections())
    for (const auto& l : lines)
      for (int v = 0; v < kVoiceCount; ++v)
      {
        const std::string voice = voiceId(v);
        if (key.rfind("runner ", 0) == 0 && key != "runner " + voice)
          continue;
        const std::string what = voice + "/" + l.id;
        if (!bank.hasClip(voice, l))
        {
          check(false, what + " is recorded");
          continue;
        }
        const auto pcm = bank.clip(voice, l);
        const double secs = pcm ? double(pcm->size()) / kAudioRate : 0.0;
        check(secs > 0.4 && secs < 8.0, what + " decodes to a line (" + std::to_string(secs) + " s)");
        // The clip starts and ends quiet: the index lines up with the bank.
        if (pcm && pcm->size() > 400)
        {
          float edge = 0.0f;
          for (std::size_t i = 0; i < 200; ++i)
            edge = std::max({edge, std::abs((*pcm)[i]), std::abs((*pcm)[pcm->size() - 1 - i])});
          check(edge < 0.2f, what + " starts and ends at a quiet spot");
        }
        ++clips;
      }
  std::printf("%d clips\n", clips);

  // Voices through runners.txt.
  RunnerParts robot;
  robot.body = 1;
  CharacterDef d = makeCustomRunner("c0voice1", "SPARKY", robot, Weapon::Laser, 3, 3, 3);
  check(runnerVoice(d) == "bolt", "a robot sounds like Bolt by default");
  d.voice = "jade";
  CharacterDef back;
  check(decodeRunner(encodeRunner(d), back) && runnerVoice(back) == "jade", "the voice survives runners.txt");
  check(decodeRunner("v1 c0old 0 3 3 3 0 1 0 0 0 0 0 0 0 0 0 0 OLD TIMER", back) && back.name == "OLD TIMER" &&
          runnerVoice(back) == "dash",
    "a runner from before voices still loads, with a voice");
  check(runnerVoice(characterByIndex(1)) == "rocco", "Rocco is Rocco");

  if (gFailures)
    std::fprintf(stderr, "%d failures\n", gFailures);
  else
    std::printf("voices: all good\n");
  return gFailures ? 1 : 0;
}
