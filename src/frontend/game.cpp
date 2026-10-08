#include "frontend/game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gr
{

namespace
{

constexpr Color kInk = rgb(16, 12, 26);

} // namespace

Game::Game(const GameOptions& options, Renderer& renderer)
  : mOptions(options)
  , mRenderer(renderer)
  , mThemeIndex(options.theme)
  , mArt(std::make_unique<Art>(Art::build(themeByIndex(options.theme), renderer)))
  , mLevel(Level::loadFile(options.levelPath))
  , mCursor(options.autoplay ? 0 : options.character)
{
  buildPanels();
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
  auto newArt = std::make_unique<Art>(Art::build(theme(), mRenderer));
  buildPanels();
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

void Game::buildPanels()
{
  const auto& t = theme();
  mCardPanel = makePanel(mRenderer, 340, 470, rgba(10, 8, 26, 170), rgba(255, 255, 255, 70), 22);
  mCardPanelSelected = makePanel(mRenderer, 340, 470, rgba(14, 10, 34, 215), t.accentA, 22);
  mBannerPanel = makePanel(mRenderer, 760, 170, rgba(8, 6, 22, 200), t.accentA, 24);
  mClearPanel = makePanel(mRenderer, 820, 400, rgba(8, 6, 22, 215), t.accentA, 28);
}

void Game::render()
{
  mRenderer.beginFrame();
  mRenderer.clear(rgb(0, 0, 0));
  switch (mMode)
  {
    case Mode::Select:
      renderSelect();
      break;
    case Mode::Play:
      mWorld->draw(mRenderer, mFrame);
      renderPlayOverlay();
      break;
    case Mode::Clear:
      mWorld->draw(mRenderer, mFrame);
      renderClear();
      break;
  }
}

void Game::renderSelect()
{
  auto& r = mRenderer;
  const auto& t = theme();
  drawBackdrop(r, *mArt, float(mFrame) * 1.5f, 0.0f, 0.0f);
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(4, 2, 12, 110));
  r.draw(mArt->vignette, 0, 0);

  const float bounce = std::sin(float(mFrame) * 0.06f) * 5.0f;
  const float cx = float(kScreenW) / 2.0f;
  r.drawText("GUNRUNNERS", cx + 7, 22 + bounce + 7, {104.0f, withAlpha(t.platform, 200), 0, true}, Align::Center);
  r.drawText("GUNRUNNERS", cx, 22 + bounce, {104.0f, t.accentA, kInk, true}, Align::Center);
  r.drawText("CHOOSE YOUR RUNNER", cx, 150, {26.0f, t.hudText, kInk}, Align::Center);

  for (int i = 0; i < kCharacterCount; ++i)
  {
    const auto& def = characterByIndex(i);
    const bool selected = i == mCursor;
    const float x = 100.0f + float(i) * 370.0f;
    const float y = 196.0f;
    if (selected)
      drawGlow(r, *mArt, x + 170, y + 200, 300, t.accentA, 0.18f + 0.06f * std::sin(float(mFrame) * 0.12f));
    r.draw(selected ? mCardPanelSelected : mCardPanel, x, y);

    DrawOpts portrait;
    if (!selected)
      portrait.tint = rgb(120, 116, 140);
    const float hop = selected ? -std::abs(std::sin(float(mFrame) * 0.1f)) * 10.0f : 0.0f;
    r.draw(mArt->characters[std::size_t(i)].portrait, x + 170, y - 18 + hop, portrait);

    r.drawText(def.name, x + 170, y + 254, {40.0f, selected ? t.accentA : t.hudText, kInk, true}, Align::Center);
    r.drawText(def.role, x + 170, y + 306, {18.0f, rgb(180, 178, 200)}, Align::Center);
    const char* labels[3] = {"SPEED", "JUMP", "POWER"};
    const int pips[3] = {def.speedPips, def.jumpPips, def.powerPips};
    for (int s = 0; s < 3; ++s)
    {
      const float sy = y + 348 + float(s) * 36;
      r.drawText(labels[s], x + 28, sy, {17.0f, rgb(205, 205, 222)});
      for (int k = 0; k < 5; ++k)
        r.fillRect(x + 128 + float(k) * 38, sy + 5, 32, 13, k < pips[s] ? t.accentB : rgba(255, 255, 255, 40));
    }
  }

  r.drawText("LEFT / RIGHT  pick      JUMP / FIRE  go      T  theme", cx, 680, {20.0f, rgb(210, 210, 228), kInk}, Align::Center);
}

void Game::renderPlayOverlay()
{
  auto& r = mRenderer;
  const auto& t = theme();
  const int ticks = mWorld->stats().ticks;
  if (ticks > 5 && ticks < 160 && mWorld->state() == WorldState::Playing)
  {
    const float a = std::min({1.0f, float(ticks - 5) / 15.0f, float(160 - ticks) / 15.0f});
    const float y = 130.0f - (1.0f - a) * 20.0f;
    DrawOpts o;
    o.alpha = a;
    r.draw(mBannerPanel, 260, y, o);
    std::string label = mLevel.name.empty() ? "STAGE 1" : mLevel.name;
    if (mAttempt > 1)
      label += "  -  TRY " + std::to_string(mAttempt);
    r.drawText(label, 640, y + 16, {22.0f, t.hudText}, Align::Center, a);
    r.drawText(t.name, 640, y + 46, {56.0f, t.accentA, kInk, true}, Align::Center, a);
    r.drawText(t.tagline, 640, y + 122, {20.0f, rgb(200, 200, 216)}, Align::Center, a);
  }
  if (mWorld->state() == WorldState::Dead && mWorld->stateTicks() > 30)
    r.drawText("OUCH!", 640, 250, {130.0f, rgb(255, 70, 100), kInk, true}, Align::Center);
}

void Game::renderClear()
{
  auto& r = mRenderer;
  const auto& t = theme();
  const auto& s = mWorld->stats();
  const float a = std::min(1.0f, float(mModeTicks) / 20.0f);
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(0, 0, 0, int(100 * a)));
  DrawOpts o;
  o.alpha = a;
  const float y = 160.0f + (1.0f - a) * 40.0f;
  r.draw(mClearPanel, 230, y, o);
  r.drawText("LEVEL CLEAR!", 646, y + 34, {80.0f, withAlpha(t.platform, 200), 0, true}, Align::Center, a);
  r.drawText("LEVEL CLEAR!", 640, y + 28, {80.0f, t.accentA, kInk, true}, Align::Center, a);
  char buf[96];
  std::snprintf(buf, sizeof(buf), "%s made it in %d.%d seconds", mWorld->character().name, s.ticks / 60, (s.ticks % 60) / 6);
  r.drawText(buf, 640, y + 150, {28.0f, t.hudText}, Align::Center, a);
  std::snprintf(buf, sizeof(buf), "GEMS  %d / %d        BOTS  %d / %d", s.gems, s.gemsTotal, s.kills, s.enemiesTotal);
  r.drawText(buf, 640, y + 205, {26.0f, t.accentB, kInk}, Align::Center, a);
  std::snprintf(buf, sizeof(buf), "SCORE  %06d", s.score);
  r.drawText(buf, 640, y + 270, {54.0f, t.accentA, kInk, true}, Align::Center, a);
}

} // namespace gr
