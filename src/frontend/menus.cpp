// Pause menu, savegame slots, the runner switcher and the secret cheats.
// Menus freeze the game underneath and work the same with the keyboard and
// a gamepad.

#include "frontend/game.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <future>

namespace gr
{

namespace
{

constexpr Color kInk = rgb(16, 12, 26);

enum PauseItem
{
  kResume,
  kSave,
  kLoad,
  kQuickSave,
  kQuickLoad,
  kChangeRunner,
  kCheats,
  kFullscreen,
  kControls,
  kQuitToTitle,
  kQuitGame,
  kPauseItemCount,
};

const char* const kPauseLabels[kPauseItemCount] = {
  "RESUME", "SAVE GAME", "LOAD GAME", "QUICK SAVE", "QUICK LOAD", "CHANGE RUNNER", "CHEATS", "FULLSCREEN", "CONTROLS", "QUIT TO TITLE",
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
  std::vector<int> items;
  for (int i = 0; i < kPauseItemCount; ++i)
    if ((i != kCheats || mCheatsUnlocked || mOptions.cheats) && (i != kFullscreen || mOptions.window))
      items.push_back(i);
  return items;
}

void Game::setFullscreen(bool on)
{
  mFullscreenNow = on;
  if (mProfile.fullscreen == on)
    return;
  mProfile.fullscreen = on;
  mProfile.save(saveDir());
}

void Game::cycleTouchSize()
{
  mProfile.touchSize = (mProfile.touchSize + 1) % 3;
  mProfile.save(saveDir());
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
  if (mMenu == Menu::None && mMode == Mode::Play && mWorld && mWorld->state() == WorldState::Playing)
    openMenu(Menu::Pause);
  mProfile.save(saveDir());
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
  const std::uint64_t expected = generation->load();
  mCloudRefreshGeneration = expected;
  mCloudRefreshPending = false;
  mCloudRefresh = std::async(std::launch::async, [config, dir, generation, commitMutex, expected]() {
    const S3Active active{[generation, expected]() {
      return generation->load() == expected;
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
    case Menu::Controls:
    case Menu::TouchEdit:
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
  mPrevLogicInput = mPrev;
  mPrevLogicInput.jump = mPrevLogicInput.fire = true;
}

void Game::switchRunner(int index)
{
  if (mWorld && mWorld->switchCharacter(index))
    mCursor = index;
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
  // A copy: loading a level refreshes mSlots, which may hold the original.
  if (campaign() && !s.levelFile.empty() && (!mLevel || s.levelFile != mLevelPath))
  {
    // Campaign saves bring their level along.
    mWorld.reset();
    mMainWorld.reset();
    mBonusOnly = false;
    if (!loadLevel(s.levelFile))
      return false;
    mLevelNumber = s.levelNumber;
  }
  if (!mLevel || s.levelName != mLevel->name)
  {
    sound(Sfx::Hurt);
    notice("THAT SAVE IS FROM ANOTHER LEVEL");
    return false;
  }
  if (s.theme >= 0 && s.theme < themeTotal() && s.theme != mThemeIndex)
  {
    // A fresh world comes next, so only the art needs rebuilding (the
    // world holds references into it).
    mWorld.reset();
    mMainWorld.reset();
    mThemeIndex = s.theme;
    mArt = std::make_unique<Art>(Art::build(theme(), mRenderer));
    buildPanels();
  }
  auto world = std::make_unique<World>(mLevel, std::clamp(s.character, 0, characterCount() - 1), theme(), *mArt);
  prepareWorld(*world);
  if (!world->restore(s))
  {
    sound(Sfx::Hurt);
    notice("THAT SAVE DOES NOT FIT THIS LEVEL");
    return false;
  }
  mWorld = std::move(world);
  mCursor = mWorld->characterIndex();
  mBot = Bot{};
  mSubTick = 0;
  mMainWorld.reset();
  mBonusOnly = false;
  if (mMode != Mode::Play)
    setMode(Mode::Play);
  sound(Sfx::Teleport);
  closeMenu();
  playLevelMusic();
  mWorld->notify(message);
  return true;
}

bool Game::tickMenu(const Input& in)
{
  auto edge = [&](bool Input::*f) { return in.*f && !(mPrev.*f); };
  if (mMenuHold)
  {
    // The key or button just bound is still down: it must not also act.
    mMenuHold = in.left || in.right || in.up || in.down || in.jump || in.fire || in.confirm || in.pause ||
      in.back || in.swap || in.map;
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
          case kFullscreen:
            mFullscreenToggle = true;
            break;
          case kControls:
            mControlsReturn = Menu::Pause;
            openControls();
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
      else if (ok)
      {
        closeMenu();
        switchRunner(mRunnerCursor);
      }
      break;

    case Menu::Controls:
    case Menu::TouchEdit:
      tickControls(in, ok, cancel, dir, side);
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
      const std::string label =
        item == kFullscreen ? (mFullscreenNow ? "FULLSCREEN: ON" : "FULLSCREEN: OFF")
                             : kPauseLabels[item];
      r.drawText(label, 640, y, {size, sel ? t.accentA : t.hudText, kInk, sel},
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
      DrawOpts o;
      o.scale = 0.55f;
      if (!sel)
        o.tint = rgb(130, 126, 150);
      r.draw(mArt->portrait(def), x, 210, o);
      r.drawText(def.name, x, 360, {26.0f, sel ? t.accentA : t.hudText, kInk, true}, Align::Center);
      r.drawText(i == current ? "PLAYING" : weaponName(def.startWeapon), x, 396, {15.0f, rgb(190, 188, 214)},
        Align::Center);
    }
    char count[32];
    std::snprintf(count, sizeof(count), "< %d / %d >", mRunnerCursor + 1, n);
    r.drawText(count, 640, 424, {18.0f, t.accentB, kInk, true}, Align::Center);
    r.drawText("Health carries over as a share of the new runner's hearts.", 640, 466, hint, Align::Center);
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
