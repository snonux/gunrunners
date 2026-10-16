#include "frontend/game.hpp"

#include "data/campaign.hpp"
#include "data/hard_mode.hpp"

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

// The cloud settings a run starts with: the command line's, or else
// cloud.txt's, with the command line's CA bundle (Android's bundled roots).
//
// First it removes what an earlier run killed mid-sync or mid-write left
// (Android kills a backgrounded app without running ~Game): sync snapshots
// and downloads, write temps, cloud.txt temps holding the secret key. That
// runs before this run's first sync, and whether or not the cloud is on
// now; the age limit spares the files of another game process syncing the
// same saves right now (see removeCloudLeftovers).
S3Config startupCloud(const std::string& saveDir, const S3Config& given)
{
  removeCloudLeftovers(saveDir);
  if (given.enabled())
    return given;
  S3Config persisted = loadS3Config(saveDir);
  persisted.caBundle = given.caBundle;
  return persisted;
}

} // namespace

Game::Game(const GameOptions& options, Renderer& renderer, Audio* audio)
  : mOptions(options)
  , mRenderer(renderer)
  , mAudio(audio)
  , mThemeIndex(options.theme)
  , mLook(themeByIndex(options.theme))
  , mArt(std::make_unique<Art>(Art::build(themeByIndex(options.theme), renderer)))
  , mCursor(options.autoplay ? 0 : options.character)
{
  mCampaign = options.levelPath.empty();
  mOptions.s3 = startupCloud(saveDir(), mOptions.s3);
  beginCloudRefresh();
  loadCustomRunners(saveDir());
  mCursor = std::clamp(mCursor, 0, characterCount() - 1);
  mSelectScroll = float(mCursor);
  buildPanels();
  mVoiceBank.load(voicesDir());
  mVoice.setLog(mOptions.voiceLog);
  mCutsceneVoices.setDir(voicesDir() + "/cutscenes");
  // A stand-alone level plays with a default profile (no assists, bindings
  // or Arsenal of the player's), which saveProfile never writes to disk.
  if (!campaign())
  {
    mHard = options.hard;
    Level level = Level::loadFile(options.levelPath);
    if (mHard)
      addHardModeEnemies(level);
    useLevel(std::make_shared<const Level>(std::move(level)), {}); // its saves name no file
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
  mHard = mOptions.hard || playOption("opt.hard");
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

Game::~Game()
{
  // Without this, the cloud futures' destructors would hold up quitting until
  // the requests time out (10-40 s on an unreachable or stalling server).
  // Uploads still get a short grace period to finish (cancelCloudSync).
  cancelCloudSync();
}

void Game::sound(Sfx s)
{
  if (mAudio)
    mAudio->play(s);
}

void Game::cycleTheme()
{
  // Cutscenes hold on to the current art.
  if (mMode == Mode::Cutscene)
    return;
  setTheme((mThemeIndex + 1) % themeCount());
}

void Game::setTheme(int index)
{
  if (index == mThemeIndex && mArt)
    return;
  // Only the look changes: the run (a boss phase, a ride, a bonus level's
  // clock, the level parked behind a bonus) carries on untouched.
  useTheme(index);
}

void Game::useTheme(int index)
{
  mThemeIndex = index;
  mLook = themeByIndex(index);
  // In place: worlds keep their references to the palette and the art.
  if (mArt)
    *mArt = Art::build(mLook, mRenderer);
  else
    mArt = std::make_unique<Art>(Art::build(mLook, mRenderer));
  buildPanels();
}

void Game::recoverTextures(RenderReset lost)
{
  mRenderer.recover(lost);
  if (lost != RenderReset::Device)
    return;
  // Every texture is dead. The art is baked again in place, as on a theme
  // switch: worlds, the cutscene's clip kit and the runner editor keep
  // their references to it. Its lazily baked runners and styled enemies
  // start over empty and are baked again as they are drawn.
  if (mArt)
    *mArt = Art::build(mLook, mRenderer);
  else
    mArt = std::make_unique<Art>(Art::build(mLook, mRenderer));
  buildPanels();
  freeMap(); // an open map bakes itself again on its next render
  if (mClipKit)
    mClipKit->cache.clear(); // the clips bake theirs again on first use
  if (mEditor)
    mEditor->bakeTextures();
}

void Game::setMode(Mode m)
{
  // A line can finish over the tally; anywhere else it stops.
  if (m != Mode::Play && m != Mode::Bonus)
    mVoice.stop(mAudio);
  mMode = m;
  mModeTicks = 0;
}

void Game::prepareWorld(World& world) const
{
  // People can also climb into a vehicle with up; the bot always uses USE,
  // so it never boards one by accident in a campaign level.
  world.setUpBoards(!mOptions.autoplay);
  world.setDoubleJump(!mOptions.autoplay); // the bot's moves predate it
  const auto& keys = mBindings.keys[std::size_t(Act::Use)];
  std::string label = keyName(keys[0] > 0 ? keys[0] : keys[1]);
  const auto& pad = mBindings.pad[std::size_t(Act::Use)];
  if (!pad.empty())
    label += " / " + padName(pad[0]);
  world.setUseLabel(label);
  world.setPlayOptions(playOptions());
  // Level 41: QUACK MODE is on the ballot once every duck of levels 1-40 is found.
  bool quack = true;
  for (int n = 1; n <= 40; ++n)
    quack = quack && mProfile.ducks.count(n) != 0;
  world.setSweepsQuack(quack);
  // Level 42: the Encore's wheel holds every prototype logged to the Arsenal.
  world.setFinaleArsenal({mProfile.protos.begin(), mProfile.protos.end()});
}

void Game::startLevel()
{
  mWorld = std::make_unique<World>(mLevel, mCursor, theme(), *mArt);
  prepareWorld(*mWorld);
  mLevelRunner = mCursor;
  if (const auto it = mProfile.scores.find(2); it != mProfile.scores.end())
    mWorld->setHiScore(it->second);
  mBot = Bot{};
  mBot.setTakeBonus(campaign() && mLevelNumber > 0 && !bonusFile(dataDir(), mLevelNumber).empty());
  mSubTick = 0;
  mLatched = PlayerInput{};
  mRun.reset();
  mPrevLogicInput = Input{};
  setMode(Mode::Play);
  // A level (re)starts, campaign or arcade: the preloads it does not ask
  // for again in playLevelMusic (a skipped scene's tracks, another level's
  // variants) are freed there.
  if (mAudio)
    mAudio->markPreloadsStale();
  playLevelMusic();
  beginVoices();
}

bool Game::tick(const Input& input)
{
  const Input raw = input | takeTapInput();
  ++mFrame;
  ++mModeTicks;
  if (mNoticeTicks > 0)
    --mNoticeTicks;
  pollCloud();
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
    case Mode::Worlds:
      tickWorlds(in);
      break;
    case Mode::Cloud:
      tickCloud(in);
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
        switchRunner(mWorld->nextFreeRunner(mWorld->characterIndex()));
      else if (edge(&Input::randomRunner))
        switchRandomRunner();
      if (edge(&Input::nextWeapon))
        mWorld->nextWeapon();
      if (edge(&Input::say) && mWorld->state() == WorldState::Playing)
        mVoice.sayRandom(*mWorld, mAudio, voiceVolume());
      // DIFFICULTY switched in a fight or a ride: the level changes the
      // moment it could be saved again.
      if (mRefitHard && refitDifficulty())
      {
        mRefitHard = false;
        notice(mHard ? "HARD - THE EXTRA ENEMIES ARE IN" : "NORMAL - THE EXTRA ENEMIES ARE GONE");
      }
      mVoice.tick();
      tickPlay(raw);
      // SWAP RUNNER ON DEATH: every runner is down, so the level starts over
      // from scratch with all of them.
      if (mMode == Mode::Play && mWorld && !mMainWorld && mWorld->wipedOut())
      {
        mCursor = mLevelRunner;
        startLevel();
        mWorld->notify("ALL RUNNERS DOWN - THE LEVEL STARTS OVER");
      }
      break;
    case Mode::Bonus:
      mVoice.tick();
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
      "%s: %s, %.1f s, deaths %d, hits %s, bots %d/%d, gems %d/%d, merch %d/%d, letters %s, score %d (+%d bonus)\n",
      mWorld->bonusFailed() ? "bonus failed" : "cleared",
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
    // SWAP RUNNER ON DEATH: every runner is back for the next level; if the
    // one picked fell here, the runner who finished carries on.
    if (mWorld->benched(mCursor))
      mCursor = mWorld->characterIndex();
    mWorld.reset();
    const int n = mLevelNumber;
    std::vector<std::string> next;
    // The main story and the space episode each end with their own scenes.
    const bool last = n == kCampaignLevels || n == kAllLevels;
    if (n == kCampaignLevels)
    {
      // The post-credits scene plays once every $ cache and every G-U-N
      // of the campaign has been found.
      next = {"finale"};
      bool all = true;
      for (int k = 1; k <= kCampaignLevels; ++k)
        all = all && mProfile.secrets.count(k) != 0;
      if (all)
        next.push_back("post_credits");
    }
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
  // Touch (thumbs): the stick pushed up jumps, but on a ladder it climbs
  // (a diagonal still jumps off).
  const auto& player = mWorld->player();
  if (in.upJump && player.state == PlayerState::Ladder && !in.left && !in.right)
    in.jump = false;
  // A shot aimed left or right turns the runner that way first, unless the
  // stick is steering (turning on the spot takes one logic frame).
  if (in.fire && (in.aimLeft || in.aimRight) && !in.left && !in.right && player.facing != (in.aimLeft ? -1 : 1))
    (in.aimLeft ? in.left : in.right) = true;
  // Running: a short tap of a way, then that way again and held. Only the
  // player's own presses count (the bot's steps would read as taps).
  mRun.update(raw.left == raw.right ? 0 : (raw.left ? -1 : 1));
  mLatched.left = in.left;
  mLatched.right = in.right;
  mLatched.run = mRun.running();
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

  // GAME SPEED 75%: every fourth logic frame waits (presses stay latched).
  bool logicFrame = mSubTick == 0;
  if (logicFrame)
  {
    mHeldFrame = playOption("assist.slow") && !mOptions.autoplay && !mWorld->beatTimed() && ++mSlowCount % 4 == 0;
    logicFrame = !mHeldFrame;
  }
  if (logicFrame)
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
    const auto sounds = mWorld->takeSounds();
    for (const auto s : sounds)
      sound(s);
    mVoice.update(*mWorld, sounds, mAudio, voiceVolume());
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
      std::fprintf(stderr, "f%d pos %d,%d %s hp %d w%d/%d turbo %d virus %d in %s%s%s%s%s%s%s\n", mWorld->stats().frames, p.x, p.y,
        stateName(p.state), p.hp, int(p.weapon), p.ammo, p.turbo, p.virus, frameInput.left ? "L" : "", frameInput.right ? "R" : "", frameInput.up ? "U" : "",
        frameInput.down ? "D" : "", frameInput.jump.pressed ? "J" : "", frameInput.fire.pressed ? "F" : "",
        frameInput.run ? " run" : "");
      if (const auto& tw = mWorld->tower(); tw.wrap)
        std::fprintf(stderr, "  tower rot %d route %d\n", tw.rot, tw.routeAt); // Level 35
      if (const auto& ws = mWorld->writers(); ws.on) // Level 38
      {
        std::fprintf(stderr, "  route %d typed %d words", ws.routeAt, ws.bridge.typed);
        for (const auto& w : ws.words)
          std::fprintf(stderr, " %s:%d", w.id.c_str(), int(w.state));
        std::fprintf(stderr, "\n");
      }
      if (const auto& wr = mWorld->wardrobe(); wr.on) // Level 37
        std::fprintf(stderr, "  runner %d route %d booth %d rack %d tumble %d\n", mWorld->characterIndex(), wr.routeAt,
          wr.boothT, wr.onRack, wr.tumble);
      if (const auto& wh = mWorld->warehouse(); wh.on) // Level 36
      {
        std::fprintf(stderr, "  route %d carry %d", wh.routeAt, wh.carry);
        for (const auto& c : wh.crates)
          std::fprintf(stderr, " %s:%d,%d/%d", c.id.c_str(), c.x, c.y, int(c.state));
        for (const auto& c : wh.cranes)
          std::fprintf(stderr, " %s:%d/%d", c.id.c_str(), c.x, int(c.phase));
        std::fprintf(stderr, "\n");
      }
      if (const auto& t = mWorld->trench(); t.line)
        std::fprintf(stderr, "  line %d-%d @%d\n", t.lineX0, t.lineX1, t.lineY); // Level 30
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
  if (mHeldFrame)
    return 1.0f; // a logic frame skipped for GAME SPEED 75%: stay on the newest state
  const int sinceUpdate = mSubTick == 0 ? kTicksPerLogicFrame : mSubTick;
  return float(sinceUpdate) / float(kTicksPerLogicFrame);
}

void Game::render()
{
  clearTapTargets();
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
      tapTarget(0, 0, float(kScreenW), float(kScreenH), [this](float, float) { tapPress(&Input::confirm); });
      break;
    case Mode::Arsenal:
      renderArsenal();
      break;
    case Mode::List:
      renderList();
      break;
    case Mode::Worlds:
      renderWorlds();
      break;
    case Mode::Cloud:
      renderCloud();
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
      renderSubtitle();
      break;
    case Mode::Bonus:
      mWorld->draw(mRenderer, mFrame, 0.0f);
      renderBonus();
      renderSubtitle();
      tapTarget(0, 0, float(kScreenW), float(kScreenH), [this](float, float) { tapPress(&Input::confirm); });
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

std::string Game::voicesDir() const
{
#ifdef __ANDROID__
  // Read straight from the APK's assets (they are not unpacked).
  return "data/voices";
#else
  return dataDir() + "/voices";
#endif
}

float Game::voiceVolume() const
{
  // RUNNER VOICES: OFF, QUIET, NORMAL (the default), LOUD.
  static const float kVolumes[] = {0.0f, 0.45f, 0.8f, 1.15f};
  const auto it = mProfile.controls.find("opt.voices");
  const int v = it == mProfile.controls.end() || it->second.empty() ? 2 : it->second[0];
  return kVolumes[std::clamp(v, 0, 3)];
}

void Game::beginVoices()
{
  if (mLevel)
    mVoice.begin(&mVoiceBank, *mLevel);
}

void Game::renderSubtitle()
{
  if (mVoice.subtitle().empty() || !playOption("opt.subtitles", true))
    return;
  const std::string text = mVoice.speaker() + ": " + mVoice.subtitle();
  const TextStyle style{22.0f, rgb(255, 255, 255), kInk, true};
  // A dark band behind the line keeps it readable over signs and sky.
  const Texture& tex = mRenderer.text(text, style);
  mRenderer.fillRect(640.0f - float(tex.w()) / 2.0f - 14.0f, 594.0f, float(tex.w()) + 28.0f, float(tex.h()) + 12.0f,
    rgba(0, 0, 0, 150));
  mRenderer.drawText(text, 640, 600, style, Align::Center);
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
