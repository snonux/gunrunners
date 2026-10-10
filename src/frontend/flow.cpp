// The campaign around the levels: title menu, runner select, briefings and
// episode cutscenes, bonus levels behind the B, the Arsenal, the Bonus
// Channel and Reruns, and the profile that remembers all of it.

#include "frontend/game.hpp"

#include "data/campaign.hpp"
#include "data/weapons.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>

namespace gr
{

namespace
{

constexpr Color kInk = rgb(16, 12, 26);

// Briefings first, then episode endings, in story order.
int cutsceneOrder(const std::string& name)
{
  if (name == "opening")
    return 0;
  if (name.rfind("brief_", 0) == 0)
  {
    const int n = std::atoi(name.c_str() + 6);
    return n * 10 + 1;
  }
  if (name.rfind("end_e", 0) == 0)
    return episode(std::atoi(name.c_str() + 5)).last * 10 + 5;
  if (name == "finale")
    return 1000;
  if (name == "post_credits")
    return 1001;
  return 2000;
}

std::string cutsceneLabel(const std::string& name)
{
  char buf[96];
  if (name == "opening")
    return "OPENING  -  THE STATION 42 DINER";
  if (name.rfind("brief_", 0) == 0)
  {
    const int n = std::atoi(name.c_str() + 6);
    std::snprintf(buf, sizeof(buf), "BRIEFING %02d  -  %s", n, campaignLevel(n).title);
    return buf;
  }
  if (name.rfind("end_e", 0) == 0)
  {
    const int e = std::atoi(name.c_str() + 5);
    std::snprintf(buf, sizeof(buf), "EPISODE %d ENDING  -  %s", e, episode(e).name);
    return buf;
  }
  if (name == "finale")
    return "THE FINALE";
  if (name == "post_credits")
    return "AFTER THE CREDITS";
  std::string up = name;
  for (auto& c : up)
    c = c == '_' ? ' ' : char(std::toupper(static_cast<unsigned char>(c)));
  return up;
}

// Episodes and art sets until every level has its own look: one of the
// three theme families per episode.
int themeFor(int ep, const std::string& key, int fallback)
{
  if (const int v = themeIndexForKey(key.c_str()); v >= 0)
    return v;
  if (key.rfind("station", 0) == 0)
    return int(ThemeId::StationZero);
  if (ep <= 0)
    return fallback;
  return (ep - 1) % 3;
}

const char* modeName(FireMode m)
{
  switch (m)
  {
    case FireMode::Tap: return "TAP";
    case FireMode::Auto: return "AUTO";
    case FireMode::Hold: return "HOLD";
    case FireMode::Charge: return "CHARGE";
    case FireMode::Place: return "PLACE";
  }
  return "";
}

} // namespace

std::string Game::dataDir() const
{
  return mOptions.dataDir.empty() ? std::string(GR_DATA_DIR) : mOptions.dataDir;
}

std::string Game::saveDir() const { return mOptions.saveDir.empty() ? defaultSaveDir() : mOptions.saveDir; }

// --- Title -------------------------------------------------------------------

void Game::goTitle()
{
  mWorld.reset();
  mMainWorld.reset();
  mCutscene.reset();
  mBonusOnly = false;
  mLevelNumber = 0;
  mTitleCursor = 0;
  setMode(Mode::Title);
  if (mAudio)
    mAudio->playMusic(Music::Menu);
}

std::vector<std::string> Game::titleItems() const
{
  std::vector<std::string> items{"NEW GAME"};
  const int reached = std::clamp(mProfile.reached, 1, kCampaignLevels);
  if (reached > 1 && !levelFile(dataDir(), reached).empty())
    items.emplace_back("CONTINUE");
  // Episode 7 is a side episode: open from the start, with its own progress.
  if (firstSpaceLevel(dataDir()) > 0)
    items.emplace_back("DEEP SPACE");
  items.emplace_back("LOAD GAME");
  if (reached > 1 || mProfile.spaceReached > 0)
    items.emplace_back("LEVEL SELECT");
  items.emplace_back("ARSENAL");
  if (!mProfile.stars.empty())
    items.emplace_back("BONUS CHANNEL");
  if (!mProfile.cutscenes.empty())
    items.emplace_back("RERUNS");
  if (!extraFile(dataDir(), 0).empty())
    items.emplace_back("EXTRAS");
  if (std::ifstream(dataDir() + "/levels/level1.txt"))
    items.emplace_back("TRAINING STAGE");
  if (mOptions.window)
    items.emplace_back(mFullscreenNow ? "FULLSCREEN: ON" : "FULLSCREEN: OFF");
  items.emplace_back("CONTROLS");
  items.emplace_back("QUIT");
  return items;
}

void Game::tickTitle(const Input& raw)
{
  Input in = raw;
  if (mOptions.autoplay && mModeTicks > 90 && mModeTicks % 20 == 0)
    in.confirm = true; // NEW GAME is the first item
  auto edge = [&](bool Input::*f) { return in.*f && !(mPrev.*f); };
  const auto items = titleItems();
  const int n = int(items.size());
  if (edge(&Input::up) || edge(&Input::down))
  {
    mTitleCursor = (mTitleCursor + (edge(&Input::up) ? n - 1 : 1)) % n;
    sound(Sfx::MenuMove);
  }
  mTitleCursor = std::clamp(mTitleCursor, 0, n - 1);
  if (edge(&Input::pause) && !in.confirm)
  {
    mQuit = true;
    return;
  }
  if (!(edge(&Input::confirm) || edge(&Input::jump)) || mModeTicks < 20)
    return;
  sound(Sfx::MenuSelect);
  const std::string& item = items[std::size_t(mTitleCursor)];
  if (item == "NEW GAME" || item == "CONTINUE" || item == "TRAINING STAGE")
  {
    mPendingLevel = item == "NEW GAME" ? 1 : (item == "CONTINUE" ? std::clamp(mProfile.reached, 1, kCampaignLevels) : 0);
    mPendingNewGame = item == "NEW GAME";
    if (mOptions.autoplay)
      mCursor = 0;
    setMode(Mode::Select);
  }
  else if (item == "DEEP SPACE")
  {
    // Carry on from the furthest space level started, if it has been built.
    const int furthest = std::clamp(mProfile.spaceReached, kSpaceFirst, kAllLevels);
    mPendingLevel = !levelFile(dataDir(), furthest).empty() ? furthest : firstSpaceLevel(dataDir());
    mPendingNewGame = false;
    if (mOptions.autoplay)
      mCursor = 0;
    setMode(Mode::Select);
  }
  else if (item == "LOAD GAME")
  {
    mSlotsForSave = false;
    openMenu(Menu::Slots);
  }
  else if (item == "LEVEL SELECT")
  {
    openList(ListKind::Levels);
  }
  else if (item == "ARSENAL")
  {
    setMode(Mode::Arsenal);
  }
  else if (item == "BONUS CHANNEL")
  {
    openList(ListKind::BonusChannel);
  }
  else if (item == "RERUNS")
  {
    openList(ListKind::Reruns);
  }
  else if (item == "EXTRAS")
  {
    openList(ListKind::Extras);
  }
  else if (item.rfind("FULLSCREEN", 0) == 0)
  {
    mFullscreenToggle = true;
  }
  else if (item == "CONTROLS")
  {
    mControlsReturn = Menu::None;
    openControls();
  }
  else if (item == "QUIT")
  {
    mQuit = true;
  }
}

void Game::renderTitle()
{
  auto& r = mRenderer;
  const auto& t = theme();
  drawBackdrop(r, *mArt, float(mFrame) * 1.5f, 0.0f, 0.0f);
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(4, 2, 12, 120));
  r.draw(mArt->vignette, 0, 0);

  const float bounce = std::sin(float(mFrame) * 0.06f) * 5.0f;
  const float cx = float(kScreenW) / 2.0f;
  r.drawText("GUNRUNNERS", cx + 7, 40 + bounce + 7, {120.0f, withAlpha(t.platform, 200), 0, true}, Align::Center);
  r.drawText("GUNRUNNERS", cx, 40 + bounce, {120.0f, t.accentA, kInk, true}, Align::Center);
  r.drawText("42 LEVELS  -  42 PROTOTYPES  -  ONE SHOW", cx, 186, {22.0f, t.hudText, kInk}, Align::Center);

  const auto items = titleItems();
  // Twelve items at most (every unlock, a window): they close up a little.
  const bool crowded = items.size() > 10;
  const float top = crowded ? 218.0f : 236.0f;
  const float step = items.size() > 11 ? 34.5f : (crowded ? 38.0f : 44.0f);
  for (std::size_t i = 0; i < items.size(); ++i)
  {
    const bool sel = int(i) == mTitleCursor;
    const float y = top + float(i) * step;
    if (sel)
    {
      r.fillRect(cx - 200, y - 4, 400, 40, withAlpha(t.accentA, 60));
      drawGlow(r, *mArt, cx, y + 16, 170, t.accentA, 0.22f);
    }
    r.drawText(items[i], cx, y, {28.0f, sel ? t.accentA : t.hudText, kInk, sel}, Align::Center);
  }

  char buf[160];
  std::snprintf(buf, sizeof(buf), "ARSENAL %d/%d     BONUS STARS %d/42     DUCKS %d/42     CAMERAS %d/20",
    int(mProfile.protos.size()), kProtoCount, int(mProfile.stars.size()), int(mProfile.ducks.size()),
    int(mProfile.cameras.size()));
  r.drawText(buf, cx, 640, {17.0f, t.accentB, kInk}, Align::Center);
  r.drawText("UP/DOWN choose   ENTER / A select   ESC quit", cx, 676, {15.0f, rgb(200, 200, 220), kInk},
    Align::Center);
}

// --- Levels ------------------------------------------------------------------

bool Game::loadLevel(const std::string& path)
{
  const std::string full = path.empty() || path[0] == '/' ? path : dataDir() + "/" + path;
  try
  {
    mLevel = std::make_shared<const Level>(Level::loadFile(full));
  }
  catch (const std::exception& e)
  {
    std::fprintf(stderr, "cannot load %s: %s\n", full.c_str(), e.what());
    notice("THAT LEVEL WOULD NOT LOAD");
    return false;
  }
  mLevelPath = path;
  return true;
}

void Game::applyLevelLook(int ep, const std::string& themeKey)
{
  const int idx = themeFor(ep, themeKey, mOptions.theme);
  if (idx == mThemeIndex || mMainWorld)
    return;
  mWorld.reset(); // holds references into the art
  mThemeIndex = idx;
  mArt = std::make_unique<Art>(Art::build(theme(), mRenderer));
  buildPanels();
}

void Game::playLevelMusic()
{
  if (!mAudio)
    return;
  if (!mLevel || mLevel->music.empty())
  {
    mAudio->playMusic(Music::Level);
    return;
  }
  // The beat signs run on the level's clock; the music follows it.
  mPlayingOverride = mWorld ? mWorld->musicOverride() : std::string();
  const std::string track = !mPlayingOverride.empty() ? mPlayingOverride : mLevel->music;
  mAudio->playMusicNamed(track);
  mAudio->seekMusic(mWorld ? double(mWorld->clock()) / 15.0 : 0.0);
  // Tracks a level switches to mid-run (the club's chiptune, the cab
  // radio's cover, the blackout's layered score) render in the background.
  if (mWorld)
    for (const auto& id : mWorld->musicVariants())
      mAudio->preloadMusic(id);
  if (const auto at = track.find('@'); at != std::string::npos)
    for (int k = 0; k <= 4; ++k)
      mAudio->preloadMusic(track.substr(0, at + 1) + std::to_string(k));
}

void Game::beginCampaignLevel(int number, bool fromNewGame)
{
  mMainWorld.reset();
  mWorld.reset();
  mBonusOnly = false;
  mLevelNumber = number;
  if (number <= 0)
  {
    // The PoC's training stage.
    if (!loadLevel("levels/level1.txt"))
      return goTitle();
    applyLevelLook(0, "");
    startLevel();
    return;
  }
  const std::string path = levelFile(dataDir(), number);
  if (path.empty())
  {
    setMode(Mode::Continued);
    if (mAudio)
      mAudio->playMusic(Music::Victory);
    return;
  }
  char rel[96];
  std::snprintf(rel, sizeof(rel), "levels/%02d_%s.txt", number, campaignLevel(number).slug);
  if (!loadLevel(rel))
    return goTitle();
  if (mAudio)
    mAudio->preloadMusic(mLevel->music);
  applyLevelLook(episodeOfLevel(number), mLevel->themeKey);
  int& reached = spaceLevel(number) ? mProfile.spaceReached : mProfile.reached;
  if (number > reached)
  {
    reached = number;
    mProfile.save(saveDir());
  }
  std::vector<std::string> cuts;
  if (fromNewGame && number == 1)
    cuts.emplace_back("opening");
  char brief[32];
  std::snprintf(brief, sizeof(brief), "brief_%02d", number);
  cuts.emplace_back(brief);
  playCutscenes(cuts, After::StartLevel);
}

void Game::recordClear()
{
  if (mLevelNumber <= 0 || mBonusOnly || !mWorld || mWorld->stats().cheated)
    return;
  const auto& st = mWorld->stats();
  const int n = mLevelNumber;
  if (st.protoFound && !mLevel->weapon.empty())
  {
    mProfile.protos.insert(mLevel->weapon);
    int& best = mProfile.protoKills[mLevel->weapon];
    best = std::max(best, st.protoKills);
  }
  int& score = mProfile.scores[n];
  score = std::max(score, st.score);
  if (st.duck)
    mProfile.ducks.insert(n);
  if (st.camera)
    mProfile.cameras.insert(n);
  if (mWorld->bonusStar())
    mProfile.stars.insert(n);
  if (spaceLevel(n))
    mProfile.spaceReached = std::max(mProfile.spaceReached, std::min(kAllLevels, n + 1));
  else
    mProfile.reached = std::max(mProfile.reached, std::min(kCampaignLevels, n + 1));
  mProfile.duckMode = mProfile.ducks.size() >= std::size_t(kCampaignLevels);
  if (!mProfile.save(saveDir()))
    std::fprintf(stderr, "could not save the profile to %s\n", saveDir().c_str());
}

// --- Cutscenes ---------------------------------------------------------------

void Game::playCutscenes(std::vector<std::string> names, After after)
{
  mCutQueue.clear();
  const bool forced = after == After::Quit || after == After::List;
  for (auto& name : names)
  {
    if (mOptions.noCutscenes && !forced)
      continue;
    if (cutsceneFile(dataDir(), name).empty())
      continue;
    mCutQueue.push_back(std::move(name));
  }
  mAfter = after;
  mClipKit.reset();
  // Have every track these scenes cue rendering in the background.
  if (mAudio)
    for (const auto& name : mCutQueue)
    {
      const Cutscene cs = Cutscene::load(cutsceneFile(dataDir(), name));
      mAudio->preloadMusic(cs.music);
      for (const auto& shot : cs.shots)
        for (const auto& cue : shot.cues)
          if (cue.key == "music")
            mAudio->preloadMusic(cue.value);
    }
  nextCutscene();
}

void Game::nextCutscene()
{
  mCutscene.reset();
  if (mCutQueue.empty())
  {
    mClipKit.reset();
    if (mAudio)
      mAudio->stopAllNamed();
    afterCutscenes();
    return;
  }
  const std::string name = mCutQueue.front();
  mCutQueue.pop_front();
  mCutscene = std::make_unique<CutscenePlayer>(Cutscene::load(cutsceneFile(dataDir(), name)), mAudio);
  if (!mClipKit)
    mClipKit.reset(new ClipKit{mRenderer, *mArt, theme(), mLevelNumber, {}, {}});
  mClipKit->level = mLevelNumber;
  mClipKit->cameras = mProfile.cameras;
  if (mProfile.cutscenes.insert(name).second && mCampaign)
    mProfile.save(saveDir());
  setMode(Mode::Cutscene);
}

void Game::tickCutscene(const Input& raw)
{
  if (!mCutscene)
    return nextCutscene();
  Input in = raw;
  // The autopilot presses a button once a panel has waited long enough to
  // read (2.5 s), so recorded playthroughs keep the story legible.
  mAutoWait = mCutscene->waiting() ? mAutoWait + 1 : 0;
  if (mOptions.autoplay && mAutoWait >= 150)
  {
    in.confirm = true;
    mAutoWait = 0;
  }
  if (!mCutscene->tick(in, mPrev))
  {
    mModeTicks = 0;
    nextCutscene();
  }
}

void Game::afterCutscenes()
{
  switch (mAfter)
  {
    case After::StartLevel:
      startLevel();
      break;
    case After::NextLevel:
      beginCampaignLevel(mLevelNumber + 1, false);
      break;
    case After::Title:
      goTitle();
      break;
    case After::EnterBonus:
      startBonusWorld();
      break;
    case After::LeaveBonus:
      mWorld = std::move(mMainWorld);
      mLevel = mMainLevel;
      mMainLevel.reset();
      mBot = mMainBot;
      mSubTick = 0;
      mLatched = PlayerInput{};
      mWorld->addBonusReward(mBonusScore, mBonusGems, mBonusWon);
      if (mBonusCheated)
        mWorld->markCheated();
      setMode(Mode::Play);
      playLevelMusic();
      break;
    case After::List:
      openList(mListKind);
      break;
    case After::Quit:
      mQuit = true;
      break;
  }
}

// --- Bonus levels ------------------------------------------------------------

void Game::enterBonus()
{
  const std::string path = bonusFile(dataDir(), mLevelNumber);
  if (path.empty() || !mWorld)
  {
    if (mWorld)
      mWorld->notify("NO SIGNAL");
    return;
  }
  auto level = std::make_shared<const Level>(Level::loadFile(path));
  mMainLevel = mLevel;
  mLevel = std::move(level);
  mMainWorld = std::move(mWorld);
  mMainBot = mBot;
  playCutscenes({"sting_break"}, After::EnterBonus);
}

void Game::startBonusWorld()
{
  const int who = mMainWorld ? mMainWorld->characterIndex() : mCursor;
  mWorld = std::make_unique<World>(mLevel, who, theme(), *mArt);
  prepareWorld(*mWorld);
  mBot = Bot{};
  mSubTick = 0;
  mLatched = PlayerInput{};
  mPrevLogicInput = Input{};
  setMode(Mode::Play);
  playLevelMusic();
}

void Game::leaveBonus()
{
  const auto& st = mWorld->stats();
  mBonusScore = st.score;
  mBonusGems = st.gems;
  mBonusWon = !mWorld->bonusFailed();
  mBonusCheated = st.cheated;
  if (mBonusWon && mLevelNumber > 0 && !mBonusCheated)
  {
    mProfile.stars.insert(mLevelNumber);
    mProfile.save(saveDir());
  }
  mWorld.reset();
  if (mBonusOnly)
  {
    mBonusOnly = false;
    if (mLevelNumber <= 0)
      notice(mBonusWon ? "LEVEL CLEARED" : "BETTER LUCK NEXT TIME");
    else
      notice(mBonusWon ? "BONUS STAR!" : "BETTER LUCK NEXT TIME");
    playCutscenes({"sting_back"}, After::List);
    return;
  }
  playCutscenes({"sting_back"}, After::LeaveBonus);
}

// --- Arsenal -----------------------------------------------------------------

void Game::tickArsenal(const Input& in)
{
  auto edge = [&](bool Input::*f) { return in.*f && !(mPrev.*f); };
  // The grid has a cell per level; levels without a prototype (the space
  // ship's) are skipped.
  int next = mArsenalCursor;
  if (edge(&Input::left))
    next = (next + kProtoCount - 1) % kProtoCount;
  if (edge(&Input::right))
    next = (next + 1) % kProtoCount;
  if (edge(&Input::up) || edge(&Input::down))
  {
    const int step = edge(&Input::up) ? -7 : 7;
    const int rows = kEpisodes * 7;
    for (int k = 1; k <= kEpisodes; ++k)
    {
      const int want = ((protoDef(mArsenalCursor).level - 1 + step * k) % rows + rows) % rows + 1;
      int found = -1;
      for (int i = 0; i < kProtoCount && found < 0; ++i)
        if (protoDef(i).level == want)
          found = i;
      if (found >= 0)
      {
        next = found;
        break;
      }
    }
  }
  if (next != mArsenalCursor)
  {
    mArsenalCursor = next;
    sound(Sfx::MenuMove);
  }
  if (edge(&Input::back) || edge(&Input::pause) || edge(&Input::confirm) || (mOptions.autoplay && mModeTicks > 200))
  {
    sound(Sfx::MenuSelect);
    goTitle();
  }
}

void Game::renderArsenal()
{
  auto& r = mRenderer;
  const auto& t = theme();
  drawBackdrop(r, *mArt, float(mFrame) * 0.8f, 0.0f, 0.0f);
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(4, 2, 12, 190));
  r.drawText("THE ARSENAL", 640, 22, {56.0f, t.accentA, kInk, true}, Align::Center);
  char buf[160];
  std::snprintf(buf, sizeof(buf), "%d OF %d PROTOTYPES RETURNED TO THE CLIENT", int(mProfile.protos.size()),
    kProtoCount);
  r.drawText(buf, 640, 92, {18.0f, t.hudText, kInk}, Align::Center);

  // One row per episode, one cell per level.
  const float gx = 186.0f, gy = 124.0f, cw = 82.0f, ch = 74.0f * 6.0f / float(kEpisodes);
  for (int i = 0; i < kProtoCount; ++i)
  {
    const auto& def = protoDef(i);
    const bool logged = mProfile.protos.count(def.key) > 0;
    const bool sel = i == mArsenalCursor;
    const int slot = def.level - 1;
    const float x = gx + float(slot % 7) * cw, y = gy + float(slot / 7) * ch;
    r.fillRect(x, y, cw - 8, ch - 8, sel ? withAlpha(t.accentA, 90) : rgba(255, 255, 255, logged ? 30 : 12));
    if (logged)
    {
      drawGlow(r, *mArt, x + cw * 0.5f - 4, y + 26, 34, def.color, 0.5f);
      DrawOpts io;
      const Texture& icon = mArt->items[kIconProto];
      io.scale = 50.0f / float(std::max(1, icon.w()));
      io.tint = lerpColor(def.color, rgb(255, 255, 255), 0.3f);
      // Centred in the cell, whatever the icon's anchor.
      r.draw(icon, x + cw * 0.5f - 4 - (0.5f * float(icon.w()) - icon.anchorX()) * io.scale,
        y + 28 - (0.5f * float(icon.h()) - icon.anchorY()) * io.scale, io);
    }
    else
    {
      r.drawText("?", x + cw * 0.5f - 4, y + 12, {30.0f, rgba(255, 255, 255, 70)}, Align::Center);
    }
    std::snprintf(buf, sizeof(buf), "%02d", def.level);
    r.drawText(buf, x + 6, y + 44, {14.0f, sel ? t.hudText : rgb(150, 148, 170)});
  }
  for (int e = 1; e <= kEpisodes; ++e)
    r.drawText(episode(e).name, gx - 10, gy + float(e - 1) * ch + 20, {12.0f, rgb(150, 148, 170)}, Align::Right);

  // Details of the selected one.
  const auto& def = protoDef(mArsenalCursor);
  const bool logged = mProfile.protos.count(def.key) > 0;
  const float px = 770.0f, py = 132.0f;
  r.fillRect(px, py, 460, 424, rgba(8, 6, 22, 220));
  r.fillRect(px, py, 460, 4, logged ? def.color : rgba(255, 255, 255, 60));
  std::snprintf(buf, sizeof(buf), "LEVEL %02d  -  %s", def.level, campaignLevel(def.level).title);
  r.drawText(buf, px + 24, py + 22, {16.0f, rgb(180, 178, 200)});
  r.drawText(logged ? def.name : "NOT FOUND YET", px + 24, py + 50, {34.0f, logged ? def.color : t.hudText, kInk, true});
  if (logged)
  {
    r.drawText(def.blurb, px + 24, py + 104, {16.0f, t.hudText});
    std::snprintf(buf, sizeof(buf), "MODE %s    DAMAGE %d    AMMO %d/%d", modeName(def.mode), def.damage, def.boxAmmo,
      def.maxAmmo);
    r.drawText(buf, px + 24, py + 150, {18.0f, t.accentB, kInk});
    const auto it = mProfile.protoKills.find(def.key);
    std::snprintf(buf, sizeof(buf), "BEST RUN  %d KILLS", it == mProfile.protoKills.end() ? 0 : it->second);
    r.drawText(buf, px + 24, py + 190, {22.0f, t.accentA, kInk, true});
  }
  else
  {
    r.drawText("Find its green box in that level and reach", px + 24, py + 110, {16.0f, rgb(180, 178, 200)});
    r.drawText("the exit to log it here.", px + 24, py + 134, {16.0f, rgb(180, 178, 200)});
  }
  r.drawText("ARROWS browse   ESC / B back", 640, 676, {15.0f, rgb(200, 200, 220), kInk}, Align::Center);
}

// --- Lists: level select, Bonus Channel, Reruns ----------------------------

void Game::openList(ListKind kind)
{
  mListKind = kind;
  mListItems.clear();
  char id[32], label[160];
  switch (kind)
  {
    case ListKind::Levels:
      for (int n = 1; n <= kAllLevels; ++n)
        if (n <= (spaceLevel(n) ? mProfile.spaceReached : std::min(mProfile.reached, kCampaignLevels)) &&
            !levelFile(dataDir(), n).empty())
        {
          std::snprintf(id, sizeof(id), "%d", n);
          std::snprintf(label, sizeof(label), "%02d  %s%s", n, campaignLevel(n).title, mProfile.stars.count(n) ? "  *" : "");
          mListItems.emplace_back(id, label);
        }
      break;
    case ListKind::BonusChannel:
      for (int n : mProfile.stars)
        if (!bonusFile(dataDir(), n).empty())
        {
          std::snprintf(id, sizeof(id), "%d", n);
          std::snprintf(label, sizeof(label), "CHANNEL %02d  -  %s", n, campaignLevel(n).bonus);
          std::string l = label;
          for (auto& c : l)
            c = c == '_' ? ' ' : char(std::toupper(static_cast<unsigned char>(c)));
          mListItems.emplace_back(id, l);
        }
      break;
    case ListKind::Reruns:
    {
      std::vector<std::string> seen(mProfile.cutscenes.begin(), mProfile.cutscenes.end());
      std::stable_sort(seen.begin(), seen.end(),
        [](const std::string& a, const std::string& b) { return cutsceneOrder(a) < cutsceneOrder(b); });
      for (const auto& s : seen)
        if (!cutsceneFile(dataDir(), s).empty())
          mListItems.emplace_back(s, cutsceneLabel(s));
      break;
    }
    case ListKind::Extras:
      for (int i = 0; i < kExtraLevels; ++i)
        if (!extraFile(dataDir(), i).empty())
          mListItems.emplace_back(std::to_string(i), extraLevel(i).title);
      break;
  }
  mListCursor = std::clamp(mListCursor, 0, std::max(0, int(mListItems.size()) - 1));
  setMode(Mode::List);
  if (mAudio)
    mAudio->playMusic(Music::Menu);
}

void Game::tickList(const Input& in)
{
  auto edge = [&](bool Input::*f) { return in.*f && !(mPrev.*f); };
  const int n = int(mListItems.size());
  if (edge(&Input::back) || edge(&Input::pause) || n == 0)
  {
    if (n == 0 && mModeTicks < 30)
      return;
    sound(Sfx::MenuSelect);
    goTitle();
    return;
  }
  if (edge(&Input::up) || edge(&Input::down))
  {
    mListCursor = (mListCursor + (edge(&Input::up) ? n - 1 : 1)) % n;
    sound(Sfx::MenuMove);
  }
  if (!(edge(&Input::confirm) || edge(&Input::jump)) || mModeTicks < 15)
    return;
  sound(Sfx::MenuSelect);
  const std::string& id = mListItems[std::size_t(mListCursor)].first;
  switch (mListKind)
  {
    case ListKind::Levels:
      mPendingLevel = std::atoi(id.c_str());
      mPendingNewGame = false;
      setMode(Mode::Select);
      break;
    case ListKind::BonusChannel:
    {
      const int lv = std::atoi(id.c_str());
      const std::string path = bonusFile(dataDir(), lv);
      mLevelNumber = lv;
      try
      {
        mLevel = std::make_shared<const Level>(Level::loadFile(path));
      }
      catch (const std::exception&)
      {
        notice("THAT CHANNEL IS OFF THE AIR");
        return;
      }
      applyLevelLook(episodeOfLevel(lv), mLevel->themeKey);
      mBonusOnly = true;
      playCutscenes({"sting_break"}, After::EnterBonus);
      break;
    }
    case ListKind::Reruns:
      playCutscenes({id}, After::List);
      break;
    case ListKind::Extras:
    {
      // Played like a bonus level from the Bonus Channel: on its own, and
      // back to this list after.
      const std::string path = extraFile(dataDir(), std::atoi(id.c_str()));
      try
      {
        mLevel = std::make_shared<const Level>(Level::loadFile(path));
      }
      catch (const std::exception&)
      {
        notice("THAT LEVEL WOULD NOT LOAD");
        return;
      }
      mLevelNumber = 0;
      applyLevelLook(0, mLevel->themeKey);
      mBonusOnly = true;
      playCutscenes({}, After::EnterBonus);
      break;
    }
  }
}

void Game::renderList()
{
  auto& r = mRenderer;
  const auto& t = theme();
  drawBackdrop(r, *mArt, float(mFrame) * 0.8f, 0.0f, 0.0f);
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(4, 2, 12, 180));
  const char* title = mListKind == ListKind::Levels ? "LEVEL SELECT"
    : mListKind == ListKind::BonusChannel           ? "THE BONUS CHANNEL"
    : mListKind == ListKind::Extras                 ? "EXTRAS"
                                                     : "RERUNS";
  r.drawText(title, 640, 30, {56.0f, t.accentA, kInk, true}, Align::Center);
  if (mListItems.empty())
  {
    r.drawText("NOTHING HERE YET", 640, 320, {28.0f, t.hudText, kInk}, Align::Center);
    return;
  }
  // Ten rows, scrolled to keep the cursor in view.
  const int rows = 10;
  const int first = std::clamp(mListCursor - rows / 2, 0, std::max(0, int(mListItems.size()) - rows));
  for (int i = first; i < std::min(int(mListItems.size()), first + rows); ++i)
  {
    const bool sel = i == mListCursor;
    const float y = 120.0f + float(i - first) * 50.0f;
    r.fillRect(240, y - 4, 800, 44, sel ? withAlpha(t.accentA, 70) : rgba(255, 255, 255, 16));
    r.drawText(mListItems[std::size_t(i)].second, 270, y + 4, {24.0f, sel ? t.accentA : t.hudText, kInk, sel});
  }
  r.drawText("UP/DOWN choose   ENTER / A play   ESC / B back", 640, 676, {15.0f, rgb(200, 200, 220), kInk},
    Align::Center);
}

void Game::renderContinued()
{
  auto& r = mRenderer;
  const auto& t = theme();
  drawBackdrop(r, *mArt, float(mFrame) * 0.5f, 0.0f, 0.0f);
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(4, 2, 12, 170));
  const float a = std::min(1.0f, float(mModeTicks) / 40.0f);
  r.drawText("TO BE CONTINUED...", 640, 250, {80.0f, t.accentA, kInk, true}, Align::Center, a);
  char buf[128];
  std::snprintf(buf, sizeof(buf), "LEVEL %d  -  %s  -  IS STILL BEING BUILT", mLevelNumber,
    campaignLevel(std::clamp(mLevelNumber, 1, kAllLevels)).title);
  r.drawText(buf, 640, 370, {22.0f, t.hudText, kInk}, Align::Center, a);
  r.drawText("Stay tuned. Your progress and the Arsenal are saved.", 640, 410, {18.0f, rgb(200, 200, 220)},
    Align::Center, a);
}

} // namespace gr
