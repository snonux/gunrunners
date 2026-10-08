#include "frontend/game.hpp"

#include <cmath>
#include <cstdio>

namespace td
{

namespace
{

constexpr Color kInk = rgb(20, 16, 28);

} // namespace

Game::Game(const GameOptions& options)
  : mOptions(options)
  , mThemeIndex(options.theme)
  , mArt(std::make_unique<Art>(Art::build(themeByIndex(options.theme))))
  , mLevel(Level::loadFile(options.levelPath))
  , mCursor(options.autoplay ? 0 : options.character)
{
  if (mOptions.skipMenu)
  {
    mCursor = mOptions.character;
    startLevel();
  }
}

void Game::cycleTheme()
{
  mThemeIndex = (mThemeIndex + 1) % themeCount();
  // World holds references into the art, so rebuild it alongside.
  auto newArt = std::make_unique<Art>(Art::build(theme()));
  if (mWorld)
  {
    mWorld.reset();
    mArt = std::move(newArt);
    startLevel();
  }
  else
  {
    mArt = std::move(newArt);
  }
}

void Game::setMode(Mode m)
{
  mMode = m;
  mModeTicks = 0;
}

void Game::startLevel()
{
  mWorld = std::make_unique<World>(mLevel, mCursor, theme(), *mArt);
  mBot = Bot{};
  setMode(Mode::Play);
}

bool Game::tick(const Input& raw)
{
  ++mFrame;
  ++mModeTicks;
  Input in = raw;

  switch (mMode)
  {
    case Mode::Select:
    {
      if (mOptions.autoplay)
        in = mBot.menu(mCursor, mOptions.character, mModeTicks);
      if (in.left && !mPrev.left)
        mCursor = (mCursor + kCharacterCount - 1) % kCharacterCount;
      if (in.right && !mPrev.right)
        mCursor = (mCursor + 1) % kCharacterCount;
      const bool go = (in.confirm && !mPrev.confirm) || (in.jump && !mPrev.jump) ||
        (in.fire && !mPrev.fire);
      if (go && mModeTicks > 20)
      {
        mAttempt = 1;
        startLevel();
      }
      break;
    }
    case Mode::Play:
    {
      if (mOptions.autoplay)
        in = mBot.play(*mWorld);
      mWorld->update(in);
      if (mWorld->state() == WorldState::Cleared && mWorld->stateTicks() > 70)
        setMode(Mode::Clear);
      else if (mWorld->state() == WorldState::Dead && mWorld->stateTicks() > 120)
      {
        ++mAttempt;
        startLevel();
      }
      break;
    }
    case Mode::Clear:
    {
      mWorld->update(Input{});
      const bool go = (in.confirm && !mPrev.confirm) || (in.fire && !mPrev.fire);
      if (mModeTicks > 240 || (go && mModeTicks > 60))
      {
        if (mOptions.quitAfterClear)
        {
          const auto& s = mWorld->stats();
          std::fprintf(stderr, "cleared: %s, try %d, %.1f s, gems %d/%d, bots %d/%d, score %d\n",
            mWorld->character().name, mAttempt, double(s.ticks) / 60.0, s.gems, s.gemsTotal,
            s.kills, s.enemiesTotal, s.score);
          return false;
        }
        mWorld.reset();
        setMode(Mode::Select);
      }
      break;
    }
  }
  mPrev = in;
  return true;
}

void Game::render(Canvas& c) const
{
  switch (mMode)
  {
    case Mode::Select:
      renderSelect(c);
      break;
    case Mode::Play:
      mWorld->draw(c, mFrame);
      renderPlayOverlay(c);
      break;
    case Mode::Clear:
      mWorld->draw(c, mFrame);
      renderClear(c);
      break;
  }
}

void Game::renderSelect(Canvas& c) const
{
  const auto& t = theme();
  drawSky(c, t, mFrame);
  drawBackdrop(c, *mArt, float(mFrame) * 0.6f, 0.0f, 0.0f);
  c.fillRect(0, 0, c.w, c.h, rgba(0, 0, 0, 110));

  // Logo
  const int bounce = int(std::sin(float(mFrame) * 0.08f) * 2.0f);
  c.drawTextCentered("TURBODUDES", c.w / 2 + 2, 6 + bounce + 2, t.platform, 3);
  c.drawTextCentered("TURBODUDES", c.w / 2, 6 + bounce, t.accentA, 3);
  c.drawTextCentered("CHOOSE YOUR DUDE", c.w / 2, 31, t.hudText, 1, kInk);

  for (int i = 0; i < kCharacterCount; ++i)
  {
    const auto& def = characterByIndex(i);
    const bool selected = i == mCursor;
    const int x = 14 + i * 100;
    const int y = 41;
    const int w = 92;
    const int h = 126;
    c.fillRect(x, y, w, h, selected ? rgba(0, 0, 0, 170) : rgba(0, 0, 0, 120));
    if (selected)
    {
      const bool pulse = (mFrame / 8) % 2 == 0;
      c.frameRect(x, y, w, h, pulse ? t.accentA : t.accentB);
      c.frameRect(x + 1, y + 1, w - 2, h - 2, withAlpha(t.accentA, 120));
    }
    else
    {
      c.frameRect(x, y, w, h, rgba(255, 255, 255, 60));
    }

    int frame = kFrameIdle;
    if (selected)
    {
      static constexpr int kCycle[4] = {kFrameRun1, kFrameIdle, kFrameRun2, kFrameIdle};
      frame = kCycle[(mFrame / 7) % 4];
    }
    const auto& img = mArt->characters[std::size_t(i)].frames[std::size_t(frame)];
    c.blit(img, x + (w - 48) / 2, y + 4, false, 0, 3);
    if (!selected)
      c.fillRect(x + 1, y + 1, w - 2, 76, rgba(0, 0, 0, 90));

    c.drawTextCentered(def.name, x + w / 2, y + 80, selected ? t.accentA : t.hudText, 1, kInk);
    c.drawTextCentered(def.role, x + w / 2, y + 89, rgb(170, 170, 190));
    const char* labels[3] = {"SPD", "JMP", "PWR"};
    const int pips[3] = {def.speedPips, def.jumpPips, def.powerPips};
    for (int s = 0; s < 3; ++s)
    {
      const int sy = y + 99 + s * 9;
      c.drawText(labels[s], x + 8, sy, rgb(200, 200, 215));
      for (int k = 0; k < 5; ++k)
        c.fillRect(x + 30 + k * 10, sy + 1, 8, 5, k < pips[s] ? t.accentB : rgba(255, 255, 255, 40));
    }
  }

  c.drawTextCentered("< > PICK    JUMP/FIRE: GO    T: THEME", c.w / 2, 171, rgb(200, 200, 215), 1, kInk);
}

void Game::renderPlayOverlay(Canvas& c) const
{
  const auto& t = theme();
  const int ticks = mWorld->stats().ticks;
  if (ticks > 5 && ticks < 150 && mWorld->state() == WorldState::Playing)
  {
    const int y = 34;
    c.fillRect(40, y, c.w - 80, 40, rgba(0, 0, 0, 170));
    c.frameRect(40, y, c.w - 80, 40, t.accentA);
    std::string label = mLevel.name.empty() ? "STAGE 1" : mLevel.name;
    if (mAttempt > 1)
      label += "  -  TRY " + std::to_string(mAttempt);
    c.drawTextCentered(label, c.w / 2, y + 5, t.hudText);
    c.drawTextCentered(t.name, c.w / 2, y + 15, t.accentA, 2, kInk);
    c.drawTextCentered(t.tagline, c.w / 2, y + 31, rgb(190, 190, 205));
  }
  if (mWorld->state() == WorldState::Dead && mWorld->stateTicks() > 30)
  {
    c.drawTextCentered("OUCH!", c.w / 2 + 2, 66, kInk, 4);
    c.drawTextCentered("OUCH!", c.w / 2, 64, rgb(255, 70, 90), 4);
  }
}

void Game::renderClear(Canvas& c) const
{
  const auto& t = theme();
  const auto& s = mWorld->stats();
  const int panelH = std::min(100, mModeTicks * 6);
  const int y = 90 - panelH / 2;
  c.fillRect(30, y, c.w - 60, panelH, rgba(0, 0, 0, 190));
  c.frameRect(30, y, c.w - 60, panelH, t.accentA);
  if (panelH < 100)
    return;
  c.drawTextCentered("LEVEL CLEAR!", c.w / 2 + 2, y + 10, t.platform, 3);
  c.drawTextCentered("LEVEL CLEAR!", c.w / 2, y + 8, t.accentA, 3);
  char buf[96];
  std::snprintf(buf, sizeof(buf), "%s MADE IT IN %d.%d S", mWorld->character().name, s.ticks / 60, (s.ticks % 60) / 6);
  c.drawTextCentered(buf, c.w / 2, y + 38, t.hudText);
  std::snprintf(buf, sizeof(buf), "GEMS %d/%d    BOTS %d/%d", s.gems, s.gemsTotal, s.kills, s.enemiesTotal);
  c.drawTextCentered(buf, c.w / 2, y + 52, t.accentB);
  std::snprintf(buf, sizeof(buf), "SCORE %06d", s.score);
  c.drawTextCentered(buf, c.w / 2, y + 68, t.accentA, 2, kInk);
}

} // namespace td
