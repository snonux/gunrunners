#include "frontend/game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gr
{

namespace
{

constexpr Color kInk = rgb(16, 12, 26);
constexpr int kBonusStep = 70; // ticks between bonus lines

const char* stateName(PlayerState s)
{
  switch (s)
  {
    case PlayerState::OnGround: return "ground";
    case PlayerState::Jumping: return "jump";
    case PlayerState::Falling: return "fall";
    case PlayerState::Recovering: return "recover";
    case PlayerState::Ladder: return "ladder";
    case PlayerState::Pipe: return "pipe";
    case PlayerState::Jetpack: return "jetpack";
    case PlayerState::Dying: return "dying";
    case PlayerState::Teleporting: return "exit";
  }
  return "?";
}

} // namespace

Game::Game(const GameOptions& options, Renderer& renderer, Audio* audio)
  : mOptions(options)
  , mRenderer(renderer)
  , mAudio(audio)
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
  else if (mAudio)
  {
    mAudio->playMusic(Music::Menu);
  }
}

void Game::sound(Sfx s)
{
  if (mAudio)
    mAudio->play(s);
}

void Game::cycleTheme()
{
  mThemeIndex = (mThemeIndex + 1) % themeCount();
  // World holds references into the art, so rebuild it alongside.
  auto newArt = std::make_unique<Art>(Art::build(theme(), mRenderer));
  buildPanels();
  if (mWorld && mMode == Mode::Play)
  {
    mWorld.reset();
    mArt = std::move(newArt);
    startLevel();
  }
  else
  {
    mWorld.reset();
    mArt = std::move(newArt);
    if (mMode != Mode::Select)
      setMode(Mode::Select);
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
  mSubTick = 0;
  mLatched = PlayerInput{};
  mPrevLogicInput = Input{};
  setMode(Mode::Play);
  if (mAudio)
    mAudio->playMusic(Music::Level);
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
      {
        mCursor = (mCursor + kCharacterCount - 1) % kCharacterCount;
        sound(Sfx::MenuMove);
      }
      if (in.right && !mPrev.right)
      {
        mCursor = (mCursor + 1) % kCharacterCount;
        sound(Sfx::MenuMove);
      }
      const bool go = (in.confirm && !mPrev.confirm) || (in.jump && !mPrev.jump) ||
        (in.fire && !mPrev.fire);
      if (go && mModeTicks > 20)
      {
        sound(Sfx::MenuSelect);
        startLevel();
      }
      break;
    }
    case Mode::Play:
      tickPlay(raw);
      break;
    case Mode::Bonus:
      tickBonus(in);
      if (mModeTicks > kBonusStep * (int(mBonuses.size()) + 2) + 200)
      {
        if (mOptions.quitAfterClear)
        {
          const auto& s = mWorld->stats();
          std::fprintf(stderr,
            "cleared: %s, %.1f s, deaths %d, hits %s, bots %d/%d, gems %d/%d, merch %d/%d, letters %s, score %d (+%d bonus)\n",
            mWorld->character().name, double(s.frames) / 15.0, s.deaths, s.tookDamage ? "taken" : "none",
            s.kills, s.enemiesTotal, s.gems, s.gemsTotal, s.merch, s.merchTotal,
            s.letters.empty() ? "-" : s.letters.c_str(), mShownScore, mShownScore - mScoreBeforeBonus);
          return false;
        }
        mWorld.reset();
        setMode(Mode::Select);
        if (mAudio)
          mAudio->playMusic(Music::Menu);
      }
      break;
  }
  mPrev = in;
  return true;
}

void Game::tickPlay(const Input& raw)
{
  // The logic runs at 15 Hz; between logic frames, button presses are
  // latched so short taps still register.
  Input in = raw;
  if (mOptions.autoplay)
  {
    if (mSubTick == 0)
      mBotInput = mBot.play(*mWorld);
    in = mBotInput;
  }
  mLatched.left = in.left;
  mLatched.right = in.right;
  mLatched.up = in.up;
  mLatched.down = in.down;
  mLatched.jump.pressed = in.jump;
  mLatched.fire.pressed = in.fire;
  if (in.jump && !mPrevLogicInput.jump)
    mLatched.jump.triggered = true;
  if (in.fire && !mPrevLogicInput.fire)
    mLatched.fire.triggered = true;
  mPrevLogicInput = in;

  if (mSubTick == 0)
  {
    // Held states use the latest input; a tap that was already released
    // still counts as pressed for this frame.
    PlayerInput frameInput = mLatched;
    if (frameInput.jump.triggered)
      frameInput.jump.pressed = true;
    if (frameInput.fire.triggered)
      frameInput.fire.pressed = true;
    mWorld->update(frameInput);
    mLatched.jump.triggered = false;
    mLatched.fire.triggered = false;
    for (const auto s : mWorld->takeSounds())
      sound(s);

    if (mOptions.trace)
    {
      const auto& p = mWorld->player();
      std::fprintf(stderr, "f%d pos %d,%d %s hp %d w%d/%d in %s%s%s%s%s%s\n", mWorld->stats().frames, p.x, p.y,
        stateName(p.state), p.hp, int(p.weapon), p.ammo, frameInput.left ? "L" : "", frameInput.right ? "R" : "", frameInput.up ? "U" : "",
        frameInput.down ? "D" : "", frameInput.jump.pressed ? "J" : "", frameInput.fire.pressed ? "F" : "");
    }
  }
  mWorld->tickEffects();
  mSubTick = (mSubTick + 1) % kTicksPerLogicFrame;

  if (mWorld->state() == WorldState::Done)
  {
    mBonuses = mWorld->bonuses();
    mScoreBeforeBonus = mShownScore = mWorld->stats().score;
    mBonusesShown = 0;
    setMode(Mode::Bonus);
    if (mAudio)
      mAudio->playMusic(Music::Victory);
  }
}

void Game::tickBonus(const Input& /*in*/)
{
  // Each achieved bonus pops in and its points tick onto the score.
  const int step = mModeTicks / kBonusStep;
  if (step >= 1 && step - 1 < int(mBonuses.size()) && mBonusesShown < step && mModeTicks % kBonusStep == 0)
  {
    mBonusesShown = step;
    sound(Sfx::Tally);
  }
  int target = mScoreBeforeBonus;
  for (int i = 0; i < mBonusesShown; ++i)
    target += mBonuses[std::size_t(i)].points;
  if (mShownScore < target)
  {
    mShownScore = std::min(target, mShownScore + 4000);
    if (mFrame % 4 == 0)
      sound(Sfx::Gem);
  }
}

void Game::buildPanels()
{
  const auto& t = theme();
  mCardPanel = makePanel(mRenderer, 340, 470, rgba(10, 8, 26, 170), rgba(255, 255, 255, 70), 22);
  mCardPanelSelected = makePanel(mRenderer, 340, 470, rgba(14, 10, 34, 215), t.accentA, 22);
  mBannerPanel = makePanel(mRenderer, 760, 170, rgba(8, 6, 22, 200), t.accentA, 24);
  mBonusPanel = makePanel(mRenderer, 860, 560, rgba(8, 6, 22, 220), t.accentA, 28);
}

void Game::render()
{
  mRenderer.beginFrame();
  mRenderer.clear(rgb(0, 0, 0));
  const float alpha = float(mSubTick) / float(kTicksPerLogicFrame);
  switch (mMode)
  {
    case Mode::Select:
      renderSelect();
      break;
    case Mode::Play:
      mWorld->draw(mRenderer, mFrame, alpha);
      renderPlayOverlay();
      break;
    case Mode::Bonus:
      mWorld->draw(mRenderer, mFrame, 0.0f);
      renderBonus();
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

    r.drawText(def.name, x + 170, y + 238, {40.0f, selected ? t.accentA : t.hudText, kInk, true}, Align::Center);
    std::string role = def.role;
    role += "  -  ";
    role += weaponName(def.startWeapon);
    r.drawText(role, x + 170, y + 290, {17.0f, rgb(180, 178, 200)}, Align::Center);
    const char* labels[3] = {"HEALTH", "JUMP", "POWER"};
    const int pips[3] = {def.healthPips, def.jumpPips, def.powerPips};
    for (int s = 0; s < 3; ++s)
    {
      const float sy = y + 336 + float(s) * 38;
      r.drawText(labels[s], x + 28, sy, {17.0f, rgb(205, 205, 222)});
      for (int k = 0; k < 5; ++k)
        r.fillRect(x + 128 + float(k) * 38, sy + 5, 32, 13, k < pips[s] ? t.accentB : rgba(255, 255, 255, 40));
    }
  }

  r.drawText("ARROWS move  -  UP climb  -  DOWN crouch  -  Z jump  -  X fire  -  T theme", cx, 680,
    {19.0f, rgb(210, 210, 228), kInk}, Align::Center);
}

void Game::renderPlayOverlay()
{
  auto& r = mRenderer;
  const auto& t = theme();
  const int ticks = mWorld->stats().frames * kTicksPerLogicFrame + mSubTick;
  if (ticks > 5 && ticks < 170 && mWorld->state() == WorldState::Playing)
  {
    const float a = std::min({1.0f, float(ticks - 5) / 15.0f, float(170 - ticks) / 15.0f});
    const float y = 200.0f - (1.0f - a) * 20.0f;
    DrawOpts o;
    o.alpha = a;
    r.draw(mBannerPanel, 260, y, o);
    std::string label = mLevel.name.empty() ? "STAGE 1" : mLevel.name;
    r.drawText(label, 640, y + 16, {22.0f, t.hudText}, Align::Center, a);
    r.drawText(t.name, 640, y + 46, {56.0f, t.accentA, kInk, true}, Align::Center, a);
    r.drawText(t.tagline, 640, y + 122, {20.0f, rgb(200, 200, 216)}, Align::Center, a);
  }
}

void Game::renderBonus()
{
  auto& r = mRenderer;
  const auto& t = theme();
  const auto& s = mWorld->stats();
  const float a = std::min(1.0f, float(mModeTicks) / 20.0f);
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(0, 0, 0, int(120 * a)));
  DrawOpts o;
  o.alpha = a;
  const float y = 90.0f + (1.0f - a) * 40.0f;
  r.draw(mBonusPanel, 210, y, o);
  r.drawText("LEVEL COMPLETE", 646, y + 30, {64.0f, withAlpha(t.platform, 200), 0, true}, Align::Center, a);
  r.drawText("LEVEL COMPLETE", 640, y + 24, {64.0f, t.accentA, kInk, true}, Align::Center, a);
  char buf[128];
  std::snprintf(buf, sizeof(buf), "%s  -  %d:%02d  -  BOTS %d/%d  -  GEMS %d/%d", mWorld->character().name,
    s.frames / 15 / 60, (s.frames / 15) % 60, s.kills, s.enemiesTotal, s.gems, s.gemsTotal);
  r.drawText(buf, 640, y + 118, {22.0f, t.hudText}, Align::Center, a);

  float ly = y + 172;
  if (mBonuses.empty() && mModeTicks > kBonusStep)
  {
    r.drawText("NO BONUSES THIS TIME", 640, ly, {28.0f, rgb(200, 200, 216), kInk}, Align::Center);
  }
  for (int i = 0; i < mBonusesShown && i < int(mBonuses.size()); ++i)
  {
    const auto& b = mBonuses[std::size_t(i)];
    std::snprintf(buf, sizeof(buf), "BONUS %d", i + 1);
    r.drawText(buf, 300, ly, {26.0f, t.accentB, kInk});
    r.drawText(b.name, 450, ly, {26.0f, t.hudText, kInk});
    std::snprintf(buf, sizeof(buf), "%d", b.points);
    r.drawText(buf, 980, ly, {26.0f, t.accentA, kInk}, Align::Right);
    ly += 46.0f;
  }
  std::snprintf(buf, sizeof(buf), "SCORE  %07d", mShownScore);
  r.drawText(buf, 640, y + 462, {54.0f, t.accentA, kInk, true}, Align::Center, a);
}

} // namespace gr
