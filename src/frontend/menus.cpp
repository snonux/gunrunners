// Pause menu, savegame slots, the runner switcher and the secret cheats.
// Menus freeze the game underneath and work the same with the keyboard and
// a gamepad.

#include "frontend/game.hpp"

#include <algorithm>
#include <cstdio>

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
  kChangeRunner,
  kCheats,
  kFullscreen,
  kControls,
  kQuitToTitle,
  kQuitGame,
  kPauseItemCount,
};

const char* const kPauseLabels[kPauseItemCount] = {
  "RESUME", "SAVE GAME", "LOAD GAME", "CHANGE RUNNER", "CHEATS", "FULLSCREEN", "CONTROLS", "QUIT TO TITLE",
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
  if (!mWorld || !mWorld->canSave() || mMainWorld || mBonusOnly)
  {
    sound(Sfx::Hurt);
    notice("YOU CAN'T SAVE RIGHT NOW");
    return;
  }
  SaveGame s = mWorld->snapshot();
  s.theme = mThemeIndex;
  if (campaign())
  {
    s.levelFile = mLevelPath;
    s.levelNumber = mLevelNumber;
  }
  const std::string dir = mOptions.saveDir.empty() ? defaultSaveDir() : mOptions.saveDir;
  std::string error;
  if (!writeSave(s, slotPath(dir, slot), &error))
  {
    sound(Sfx::Hurt);
    notice("SAVE FAILED: " + error);
    std::fprintf(stderr, "save failed: %s\n", error.c_str());
    return;
  }
  sound(Sfx::Checkpoint);
  closeMenu();
  mWorld->notify("GAME SAVED TO SLOT " + std::to_string(slot + 1));
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
  if (campaign() && !save->levelFile.empty() && (!mLevel || save->levelFile != mLevelPath))
  {
    // Campaign saves bring their level along.
    const SaveGame copy = *save;
    mWorld.reset();
    mMainWorld.reset();
    mBonusOnly = false;
    if (!loadLevel(copy.levelFile))
      return false;
    mLevelNumber = copy.levelNumber;
    mSlots[std::size_t(slot)] = copy;
  }
  if (!mLevel || save->levelName != mLevel->name)
  {
    sound(Sfx::Hurt);
    notice("THAT SAVE IS FROM ANOTHER LEVEL");
    return false;
  }
  const SaveGame s = *save; // setTheme below rebuilds things; keep a copy
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
  auto world = std::make_unique<World>(mLevel, std::clamp(s.character, 0, kCharacterCount - 1), theme(), *mArt);
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
  mWorld->notify("GAME LOADED FROM SLOT " + std::to_string(slot + 1));
  return true;
}

bool Game::tickMenu(const Input& in)
{
  auto edge = [&](bool Input::*f) { return in.*f && !(mPrev.*f); };
  if (mMenuHold)
  {
    // The key or button just bound is still down: it must not also act.
    mMenuHold = in.left || in.right || in.up || in.down || in.jump || in.fire || in.confirm || in.pause ||
      in.back || in.swap;
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
      mRunnerCursor = (mRunnerCursor + dir + side + kCharacterCount) % kCharacterCount;
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
  mRenderer.fillRect(340, 640, 600, 44, rgba(8, 6, 22, int(210 * a)));
  mRenderer.drawText(mNotice, 640, 648, {22.0f, theme().accentA, kInk, true}, Align::Center, a);
}

void Game::renderMenu()
{
  auto& r = mRenderer;
  const auto& t = theme();
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(0, 0, 0, 140));
  const TextStyle hint{16.0f, rgb(190, 188, 214), kInk};

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
      if (sel)
        r.fillRect(420, y - 4, 440, step - 4, withAlpha(t.accentB, 60));
      std::string label = kCheatLabels[i];
      if (Cheat(i) == Cheat::God && mWorld)
        label += mWorld->godMode() ? "  [ON]" : "  [OFF]";
      r.drawText(label, 640, y, {22.0f, sel ? t.accentB : t.hudText, kInk, sel}, Align::Center);
    }
    r.drawText("Cheating forfeits this level's bonuses and records.", 640, 522, hint, Align::Center);
    r.drawText("ENTER / A use   ESC / B back", 640, 560, hint, Align::Center);
    return;
  }

  if (mMenu == Menu::Pause)
  {
    r.draw(mMenuPanel, 380, 120);
    r.drawText("PAUSED", 640, 140, {48.0f, t.accentA, kInk, true}, Align::Center);
    const auto items = pauseItems();
    const float step = items.size() > 6 ? 44.0f : 50.0f;
    for (int i = 0; i < int(items.size()); ++i)
    {
      const bool sel = i == mMenuCursor;
      const float y = 216.0f + float(i) * step;
      if (sel)
      {
        r.fillRect(420, y - 6, 440, 42, withAlpha(t.accentA, 60));
        drawGlow(r, *mArt, 640, y + 15, 160, t.accentA, 0.25f);
      }
      const int item = items[std::size_t(i)];
      const std::string label =
        item == kFullscreen ? (mFullscreenNow ? "FULLSCREEN: ON" : "FULLSCREEN: OFF")
                             : kPauseLabels[item];
      r.drawText(label, 640, y, {28.0f, sel ? t.accentA : t.hudText, kInk, sel},
        Align::Center);
    }
    r.drawText("UP/DOWN choose   ENTER / A select   ESC / B resume", 640, 560, hint, Align::Center);
    return;
  }

  if (mMenu == Menu::Runner)
  {
    r.draw(mMenuPanel, 380, 120);
    r.drawText("CHANGE RUNNER", 640, 140, {40.0f, t.accentA, kInk, true}, Align::Center);
    const int current = mWorld ? mWorld->characterIndex() : mCursor;
    for (int i = 0; i < kCharacterCount; ++i)
    {
      const auto& def = characterByIndex(i);
      const bool sel = i == mRunnerCursor;
      const float x = 470.0f + float(i) * 170.0f;
      if (sel)
        drawGlow(r, *mArt, x, 330, 120, t.accentA, 0.35f);
      DrawOpts o;
      o.scale = 0.55f;
      if (!sel)
        o.tint = rgb(130, 126, 150);
      r.draw(mArt->characters[std::size_t(i)].portrait, x, 210, o);
      r.drawText(def.name, x, 360, {26.0f, sel ? t.accentA : t.hudText, kInk, true}, Align::Center);
      r.drawText(i == current ? "PLAYING" : weaponName(def.startWeapon), x, 396, {15.0f, rgb(190, 188, 214)},
        Align::Center);
    }
    r.drawText("Health carries over as a share of the new runner's hearts.", 640, 450, hint, Align::Center);
    r.drawText("LEFT/RIGHT choose   ENTER / A swap   ESC / B back", 640, 560, hint, Align::Center);
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
    r.fillRect(180, y, 920, 60, sel ? withAlpha(t.accentA, 70) : rgba(255, 255, 255, 18));
    std::snprintf(buf, sizeof(buf), "%d", i + 1);
    r.drawText(buf, 214, y + 12, {30.0f, sel ? t.accentA : t.hudText, kInk, true}, Align::Center);
    const auto& s = mSlots[std::size_t(i)];
    if (!s)
    {
      r.drawText("- EMPTY -", 640, y + 16, {22.0f, rgb(150, 148, 170)}, Align::Center);
      continue;
    }
    const auto& def = characterByIndex(std::clamp(s->character, 0, kCharacterCount - 1));
    DrawOpts o;
    o.scale = 0.2f;
    r.draw(mArt->characters[std::size_t(std::clamp(s->character, 0, kCharacterCount - 1))].portrait, 280, y + 2, o);
    r.drawText(def.name, 320, y + 6, {24.0f, sel ? t.accentA : t.hudText, kInk, true});
    r.drawText(s->levelName.empty() ? "STAGE 1" : s->levelName, 320, y + 34, {15.0f, rgb(190, 188, 214)});
    std::snprintf(buf, sizeof(buf), "SCORE %07d   %d:%02d   LETTERS %s", s->score, s->frames / 15 / 60,
      (s->frames / 15) % 60, s->letters.empty() ? "-" : s->letters.c_str());
    r.drawText(buf, 640, y + 8, {20.0f, t.hudText, kInk});
    r.drawText(s->savedAt, 1080, y + 34, {15.0f, rgb(190, 188, 214)}, Align::Right);
  }
  r.drawText(mSlotsForSave ? "ENTER / A save here (overwrites)   ESC / B back" : "ENTER / A load   ESC / B back", 640, 570,
    hint, Align::Center);
}

} // namespace gr
