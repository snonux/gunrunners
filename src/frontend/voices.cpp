#include "frontend/voices.hpp"

#include "audio/audio.hpp"
#include "audio/voice_clip.hpp"
#include "data/enemies.hpp"
#include "data/level.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace gr
{

namespace
{

constexpr int kTicksPerSecond = 60;
constexpr int kStartTicks = 2 * kTicksPerSecond;      // `start` lines
constexpr int kGapTicks = 3 * kTicksPerSecond;        // quiet before a level line
constexpr int kBarkGap = 8 * kTicksPerSecond;         // quiet before a bark
constexpr int kStaleTicks = 10 * kTicksPerSecond;     // a waiting line older than this is dropped
constexpr int kNoBarkTicks = 8 * kTicksPerSecond;     // no barks this early in a level
constexpr int kBarkCooldown = 45 * kTicksPerSecond;   // per event
constexpr int kHotkeyCooldown = 2 * kTicksPerSecond;
constexpr int kTailTicks = 12; // a beat after a clip before the subtitle goes
constexpr std::size_t kCacheClips = 24;

std::string trim(const std::string& s)
{
  const auto a = s.find_first_not_of(" \t\r");
  if (a == std::string::npos)
    return {};
  const auto b = s.find_last_not_of(" \t\r");
  return s.substr(a, b - a + 1);
}

// How long a line without a clip (no audio, a missing file) is taken to
// last: about three words a second.
int estimateTicks(const std::string& text)
{
  const int words = int(std::count(text.begin(), text.end(), ' ')) + 1;
  return kTicksPerSecond / 2 + words * kTicksPerSecond / 3;
}

std::string upper(std::string s)
{
  for (auto& c : s)
    c = char(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

} // namespace

// --- The bank ----------------------------------------------------------------

bool VoiceBank::load(const std::string& dir)
{
  mDir = dir;
  mSections.clear();
  mCache.clear();
  mBanks.clear();
  std::vector<unsigned char> data;
  if (!readDataFile(dir + "/lines.txt", data))
    return false;
  std::string error;
  if (!parse(std::string(data.begin(), data.end()), &error))
  {
    std::fprintf(stderr, "voices/lines.txt: %s\n", error.c_str());
    return false;
  }
  return true;
}

bool VoiceBank::parse(const std::string& text, std::string* error)
{
  mSections.clear();
  std::istringstream in(text);
  std::string raw;
  std::string section, idBase;
  int number = 0;
  auto fail = [&](const std::string& why) {
    if (error)
      *error = "line " + std::to_string(number) + ": " + why;
    return false;
  };
  std::vector<VoiceLine>* lines = nullptr;
  while (std::getline(in, raw))
  {
    ++number;
    const std::string s = trim(raw);
    if (s.empty() || s[0] == '#')
      continue;
    if (s.front() == '[')
    {
      if (s.back() != ']')
        return fail("bad section header");
      std::istringstream h(s.substr(1, s.size() - 2));
      std::string kind, name;
      h >> kind >> name;
      if ((kind != "level" && kind != "event" && kind != "runner") || name.empty())
        return fail("unknown section " + s);
      section = kind + " " + name;
      idBase = kind == "level" ? name : kind + "_" + name;
      lines = &mSections[section];
      continue;
    }
    if (!lines)
      return fail("a line outside a section");
    const auto bar = s.find('|');
    if (bar == std::string::npos)
      return fail("no | between trigger and text");
    VoiceLine line;
    line.text = trim(s.substr(bar + 1));
    if (line.text.empty())
      return fail("no text");
    std::istringstream t(s.substr(0, bar));
    std::string word;
    t >> word;
    const bool level = section.rfind("level ", 0) == 0;
    if (word == "say" && !level)
      line.trigger = VoiceTrigger::Say;
    else if (!level)
      return fail("event and runner lines use `say`");
    else if (word == "start")
      line.trigger = VoiceTrigger::Start;
    else if (word == "clear")
      line.trigger = VoiceTrigger::Clear;
    else if (word == "see")
    {
      line.trigger = VoiceTrigger::See;
      if (!(t >> line.enemyKey))
        return fail("see needs an enemy");
      line.enemy = enemyIndex(line.enemyKey);
    }
    else if (word == "at")
    {
      line.trigger = VoiceTrigger::At;
      if (!(t >> line.x >> line.y))
        return fail("at needs x y");
    }
    else if (word == "reach")
    {
      line.trigger = VoiceTrigger::Reach;
      if (!(t >> line.x >> line.y >> line.r))
        return fail("reach needs x y r");
    }
    else if (word == "time")
    {
      line.trigger = VoiceTrigger::Time;
      if (!(t >> line.seconds))
        return fail("time needs seconds");
    }
    else
      return fail("unknown trigger " + word);
    line.id = idBase + "_" + std::to_string(lines->size() + 1);
    lines->push_back(std::move(line));
  }
  return true;
}

const std::vector<VoiceLine>* VoiceBank::section(const std::string& key) const
{
  const auto it = mSections.find(key);
  return it == mSections.end() ? nullptr : &it->second;
}

VoiceBank::Bank& VoiceBank::bank(const std::string& voice)
{
  Bank& b = mBanks[voice];
  if (b.loaded)
    return b;
  b.loaded = true;
  std::vector<unsigned char> text;
  if (!readDataFile(mDir + "/" + voice + ".txt", text) || !readDataFile(mDir + "/" + voice + ".ogg", b.ogg))
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

bool VoiceBank::hasClip(const std::string& voice, const VoiceLine& line)
{
  return bank(voice).index.count(line.id) != 0;
}

std::shared_ptr<const std::vector<float>> VoiceBank::clip(const std::string& voice, const VoiceLine& line)
{
  const std::string key = voice + "/" + line.id;
  for (auto it = mCache.begin(); it != mCache.end(); ++it)
    if (it->first == key)
    {
      mCache.splice(mCache.begin(), mCache, it);
      return it->second;
    }
  std::shared_ptr<const std::vector<float>> pcm;
  const Bank& b = bank(voice);
  if (const auto it = b.index.find(line.id); it != b.index.end())
  {
    auto decoded = decodeOggVorbisRange(b.ogg, it->second.first, it->second.second);
    if (!decoded.empty())
      pcm = std::make_shared<const std::vector<float>>(std::move(decoded));
  }
  mCache.emplace_front(key, pcm);
  if (mCache.size() > kCacheClips)
    mCache.pop_back();
  return pcm;
}

// --- The director ------------------------------------------------------------

void VoiceDirector::begin(VoiceBank* bank, const Level& level)
{
  mBank = bank;
  mSlug = level.slug;
  mLines = bank ? bank->section("level " + level.slug) : nullptr;
  mFired.assign(mLines ? mLines->size() : 0, false);
  mPending.clear();
  mLastBark.clear();
  // Seeded by the level, so a recording of the same run says the same.
  mRng.seed(std::uint32_t(std::hash<std::string>()(level.slug)));
  mTicks = 0;
  mSpeaking = 0;
  mQuietSince = -100000;
  mLastHotkey = -100000;
  mLastSaid.clear();
  mSubtitle.clear();
  mSpeaker.clear();
  mPrevHp = -1;
  mPrevVehicle = -1;
  mDied = false;
  mBackAt = -1;
  mExiting = false;
}

void VoiceDirector::stop(Audio* audio)
{
  if (audio && mSpeaking > 0)
    audio->stopVoice();
  mSpeaking = 0;
  mPending.clear();
  mSubtitle.clear();
}

void VoiceDirector::tick()
{
  ++mTicks;
  if (mSpeaking > 0 && --mSpeaking == 0)
  {
    mQuietSince = mTicks;
    mSubtitle.clear();
  }
}

bool VoiceDirector::fired(const World& w, const VoiceLine& line) const
{
  const auto& cam = w.camera();
  const int camX = cam.x(), camY = cam.y();
  const int viewW = int(cam.viewW()), viewH = int(cam.viewH()); // cells
  switch (line.trigger)
  {
    case VoiceTrigger::Start: return mTicks >= kStartTicks;
    case VoiceTrigger::Time: return mTicks >= line.seconds * kTicksPerSecond;
    case VoiceTrigger::Clear: return w.state() != WorldState::Playing;
    case VoiceTrigger::Say: return false;
    case VoiceTrigger::See:
      for (const auto& e : w.enemies())
      {
        if (!e.alive || e.hidden || e.def != line.enemy)
          continue;
        // Its middle on screen, two cells in from the edges (a boss can
        // be bigger than the view).
        const int cx = e.x + e.w / 2, cy = e.y - e.h / 2;
        if (cx >= camX + 2 && cx <= camX + viewW - 2 && cy >= camY + 2 && cy <= camY + viewH - 2)
          return true;
      }
      return false;
    case VoiceTrigger::At:
    {
      // The block's middle in the view (a cell in from the sides; the view
      // stops at the map's edges, so a spot at the bottom row still shows).
      const float cx = float(line.x * kCellsPerTile + 1), cy = float(line.y * kCellsPerTile + 1);
      return cx >= float(camX) + 1.0f && cx <= float(camX) + cam.viewW() - 1.0f && cy >= float(camY) &&
        cy < float(camY) + cam.viewH();
    }
    case VoiceTrigger::Reach:
    {
      const auto& p = w.player();
      const int dx = (p.x + 1) / kCellsPerTile - line.x, dy = (p.y - 2) / kCellsPerTile - line.y;
      return dx * dx + dy * dy <= line.r * line.r;
    }
  }
  return false;
}

void VoiceDirector::say(const World& w, const VoiceLine& line, Audio* audio, float volume, const char* why)
{
  const std::string voice = runnerVoice(w.character());
  std::shared_ptr<const std::vector<float>> pcm;
  if (audio && mBank)
    pcm = mBank->clip(voice, line);
  if (pcm)
    audio->playVoice(pcm, volume);
  else if (audio)
    audio->stopVoice();
  const int ticks = pcm ? int(pcm->size() * kTicksPerSecond / kAudioRate) : estimateTicks(line.text);
  mSpeaking = ticks + kTailTicks;
  mSubtitle = line.text;
  mSpeaker = w.character().name;
  mLastSaid = line.id;
  if (mLog)
    std::fprintf(stderr, "voice: say %s (%s) frame %d [%s] %s\n", line.id.c_str(), voice.c_str(), w.stats().frames, why,
      line.text.c_str());
}

void VoiceDirector::bark(const World& w, const std::string& event, Audio* audio, float volume)
{
  if (!mBank || mSpeaking > 0 || mTicks < kNoBarkTicks || mTicks - mQuietSince < kBarkGap || !mPending.empty())
    return;
  if (const auto it = mLastBark.find(event); it != mLastBark.end() && mTicks - it->second < kBarkCooldown)
    return;
  const auto* lines = mBank->section("event " + event);
  if (!lines || lines->empty())
    return;
  // Every other time or so, so they stay a surprise.
  if (std::uniform_int_distribution<int>(0, 1)(mRng) == 0)
    return;
  mLastBark[event] = mTicks;
  const auto& line = (*lines)[std::uniform_int_distribution<std::size_t>(0, lines->size() - 1)(mRng)];
  say(w, line, audio, volume, event.c_str());
}

void VoiceDirector::update(const World& w, const std::vector<Sfx>& sounds, Audio* audio, float volume)
{
  if (volume <= 0.0f || w.film().on)
    return;
  // The level's own lines: each says once, when its sight first comes up.
  if (mLines)
    for (std::size_t i = 0; i < mLines->size(); ++i)
    {
      if (mFired[i] || !fired(w, (*mLines)[i]))
        continue;
      mFired[i] = true;
      const auto& line = (*mLines)[i];
      if (mLog)
        std::fprintf(stderr, "voice: trigger %s frame %d\n", line.id.c_str(), w.stats().frames);
      mPending.push_back({&line, mTicks, line.trigger == VoiceTrigger::Clear});
    }
  mPending.erase(std::remove_if(mPending.begin(), mPending.end(),
                   [this](const Pending& p) { return !p.urgent && mTicks - p.at > kStaleTicks; }),
    mPending.end());
  for (auto it = mPending.begin(); it != mPending.end(); ++it)
    if (it->urgent || (mSpeaking == 0 && mTicks - mQuietSince >= kGapTicks))
    {
      const VoiceLine& line = *it->line;
      mPending.erase(it);
      say(w, line, audio, volume, "level");
      break;
    }

  // Game events. The first frame only notes where things stand.
  const auto& p = w.player();
  std::string event;
  for (const Sfx s : sounds)
  {
    if (s == Sfx::TurboOn)
      event = "turbo";
    else if (s == Sfx::VirusOn)
      event = "virus";
    else if (s == Sfx::LettersComplete)
      event = "letters";
    else if (s == Sfx::WeaponPickup && event.empty())
      event = "weapon";
    else if (s == Sfx::Death)
      mDied = true;
  }
  if (mDied && p.state != PlayerState::Dying && p.hp > 0)
  {
    // Back from the checkpoint: a beat to look around first.
    if (mBackAt < 0)
      mBackAt = mTicks + kTicksPerSecond;
    else if (mTicks >= mBackAt)
    {
      mDied = false;
      mBackAt = -1;
      event = "respawn";
    }
  }
  if (mPrevHp > 1 && p.hp == 1 && p.state != PlayerState::Dying)
    event = "lowhp";
  if (mPrevVehicle < 0 && p.vehicle >= 0 && mPrevHp >= 0)
    event = "vehicle";
  mPrevHp = p.hp;
  mPrevVehicle = p.vehicle;
  if (!event.empty() && w.state() == WorldState::Playing)
    bark(w, event, audio, volume);
}

void VoiceDirector::sayRandom(const World& w, Audio* audio, float volume)
{
  if (!mBank || volume <= 0.0f || w.film().on || mSpeaking > 0 || mTicks - mLastHotkey < kHotkeyCooldown)
    return;
  // The runner's own catchphrases and this level's lines.
  std::vector<const VoiceLine*> pool;
  if (const auto* own = mBank->section("runner " + runnerVoice(w.character())))
    for (const auto& l : *own)
      pool.push_back(&l);
  if (mLines)
    for (const auto& l : *mLines)
      pool.push_back(&l);
  if (pool.size() > 1)
    pool.erase(std::remove_if(pool.begin(), pool.end(), [this](const VoiceLine* l) { return l->id == mLastSaid; }),
      pool.end());
  if (pool.empty())
    return;
  mLastHotkey = mTicks;
  say(w, *pool[std::uniform_int_distribution<std::size_t>(0, pool.size() - 1)(mRng)], audio, volume, "hotkey");
}

} // namespace gr
