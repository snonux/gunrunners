// The campaign around the levels: title menu, runner select, briefings and
// episode cutscenes, bonus levels behind the B, the Arsenal, the Bonus
// Channel and Reruns, and the profile that remembers all of it.

#include "frontend/game.hpp"

#include "data/campaign.hpp"
#include "data/hard_mode.hpp"
#include "data/weapons.hpp"
#include "audio/voice_clip.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

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
  if (key == "deep_sea" && ep == 5)
    return themeFor(ep, "fiber_trench", fallback); // Level 30 and Bubble Up; Deep Dive (an extra) keeps its reef
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

bool Game::saveProfile()
{
  if (!campaign())
    return true;
  return mProfile.save(saveDir());
}

// --- Title -------------------------------------------------------------------

void Game::goTitle()
{
  // The menus keep what the disk said about each level file (data/campaign.hpp);
  // opening the title asks again, so level files added (or removed) while the
  // game runs show up here. Not every frame: a few dozen lookups once.
  forgetDataFiles();
  mWorld.reset();
  mMainWorld.reset();
  mCutscene.reset();
  mBonusOnly = false;
  mSelectFromList = false;
  mLevelNumber = 0;
  mTitleCursor = 0;
  setMode(Mode::Title);
  if (mAudio)
  {
    // Nothing at the title plays a preload; up to ~100 MB of them would
    // otherwise wait here for the next level. Dropped first, as the drop
    // would cancel the menu track's own request.
    mAudio->dropPreloads();
    mAudio->playMusic(Music::Menu);
  }
  // Quit from inside a bonus level: the title keeps the level it was entered
  // from, with that level's file, as after quitting that level itself (a save
  // loaded on the title tells by them whether to read its level again).
  restoreMainLevel();
}

#ifdef __EMSCRIPTEN__
constexpr const char* kTitleHint = "UP/DOWN choose   ENTER / A select";
#else
constexpr const char* kTitleHint = "UP/DOWN choose   ENTER / A select   ESC quit";
#endif

std::vector<std::string> Game::titleItems() const
{
  std::vector<std::string> items{"NEW GAME"};
  const int reached = std::clamp(mProfile.reached, 1, kCampaignLevels);
  if (reached > 1 && !levelFile(dataDir(), reached).empty())
    items.emplace_back("CONTINUE");
  // Every episode's levels, DEEP SPACE and the EXTRAS, on one screen.
  items.emplace_back("WORLD SELECT");
  items.emplace_back("LOAD GAME");
  items.emplace_back("ARSENAL");
  if (!mProfile.stars.empty())
    items.emplace_back("BONUS CHANNEL");
  if (!mProfile.cutscenes.empty())
    items.emplace_back("RERUNS");
  // Called every frame: the existence check is cached (data/campaign.cpp).
  if (dataFileExists(dataDir() + "/levels/level1.txt"))
    items.emplace_back("TRAINING STAGE");
  // Difficulty, controls, assists, play options and fullscreen.
  items.emplace_back("SETTINGS");
#ifndef __EMSCRIPTEN__
  // A browser page has no libcurl and nothing to quit to.
  items.emplace_back("CLOUD SAVES");
  items.emplace_back("QUIT");
#endif
  return items;
}

namespace
{

// The banner drops in over this many frames as the shout starts.
constexpr int kBannerSlam = 12;

} // namespace

void Game::titleShout()
{
  mTitleShoutFrame = mFrame;
  // The runners' voice volume (RUNNER VOICES: OFF, QUIET, NORMAL, LOUD).
  const float volume = voiceVolume();
  const std::string path = voicesDir() + "/title_shout.ogg";
  std::vector<unsigned char> data;
  auto clip = std::make_shared<const std::vector<float>>(
    volume > 0.0f && readDataFile(path, data) ? decodeOggVorbis(data) : std::vector<float>{});
  if (mOptions.voiceLog)
    std::printf("voice: title shout frame %d %.2fs%s\n", mFrame, double(clip->size()) / kAudioRate,
      volume > 0.0f ? "" : " (voices off)");
  if (mAudio && !clip->empty())
    mAudio->playVoice(std::move(clip), volume);
}

void Game::tickTitle(const Input& raw)
{
  if (mTitleShoutFrame < 0)
    titleShout();
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
#ifndef __EMSCRIPTEN__
  if (edge(&Input::pause) && !in.confirm)
  {
    mQuit = true;
    return;
  }
#endif
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
  else if (item == "WORLD SELECT")
  {
    openWorlds();
  }
  else if (item == "LOAD GAME")
  {
    mSlotsForSave = false;
    openMenu(Menu::Slots);
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
  else if (item == "SETTINGS")
  {
    openSettings(Menu::None);
  }
  else if (item == "CLOUD SAVES")
  {
    mCloudCursor = 0;
    setMode(Mode::Cloud);
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
  const int slam = mTitleShoutFrame < 0 ? 0 : mFrame - mTitleShoutFrame;
  if (slam < kBannerSlam)
  {
    // With the shout: the banner comes at the screen big and lands.
    const float k = float(slam) / float(kBannerSlam);
    const float scale = 1.0f + 1.4f * (1.0f - k) * (1.0f - k);
    const Texture& shadow = r.text("GUNRUNNERS", {120.0f, withAlpha(t.platform, 200), 0, true});
    const Texture& banner = r.text("GUNRUNNERS", {120.0f, t.accentA, kInk, true});
    DrawOpts o;
    o.scale = scale;
    o.alpha = 0.35f + 0.65f * k;
    o.cull = false;
    // Scaled about the middle of where drawText puts it.
    const auto drawScaled = [&](const Texture& tex, float x, float y) {
      const float midY = y - tex.anchorY() + float(tex.h()) * 0.5f;
      r.draw(tex, x - (float(tex.w()) * 0.5f - tex.anchorX()) * scale,
        midY - (float(tex.h()) * 0.5f - tex.anchorY()) * scale, o);
    };
    drawScaled(shadow, cx + 7, 40 + bounce + 7);
    drawScaled(banner, cx, 40 + bounce);
  }
  else
  {
    r.drawText("GUNRUNNERS", cx + 7, 40 + bounce + 7, {120.0f, withAlpha(t.platform, 200), 0, true}, Align::Center);
    r.drawText("GUNRUNNERS", cx, 40 + bounce, {120.0f, t.accentA, kInk, true}, Align::Center);
  }
  r.drawText("42 LEVELS  -  42 PROTOTYPES  -  ONE SHOW", cx, 186, {22.0f, t.hudText, kInk}, Align::Center);

  const auto items = titleItems();
  // All unlocked items plus settings still fit above the status line.
  const bool crowded = items.size() > 9;
  const float top = crowded ? 218.0f : 236.0f;
  const float step = items.size() > 12 ? 31.0f : (items.size() > 11 ? 34.5f : (crowded ? 38.0f : 44.0f));
  for (std::size_t i = 0; i < items.size(); ++i)
  {
    const bool sel = int(i) == mTitleCursor;
    const float y = top + float(i) * step;
    tapTarget(cx - 220, y + 16 - step * 0.5f, 440, step, [this, i](float, float) {
      mTitleCursor = int(i);
      tapPress(&Input::confirm);
    });
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
  r.drawText(mOptions.touch ? "TAP to choose" : kTitleHint, cx, 676,
    {15.0f, rgb(200, 200, 220), kInk}, Align::Center);
}

void Game::tickCloud(const Input& in)
{
  auto edge = [&](bool Input::*f) { return in.*f && !(mPrev.*f); };
  constexpr int kItems = 3;
  if (edge(&Input::back) || edge(&Input::pause))
  {
    goTitle();
    return;
  }
  if (edge(&Input::up) || edge(&Input::down))
  {
    mCloudCursor = (mCloudCursor + (edge(&Input::up) ? kItems - 1 : 1)) % kItems;
    sound(Sfx::MenuMove);
  }
  if (!(edge(&Input::confirm) || edge(&Input::jump)) || mModeTicks < 15)
    return;
  sound(Sfx::MenuSelect);
  if (mCloudCursor == 2)
  {
    goTitle();
    return;
  }
  if (mCloudCursor == 1)
  {
    std::string error;
    if (!removeS3Config(saveDir(), &error))
    {
      std::fprintf(stderr, "could not remove cloud settings: %s\n", error.c_str());
      notice("COULD NOT REMOVE CLOUD SETTINGS");
      return;
    }
    {
      std::lock_guard<std::mutex> lock(*mCloudCommitMutex);
      mCloudGeneration->fetch_add(1);
    }
    mCloudRefreshPending = false;
    const std::string caBundle = mOptions.s3.caBundle;
    mOptions.s3 = {};
    mOptions.s3.caBundle = caBundle;
    notice("CLOUD SAVES DISABLED");
    return;
  }

  char* clipboard = SDL_GetClipboardText();
  const std::string text = clipboard ? clipboard : "";
  SDL_free(clipboard);
  S3Config config;
  std::string error;
  if (!parseS3Config(text, config, &error))
  {
    std::fprintf(stderr, "invalid cloud settings on clipboard: %s\n", error.c_str());
    notice("CLIPBOARD IS NOT A CLOUD CONFIG");
    return;
  }
  if (!saveS3Config(saveDir(), config, &error))
  {
    std::fprintf(stderr, "could not save cloud settings: %s\n", error.c_str());
    notice("COULD NOT SAVE CLOUD SETTINGS");
    return;
  }
  config.caBundle = mOptions.s3.caBundle;
  {
    std::lock_guard<std::mutex> lock(*mCloudCommitMutex);
    mCloudGeneration->fetch_add(1);
  }
  mOptions.s3 = std::move(config);
  SDL_SetClipboardText("");
  beginCloudRefresh();
  notice("CLOUD SAVES CONFIGURED");
}

void Game::renderCloud()
{
  auto& r = mRenderer;
  const auto& t = theme();
  drawBackdrop(r, *mArt, float(mFrame) * 0.8f, 0.0f, 0.0f);
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(4, 2, 12, 190));
  r.drawText("CLOUD SAVES", 640, 60, {56.0f, t.accentA, kInk, true}, Align::Center);
  r.drawText(mOptions.s3.enabled() ? "ENABLED" : "NOT CONFIGURED", 640, 140,
    {24.0f, mOptions.s3.enabled() ? t.accentB : t.hudText, kInk, true}, Align::Center);
  if (mOptions.s3.enabled())
  {
    r.drawText(mOptions.s3.endpoint, 640, 180, {17.0f, t.hudText, kInk}, Align::Center);
    r.drawText("BUCKET " + mOptions.s3.bucket +
        (mOptions.s3.prefix.empty() ? "" : "  /  " + mOptions.s3.prefix),
      640, 210, {17.0f, t.hudText, kInk}, Align::Center);
  }
  r.drawText("Copy one block containing endpoint, bucket, region, prefix,",
    640, 270, {18.0f, t.hudText, kInk}, Align::Center);
  r.drawText("access_key and secret_key lines, then paste it here.",
    640, 300, {18.0f, t.hudText, kInk}, Align::Center);
  r.drawText("The keys are stored privately and never shown on screen.",
    640, 335, {16.0f, rgb(190, 190, 210), kInk}, Align::Center);

  const std::string items[3] = {"PASTE CONFIG FROM CLIPBOARD", "DISABLE CLOUD SAVES", "BACK"};
  for (int i = 0; i < 3; ++i)
  {
    const float y = 410.0f + float(i) * 65.0f;
    const bool selected = i == mCloudCursor;
    tapTarget(350, y - 10, 580, 54, [this, i](float, float) {
      mCloudCursor = i;
      tapPress(&Input::confirm);
    });
    r.fillRect(350, y - 8, 580, 48,
      selected ? withAlpha(t.accentA, 65) : rgba(255, 255, 255, 16));
    r.drawText(items[i], 640, y, {24.0f, selected ? t.accentA : t.hudText, kInk, selected}, Align::Center);
  }
  r.drawText(mOptions.touch ? "TAP to choose" : "UP/DOWN choose   ENTER / A select   ESC / B back",
    640, 665, {15.0f, rgb(200, 200, 220), kInk}, Align::Center);
}

// --- Levels ------------------------------------------------------------------

std::shared_ptr<const Level> Game::readLevelFile(const std::string& file, bool hard)
{
  try
  {
    Level level = Level::loadFile(file);
    if (hard)
      addHardModeEnemies(level);
    return std::make_shared<const Level>(std::move(level));
  }
  catch (const std::exception& e)
  {
    std::fprintf(stderr, "cannot load %s: %s\n", file.c_str(), e.what());
    // Missing or broken. A level picked from a menu may have been deleted
    // since its lookup was kept (data/campaign.hpp), and a save may name a
    // level that is gone: either way the kept lookups are forgotten, so the
    // menus ask the disk again the next time they look.
    forgetDataFiles();
    notice("THAT LEVEL WOULD NOT LOAD");
    return nullptr;
  }
}

std::shared_ptr<const Level> Game::readLevel(const std::string& path, bool hard)
{
  return readLevelFile(path.empty() || path[0] == '/' ? path : dataDir() + "/" + path, hard);
}

bool Game::loadLevel(const std::string& path)
{
  auto level = readLevel(path, mHard);
  if (!level)
    return false;
  useLevel(std::move(level), path);
  return true;
}

void Game::useLevel(std::shared_ptr<const Level> level, std::string path)
{
  mLevel = std::move(level);
  mLevelPath = std::move(path);
}

void Game::setHard(bool hard)
{
  if (hard == mHard)
    return;
  mHard = hard;
  mProfile.controls["opt.hard"] = {hard ? 1 : 0};
  saveProfile();
}

void Game::applyLevelLook(int ep, const std::string& themeKey)
{
  const int idx = themeFor(ep, themeKey, mOptions.theme);
  if (idx == mThemeIndex || mMainWorld)
    return;
  useTheme(idx);
}

void Game::playLevelMusic()
{
  if (!mAudio)
    return;
  if (!mLevel || mLevel->music.empty())
  {
    mAudio->playMusic(Music::Level);
    mAudio->dropStalePreloads(); // as below
    return;
  }
  // The beat signs run on the level's clock; the music follows it, also
  // when its track is still rendering and starts late (Audio::seekMusic).
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
  // The level has asked for everything it plays: the preloads a level
  // start marked stale (startLevel, beginCampaignLevel) and nobody asked
  // for again go now. Without a level start in between nothing is stale.
  mAudio->dropStalePreloads();
}

void Game::beginCampaignLevel(int number, bool fromNewGame)
{
  mMainWorld.reset();
  mWorld.reset();
  mBonusOnly = false;
  mLevelNumber = number;
  // The last level's preloads that never played (a variant nobody
  // triggered, a cutscene skipped) would hold ~12 MB each: they turn stale
  // and their queue stops, and playLevelMusic frees those this level did
  // not ask for again (theme_chiptune, say, stays for the next club).
  if (mAudio)
    mAudio->markPreloadsStale();
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
    {
      // No level follows to play them. Dropped first, as the drop would
      // cancel the victory track's own request.
      mAudio->dropPreloads();
      mAudio->playMusic(Music::Victory);
    }
    return;
  }
  char rel[96];
  std::snprintf(rel, sizeof(rel), "levels/%02d_%s.txt", number, campaignLevel(number).slug);
  if (!loadLevel(rel))
    return goTitle();
  applyLevelLook(episodeOfLevel(number), mLevel->themeKey);
  int& reached = spaceLevel(number) ? mProfile.spaceReached : mProfile.reached;
  if (number > reached)
  {
    reached = number;
    saveProfile();
  }
  playBriefing(number, fromNewGame);
}

// The level's briefing (after the opening on a new game), then the level.
void Game::playBriefing(int number, bool fromNewGame)
{
  std::vector<std::string> cuts;
  if (fromNewGame && number == 1)
    cuts.emplace_back("opening");
  char brief[32];
  std::snprintf(brief, sizeof(brief), "brief_%02d", number);
  cuts.emplace_back(brief);
  // The single preload worker renders in request order, and
  // beginCampaignLevel's markPreloadsStale has just emptied its queue, so
  // asking for the level's music before playCutscenes queues the scenes'
  // tracks makes it the next the worker renders. The level always plays
  // it, while a skipped briefing plays none of its cues; queued behind them
  // it would be cancelled by the level start (startLevel's
  // markPreloadsStale) and asked for again from there, starting late.
  // The first scene's music is asked for at once. Every shipped briefing
  // opens with its level's track (22-28 open silent, "stop", and cue it
  // later), so it starts as the worker's render is done; the music before
  // it plays on until then (Audio::playMusicNamed never blocks with a
  // sound device). The only different first track is the opening's
  // theme_opening on the new-game path: it goes to the front of the queue
  // and starts after the level's render in progress and its own. Without
  // cutscenes the level starts at once and its track as soon as the
  // worker has it. (Headless recording renders a track on the spot, or
  // waits for the worker's render in progress, so its clips stay the same
  // from run to run.)
  if (mAudio)
    mAudio->preloadMusic(mLevel->music);
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
  if (mWorld->secretsComplete())
    mProfile.secrets.insert(n); // every $ cache and G-U-N: Level 42's post-credits scene counts them
  if (mWorld->bonusStar())
    mProfile.stars.insert(n);
  if (spaceLevel(n))
    mProfile.spaceReached = std::max(mProfile.spaceReached, std::min(kAllLevels, n + 1));
  else
    mProfile.reached = std::max(mProfile.reached, std::min(kCampaignLevels, n + 1));
  mProfile.duckMode = mProfile.ducks.size() >= std::size_t(kCampaignLevels);
  if (!saveProfile())
  {
    std::fprintf(stderr, "could not save the profile to %s\n", saveDir().c_str());
    notice("PROGRESS NOT SAVED - THE DISK REFUSED IT");
  }
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
  mCutscene->setVoices(&mCutsceneVoices, voiceVolume(), playOption("opt.subtitles", true));
  if (!mClipKit)
    mClipKit.reset(new ClipKit{mRenderer, *mArt, theme(), mLevelNumber, {}, {}});
  mClipKit->level = mLevelNumber;
  mClipKit->cameras = mProfile.cameras;
  if (mProfile.cutscenes.insert(name).second)
    saveProfile();
  setMode(Mode::Cutscene);
}

void Game::tickCutscene(const Input& raw)
{
  if (!mCutscene)
    return nextCutscene();
  Input in = raw;
  // The autopilot presses a button once a panel has waited long enough to
  // read (2.5 s), so recorded playthroughs keep the story legible.
  mAutoWait = mCutscene->waiting() && !mCutscene->speaking() ? mAutoWait + 1 : 0;
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
      restoreMainLevel();
      mBot = mMainBot;
      mVoice = mMainVoice;
      mSubTick = 0;
      mLatched = PlayerInput{};
      mRun.reset();
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
  std::shared_ptr<const Level> level;
  try
  {
    level = std::make_shared<const Level>(Level::loadFile(path));
  }
  catch (const std::exception& e)
  {
    // Deleted since its lookup was kept (data/campaign.hpp), or broken: no
    // bonus level, as if it had never been built, and ask the disk again.
    std::fprintf(stderr, "cannot load %s: %s\n", path.c_str(), e.what());
    forgetDataFiles();
    mWorld->notify("NO SIGNAL");
    return;
  }
  // The level and its file are parked with its world: no save names the
  // bonus level, so a save loaded in it always reads its level again.
  mMainLevel = mLevel;
  mMainLevelPath = mLevelPath;
  useLevel(std::move(level), {});
  mMainWorld = std::move(mWorld);
  mMainBot = mBot;
  mMainVoice = mVoice;
  playCutscenes({"sting_break"}, After::EnterBonus);
}

void Game::restoreMainLevel()
{
  if (!mMainLevel)
    return;
  useLevel(std::move(mMainLevel), std::move(mMainLevelPath));
  mMainLevel.reset();
  mMainLevelPath.clear();
}

void Game::startBonusWorld()
{
  const int who = mMainWorld ? mMainWorld->characterIndex() : mCursor;
  mWorld = std::make_unique<World>(mLevel, who, theme(), *mArt);
  prepareWorld(*mWorld);
  if (mMainWorld)
    mWorld->setBench(mMainWorld->bench());
  // Cheats used on the way to the TV count for its bonus level too: no star.
  if (mMainWorld && mMainWorld->stats().cheated)
    mWorld->markCheated();
  mBot = Bot{};
  mSubTick = 0;
  mLatched = PlayerInput{};
  mRun.reset();
  mPrevLogicInput = Input{};
  setMode(Mode::Play);
  playLevelMusic();
  beginVoices();
}

void Game::leaveBonus()
{
  const auto& st = mWorld->stats();
  mBonusScore = st.score;
  mBonusGems = st.gems;
  mBonusWon = !mWorld->bonusFailed();
  mBonusCheated = st.cheated;
  bool saved = true;
  if (mBonusWon && mLevelNumber > 0 && !mBonusCheated)
  {
    mProfile.stars.insert(mLevelNumber);
    saved = saveProfile();
    if (!saved)
      notice("BONUS STAR NOT SAVED - THE DISK REFUSED IT");
  }
  mWorld.reset();
  if (mBonusOnly)
  {
    mBonusOnly = false;
    if (mLevelNumber <= 0)
      notice(mBonusWon ? "LEVEL CLEARED" : "BETTER LUCK NEXT TIME");
    else if (saved)
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
    tapTarget(x - 4, y - 4, cw, ch, [this, i](float, float) {
      if (mArsenalCursor != i)
        sound(Sfx::MenuMove);
      mArsenalCursor = i;
    });
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
  r.drawText(mOptions.touch ? "TAP a prototype   BACK to leave" : "ARROWS browse   ESC / B back", 640, 676, {15.0f, rgb(200, 200, 220), kInk}, Align::Center);
}

// --- Lists: an episode's levels, Bonus Channel, Reruns, Extras -------------

void Game::openList(ListKind kind)
{
  mListKind = kind;
  mListItems.clear();
  char id[32], label[160];
  switch (kind)
  {
    case ListKind::Levels:
      for (int n : worldLevels(mListEpisode))
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

// Loads a level picked from a list (the Bonus Channel, the EXTRAS) into
// mLevel, with HARD's extra enemies if hard. A file that would not load puts
// up failNotice and makes the menus ask the disk again (data/campaign.hpp),
// so they stop offering a level file deleted while the game runs.
bool Game::loadListedLevel(const std::string& path, bool hard, const char* failNotice)
{
  try
  {
    Level level = Level::loadFile(path);
    if (hard)
      addHardModeEnemies(level);
    mLevel = std::make_shared<const Level>(std::move(level));
    return true;
  }
  catch (const std::exception& e)
  {
    std::fprintf(stderr, "cannot load %s: %s\n", path.c_str(), e.what());
    forgetDataFiles();
    notice(failNotice);
    return false;
  }
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
    // An episode's levels and the EXTRAS were opened from WORLD SELECT.
    if (mListKind == ListKind::Levels || mListKind == ListKind::Extras)
      openWorlds();
    else
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
  pickListItem(mListItems[std::size_t(mListCursor)].first);
}

// What the list screen's confirm does with the item under the cursor (its
// id: a level number, a cutscene name or an extra's index). Takes a copy:
// what it opens may refill mListItems.
void Game::pickListItem(std::string id)
{
  switch (mListKind)
  {
    case ListKind::Levels:
      mPendingLevel = std::atoi(id.c_str());
      mPendingNewGame = false;
      mSelectFromList = true;
      setMode(Mode::Select);
      break;
    case ListKind::BonusChannel:
      playBonusChannel(std::atoi(id.c_str()));
      break;
    case ListKind::Reruns:
      playCutscenes({id}, After::List);
      break;
    case ListKind::Extras:
      playExtra(std::atoi(id.c_str()));
      break;
  }
}

void Game::playBonusChannel(int level)
{
  const std::string path = bonusFile(dataDir(), level);
  std::shared_ptr<const Level> bonus;
  try
  {
    bonus = std::make_shared<const Level>(Level::loadFile(path));
  }
  catch (const std::exception& e)
  {
    // Deleted since its lookup was kept (data/campaign.hpp), or broken: ask
    // the disk again, so the Bonus Channel stops offering a deleted one the
    // next time it opens (the open list is not rebuilt).
    std::fprintf(stderr, "cannot load %s: %s\n", path.c_str(), e.what());
    forgetDataFiles();
    notice("THAT CHANNEL IS OFF THE AIR");
    return;
  }
  // No save names a bonus level (see mLevelPath). The number is the level's
  // whose bonus level it is: a star won goes to it.
  useLevel(std::move(bonus), {});
  mLevelNumber = level;
  applyLevelLook(episodeOfLevel(level), mLevel->themeKey);
  mBonusOnly = true;
  playCutscenes({"sting_break"}, After::EnterBonus);
}

void Game::playExtra(int index)
{
  // readLevelFile, not readLevel: extraFile's path already starts with the
  // data dir, which can be relative (".."). HARD is the current setting
  // (runner select's toggle, --hard, or the difficulty a loaded save brought).
  auto level = readLevelFile(extraFile(dataDir(), index), mHard);
  if (!level)
    return;
  useLevel(std::move(level), {}); // no save names an EXTRAS level
  mLevelNumber = 0;
  applyLevelLook(0, mLevel->themeKey);
  mBonusOnly = true;
  playCutscenes({}, After::EnterBonus);
}

void Game::renderList()
{
  auto& r = mRenderer;
  const auto& t = theme();
  drawBackdrop(r, *mArt, float(mFrame) * 0.8f, 0.0f, 0.0f);
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(4, 2, 12, 180));
  char episodeTitle[96];
  const auto& ep = episode(mListEpisode);
  if (ep.number == kEpisodes)
    std::snprintf(episodeTitle, sizeof(episodeTitle), "%s", ep.name);
  else
    std::snprintf(episodeTitle, sizeof(episodeTitle), "EPISODE %d  -  %s", ep.number, ep.name);
  const char* title = mListKind == ListKind::Levels ? episodeTitle
    : mListKind == ListKind::BonusChannel           ? "THE BONUS CHANNEL"
    : mListKind == ListKind::Extras                 ? "EXTRAS"
                                                     : "RERUNS";
  r.drawText(title, 640, 30, {mListKind == ListKind::Levels ? 46.0f : 56.0f, t.accentA, kInk, true}, Align::Center);
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
    tapTarget(240, y - 6, 800, 50, [this, i](float, float) {
      mListCursor = i;
      tapPress(&Input::confirm);
    });
    r.fillRect(240, y - 4, 800, 44, sel ? withAlpha(t.accentA, 70) : rgba(255, 255, 255, 16));
    r.drawText(mListItems[std::size_t(i)].second, 270, y + 4, {24.0f, sel ? t.accentA : t.hudText, kInk, sel});
  }
  r.drawText(mOptions.touch ? "TAP to play   DRAG to scroll   BACK to leave" : "UP/DOWN choose   ENTER / A play   ESC / B back", 640, 676, {15.0f, rgb(200, 200, 220), kInk},
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
  tapTarget(0, 0, float(kScreenW), float(kScreenH), [this](float, float) { tapPress(&Input::confirm); });
}

} // namespace gr
