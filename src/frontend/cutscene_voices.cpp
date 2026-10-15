#include "frontend/cutscene_voices.hpp"

#include "audio/voice_clip.hpp"

#include <cstdint>
#include <cstdio>
#include <sstream>

namespace gr
{

void CutsceneVoices::setDir(const std::string& dir)
{
  mDir = dir;
  mBanks.clear();
}

std::string CutsceneVoices::voiceOf(const std::string& speaker)
{
  // The runners keep their in-game voices; everyone else has their own.
  static const std::map<std::string, std::string> kVoices = {
    {"DASH", "dash"}, {"ROCCO", "rocco"}, {"NOVA", "nova"}, {"MAX", "max"}, {"MAXINE", "maxine"},
    {"ZERO", "zero"}, {"LANCE", "lance"}, {"S", "signal"}, {"NARRATOR", "narrator"}, {"BOARD", "board"},
    {"ANNOUNCER", "announcer"}};
  const auto it = kVoices.find(speaker);
  return it == kVoices.end() ? std::string() : it->second;
}

std::string CutsceneVoices::clipId(const std::string& cutscene, const std::string& speaker, const std::string& text)
{
  std::uint32_t h = 2166136261u;
  for (const char c : speaker + "|" + text)
  {
    h ^= static_cast<unsigned char>(c);
    h *= 16777619u;
  }
  char hex[9];
  std::snprintf(hex, sizeof hex, "%08x", unsigned(h));
  return cutscene + "_" + hex;
}

CutsceneVoices::Bank& CutsceneVoices::bank(const std::string& voice)
{
  Bank& b = mBanks[voice];
  if (b.loaded)
    return b;
  b.loaded = true;
  std::vector<unsigned char> text;
  if (mDir.empty() || !readDataFile(mDir + "/" + voice + ".txt", text) ||
      !readDataFile(mDir + "/" + voice + ".ogg", b.ogg))
    return b;
  std::istringstream in(std::string(text.begin(), text.end()));
  std::string row;
  while (std::getline(in, row))
  {
    std::istringstream r(row);
    std::string id;
    int start = 0, count = 0;
    if (row.empty() || row[0] == '#' || !(r >> id >> start >> count))
      continue;
    b.index[id] = {start, count};
  }
  return b;
}

std::shared_ptr<const std::vector<float>> CutsceneVoices::clip(const std::string& cutscene,
  const std::string& speaker, const std::string& text)
{
  const std::string voice = voiceOf(speaker);
  if (voice.empty())
    return nullptr;
  const Bank& b = bank(voice);
  const auto it = b.index.find(clipId(cutscene, speaker, text));
  if (it == b.index.end())
    return nullptr;
  auto pcm = decodeOggVorbisRange(b.ogg, it->second.first, it->second.second);
  if (pcm.empty())
    return nullptr;
  return std::make_shared<const std::vector<float>>(std::move(pcm));
}

} // namespace gr
