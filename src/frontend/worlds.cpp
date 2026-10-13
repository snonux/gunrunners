// WORLD SELECT: one screen for everything outside NEW GAME and CONTINUE. A
// card per episode of the campaign, one for DEEP SPACE (Episode 7, a side
// episode open from the start with its own progress) and one for the
// EXTRAS. A card opens its levels in the list screen (flow.cpp).
//
// What is open: Episode 1 from the start, a later campaign episode once the
// campaign has reached its first level, DEEP SPACE and the EXTRAS always.
// Inside an episode, every built level up to the furthest one reached.

#include "frontend/game.hpp"

#include "data/campaign.hpp"
#include "data/theme.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gr
{

namespace
{

constexpr Color kInk = rgb(16, 12, 26);

// The six episodes, DEEP SPACE, then EXTRAS: two rows of four.
constexpr int kWorldCards = kEpisodes + 1;
constexpr int kExtrasCard = kEpisodes;
constexpr int kColumns = 4;
constexpr float kCardW = 280.0f, kCardH = 236.0f, kGap = 20.0f;
constexpr float kGridX = (1280.0f - kColumns * kCardW - (kColumns - 1) * kGap) / 2.0f;
constexpr float kGridY = 126.0f;

// The palette each card is painted in: a level of that world.
const Theme& cardTheme(int card)
{
  static const char* const keys[kWorldCards] = {
    "club_laserdisc", "sandstone_traps", "station_hangar", "haunted_manor",
    "dead_air", "sunset_airships", "starfall", "motor_pool",
  };
  const int i = themeIndexForKey(keys[std::clamp(card, 0, kWorldCards - 1)]);
  return themeByIndex(i >= 0 ? i : card % themeCount());
}

// Text shrunk until it fits the width.
float fitSize(Renderer& r, const std::string& s, float size, float width)
{
  while (size > 10.0f && float(r.text(s, {size}).w()) > width)
    size -= 1.0f;
  return size;
}

} // namespace

std::vector<int> Game::worldLevels(int ep) const
{
  const auto& e = episode(ep);
  // DEEP SPACE's first built level is always open; the campaign's level 1 too.
  const int furthest = e.number == kEpisodes ? std::max(mProfile.spaceReached, firstSpaceLevel(dataDir()))
                                             : std::clamp(mProfile.reached, 1, kCampaignLevels);
  std::vector<int> out;
  for (int n = e.first; n <= std::min(e.last, furthest); ++n)
    if (!levelFile(dataDir(), n).empty())
      out.push_back(n);
  return out;
}

std::string Game::worldLock(int card) const
{
  if (card == kExtrasCard)
    return extraFile(dataDir(), 0).empty() ? "COMING SOON" : "";
  const auto& e = episode(card + 1);
  if (levelFile(dataDir(), e.first).empty())
    return "COMING SOON";
  if (worldLevels(e.number).empty())
  {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "FINISH EPISODE %d", e.number - 1);
    return buf;
  }
  return "";
}

void Game::openWorlds()
{
  mSelectFromList = false;
  mWorldCursor = std::clamp(mWorldCursor, 0, kWorldCards - 1);
  setMode(Mode::Worlds);
  if (mAudio)
    mAudio->playMusic(Music::Menu);
}

void Game::tickWorlds(const Input& in)
{
  auto edge = [&](bool Input::*f) { return in.*f && !(mPrev.*f); };
  if (edge(&Input::back) || edge(&Input::pause))
  {
    sound(Sfx::MenuSelect);
    goTitle();
    return;
  }
  const int side = edge(&Input::left) ? -1 : (edge(&Input::right) ? 1 : 0);
  const int row = edge(&Input::up) || edge(&Input::down) ? kColumns : 0;
  if (side != 0 || row != 0)
  {
    mWorldCursor = (mWorldCursor + side + row + kWorldCards) % kWorldCards;
    sound(Sfx::MenuMove);
  }
  if (!(edge(&Input::confirm) || edge(&Input::jump)) || mModeTicks < 15)
    return;
  if (const std::string lock = worldLock(mWorldCursor); !lock.empty())
  {
    sound(Sfx::MenuMove);
    notice(lock == "COMING SOON" ? "THAT WORLD IS STILL BEING BUILT" : lock + " TO OPEN IT");
    return;
  }
  sound(Sfx::MenuSelect);
  mListCursor = 0;
  if (mWorldCursor == kExtrasCard)
  {
    openList(ListKind::Extras);
    return;
  }
  mListEpisode = mWorldCursor + 1;
  openList(ListKind::Levels);
  // On the furthest level reached: carry on where the run stopped.
  mListCursor = std::max(0, int(mListItems.size()) - 1);
}

void Game::renderWorlds()
{
  auto& r = mRenderer;
  const auto& t = theme();
  drawBackdrop(r, *mArt, float(mFrame) * 0.8f, 0.0f, 0.0f);
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(4, 2, 12, 190));
  r.drawText("WORLD SELECT", 640, 30, {56.0f, t.accentA, kInk, true}, Align::Center);

  char buf[96];
  for (int card = 0; card < kWorldCards; ++card)
  {
    const float x = kGridX + float(card % kColumns) * (kCardW + kGap);
    const float y = kGridY + float(card / kColumns) * (kCardH + kGap);
    const bool sel = card == mWorldCursor;
    const std::string lock = worldLock(card);
    const auto& ct = cardTheme(card);
    tapTarget(x, y, kCardW, kCardH, [this, card](float, float) {
      mWorldCursor = card;
      tapPress(&Input::confirm);
    });

    // The world's sky in bands, its far hills, then a strip of its accent.
    if (sel)
    {
      drawGlow(r, *mArt, x + kCardW / 2, y + kCardH / 2, 190, ct.accentA, 0.30f);
      r.fillRect(x - 5, y - 5, kCardW + 10, kCardH + 10, ct.accentA);
    }
    constexpr int kBands = 12;
    for (int b = 0; b < kBands; ++b)
    {
      const float f = float(b) / float(kBands - 1);
      const Color c = f < 0.5f ? lerpColor(ct.skyTop, ct.skyMid, f * 2.0f) : lerpColor(ct.skyMid, ct.skyBottom, f * 2.0f - 1.0f);
      r.fillRect(x, y + kCardH * float(b) / kBands, kCardW, kCardH / kBands + 1, c);
    }
    for (int h = 0; h < 7; ++h)
    {
      const float hx = x + float(h) * kCardW / 7.0f;
      const float hh = 26.0f + 22.0f * std::fabs(std::sin(float(card * 7 + h) * 1.7f));
      r.fillRect(hx, y + kCardH - 64 - hh, kCardW / 7.0f + 1, hh + 64, withAlpha(ct.farLayer, 230));
    }
    r.fillRect(x, y + kCardH - 64, kCardW, 64, rgba(6, 4, 16, 200));
    r.fillRect(x, y, kCardW, 6, ct.accentA);

    const float cx = x + kCardW / 2;
    if (card == kExtrasCard)
      std::snprintf(buf, sizeof(buf), "BONUS LEVELS");
    else
      std::snprintf(buf, sizeof(buf), "EPISODE %d", card + 1);
    r.drawText(buf, cx, y + 18, {18.0f, ct.accentB, kInk}, Align::Center);
    const std::string name = card == kExtrasCard ? "EXTRAS" : episode(card + 1).name;
    r.drawText(name, cx, y + 44, {fitSize(r, name, 32.0f, kCardW - 24), rgb(255, 255, 255), kInk, sel}, Align::Center);
    const std::string tag = card == kExtrasCard ? "MOTOR POOL AND DEEP DIVE" : episode(card + 1).tagline;
    r.drawText(tag, cx, y + 90, {fitSize(r, tag, 15.0f, kCardW - 24), ct.hudText, kInk}, Align::Center);

    // What has been done there.
    if (card == kExtrasCard)
    {
      int built = 0;
      for (int i = 0; i < kExtraLevels; ++i)
        built += extraFile(dataDir(), i).empty() ? 0 : 1;
      std::snprintf(buf, sizeof(buf), "%d VEHICLE LEVELS", built);
    }
    else
    {
      const auto& e = episode(card + 1);
      const int open = int(worldLevels(e.number).size());
      int stars = 0;
      for (int n = e.first; n <= e.last; ++n)
        stars += int(mProfile.stars.count(n));
      std::snprintf(buf, sizeof(buf), "LEVELS %d-%d   %d/%d OPEN   %d *", e.first, e.last, open, e.last - e.first + 1,
        stars);
    }
    if (lock.empty())
      r.drawText(buf, cx, y + kCardH - 46, {16.0f, t.hudText, kInk}, Align::Center);

    if (!lock.empty())
    {
      r.fillRect(x, y, kCardW, kCardH, rgba(4, 2, 12, 150));
      r.drawText("LOCKED", cx, y + kCardH - 58, {22.0f, rgb(200, 196, 214), kInk}, Align::Center);
      r.drawText(lock, cx, y + kCardH - 30, {15.0f, rgb(170, 166, 190), kInk}, Align::Center);
    }
  }

  r.drawText(mOptions.touch ? "TAP a world to choose a level   BACK to leave" : "ARROWS choose   ENTER / A open   ESC / B back", 640, 676, {15.0f, rgb(200, 200, 220), kInk}, Align::Center);
}

} // namespace gr
