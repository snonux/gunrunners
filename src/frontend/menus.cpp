// Pause menu, savegame slots, the runner switcher and the secret cheats.
// Menus freeze the game underneath and work the same with the keyboard and
// a gamepad.

#include "frontend/game.hpp"

#include "data/campaign.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <future>
#include <random>

namespace gr
{

namespace
{

constexpr Color kInk = rgb(16, 12, 26);

// The campaign level a save is from. Up to v0.7.0 a save loaded on the
// title, of the level just quit, played on as level 0 (see loadSave), and
// the saves made then say 'campaign 0' for a numbered level file: the number
// in the file's name stands in, so the next save is right again. The
// training stage (levels/level1.txt) has no number in its file and stays 0.
int savedLevelNumber(const SaveGame& s)
{
  return s.levelNumber > 0 ? s.levelNumber : levelNumberOfFile(s.levelFile);
}

enum PauseItem
{
  kResume,
  kSave,
  kLoad,
  kQuickSave,
  kQuickLoad,
  kChangeRunner,
  kCheats,
  kSettings,
  kQuitToTitle,
  kQuitGame,
  kPauseItemCount,
};

const char* const kPauseLabels[kPauseItemCount] = {
  "RESUME", "SAVE GAME", "LOAD GAME", "QUICK SAVE", "QUICK LOAD", "CHANGE RUNNER", "CHEATS", "SETTINGS", "QUIT TO TITLE",
  "QUIT GAME"};

// Entered on the pause menu, Konami style: up, up, down, down, left, right,
// left, right (arrows, WASD, d-pad or stick).
constexpr int kCode[] = {0, 0, 1, 1, 2, 3, 2, 3}; // up down left right
constexpr int kCodeLength = int(sizeof(kCode) / sizeof(kCode[0]));

const char* const kCheatLabels[int(Cheat::Count)] = {
  "GOD MODE", "FULL HEALTH", "PROTOTYPE + FULL AMMO", "TURBO MODE", "CURE THE VIRUS", "ACCESS CARD", "RAPID FIRE",
  "SKIP THE LEVEL"};

} // namespace

std::vector<int> Game::pauseItems() const
{
#ifdef __EMSCRIPTEN__
  constexpr bool kCanQuit = false; // a browser tab is closed by the player
#else
  constexpr bool kCanQuit = true;
#endif
  std::vector<int> items;
  for (int i = 0; i < kPauseItemCount; ++i)
    if ((i != kQuitGame || kCanQuit) && (i != kCheats || mCheatsUnlocked || mOptions.cheats))
      items.push_back(i);
  return items;
}

void Game::setFullscreen(bool on)
{
  mFullscreenNow = on;
  if (mProfile.fullscreen == on)
    return;
  mProfile.fullscreen = on;
  saveProfile();
}

void Game::cycleTouchSize()
{
  mProfile.touchSize = (mProfile.touchSize + 1) % 3;
  saveProfile();
}

std::string Game::touchSizeLabel() const
{
  static const char* const kSizes[3] = {"SMALL", "MEDIUM", "LARGE"};
  return std::string("TOUCH PAD: ") + kSizes[std::clamp(mProfile.touchSize, 0, 2)];
}

bool Game::inLevel() const
{
  return mMenu == Menu::None && mMode == Mode::Play;
}

void Game::suspend()
{
  pauseLevel();
  saveProfile();
}

void Game::pauseLevel()
{
  if (mOptions.autoplay)
    return;
  if (mMenu == Menu::None && mMode == Mode::Play && mWorld && mWorld->state() == WorldState::Playing)
    openMenu(Menu::Pause);
}

void Game::notice(const std::string& text)
{
  mNotice = text;
  mNoticeTicks = 150;
}

void Game::refreshSlots()
{
  const std::string dir = mOptions.saveDir.empty() ? defaultSaveDir() : mOptions.saveDir;
  for (int i = 0; i < kSaveSlots; ++i)
    mSlots[std::size_t(i)] = readSave(slotPath(dir, i));
}

void Game::beginCloudRefresh()
{
  if (!mOptions.s3.enabled())
    return;
  if (mCloudRefresh.valid())
  {
    mCloudRefreshPending = true;
    return;
  }
  const S3Config config = mOptions.s3;
  const std::string dir = saveDir();
  const auto generation = mCloudGeneration;
  const auto commitMutex = mCloudCommitMutex;
  const auto quitting = mCloudQuitting;
  const std::uint64_t expected = generation->load();
  mCloudRefreshGeneration = expected;
  mCloudRefreshPending = false;
  mCloudRefresh = std::async(std::launch::async, [config, dir, generation, commitMutex, quitting, expected]() {
    // Unlike an upload, a refresh is also stopped at once by quitting.
    const S3Active active{[generation, quitting, expected]() {
      return generation->load() == expected && !quitting->load();
    }, commitMutex};
    std::vector<std::future<std::string>> slots;
    slots.reserve(kSaveSlots + 1);
    for (int i = 0; i < kSaveSlots + 1; ++i)
    {
      slots.push_back(std::async(std::launch::async, [config, dir, i, active]() {
        try
        {
          const std::string path = i == kSaveSlots ? quickSavePath(dir) : slotPath(dir, i);
          const std::string name = std::filesystem::path(path).filename().string();
          std::string error;
          if (syncS3File(config, config.object(name), path, &error, active) == S3SyncResult::Error)
            return name + ": " + error;
          return std::string{};
        }
        catch (const std::exception& e)
        {
          return std::string("cloud sync exception: ") + e.what();
        }
      }));
    }
    std::string failures;
    for (auto& slot : slots)
    {
      const std::string error = slot.get();
      if (!error.empty())
      {
        if (!failures.empty())
          failures += "; ";
        failures += error;
      }
    }
    return failures;
  });
}

void Game::queueCloudUpload(const std::string& path)
{
  // One upload at a time preserves write order. A dirty marker left by a
  // later save is picked up when the current request finishes.
  if (mCloudRefresh.valid() || !mCloudUploads.empty())
    return;
  const S3Config config = mOptions.s3;
  const auto generation = mCloudGeneration;
  const auto commitMutex = mCloudCommitMutex;
  const std::uint64_t expected = generation->load();
  CloudUpload upload;
  upload.generation = expected;
  upload.result = std::async(std::launch::async, [config, path, generation, commitMutex, expected]() {
    try
    {
      const std::string name = std::filesystem::path(path).filename().string();
      std::string error;
      const S3Active active{[generation, expected]() {
        return generation->load() == expected;
      }, commitMutex};
      if (syncS3File(config, config.object(name), path, &error, active) == S3SyncResult::Error)
        return name + ": " + error;
      return std::string{};
    }
    catch (const std::exception& e)
    {
      return std::string("cloud upload exception: ") + e.what();
    }
  });
  mCloudUploads.push_back(std::move(upload));
}

void Game::pollCloud()
{
  using namespace std::chrono_literals;
  bool continueUploads = false;
  if (mCloudRefresh.valid() && mCloudRefresh.wait_for(0s) == std::future_status::ready)
  {
    const std::string error = mCloudRefresh.get();
    const bool current = mCloudRefreshGeneration == mCloudGeneration->load();
    if (current && !error.empty())
    {
      std::fprintf(stderr, "cloud save refresh failed: %s\n", error.c_str());
      notice("CLOUD UNAVAILABLE - USING LOCAL SAVES");
    }
    else if (current && mMenu == Menu::Slots)
    {
      refreshSlots();
      continueUploads = true;
    }
    else if (current)
      continueUploads = true;
    if (mCloudRefreshPending && mOptions.s3.enabled())
      beginCloudRefresh();
  }
  for (auto it = mCloudUploads.begin(); it != mCloudUploads.end();)
  {
    if (it->result.wait_for(0s) != std::future_status::ready)
    {
      ++it;
      continue;
    }
    const std::string error = it->result.get();
    const bool current = it->generation == mCloudGeneration->load();
    if (current && !error.empty())
    {
      std::fprintf(stderr, "cloud save upload failed: %s\n", error.c_str());
      notice("CLOUD SAVE FAILED - SAVED LOCALLY");
    }
    else if (current)
      continueUploads = true;
    it = mCloudUploads.erase(it);
  }
  if (!continueUploads || !mOptions.s3.enabled() || mCloudRefresh.valid() ||
      !mCloudUploads.empty())
    return;
  const std::string dir = saveDir();
  for (int i = 0; i < kSaveSlots + 1; ++i)
  {
    const std::string path = i == kSaveSlots ? quickSavePath(dir) : slotPath(dir, i);
    if (std::filesystem::exists(path + ".cloud-dirty"))
    {
      queueCloudUpload(path);
      break;
    }
  }
}

namespace
{

// How long quitting lets cloud uploads still running finish. A SAVE right
// before quitting can then still reach the cloud, so another device does
// not get an older copy, and the next start does not re-upload it over a
// newer save made elsewhere in the meantime. That only holds when the SAVE
// queued its own upload, though: while a refresh or another upload runs,
// queueCloudUpload leaves the save to its .cloud-dirty marker, quitting
// does not wait for it, and the next start uploads it.
constexpr auto kQuitUploadGrace = std::chrono::seconds(2);

// Makes every cloud request started so far stale. Under the commit
// mutex, like the CLOUD menu's bumps: a worker that is committing a download
// or clearing a dirty marker finishes that step first, and every later
// S3Active check sees the new generation.
void staleCloudRequests(std::atomic<std::uint64_t>& generation, std::mutex& commitMutex)
{
  std::lock_guard<std::mutex> lock(commitMutex);
  generation.fetch_add(1);
}

} // namespace

Game::CloudCancel::~CloudCancel()
{
  staleCloudRequests(*generation, *commitMutex);
}

// Quitting waits about 3 s at most: the refresh is cancelled at once and
// aborts within about a second, alongside the uploads' grace of
// kQuitUploadGrace, after which the uploads left abort within about a
// second too. With libcurl older than 7.87 (no CURLOPT_QUICK_EXIT, see
// s3.cpp), a request cancelled during its DNS lookup adds the wait for
// getaddrinfo to return.
void Game::cancelCloudSync()
{
  // The refresh stops at once (under the commit mutex, as above): it mostly
  // downloads, and the next start runs it again. A dirty save it was
  // uploading keeps its marker.
  {
    std::lock_guard<std::mutex> lock(*mCloudCommitMutex);
    mCloudQuitting->store(true);
  }
  mCloudRefreshPending = false;
  // No pollCloud runs any more, so no new upload starts meanwhile.
  const auto deadline = std::chrono::steady_clock::now() + kQuitUploadGrace;
  for (auto& upload : mCloudUploads)
    upload.result.wait_until(deadline);
  // An upload cut off here keeps its .cloud-dirty marker, so the next start
  // uploads that save again. The results are stale and are dropped.
  staleCloudRequests(*mCloudGeneration, *mCloudCommitMutex);
  if (mCloudRefresh.valid())
    mCloudRefresh.wait();
  for (auto& upload : mCloudUploads)
    upload.result.wait();
  mCloudRefresh = std::future<std::string>();
  mCloudUploads.clear();
}

void Game::openMenu(Menu m)
{
  mMenu = m;
  switch (m)
  {
    case Menu::Pause:
      mMenuCursor = 0;
      mCodeStep = 0;
      break;
    case Menu::Cheats:
    case Menu::Settings:
    case Menu::Controls:
    case Menu::TouchEdit:
    case Menu::Assists:
    case Menu::Map:
      break;
    case Menu::Slots:
      refreshSlots();
      if (!mSlotsForSave)
      {
        // Start on the most recent save.
        int best = -1;
        for (int i = 0; i < kSaveSlots; ++i)
        {
          const auto& s = mSlots[std::size_t(i)];
          if (s && (best < 0 || s->savedAt > mSlots[std::size_t(best)]->savedAt))
            best = i;
        }
        mSlotCursor = std::max(0, best);
      }
      break;
    case Menu::Runner:
      mRunnerCursor = mWorld ? mWorld->characterIndex() : mCursor;
      break;
    case Menu::None:
      break;
  }
}

void Game::closeMenu()
{
  mMenu = Menu::None;
  freeMap();
  // The music kept going under the menu; put it back on the level's beat.
  if (mMode == Mode::Play && mWorld)
    playLevelMusic();
  // Buttons still held from the menu must not fire or jump in the game.
  mLatched = PlayerInput{};
  mRun.reset();
  mPrevLogicInput = mPrev;
  mPrevLogicInput.jump = mPrevLogicInput.fire = true;
}

void Game::switchRunner(int index)
{
  if (mWorld && mWorld->switchCharacter(index))
    mCursor = index;
}

void Game::switchRandomRunner()
{
  if (!mWorld)
    return;
  // Never the one already playing, nor one sitting out after a death
  // (SWAP RUNNER ON DEATH): a pick from the others.
  std::vector<int> free;
  for (int i = 0; i < characterCount(); ++i)
    if (i != mWorld->characterIndex() && !mWorld->benched(i))
      free.push_back(i);
  if (free.empty())
    return;
  static std::mt19937 rng{std::random_device{}()};
  switchRunner(free[std::size_t(std::uniform_int_distribution<int>(0, int(free.size()) - 1)(rng))]);
}

void Game::saveToSlot(int slot)
{
  if (!saveTo(slotPath(saveDir(), slot)))
    return;
  closeMenu();
  mWorld->notify("GAME SAVED TO SLOT " + std::to_string(slot + 1));
}

void Game::quickSave()
{
  if (!saveTo(quickSavePath(saveDir())))
    return;
  // From the pause menu or a hotkey mid-level; a hotkey leaves the music be.
  if (mMenu != Menu::None)
    closeMenu();
  mWorld->notify("QUICK SAVED");
}

bool Game::saveTo(const std::string& path)
{
  if (mMode != Mode::Play || !mWorld || !mWorld->canSave() || mMainWorld || mBonusOnly)
  {
    sound(Sfx::Hurt);
    notice("YOU CAN'T SAVE RIGHT NOW");
    return false;
  }
  SaveGame s = mWorld->snapshot();
  s.theme = mThemeIndex;
  if (campaign())
  {
    s.levelFile = mLevelPath;
    s.levelNumber = mLevelNumber;
  }
  std::string error;
  const bool saved = mOptions.s3.enabled() ? writeS3Save(s, path, &error) :
    writeSave(s, path, &error);
  if (!saved)
  {
    sound(Sfx::Hurt);
    notice("SAVE FAILED: " + error);
    std::fprintf(stderr, "save failed: %s\n", error.c_str());
    return false;
  }
  if (mOptions.s3.enabled())
    queueCloudUpload(path);
  sound(Sfx::Checkpoint);
  return true;
}

bool Game::loadFromSlot(int slot)
{
  const auto& save = mSlots[std::size_t(slot)];
  if (!save)
  {
    sound(Sfx::Hurt);
    notice("SLOT " + std::to_string(slot + 1) + " IS EMPTY");
    return false;
  }
  return loadSave(*save, "GAME LOADED FROM SLOT " + std::to_string(slot + 1));
}

bool Game::quickLoad()
{
  const auto save = readSave(quickSavePath(saveDir()));
  if (!save)
  {
    sound(Sfx::Hurt);
    notice(mCloudRefresh.valid() ? "CLOUD SAVES ARE STILL SYNCING" :
      "NO QUICK SAVE YET - F5 OR THE PAUSE MENU MAKES ONE");
    return false;
  }
  return loadSave(*save, "QUICK SAVE LOADED");
}

bool Game::loadSave(SaveGame s, const std::string& message)
{
  // A copy, to be safe: the caller's save may be one of mSlots. Nothing in
  // here refreshes them (only opening the slot menu and pollCloud do).
  // A save of the level being played keeps the runner it started with (for
  // SWAP RUNNER ON DEATH's start over); one from elsewhere starts with its own.
  // Inside a bonus level the level being played is the one parked behind it.
  const Level* playing = mMainWorld ? mMainLevel.get() : mLevel.get();
  const bool sameLevel = mMode == Mode::Play && mWorld && playing && playing->name == s.levelName;
  // Campaign saves bring their level along, with (or without) the HARD
  // enemies they were played with: it is read again unless it is the one
  // loaded (that file, by that name), at the same difficulty. A bonus or
  // EXTRAS level has no file (mLevelPath), so a save loaded in one always
  // reads its level. The name is checked too: a level of another name could
  // never take the save. It is read aside, and the world is built aside too:
  // until the save is known to fit, the run in progress (its world, level,
  // number and file, the HARD setting, the level parked behind a bonus
  // level) stays as it is, so a refused load carries on with it.
  const bool rereadLevel = campaign() && !s.levelFile.empty() &&
    (!mLevel || s.levelFile != mLevelPath || mLevel->name != s.levelName || mLevel->hard != s.hard);
  std::shared_ptr<const Level> level = rereadLevel ? readLevel(s.levelFile, s.hard) : mLevel;
  if (rereadLevel && !level)
  {
    sound(Sfx::Hurt); // readLevel said THAT LEVEL WOULD NOT LOAD
    return false;
  }
  auto world = restoreAside(s, level);
  if (!world)
    return false;
  if (rereadLevel)
    useLevel(std::move(level), s.levelFile);
  // A campaign save brings its level's number, also when its level is the
  // one loaded (its file too, then) and not read again: every way to the
  // title (QUIT TO TITLE, the end of the story or of DEEP SPACE) keeps the
  // last level but sets the number to 0, and a save of that level would
  // otherwise play on as no campaign level (no clear recorded, no bonus
  // level, Level 1 next). A stand-alone level (--level) stays without one.
  if (campaign() && !s.levelFile.empty())
    mLevelNumber = savedLevelNumber(s);
  // A save brings its difficulty: the levels after it follow.
  if (campaign())
    setHard(s.hard);
  if (!sameLevel)
    mLevelRunner = world->characterIndex();
  startLoadedWorld(std::move(world), s.theme, message);
  return true;
}

std::unique_ptr<World> Game::restoreAside(const SaveGame& s, const std::shared_ptr<const Level>& level)
{
  if (!level || s.levelName != level->name)
  {
    sound(Sfx::Hurt);
    notice("THAT SAVE IS FROM ANOTHER LEVEL");
    return nullptr;
  }
  auto world = std::make_unique<World>(level, std::clamp(s.character, 0, characterCount() - 1), theme(), *mArt);
  prepareWorld(*world);
  if (!world->restore(s))
  {
    sound(Sfx::Hurt);
    notice("THAT SAVE DOES NOT FIT THIS LEVEL");
    return nullptr;
  }
  return world;
}

void Game::startLoadedWorld(std::unique_ptr<World> world, int themeIndex, const std::string& message)
{
  // The save's look: the art changes in place under the new world.
  if (themeIndex >= 0 && themeIndex < themeTotal() && themeIndex != mThemeIndex)
    useTheme(themeIndex);
  mWorld = std::move(world);
  mCursor = mWorld->characterIndex();
  beginVoices();
  mBot = Bot{};
  mSubTick = 0;
  // A load mid-bonus ends the bonus level: what was parked behind it goes
  // (loadSave made the save's level the one played), and a Bonus Channel or
  // EXTRAS run is over.
  mMainWorld.reset();
  mMainLevel.reset();
  mMainLevelPath.clear();
  mBonusOnly = false;
  if (mMode != Mode::Play)
    setMode(Mode::Play);
  sound(Sfx::Teleport);
  closeMenu();
  playLevelMusic();
  mWorld->notify(message);
}

bool Game::tickMenu(const Input& in)
{
  auto edge = [&](bool Input::*f) { return in.*f && !(mPrev.*f); };
  if (mMenuHold)
  {
    // The key or button just bound is still down: it must not also act.
    mMenuHold = in.left || in.right || in.up || in.down || in.jump || in.fire || in.confirm || in.pause ||
      in.back || in.swap || in.randomRunner || in.map;
    return !mQuit;
  }
  // Start both opens the pause menu and confirms; in a menu it backs out.
  const bool cancel = edge(&Input::back) || edge(&Input::pause);
  const bool ok = !cancel && (edge(&Input::confirm) || edge(&Input::jump));
  const int dir = edge(&Input::up) ? -1 : (edge(&Input::down) ? 1 : 0);
  const int side = edge(&Input::left) ? -1 : (edge(&Input::right) ? 1 : 0);
  if (dir != 0 || side != 0)
    sound(Sfx::MenuMove);

  switch (mMenu)
  {
    case Menu::Pause:
    {
      // The cheat code rides on the menu's own moves.
      const int pressed = edge(&Input::up) ? 0 : edge(&Input::down) ? 1 : edge(&Input::left) ? 2 : edge(&Input::right) ? 3 : -1;
      if (pressed >= 0 && !mCheatsUnlocked && !mOptions.cheats)
      {
        if (pressed == kCode[mCodeStep])
          ++mCodeStep;
        else
          mCodeStep = pressed == kCode[0] ? 1 : 0;
        if (mCodeStep == kCodeLength)
        {
          mCheatsUnlocked = true;
          mCodeStep = 0;
          sound(Sfx::TurboOn);
          notice("CHEATS UNLOCKED - SEE THE PAUSE MENU");
        }
      }
      const auto items = pauseItems();
      const int count = int(items.size());
      mMenuCursor = (mMenuCursor + dir + count) % count;
      if (cancel)
      {
        closeMenu();
      }
      else if (ok)
      {
        sound(Sfx::MenuSelect);
        switch (items[std::size_t(mMenuCursor)])
        {
          case kResume:
            closeMenu();
            break;
          case kCheats:
            openMenu(Menu::Cheats);
            break;
          case kSettings:
            openSettings(Menu::Pause);
            break;
          case kSave:
            mSlotsForSave = true;
            openMenu(Menu::Slots);
            break;
          case kLoad:
            mSlotsForSave = false;
            openMenu(Menu::Slots);
            break;
          case kQuickSave:
            quickSave();
            break;
          case kQuickLoad:
            quickLoad();
            break;
          case kChangeRunner:
            openMenu(Menu::Runner);
            break;
          case kQuitToTitle:
            closeMenu();
            if (campaign())
            {
              goTitle();
              break;
            }
            mWorld.reset();
            setMode(Mode::Select);
            if (mAudio)
              mAudio->playMusic(Music::Menu);
            break;
          case kQuitGame:
            mQuit = true;
            break;
        }
      }
      break;
    }

    case Menu::Cheats:
    {
      const int count = int(Cheat::Count);
      mCheatCursor = (mCheatCursor + dir + count) % count;
      if (cancel)
      {
        openMenu(Menu::Pause);
      }
      else if (ok && mWorld)
      {
        const Cheat c = Cheat(mCheatCursor);
        if (!mWorld->cheat(c))
        {
          sound(Sfx::Hurt);
          notice("NOT RIGHT NOW");
        }
        else
        {
          sound(Sfx::MenuSelect);
          notice(c == Cheat::God ? (mWorld->godMode() ? "GOD MODE ON" : "GOD MODE OFF") : kCheatLabels[mCheatCursor]);
          if (c == Cheat::Exit || c == Cheat::Turbo)
            closeMenu();
        }
      }
      break;
    }

    case Menu::Slots:
      mSlotCursor = (mSlotCursor + dir + kSaveSlots) % kSaveSlots;
      if (cancel)
      {
        if (mMode == Mode::Play)
          openMenu(Menu::Pause);
        else
          closeMenu();
      }
      else if (ok)
      {
        if (mSlotsForSave)
          saveToSlot(mSlotCursor);
        else
          loadFromSlot(mSlotCursor);
      }
      break;

    case Menu::Runner:
      mRunnerCursor = (mRunnerCursor + dir + side + characterCount()) % characterCount();
      if (cancel)
      {
        openMenu(Menu::Pause);
      }
      else if (ok && mWorld && mWorld->benched(mRunnerCursor))
      {
        sound(Sfx::Hurt); // sitting out after a death (SWAP RUNNER ON DEATH)
      }
      else if (ok)
      {
        closeMenu();
        switchRunner(mRunnerCursor);
      }
      else if (edge(&Input::randomRunner))
      {
        closeMenu();
        switchRandomRunner();
      }
      break;

    case Menu::Settings:
      tickSettings(ok, cancel, dir, side);
      break;

    case Menu::Controls:
    case Menu::TouchEdit:
      tickControls(in, ok, cancel, dir, side);
      break;

    case Menu::Assists:
      tickAssists(ok, cancel, dir);
      break;

    case Menu::Map:
      tickMap(in, cancel || edge(&Input::map), ok);
      break;

    case Menu::None:
      break;
  }
  return !mQuit;
}

void Game::renderNotice()
{
  if (mNoticeTicks <= 0)
    return;
  const float a = std::min(1.0f, float(mNoticeTicks) / 20.0f);
  // Runner select has its EDIT button down there.
  const float y = mMode == Mode::Select && mMenu == Menu::None ? 128.0f : 640.0f;
  const float w = y < 600.0f ? 800.0f : 600.0f;
  mRenderer.fillRect(640 - w * 0.5f, y, w, 44, rgba(8, 6, 22, int(230 * a)));
  mRenderer.drawText(mNotice, 640, y + 8, {22.0f, theme().accentA, kInk, true}, Align::Center, a);
}

void Game::renderMenu()
{
  auto& r = mRenderer;
  const auto& t = theme();
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(0, 0, 0, 140));
  const TextStyle hint{16.0f, rgb(190, 188, 214), kInk};
  // The menu covers the screen under it: only its own items take taps.
  clearTapTargets();
  const bool touch = mOptions.touch;

  if (mMenu == Menu::Map)
  {
    renderMap();
    return;
  }

  if (mMenu == Menu::Controls || mMenu == Menu::TouchEdit)
  {
    renderControls();
    return;
  }

  if (mMenu == Menu::Assists)
  {
    renderAssists();
    return;
  }

  if (mMenu == Menu::Settings)
  {
    renderSettings();
    return;
  }

  if (mMenu == Menu::Cheats)
  {
    r.draw(mMenuPanel, 380, 120);
    r.drawText("CHEATS", 640, 140, {44.0f, t.accentB, kInk, true}, Align::Center);
    const float step = 38.0f;
    for (int i = 0; i < int(Cheat::Count); ++i)
    {
      const bool sel = i == mCheatCursor;
      const float y = 208.0f + float(i) * step;
      tapTarget(400, y - 6, 480, step, [this, i](float, float) {
        mCheatCursor = i;
        tapPress(&Input::confirm);
      });
      if (sel)
        r.fillRect(420, y - 4, 440, step - 4, withAlpha(t.accentB, 60));
      std::string label = kCheatLabels[i];
      if (Cheat(i) == Cheat::God && mWorld)
        label += mWorld->godMode() ? "  [ON]" : "  [OFF]";
      r.drawText(label, 640, y, {22.0f, sel ? t.accentB : t.hudText, kInk, sel}, Align::Center);
    }
    r.drawText("Cheating forfeits this level's bonuses and records.", 640, 522, hint, Align::Center);
    r.drawText(touch ? "TAP to use   BACK to leave" : "ENTER / A use   ESC / B back", 640, 560, hint, Align::Center);
    return;
  }

  if (mMenu == Menu::Pause)
  {
    r.draw(mMenuPanel, 380, 120);
    r.drawText("PAUSED", 640, 140, {48.0f, t.accentA, kInk, true}, Align::Center);
    const auto items = pauseItems();
    // Up to eleven items share the panel: the more there are, the closer.
    const int n = int(items.size());
    const float step = n > 1 ? std::min(50.0f, 324.0f / float(n - 1)) : 50.0f;
    const float size = step < 40.0f ? 24.0f : 28.0f;
    for (int i = 0; i < n; ++i)
    {
      const bool sel = i == mMenuCursor;
      const float y = 206.0f + float(i) * step;
      tapTarget(400, y + 15 - step * 0.5f, 480, step, [this, i](float, float) {
        mMenuCursor = i;
        tapPress(&Input::confirm);
      });
      if (sel)
      {
        r.fillRect(420, y - 6, 440, std::min(42.0f, step - 2.0f), withAlpha(t.accentA, 60));
        drawGlow(r, *mArt, 640, y + 15, 160, t.accentA, 0.25f);
      }
      const int item = items[std::size_t(i)];
      r.drawText(kPauseLabels[item], 640, y, {size, sel ? t.accentA : t.hudText, kInk, sel},
        Align::Center);
    }
    r.drawText(touch ? "TAP to choose   BACK to resume" : "UP/DOWN choose   ENTER / A select   ESC / B resume", 640, 566,
      hint, Align::Center);
    return;
  }

  if (mMenu == Menu::Runner)
  {
    r.draw(mMenuPanel, 380, 120);
    r.drawText("CHANGE RUNNER", 640, 140, {40.0f, t.accentA, kInk, true}, Align::Center);
    const int current = mWorld ? mWorld->characterIndex() : mCursor;
    // Three at a time, the chosen one in the middle.
    const int n = characterCount();
    for (int k = -1; k <= 1; ++k)
    {
      if (n < 3 && k != 0)
        continue;
      const int i = (mRunnerCursor + k + n) % n;
      const auto& def = characterByIndex(i);
      const bool sel = k == 0;
      const float x = 640.0f + float(k) * 170.0f;
      // A tap on the middle runner swaps to it, on a side one turns to it.
      tapTarget(x - 85, 190, 170, 230, [this, k](float, float) {
        if (k == 0)
        {
          tapPress(&Input::confirm);
          return;
        }
        mRunnerCursor = (mRunnerCursor + k + characterCount()) % characterCount();
        sound(Sfx::MenuMove);
      });
      if (sel)
        drawGlow(r, *mArt, x, 330, 120, t.accentA, 0.35f);
      // Runners sitting out after a death (SWAP RUNNER ON DEATH) are greyed out.
      const bool out = mWorld && mWorld->benched(i);
      DrawOpts o;
      o.scale = 0.55f;
      if (out)
      {
        o.tint = rgb(70, 68, 80);
        o.alpha = 0.45f;
      }
      else if (!sel)
        o.tint = rgb(130, 126, 150);
      r.draw(mArt->portrait(def), x, 210, o);
      r.drawText(def.name, x, 360, {26.0f, out ? rgb(120, 118, 134) : sel ? t.accentA : t.hudText, kInk, true},
        Align::Center);
      const std::string under = i == current ? "PLAYING" : out ? "DOWN" : weaponName(def.startWeapon);
      r.drawText(under, x, 396, {15.0f, out ? rgb(220, 110, 110) : rgb(190, 188, 214)}, Align::Center);
    }
    char count[32];
    std::snprintf(count, sizeof(count), "< %d / %d >", mRunnerCursor + 1, n);
    r.drawText(count, 640, 424, {18.0f, t.accentB, kInk, true}, Align::Center);
    r.drawText("Health carries over as a share of the new runner's hearts.", 640, 466, hint, Align::Center);
    // A runner picked at random, never the one playing: the RANDOM RUNNER
    // hotkey works here too, and touch has no hotkey, so it is a button.
    if (n > 1)
    {
      tapTarget(500, 494, 280, 40, [this](float, float) { tapPress(&Input::randomRunner); });
      std::string random = "? RANDOM RUNNER";
      if (!touch)
      {
        const int key = mBindings.keys[std::size_t(Act::RandomRunner)][0];
        const auto& pad = mBindings.pad[std::size_t(Act::RandomRunner)];
        if (key > 0 || !pad.empty())
          random += "   " + (key > 0 ? keyName(key) : std::string()) + (key > 0 && !pad.empty() ? " / " : "") +
            (pad.empty() ? std::string() : padNames(pad));
      }
      r.drawText(random, 640, 504, {20.0f, t.accentB, kInk, true}, Align::Center);
    }
    r.drawText(touch ? "TAP the middle runner to swap   BACK to leave"
                     : "LEFT/RIGHT choose   ENTER / A swap   ESC / B back",
      640, 560, hint, Align::Center);
    return;
  }

  // Savegame slots.
  r.draw(mSlotPanel, 140, 100);
  r.drawText(mSlotsForSave ? "SAVE GAME" : "LOAD GAME", 640, 118, {44.0f, t.accentA, kInk, true}, Align::Center);
  char buf[160];
  for (int i = 0; i < kSaveSlots; ++i)
  {
    const bool sel = i == mSlotCursor;
    const float y = 196.0f + float(i) * 70.0f;
    // Loading takes one tap; saving over a slot takes a second one on it.
    tapTarget(180, y - 5, 920, 70, [this, i](float, float) {
      if (mSlotsForSave && i != mSlotCursor)
      {
        mSlotCursor = i;
        sound(Sfx::MenuMove);
        return;
      }
      mSlotCursor = i;
      tapPress(&Input::confirm);
    });
    r.fillRect(180, y, 920, 60, sel ? withAlpha(t.accentA, 70) : rgba(255, 255, 255, 18));
    std::snprintf(buf, sizeof(buf), "%d", i + 1);
    r.drawText(buf, 214, y + 12, {30.0f, sel ? t.accentA : t.hudText, kInk, true}, Align::Center);
    const auto& s = mSlots[std::size_t(i)];
    if (!s)
    {
      r.drawText("- EMPTY -", 640, y + 16, {22.0f, rgb(150, 148, 170)}, Align::Center);
      continue;
    }
    const CharacterDef def = savedRunner(s->character, s->runner);
    DrawOpts o;
    o.scale = 0.2f;
    r.draw(mArt->portrait(def), 280, y + 2, o);
    r.drawText(def.name, 320, y + 6, {24.0f, sel ? t.accentA : t.hudText, kInk, true});
    r.drawText(s->levelName.empty() ? "STAGE 1" : s->levelName, 320, y + 34, {15.0f, rgb(190, 188, 214)});
    std::snprintf(buf, sizeof(buf), "SCORE %07d   %d:%02d   LETTERS %s", s->score, s->frames / 15 / 60,
      (s->frames / 15) % 60, s->letters.empty() ? "-" : s->letters.c_str());
    r.drawText(buf, 640, y + 8, {20.0f, t.hudText, kInk});
    r.drawText(s->savedAt, 1080, y + 34, {15.0f, rgb(190, 188, 214)}, Align::Right);
  }
  const char* help = touch ? (mSlotsForSave ? "TAP a slot, TAP it again to save there (overwrites)   BACK to leave"
                                             : "TAP a save to load it   BACK to leave")
                           : (mSlotsForSave ? "ENTER / A save here (overwrites)   ESC / B back"
                                             : "ENTER / A load   ESC / B back");
  r.drawText(help, 640, 570, hint, Align::Center);
}

} // namespace gr
