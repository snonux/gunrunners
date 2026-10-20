// HINTS (SETTINGS > ASSISTS, on unless switched off): a tip line at the
// bottom of the screen whenever how to get on is not plain to see, so a
// player never stays stuck.
//
// - `tip=` in a level's header: what is special about the level, shown for
//   its first 12 seconds.
// - `@ hint rect=x0,y0,x1,y1 text="..." after=S`: while the runner is in
//   the rect (blocks, inclusive) the tip shows; with `after=` only once
//   they have been in it S seconds (a tip for a player stuck there).
//   `until=card`: only while a force field is up and the card not held.
//   {JUMP}, {FIRE} and {USE} in the text become the player's own buttons.
// - Tips the world finds itself, until the player has done it once in the
//   level: a pipe overhead (jump to grab it), a crawlspace ahead (down and
//   walk), a launch pad near by (stand on it), and a force field bumped
//   without the access card.
// - Stuck: a minute without progress (no new part of the map, nothing
//   picked up or scored, no checkpoint) and the guide arrow points the way
//   on to the exit (by the access card, if a force field is up), even with
//   GUIDE ARROW off, until something moves on.

#include "game/world.hpp"

#include "render/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace gr
{

namespace
{

constexpr int kStuckFrames = 15 * 60;  // a minute of logic frames with no progress
constexpr int kRegionBlocks = 6;       // the map is seen in squares this big
constexpr int kLevelTipFrames = 15 * 12;
constexpr Color kHudInk = rgb(10, 8, 20);

void replaceAll(std::string& s, const std::string& from, const std::string& to)
{
  for (std::size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size()))
    s.replace(at, from.size(), to);
}

} // namespace

void World::addHintZone(const CellBox& rect, const std::string& text, int afterSeconds, const std::string& until)
{
  HintZone z;
  z.rect = rect;
  z.text = text;
  z.after = std::max(0, afterSeconds) * 15;
  z.card = until == "card";
  mHintZones.push_back(z);
}

void World::setActionLabels(const std::string& jump, const std::string& fire)
{
  mJumpLabel = jump;
  mFireLabel = fire;
}

std::string World::hintWords(std::string text) const
{
  replaceAll(text, "{JUMP}", mJumpLabel);
  replaceAll(text, "{FIRE}", mFireLabel);
  replaceAll(text, "{USE}", mUseLabel);
  return text;
}

void World::noteProgress() { mProgressAt = mStats.frames; }

bool World::stuck() const
{
  return mPlay.hints && mLevel->rules.empty() && mLevel->hasExit && mStats.frames - mProgressAt >= kStuckFrames;
}

void World::updateHints()
{
  const auto& p = mPlayer;
  const CellBox b = p.box();

  // A new part of the map is progress.
  const int rw = (mMap.widthBlocks() + kRegionBlocks - 1) / kRegionBlocks;
  const int rh = (mMap.heightBlocks() + kRegionBlocks - 1) / kRegionBlocks;
  if (mVisited.size() != std::size_t(rw * rh))
    mVisited.assign(std::size_t(rw * rh), 0);
  const int rx = std::clamp((b.left() + b.w / 2) / kCellsPerTile / kRegionBlocks, 0, rw - 1);
  const int ry = std::clamp((b.top() + b.h / 2) / kCellsPerTile / kRegionBlocks, 0, rh - 1);
  if (!mVisited[std::size_t(ry * rw + rx)])
  {
    mVisited[std::size_t(ry * rw + rx)] = 1;
    noteProgress();
  }

  if (mHintFieldTicks > 0)
    --mHintFieldTicks;
  if (p.state == PlayerState::Pipe)
    mHintPipeDone = true;
  if (p.state == PlayerState::OnGround && lowCeiling(p.x))
    mHintCrawlDone = true;
  if (mLaunch > 0 && mLaunchBump == 0)
    mHintPadDone = true;

  mHintText.clear();
  if (!mPlay.hints || p.state == PlayerState::Dying || p.state == PlayerState::Teleporting)
    return;

  if (mStats.frames < kLevelTipFrames)
  {
    const std::string tip = mLevel->headerStr("tip");
    if (!tip.empty())
    {
      mHintText = hintWords(tip);
      return;
    }
  }

  const int cx = b.left() + b.w / 2, cy = b.top() + b.h / 2;
  for (auto& z : mHintZones)
  {
    if (z.card && (p.hasKey || !mMap.hasForceFields()))
    {
      z.inside = 0;
      continue;
    }
    const bool in = cx >= z.rect.left() && cx <= z.rect.right() && cy >= z.rect.top() && cy <= z.rect.bottom();
    z.inside = in ? z.inside + 1 : 0;
    if (in && z.inside > z.after && mHintText.empty())
      mHintText = hintWords(z.text);
  }
  if (!mHintText.empty())
    return;

  if (mHintFieldTicks > 0)
  {
    mHintText = "LOCKED - FIND THE ACCESS CARD: SHOOT THE WHITE BOXES TO OPEN THEM";
    return;
  }
  if (stuck())
  {
    mHintText = "STUCK? FOLLOW THE BLINKING ARROW - IT SHOWS THE WAY ON";
    return;
  }
  if (p.state != PlayerState::OnGround || p.vehicle >= 0)
    return;

  // A pipe overhead, in reach of a jump.
  if (!mHintPipeDone)
  {
    const int reach = jumpHeight() + 1;
    for (int dy = 1; dy <= reach && mHintText.empty(); ++dy)
      for (int x = b.left(); x <= b.right(); ++x)
        if (mMap.climbable(x, b.top() - dy))
        {
          mHintText = "A PIPE OVERHEAD: JUMP UP TO GRAB IT, THEN LEFT / RIGHT - DOWN + " + mJumpLabel + " LETS GO";
          break;
        }
    if (!mHintText.empty())
      return;
  }

  // A gap too low to walk through, just ahead.
  if (!mHintCrawlDone && !lowCeiling(p.x) && (lowCeiling(p.x + p.facing) || lowCeiling(p.x + 2 * p.facing)))
  {
    mHintText = "A LOW GAP: HOLD DOWN AND WALK TO CRAWL THROUGH";
    return;
  }

  // Level 3: a launch pad on this floor, near by.
  if (!mHintPadDone)
    for (const auto& pad : mPads)
      if (pad.fire > 0 && std::abs(pad.y - (b.bottom() + 1)) <= 2 && std::abs(pad.x + pad.w / 2 - cx) <= 24)
      {
        mHintText = "A LAUNCH PAD: STAND ON IT AND THE BEAT THROWS YOU UP";
        return;
      }
}

void World::drawHints(Renderer& r, int frame) const
{
  if (mHintText.empty() || mState != WorldState::Playing || vehicleInReach() >= 0)
    return;
  // Narrow enough to stay clear of the touch stick and buttons in the
  // bottom corners, and just over the voice subtitles (y 594).
  const TextStyle style{22.0f, rgb(255, 236, 120), kHudInk, true};
  constexpr float kMaxW = 480.0f, kBottom = 586.0f;
  std::vector<std::string> lines;
  std::string line, word;
  auto fits = [&](const std::string& s) { return float(r.text(s, style).w()) <= kMaxW; };
  for (std::size_t i = 0; i <= mHintText.size(); ++i)
  {
    if (i < mHintText.size() && mHintText[i] != ' ')
    {
      word += mHintText[i];
      continue;
    }
    if (word.empty())
      continue;
    const std::string longer = line.empty() ? word : line + " " + word;
    if (!line.empty() && !fits(longer))
    {
      lines.push_back(line);
      line = word;
    }
    else
      line = longer;
    word.clear();
  }
  if (!line.empty())
    lines.push_back(line);
  float textW = 0.0f;
  for (const auto& l : lines)
    textW = std::max(textW, float(r.text(l, style).w()));
  const float lineH = 30.0f;
  const float w = textW + 96.0f, h = lineH * float(lines.size()) + 14.0f;
  const float x = float(kScreenW) * 0.5f - w * 0.5f, y = kBottom - h;
  const float pulse = 0.75f + 0.25f * std::sin(float(frame) * 0.12f);
  r.fillRect(x, y, w, h, rgba(10, 8, 20, 200));
  r.fillRect(x, y, 8.0f, h, withAlpha(rgb(255, 214, 64), int(255 * pulse)));
  r.drawText("TIP", x + 22.0f, y + 9.0f, {18.0f, rgb(255, 214, 64), kHudInk, true});
  for (std::size_t i = 0; i < lines.size(); ++i)
    r.drawText(lines[i], x + 72.0f + textW * 0.5f, y + 7.0f + lineH * float(i), style, Align::Center);
}

} // namespace gr
