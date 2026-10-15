#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace gr
{

// The recorded speech for the cutscenes' `say` and `voice` cues. Every
// speaker has a voice (the runners the same as in play) and a bank in
// voices/cutscenes/: <voice>.ogg holds all of its clips one after the
// other, <voice>.txt says where each starts (tools/voices/cutscene_pack.py).
// A clip's id is the cutscene's name and a hash of who says what, so a
// line whose words were edited after recording stays a subtitle rather
// than playing the old words.
class CutsceneVoices
{
public:
  // Where the banks are. Nothing is read until a clip is asked for.
  void setDir(const std::string& dir);

  // "MAX" -> "max", "S" -> "signal"; empty for a speaker without a voice.
  static std::string voiceOf(const std::string& speaker);
  // "brief_01_1a2b3c4d": the cutscene and an FNV-1a hash of "SPEAKER|text".
  static std::string clipId(const std::string& cutscene, const std::string& speaker, const std::string& text);

  // The line, decoded to mono floats at kAudioRate; null if not recorded.
  std::shared_ptr<const std::vector<float>> clip(const std::string& cutscene, const std::string& speaker,
    const std::string& text);

private:
  struct Bank
  {
    bool loaded = false;
    std::vector<unsigned char> ogg;
    std::map<std::string, std::pair<int, int>> index; // id -> first sample, samples
  };
  Bank& bank(const std::string& voice);

  std::string mDir;
  std::map<std::string, Bank> mBanks;
};

} // namespace gr
