#pragma once

#include "game/sound_ids.hpp"

#include <list>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace gr
{

class Audio;
class World;
struct Level;

// The runners' spoken lines (voices/lines.txt, design notes in
// docs/VOICES.md). Sections:
//   [level <slug>]   lines a level's sights trigger (`start`, `see <enemy>`,
//                    `at <x> <y>`, `reach <x> <y> <r>`, `time <s>`, `clear`)
//   [event <name>]   barks on game events (turbo, virus, weapon, letters,
//                    vehicle, respawn, lowhp), trigger `say`
//   [runner <voice>] catchphrases for the hotkey, trigger `say`
// Each line is recorded in every voice (the catchphrases only in their
// owner's): voices/<voice>.ogg holds all of a voice's clips one after the
// other, and voices/<voice>.txt says where each one starts.
enum class VoiceTrigger
{
  Start,
  See,
  At,
  Reach,
  Time,
  Clear,
  Say,
};

struct VoiceLine
{
  std::string id; // "01_rooftop_run_1", "event_turbo_2", "runner_rocco_3"
  std::string text;
  VoiceTrigger trigger = VoiceTrigger::Start;
  std::string enemyKey; // see
  int enemy = -1;       // see: index into the enemy table, -1 if unknown
  int x = 0, y = 0, r = 0; // at, reach: a map block and a radius in blocks
  int seconds = 0;         // time
};

class VoiceBank
{
public:
  // Reads <dir>/lines.txt (through SDL, so an APK asset works too). No
  // file is no lines, not an error.
  bool load(const std::string& dir);
  // The text of a lines.txt; false (and the line number in error) on a
  // line it can't read.
  bool parse(const std::string& text, std::string* error = nullptr);

  // "level 01_rooftop_run", "event turbo", "runner dash"; null if none.
  const std::vector<VoiceLine>* section(const std::string& key) const;
  const std::map<std::string, std::vector<VoiceLine>>& sections() const { return mSections; }
  const std::string& dir() const { return mDir; }
  // The line in this voice, decoded and cached; null if it has no clip.
  std::shared_ptr<const std::vector<float>> clip(const std::string& voice, const VoiceLine& line);
  // Whether the voice's bank has the line (its first sample and length).
  bool hasClip(const std::string& voice, const VoiceLine& line);

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
  std::map<std::string, std::vector<VoiceLine>> mSections;
  std::list<std::pair<std::string, std::shared_ptr<const std::vector<float>>>> mCache; // newest first
};

// Decides when the runner speaks in a level: the level's own lines as
// their sights come up, a bark now and then on game events, and a random
// line on the hotkey. One line at a time, never back to back.
class VoiceDirector
{
public:
  // A new run of a level (or a bonus level, or a loaded save). No lines
  // carry over.
  void begin(VoiceBank* bank, const Level& level);
  void stop(Audio* audio);
  // One 60 Hz tick while a level plays: counts down the line being said.
  void tick();
  // One logic frame of the level. sounds: the frame's sound effects.
  // volume 0 means voices are off.
  void update(const World& w, const std::vector<Sfx>& sounds, Audio* audio, float volume);
  // The hotkey: a random line for the current runner, now, unless one is
  // being said.
  void sayRandom(const World& w, Audio* audio, float volume);

  // The line being said and who says it (subtitles); empty when quiet.
  const std::string& subtitle() const { return mSubtitle; }
  const std::string& speaker() const { return mSpeaker; }
  // Prints each line to stderr as it triggers and as it plays (--voice-log).
  void setLog(bool on) { mLog = on; }

private:
  struct Pending
  {
    const VoiceLine* line;
    int at; // play tick it triggered
    bool urgent; // `clear`: says it now, cutting off anything else
  };
  bool fired(const World& w, const VoiceLine& line) const;
  void say(const World& w, const VoiceLine& line, Audio* audio, float volume, const char* why);
  void bark(const World& w, const std::string& event, Audio* audio, float volume);

  VoiceBank* mBank = nullptr;
  std::string mSlug;
  const std::vector<VoiceLine>* mLines = nullptr;
  std::vector<bool> mFired;
  std::vector<Pending> mPending;
  std::map<std::string, int> mLastBark; // event -> play tick
  std::mt19937 mRng;
  int mTicks = 0;      // 60 Hz ticks of play
  int mSpeaking = 0;   // ticks left of the line being said
  int mQuietSince = -100000;
  int mLastHotkey = -100000;
  std::string mLastSaid;
  std::string mSubtitle, mSpeaker;
  // Event state from the previous logic frame.
  int mPrevHp = -1;
  int mPrevVehicle = -1;
  bool mDied = false;
  int mBackAt = -1;
  bool mExiting = false;
  bool mLog = false;
};

} // namespace gr
