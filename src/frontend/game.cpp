#include "frontend/game.hpp"

#include "data/campaign.hpp"

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
    case PlayerState::Swing: return "swing";
    case PlayerState::Cling: return "cling";
    case PlayerState::Swim: return "swim";
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
  , mCursor(options.autoplay ? 0 : options.character)
{
  mCampaign = options.levelPath.empty();
  loadCustomRunners(saveDir());
  mCursor = std::clamp(mCursor, 0, characterCount() - 1);
  mSelectScroll = float(mCursor);
  buildPanels();
  if (!campaign())
  {
    mLevel = std::make_shared<const Level>(Level::loadFile(options.levelPath));
    mMode = Mode::Select;
    if (mOptions.skipMenu)
    {
      mCursor = mOptions.character;
      startLevel();
    }
    else if (mAudio)
    {
      mAudio->playMusic(Music::Menu);
    }
    return;
  }
  mProfile = Profile::load(saveDir());
  mBindings = Bindings::fromProfile(mProfile.controls);
  if (!mOptions.cutscene.empty())
  {
    playCutscenes({mOptions.cutscene}, After::Quit);
  }
  else if (mOptions.skipMenu)
  {
    mCursor = mOptions.character;
    beginCampaignLevel(std::max(1, mOptions.startLevel), mOptions.startLevel <= 1);
  }
  else
  {
    goTitle();
  }
}

void Game::sound(Sfx s)
{
  if (mAudio)
    mAudio->play(s);
}

void Game::cycleTheme()
{
  // A parked level (during its bonus level) and cutscenes hold on to the
  // current art.
  if (mMainWorld || mMode == Mode::Cutscene)
    return;
  setTheme((mThemeIndex + 1) % themeCount());
}

void Game::setTheme(int index)
{
  if (index == mThemeIndex && mArt)
    return;
  // Mid-level, the run carries over into the new look.
  std::optional<SaveGame> progress;
  if (mWorld && mMode == Mode::Play && mWorld->canSave())
    progress = mWorld->snapshot();

  mThemeIndex = index;
  // World holds references into the art, so rebuild it alongside.
  auto newArt = std::make_unique<Art>(Art::build(theme(), mRenderer));
  buildPanels();
  mWorld.reset();
  mArt = std::move(newArt);
  if (progress)
  {
    mWorld = std::make_unique<World>(mLevel, progress->character, theme(), *mArt);
    prepareWorld(*mWorld);
    mWorld->restore(*progress);
    mSubTick = 0;
    mLatched = PlayerInput{};
  }
  else if (mMode == Mode::Play || mMode == Mode::Bonus)
  {
    if (campaign())
    {
      goTitle();
    }
    else
    {
      setMode(Mode::Select);
      if (mAudio)
        mAudio->playMusic(Music::Menu);
    }
  }
}

void Game::setMode(Mode m)
{
  mMode = m;
  mModeTicks = 0;
}

void Game::prepareWorld(World& world) const
{
  // People can also climb into a vehicle with up; the bot always uses USE,
  // so it never boards one by accident in a campaign level.
  world.setUpBoards(!mOptions.autoplay);
  const auto& keys = mBindings.keys[std::size_t(Act::Use)];
  std::string label = keyName(keys[0] > 0 ? keys[0] : keys[1]);
  const auto& pad = mBindings.pad[std::size_t(Act::Use)];
  if (!pad.empty())
    label += " / " + padName(pad[0]);
  world.setUseLabel(label);
}

void Game::startLevel()
{
  mWorld = std::make_unique<World>(mLevel, mCursor, theme(), *mArt);
  prepareWorld(*mWorld);
  if (const auto it = mProfile.scores.find(2); it != mProfile.scores.end())
    mWorld->setHiScore(it->second);
  mBot = Bot{};
  mBot.setTakeBonus(campaign() && mLevelNumber > 0 && !bonusFile(dataDir(), mLevelNumber).empty());
  mSubTick = 0;
  mLatched = PlayerInput{};
  mPrevLogicInput = Input{};
  setMode(Mode::Play);
  playLevelMusic();
}

bool Game::tick(const Input& raw)
{
  ++mFrame;
  ++mModeTicks;
  if (mNoticeTicks > 0)
    --mNoticeTicks;
  Input in = raw;
  auto edge = [&](bool Input::*f) { return in.*f && !(mPrev.*f); };

  if (mMenu != Menu::None)
  {
    tickMenu(in);
    mPrev = in;
    return !mQuit;
  }

  switch (mMode)
  {
    case Mode::Title:
      // F9 on the title screen picks the quick save straight back up.
      if (edge(&Input::quickLoad))
      {
        quickLoad();
        break;
      }
      tickTitle(in);
      break;
    case Mode::Cutscene:
      tickCutscene(in);
      break;
    case Mode::Arsenal:
      tickArsenal(in);
      break;
    case Mode::List:
      tickList(in);
      break;
    case Mode::Continued:
      if (mModeTicks > 60 && (edge(&Input::confirm) || edge(&Input::jump) || edge(&Input::pause) ||
                              (mOptions.autoplay && mModeTicks > 400)))
      {
        if (mOptions.autoplay || mOptions.quitAfterClear)
          return false;
        goTitle();
      }
      break;
    case Mode::Select:
      if (!tickSelect(in, raw))
        return false;
      break;
    case Mode::Editor:
      tickEditor(in);
      break;
    case Mode::Play:
      if (edge(&Input::quickLoad))
      {
        quickLoad();
        break;
      }
      if (edge(&Input::quickSave))
        quickSave();
      if (edge(&Input::pause) && mWorld->state() == WorldState::Playing)
      {
        sound(Sfx::MenuSelect);
        openMenu(Menu::Pause);
        break;
      }
      if (edge(&Input::map) && mWorld->state() == WorldState::Playing)
      {
        sound(Sfx::MenuSelect);
        openMap();
        break;
      }
      if (edge(&Input::swap))
        switchRunner((mWorld->characterIndex() + 1) % characterCount());
      tickPlay(raw);
      break;
    case Mode::Bonus:
      tickBonus(in);
      if (mModeTicks > kBonusStep * (int(mBonuses.size()) + 2) + 200 ||
          (mModeTicks > 60 && !mOptions.autoplay && (edge(&Input::confirm) || edge(&Input::jump))))
        finishTally();
      break;
  }
  mPrev = in;
  return !mQuit;
}

void Game::finishTally()
{
  if (mOptions.quitAfterClear)
  {
    const auto& s = mWorld->stats();
    std::fprintf(stderr,
      "cleared: %s, %.1f s, deaths %d, hits %s, bots %d/%d, gems %d/%d, merch %d/%d, letters %s, score %d (+%d bonus)\n",
      mWorld->character().name.c_str(), double(s.frames) / 15.0, s.deaths, s.tookDamage ? "taken" : "none", s.kills,
      s.enemiesTotal, s.gems, s.gemsTotal, s.merch, s.merchTotal, s.letters.empty() ? "-" : s.letters.c_str(),
      mShownScore, mShownScore - mScoreBeforeBonus);
    if (campaign())
      recordClear();
    mQuit = true;
    return;
  }
  if (campaign())
  {
    recordClear();
    mWorld.reset();
    const int n = mLevelNumber;
    std::vector<std::string> next;
    // The main story and the space episode each end with their own scenes.
    const bool last = n == kCampaignLevels || n == kAllLevels;
    if (n == kCampaignLevels)
      next = {"finale", "post_credits"};
    else if (n == episode(episodeOfLevel(n)).last)
      next = {"end_e" + std::to_string(episodeOfLevel(n))};
    playCutscenes(next, last ? After::Title : After::NextLevel);
    return;
  }
  mWorld.reset();
  setMode(Mode::Select);
  if (mAudio)
    mAudio->playMusic(Music::Menu);
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
    in = mBotInput | raw;
  }
  mLatched.left = in.left;
  mLatched.right = in.right;
  mLatched.up = in.up;
  mLatched.down = in.down;
  mLatched.jump.pressed = in.jump;
  mLatched.fire.pressed = in.fire;
  mLatched.use.pressed = in.use;
  if (in.use && !mPrevLogicInput.use)
    mLatched.use.triggered = true;
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
    if (frameInput.use.triggered)
      frameInput.use.pressed = true;
    mWorld->update(frameInput);
    mLatched.jump.triggered = false;
    mLatched.fire.triggered = false;
    mLatched.use.triggered = false;
    for (const auto s : mWorld->takeSounds())
      sound(s);
    if (mAudio && !mWorld->musicOverride().empty() && mWorld->musicOverride() != mPlayingOverride)
    {
      // A new track (an easter egg, a sector's layer coming back): it picks
      // up where the level's clock is.
      mPlayingOverride = mWorld->musicOverride();
      mAudio->playMusicNamed(mPlayingOverride);
      mAudio->seekMusic(double(mWorld->clock()) / 15.0);
    }

    if (mOptions.trace)
    {
      const auto& p = mWorld->player();
      std::fprintf(stderr, "f%d pos %d,%d %s hp %d w%d/%d turbo %d virus %d in %s%s%s%s%s%s\n", mWorld->stats().frames, p.x, p.y,
        stateName(p.state), p.hp, int(p.weapon), p.ammo, p.turbo, p.virus, frameInput.left ? "L" : "", frameInput.right ? "R" : "", frameInput.up ? "U" : "",
        frameInput.down ? "D" : "", frameInput.jump.pressed ? "J" : "", frameInput.fire.pressed ? "F" : "");
    }
  }
  mSubTick = (mSubTick + 1) % kTicksPerLogicFrame;
  mWorld->tickEffects(renderAlpha());

  if (mWorld->bonusRequested())
  {
    mWorld->clearBonusRequest();
    enterBonus();
    return;
  }
  if (mWorld->state() == WorldState::Done && (mMainWorld || mBonusOnly))
  {
    leaveBonus();
    return;
  }
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
  mCardPanel = makePanel(mRenderer, 340, 440, rgba(10, 8, 26, 170), rgba(255, 255, 255, 70), 22);
  mCardPanelSelected = makePanel(mRenderer, 340, 440, rgba(14, 10, 34, 215), t.accentA, 22);
  mActionButton = makePanel(mRenderer, 260, 44, rgba(10, 8, 26, 170), rgba(255, 255, 255, 80), 14);
  mActionButtonFocus = makePanel(mRenderer, 260, 44, rgba(14, 10, 34, 225), t.accentA, 14);
  mBannerPanel = makePanel(mRenderer, 760, 170, rgba(8, 6, 22, 200), t.accentA, 24);
  mBonusPanel = makePanel(mRenderer, 860, 560, rgba(8, 6, 22, 220), t.accentA, 28);
  mMenuPanel = makePanel(mRenderer, 520, 470, rgba(8, 6, 22, 230), t.accentA, 26);
  mSlotPanel = makePanel(mRenderer, 1000, 520, rgba(8, 6, 22, 235), t.accentA, 26);
  mLoadButton = makePanel(mRenderer, 220, 46, rgba(10, 8, 26, 170), rgba(255, 255, 255, 80), 14);
  mLoadButtonFocus = makePanel(mRenderer, 220, 46, rgba(14, 10, 34, 225), t.accentA, 14);
}

float Game::renderAlpha() const
{
  // render() runs after tick(), and the logic frame ran on the tick that
  // left mSubTick at 1. So 1, 2, 3 are a quarter, half and three quarters of
  // the way to the newest logic state, and 0 means we have reached it.
  // (Treating 0 as "at the previous state" made everything snap back once
  // per logic frame, which showed up as a constant judder.)
  const int sinceUpdate = mSubTick == 0 ? kTicksPerLogicFrame : mSubTick;
  return float(sinceUpdate) / float(kTicksPerLogicFrame);
}

void Game::render()
{
  mRenderer.beginFrame();
  mRenderer.clear(rgb(0, 0, 0));
  const float alpha = renderAlpha();
  switch (mMode)
  {
    case Mode::Title:
      renderTitle();
      break;
    case Mode::Cutscene:
      if (mCutscene && mClipKit)
        mCutscene->render(*mClipKit);
      break;
    case Mode::Arsenal:
      renderArsenal();
      break;
    case Mode::List:
      renderList();
      break;
    case Mode::Continued:
      renderContinued();
      break;
    case Mode::Select:
      renderSelect();
      break;
    case Mode::Editor:
      renderEditor();
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
  if (mMenu != Menu::None)
    renderMenu();
  renderNotice();
}

void Game::renderPlayOverlay()
{
  auto& r = mRenderer;
  const auto& t = theme();
  const int ticks = mWorld->stats().frames * kTicksPerLogicFrame + mSubTick;
  if (ticks > 5 && ticks < 110 && mWorld->state() == WorldState::Playing)
  {
    const float a = std::min({1.0f, float(ticks - 5) / 15.0f, float(110 - ticks) / 15.0f});
    const float y = 200.0f - (1.0f - a) * 20.0f;
    DrawOpts o;
    o.alpha = a;
    r.draw(mBannerPanel, 260, y, o);
    std::string label = mLevel->name.empty() ? "STAGE 1" : mLevel->name;
    std::string big = t.name, small = t.tagline;
    if (mLevelNumber > 0 && (mMainWorld || mBonusOnly))
    {
      label = "WE INTERRUPT THIS PROGRAM";
      big = "BONUS LEVEL";
      const bool parts = mLevel->rules.find("radial_gravity") != std::string::npos; // Planetoids
      small = mLevel->goal.rfind("collect:", 0) == 0
        ? "COLLECT " + mLevel->goal.substr(8) + (parts ? " SHIP PARTS" : " GEMS")
        : "REACH THE EXIT BEFORE THE TIME RUNS OUT";
    }
    else if (mLevelNumber > 0)
    {
      const auto& ep = episode(episodeOfLevel(mLevelNumber));
      label = "EPISODE " + std::to_string(ep.number) + "  -  LEVEL " + std::to_string(mLevelNumber);
      big = campaignLevel(mLevelNumber).title;
      small = ep.name;
    }
    r.drawText(label, 640, y + 16, {22.0f, t.hudText}, Align::Center, a);
    r.drawText(big, 640, y + 46, {56.0f, t.accentA, kInk, true}, Align::Center, a);
    r.drawText(small, 640, y + 122, {20.0f, rgb(200, 200, 216)}, Align::Center, a);
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
  std::snprintf(buf, sizeof(buf), "%s  -  %d:%02d  -  BOTS %d/%d  -  GEMS %d/%d", mWorld->character().name.c_str(),
    s.frames / 15 / 60, (s.frames / 15) % 60, s.kills, s.enemiesTotal, s.gems, s.gemsTotal);
  r.drawText(buf, 640, y + 118, {22.0f, t.hudText}, Align::Center, a);

  float ly = y + 172;
  if (mBonuses.empty() && mModeTicks > kBonusStep)
  {
    r.drawText(s.cheated ? "CHEATS USED - NO BONUSES, NO RECORDS" : "NO BONUSES THIS TIME", 640, ly,
      {28.0f, rgb(200, 200, 216), kInk}, Align::Center);
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
