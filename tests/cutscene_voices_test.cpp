// The cutscenes' speech (voices/cutscenes, tools/voices/cutscene_*.py):
//   - every speaker of a `say` or `voice` cue has a voice, and every such
//     line is recorded in its speaker's bank and decodes;
//   - a spoken line is heard to the end: the next line waits for it;
//   - with subtitles off a spoken line shows no box, and with voices off
//     every line is a subtitle again.
// Usage: cutscene_voices_test DATA_DIR

#include "audio/audio.hpp"
#include "frontend/cutscene.hpp"
#include "frontend/cutscene_voices.hpp"

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
  CutsceneVoices voices;
  voices.setDir(data + "/voices/cutscenes");

  int lines = 0;
  double seconds = 0.0;
  for (const auto& entry : std::filesystem::directory_iterator(data + "/cutscenes"))
  {
    if (entry.path().extension() != ".txt")
      continue;
    const Cutscene cs = Cutscene::load(entry.path().string());
    for (const auto& shot : cs.shots)
      for (const auto& c : shot.cues)
      {
        if (c.key != "say" && c.key != "voice")
          continue;
        const std::string what = cs.name + ": " + c.value + " \"" + c.text + "\"";
        check(!c.text.empty(), what + " has words");
        check(!CutsceneVoices::voiceOf(c.value).empty(), what + ": the speaker has a voice");
        const auto clip = voices.clip(cs.name, c.value, c.text);
        check(clip && !clip->empty(), what + " is recorded");
        if (clip)
        {
          const double s = double(clip->size()) / kAudioRate;
          check(s > 0.2 && s < 12.0, what + " is a sensible length");
          seconds += s;
        }
        ++lines;
      }
  }
  check(lines > 100, "the cutscenes have their lines");

  // The opening: MAX's second line waits for the first to be said.
  Audio audio;
  const Cutscene opening = Cutscene::load(data + "/cutscenes/opening.txt");
  const auto first = voices.clip("opening", "MAX", "Three couriers. Forty-two jobs.");
  check(first != nullptr, "the opening's first line is recorded");
  auto run = [&](float volume, bool subtitles, int& firstAt, int& secondAt, bool& boxUnderSpeech) {
    CutscenePlayer player(opening, &audio);
    player.setVoices(&voices, volume, subtitles);
    firstAt = secondAt = -1;
    boxUnderSpeech = false;
    const Input none;
    for (int t = 0; t < 60 * 60 && player.tick(none, none); ++t)
    {
      const std::string& line = player.subtitle();
      if (player.speaking() && !line.empty())
        boxUnderSpeech = true;
      if (firstAt < 0 && player.speaking())
        firstAt = t;
      if (firstAt < 0 && line.rfind("Three couriers", 0) == 0)
        firstAt = t;
      if (secondAt < 0 && line.rfind("Paid by the job", 0) == 0)
        secondAt = t;
      if (secondAt < 0 && firstAt >= 0 && t > firstAt && player.speaking() && line.empty() && !subtitles)
        secondAt = t; // subtitles off: the second line starts speaking with no box
    }
  };
  int firstAt = 0, secondAt = 0;
  bool box = false;
  run(0.8f, true, firstAt, secondAt, box);
  if (first)
  {
    const int clipTicks = int(first->size() * 60 / std::size_t(kAudioRate));
    check(firstAt >= 0 && secondAt > firstAt, "the opening says both of MAX's lines");
    check(secondAt - firstAt >= clipTicks, "MAX's second line waits for the first (" +
      std::to_string(secondAt - firstAt) + " ticks, the clip is " + std::to_string(clipTicks) + ")");
  }
  check(box, "subtitles on: a spoken line has its box");
  run(0.8f, false, firstAt, secondAt, box);
  check(!box, "subtitles off: a spoken line has no box");
  run(0.0f, true, firstAt, secondAt, box);
  check(firstAt >= 0 && secondAt > firstAt, "voices off: the lines are subtitles");

  std::printf("%d cutscene lines, %.0f s of speech\n", lines, seconds);
  return gFailures == 0 ? 0 : 1;
}
