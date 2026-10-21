#pragma once

#include "game/input.hpp"
#include "render/renderer.hpp"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace gr
{

class Audio;
class CutsceneVoices;
struct Art;
struct Theme;

// Cutscenes follow RigelEngine's movie model (ui::MoviePlayer): a list of
// shots, each a clip played at a fixed frame delay and repeated, with cues
// by frame that play sounds, show subtitles, freeze frames and change the
// delay. The scripts are data in cutscenes/NAME.txt (SPEC.md section 8).
struct CutsceneCue
{
  int frame = 0;
  std::string key;   // sound, stopsound, music, say, voice, hold, delay, fade, flash, shake, wait
  std::string value; // everything after '=' (for say and voice: the speaker)
  std::string text;  // say and voice: the line (voice: spoken only, the words are on screen)
};

struct CutsceneShot
{
  std::string clip;
  int delayMs = 100;
  int repeat = 1; // 0: loop until a wait ends
  int clipFrames = 8;
  std::vector<CutsceneCue> cues;
  std::string credits;            // engine-built credits sequence
  std::string bumperCh, bumperGenre; // Episode 4 channel bumper
};

struct Cutscene
{
  std::string name;
  std::string music;
  std::vector<CutsceneShot> shots;

  static Cutscene parse(const std::string& text);
  static Cutscene load(const std::string& path);
};

// What clips draw with: the renderer, the current theme's art and a cache
// for textures the clips bake on first use.
struct ClipKit
{
  Renderer& r;
  const Art& art;
  const Theme& theme;
  int level = 0;
  std::map<std::string, Texture> cache;
  std::set<int> cameras; // levels whose candid camera was shot (end_e3's monitor wall)
};

// Draws one frame of a clip. `frame` is the clip frame (0..frames-1),
// `t` the progress through the shot (0..1), `ticks` 60 Hz ticks since the
// shot began, (ox, oy) the screen shake offset.
void drawClip(ClipKit& kit, const std::string& clip, int frame, int frames, float t, int ticks, float ox, float oy);

class CutscenePlayer
{
public:
  CutscenePlayer(Cutscene cutscene, Audio* audio);
  ~CutscenePlayer();

  // One 60 Hz tick. Returns false once the cutscene has finished.
  bool tick(const Input& in, const Input& prev);
  void render(ClipKit& kit);
  bool done() const { return mDone; }
  bool waiting() const { return mWaiting; } // for a button press
  // Speech for the `say` and `voice` cues at this volume (0: subtitles
  // only, as before); subtitles false hides the box under a spoken line.
  // Call before the first tick: the opening shot's lines are cued already.
  void setVoices(CutsceneVoices* voices, float volume, bool subtitles);
  bool speaking() const { return mSpeakTicks > 0; }
  const std::string& subtitle() const { return mLine; } // empty: no box
  const std::string& name() const { return mCutscene.name; }
  // The skip hint speaks of taps (touch) or presses and holds.
  void setTouchHints(bool touch) { mTouchHints = touch; }
  // A press moves on one beat (the next line, or the next shot); a few
  // quick presses, or a held button, skip the whole cutscene.
  static constexpr int kSkipPresses = 3;
  static constexpr int kSkipGapTicks = 30; // between the quick presses
  static constexpr int kSkipHoldTicks = 60;

private:
  void enterFrame(bool quiet = false);
  void nextBeat();
  void nextShot();
  void finish();
  int totalFrames() const;
  bool frameSpeaks(int frame) const; // waits for a spoken line to end
  void speak(const CutsceneCue& c);
  void stopSpeaking();

  Cutscene mCutscene;
  Audio* mAudio;
  int mShot = -1;
  int mFrame = 0;
  float mFrameMs = 0.0f;
  float mFrameLength = 0.0f;
  int mDelay = 100;
  bool mWaiting = false;
  bool mDone = false;
  int mShotTicks = 0;
  int mTicks = 0;
  std::vector<bool> mFired;

  // Skipping: quick presses so far, ticks since the last press, how long
  // the button pressed last has been held, and how long the hint shows.
  int mPresses = 0;
  int mSincePress = 0;
  int mHeldTicks = 0;
  bool mHolding = false;
  int mHintTicks = 0;
  bool mTouchHints = false;

  // Speech: the clip of each cue (by shot, then cue), loaded up front.
  std::vector<std::vector<std::shared_ptr<const std::vector<float>>>> mClips;
  float mVoiceVolume = 0.0f;
  bool mSubtitles = true;
  int mSpeakTicks = 0; // ticks left of the line being said

  // Subtitle.
  std::string mSpeaker, mLine;
  bool mVoiced = false; // the line is being spoken: no blips
  int mShown = 0;
  int mRevealTicks = 0;

  // Screen effects.
  float mFadeFrom = 0.0f, mFadeTo = 0.0f, mFadeMs = 0.0f, mFadeLen = 0.0f;
  float mFlashMs = 0.0f, mFlashLen = 0.0f;
  float mShakeAmp = 0.0f, mShakeMs = 0.0f, mShakeLen = 0.0f;
};

} // namespace gr
