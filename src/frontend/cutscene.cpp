#include "frontend/cutscene.hpp"

#include "assets/art.hpp"
#include "audio/audio.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <regex>
#include <sstream>

namespace gr
{

namespace
{

constexpr float kTickMs = 1000.0f / 60.0f;
constexpr Color kInk = rgb(10, 8, 20);

std::string trim(const std::string& s)
{
  const auto b = s.find_first_not_of(" \t");
  if (b == std::string::npos)
    return {};
  return s.substr(b, s.find_last_not_of(" \t") - b + 1);
}

int keyNum(const std::string& line, const char* key, int def)
{
  const std::string k = std::string(key) + "=";
  const auto p = line.find(k);
  if (p == std::string::npos)
    return def;
  return std::atoi(line.c_str() + p + k.size());
}

std::string keyStr(const std::string& line, const char* key)
{
  const std::string k = std::string(key) + "=";
  const auto p = line.find(k);
  if (p == std::string::npos)
    return {};
  auto v = line.substr(p + k.size());
  if (!v.empty() && v[0] == '"')
  {
    const auto e = v.find('"', 1);
    return v.substr(1, e == std::string::npos ? std::string::npos : e - 1);
  }
  return v.substr(0, v.find(' '));
}

// "in:800" -> 800, "6:400" -> amount 6, ms 400.
void splitColon(const std::string& v, std::string& a, float& ms)
{
  const auto c = v.find(':');
  a = v.substr(0, c);
  ms = c == std::string::npos ? 500.0f : float(std::atof(v.c_str() + c + 1));
}

} // namespace

Cutscene Cutscene::parse(const std::string& text)
{
  Cutscene cs;
  std::istringstream in(text);
  std::string line;
  int pendingFrames = 0; // from the clip comment before a shot ("... 24 frames.")
  static const std::regex kFrames("([0-9]+) frames");
  while (std::getline(in, line))
  {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    const std::string t = trim(line);
    if (t.empty())
      continue;
    if (t[0] == '#')
    {
      std::smatch m;
      if (std::regex_search(t, m, kFrames))
        pendingFrames = std::atoi(m[1].str().c_str());
      continue;
    }
    std::istringstream ls(t);
    std::string word;
    ls >> word;
    if (word == "cutscene")
    {
      ls >> cs.name;
    }
    else if (word == "music")
    {
      ls >> cs.music;
    }
    else if (word == "shot")
    {
      CutsceneShot s;
      ls >> s.clip;
      s.delayMs = std::max(10, keyNum(t, "delay", 100));
      s.repeat = std::max(0, keyNum(t, "repeat", 1));
      s.clipFrames = pendingFrames > 0 ? pendingFrames : 8;
      pendingFrames = 0;
      cs.shots.push_back(s);
    }
    else if (word == "credits")
    {
      CutsceneShot s;
      s.clip = "credits";
      ls >> s.credits;
      s.delayMs = 100;
      s.repeat = 1;
      s.clipFrames = 450; // 45 s of scrolling
      cs.shots.push_back(s);
    }
    else if (word == "include")
    {
      std::string what;
      ls >> what;
      if (what == "bumper")
      {
        CutsceneShot s;
        s.clip = "bumper";
        s.bumperCh = keyStr(t, "CH");
        s.bumperGenre = keyStr(t, "GENRE");
        s.delayMs = 90;
        s.repeat = 1;
        s.clipFrames = 5;
        s.cues.push_back({0, "sound", "static_burst", {}});
        s.cues.push_back({4, "sound", "channel_click", {}});
        s.cues.push_back({4, "hold", "1500", {}});
        cs.shots.push_back(s);
      }
    }
    else if (word == "cue" && !cs.shots.empty())
    {
      CutsceneCue c;
      ls >> c.frame;
      std::string rest;
      std::getline(ls, rest);
      rest = trim(rest);
      const auto eq = rest.find('=');
      if (eq == std::string::npos)
        continue;
      c.key = rest.substr(0, eq);
      std::string v = rest.substr(eq + 1);
      if (c.key == "say")
      {
        const auto sp = v.find(' ');
        c.value = v.substr(0, sp);
        const auto q1 = v.find('"');
        const auto q2 = v.rfind('"');
        if (q1 != std::string::npos && q2 > q1)
          c.text = v.substr(q1 + 1, q2 - q1 - 1);
      }
      else
      {
        c.value = v.substr(0, v.find(' '));
      }
      cs.shots.back().cues.push_back(c);
    }
  }
  return cs;
}

Cutscene Cutscene::load(const std::string& path)
{
  std::ifstream f(path);
  std::stringstream ss;
  ss << f.rdbuf();
  return parse(ss.str());
}

CutscenePlayer::CutscenePlayer(Cutscene cutscene, Audio* audio) : mCutscene(std::move(cutscene)), mAudio(audio)
{
  if (mAudio && !mCutscene.music.empty())
    mAudio->playMusicNamed(mCutscene.music);
  nextShot();
}

CutscenePlayer::~CutscenePlayer()
{
  if (mAudio)
    mAudio->stopAllNamed();
}

int CutscenePlayer::totalFrames() const
{
  const auto& s = mCutscene.shots[std::size_t(mShot)];
  return s.repeat == 0 ? -1 : s.clipFrames * s.repeat;
}

void CutscenePlayer::nextShot()
{
  ++mShot;
  if (mShot >= int(mCutscene.shots.size()))
  {
    finish();
    return;
  }
  const auto& s = mCutscene.shots[std::size_t(mShot)];
  mFrame = 0;
  mFrameMs = 0.0f;
  mDelay = s.delayMs;
  mWaiting = false;
  mShotTicks = 0;
  mFired.assign(s.cues.size(), false);
  mSpeaker.clear();
  mLine.clear();
  enterFrame();
}

void CutscenePlayer::finish()
{
  mDone = true;
  if (mAudio)
    mAudio->stopAllNamed();
}

void CutscenePlayer::enterFrame()
{
  const auto& s = mCutscene.shots[std::size_t(mShot)];
  mFrameLength = float(mDelay);
  for (std::size_t i = 0; i < s.cues.size(); ++i)
  {
    const auto& c = s.cues[i];
    if (mFired[i] || c.frame != mFrame)
      continue;
    mFired[i] = true;
    if (c.key == "sound")
    {
      if (mAudio)
        mAudio->playNamed(c.value);
    }
    else if (c.key == "stopsound")
    {
      if (mAudio)
        mAudio->stopNamed(c.value);
    }
    else if (c.key == "music")
    {
      if (mAudio)
        mAudio->playMusicNamed(c.value);
    }
    else if (c.key == "say")
    {
      mSpeaker = c.value;
      mLine = c.text;
      mShown = 0;
      mRevealTicks = 0;
    }
    else if (c.key == "hold")
    {
      mFrameLength += float(std::atof(c.value.c_str()));
    }
    else if (c.key == "delay")
    {
      mDelay = std::max(10, std::atoi(c.value.c_str()));
      mFrameLength = float(mDelay);
    }
    else if (c.key == "fade")
    {
      std::string dir;
      splitColon(c.value, dir, mFadeLen);
      mFadeMs = 0.0f;
      mFadeFrom = dir == "in" ? 1.0f : 0.0f;
      mFadeTo = dir == "in" ? 0.0f : 1.0f;
      if (mFadeLen <= 0.0f)
        mFadeLen = 1.0f;
    }
    else if (c.key == "flash")
    {
      std::string col;
      splitColon(c.value, col, mFlashLen);
      mFlashMs = mFlashLen;
    }
    else if (c.key == "shake")
    {
      std::string amp;
      splitColon(c.value, amp, mShakeLen);
      mShakeAmp = float(std::atof(amp.c_str())) * 3.0f;
      mShakeMs = mShakeLen;
    }
    else if (c.key == "wait")
    {
      mWaiting = true;
    }
  }
}

bool CutscenePlayer::tick(const Input& in, const Input& prev)
{
  if (mDone)
    return false;
  ++mTicks;
  ++mShotTicks;
  auto edge = [&](bool Input::*f) { return in.*f && !(prev.*f); };
  const bool advance = edge(&Input::confirm) || edge(&Input::jump) || edge(&Input::fire);
  const bool skip = edge(&Input::back) || edge(&Input::pause);

  if (skip && mTicks > 4)
  {
    finish();
    return false;
  }
  if (advance && mTicks > 4)
  {
    if (mWaiting)
    {
      mWaiting = false;
      const auto& s = mCutscene.shots[std::size_t(mShot)];
      if (s.repeat == 0)
      {
        nextShot();
        return !mDone;
      }
      mFrameMs = mFrameLength; // move on now
    }
    else
    {
      finish();
      return false;
    }
  }

  // Subtitles appear two characters per blip.
  if (mShown < int(mLine.size()) && ++mRevealTicks >= 3)
  {
    mRevealTicks = 0;
    mShown = std::min(int(mLine.size()), mShown + 2);
    if (mAudio && mLine[std::size_t(mShown - 1)] != ' ')
      mAudio->voiceBlip(mSpeaker);
  }
  if (mFadeLen > 0.0f && mFadeMs < mFadeLen)
    mFadeMs += kTickMs;
  if (mFlashMs > 0.0f)
    mFlashMs -= kTickMs;
  if (mShakeMs > 0.0f)
    mShakeMs -= kTickMs;

  const auto& s = mCutscene.shots[std::size_t(mShot)];
  mFrameMs += kTickMs;
  if (mWaiting)
  {
    // A looping panel keeps animating while it waits; others hold.
    if (s.repeat == 0 && mFrameMs >= float(mDelay))
    {
      mFrameMs = 0.0f;
      ++mFrame;
    }
    return true;
  }
  while (mFrameMs >= mFrameLength && !mDone && !mWaiting)
  {
    mFrameMs -= mFrameLength;
    ++mFrame;
    const int total = totalFrames();
    if (total >= 0 && mFrame >= total)
    {
      nextShot();
      return !mDone;
    }
    enterFrame();
  }
  return !mDone;
}

void CutscenePlayer::render(ClipKit& kit)
{
  auto& r = kit.r;
  if (mDone || mShot < 0 || mShot >= int(mCutscene.shots.size()))
  {
    r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgb(0, 0, 0));
    return;
  }
  const auto& s = mCutscene.shots[std::size_t(mShot)];
  const int total = totalFrames();
  const float t = total > 0 ? std::min(1.0f, (float(mFrame) + mFrameMs / std::max(1.0f, mFrameLength)) / float(total)) : 0.5f;
  float ox = 0.0f, oy = 0.0f;
  if (mShakeMs > 0.0f)
  {
    const float a = mShakeAmp * mShakeMs / std::max(1.0f, mShakeLen);
    ox = std::sin(float(mTicks) * 2.3f) * a;
    oy = std::cos(float(mTicks) * 3.1f) * a;
  }
  const std::string clip = s.clip == "bumper" ? "bumper:" + s.bumperCh + ":" + s.bumperGenre :
    (s.clip == "credits" ? "credits:" + s.credits : s.clip);
  drawClip(kit, clip, mFrame % std::max(1, s.clipFrames), s.clipFrames, t, mShotTicks, ox, oy);

  // Letterbox bars and the subtitle box.
  r.fillRect(0, 0, float(kScreenW), 40, rgb(0, 0, 0));
  r.fillRect(0, float(kScreenH) - 40, float(kScreenW), 40, rgb(0, 0, 0));
  if (!mLine.empty())
  {
    const float bx = 160.0f, by = float(kScreenH) - 170.0f;
    r.fillRect(bx, by, float(kScreenW) - 320.0f, 112, rgba(6, 4, 18, 215));
    r.fillRect(bx, by, float(kScreenW) - 320.0f, 4, kit.theme.accentA);
    r.drawText(mSpeaker, bx + 24, by + 14, {22.0f, kit.theme.accentA, kInk, true});
    r.drawText(mLine.substr(0, std::size_t(mShown)), bx + 24, by + 48, {28.0f, rgb(255, 255, 255), kInk});
  }
  if (mWaiting && (mTicks / 20) % 2 == 0)
    r.drawText("PRESS A BUTTON", float(kScreenW) - 40.0f, float(kScreenH) - 34.0f, {16.0f, rgb(200, 200, 220)}, Align::Right);

  if (mFlashMs > 0.0f)
    r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(255, 255, 255, int(255.0f * mFlashMs / std::max(1.0f, mFlashLen))));
  if (mFadeLen > 0.0f)
  {
    const float f = std::clamp(mFadeMs / mFadeLen, 0.0f, 1.0f);
    const float a = mFadeFrom + (mFadeTo - mFadeFrom) * f;
    if (a > 0.0f)
      r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(0, 0, 0, int(255.0f * a)));
  }
}

} // namespace gr
