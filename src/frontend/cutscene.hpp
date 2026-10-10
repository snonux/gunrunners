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
struct Art;
struct Theme;

// Cutscenes follow RigelEngine's movie model (ui::MoviePlayer): a list of
// shots, each a clip played at a fixed frame delay and repeated, with cues
// by frame that play sounds, show subtitles, freeze frames and change the
// delay. The scripts are data in cutscenes/NAME.txt (SPEC.md section 8).
struct CutsceneCue
{
  int frame = 0;
  std::string key;   // sound, stopsound, music, say, hold, delay, fade, flash, shake, wait
  std::string value; // everything after '=' (for say: the speaker)
  std::string text;  // say: the line
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
  const std::string& name() const { return mCutscene.name; }

private:
  void enterFrame();
  void nextShot();
  void finish();
  int totalFrames() const;

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

  // Subtitle.
  std::string mSpeaker, mLine;
  int mShown = 0;
  int mRevealTicks = 0;

  // Screen effects.
  float mFadeFrom = 0.0f, mFadeTo = 0.0f, mFadeMs = 0.0f, mFadeLen = 0.0f;
  float mFlashMs = 0.0f, mFlashLen = 0.0f;
  float mShakeAmp = 0.0f, mShakeMs = 0.0f, mShakeLen = 0.0f;
};

} // namespace gr
