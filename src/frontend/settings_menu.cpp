// SETTINGS: one place for everything that can be set. DIFFICULTY, CONTROLS
// (keys, gamepad and the touch pad), ASSISTS, PLAY OPTIONS and FULLSCREEN,
// reached from the title screen and the pause menu. Every one of them takes
// effect at once, in the middle of a level too; DIFFICULTY rebuilds the
// level being played with (or without) HARD's extra enemies and carries on
// from where it was.

#include "frontend/game.hpp"

#include "data/hard_mode.hpp"

#include <algorithm>
#include <cstdio>
#include <exception>

namespace gr
{

namespace
{

constexpr Color kInk = rgb(16, 12, 26);

enum SettingsItem
{
  kDifficulty,
  kControls,
  kAssists,
  kPlayOptions,
  kFullscreen,
  kBack,
  kSettingsItemCount,
};

const char* const kSettingsHelp[kSettingsItemCount] = {
  "HARD: more enemies. The level you are in changes too.",
  "Keys, gamepad buttons and the touch pad.",
  "Help with jumps, damage, speed and the way to go.",
  "Hold to fire, rides, effects, voices and subtitles.",
  "Play in a window or on the whole screen.",
  "",
};

} // namespace

std::vector<int> Game::settingsItems() const
{
  std::vector<int> items;
  for (int i = 0; i < kSettingsItemCount; ++i)
    if ((i != kDifficulty || campaign()) && (i != kFullscreen || mOptions.window))
      items.push_back(i);
  return items;
}

void Game::openSettings(Menu back)
{
  mControlsReturn = back;
  mMenu = Menu::Settings;
  mSettingsCursor = std::clamp(mSettingsCursor, 0, int(settingsItems().size()) - 1);
}

void Game::switchDifficulty()
{
  setHard(!mHard);
  if (refitDifficulty())
  {
    mRefitHard = false;
    notice(mHard ? "HARD - MORE ENEMIES IN EVERY LEVEL" : "NORMAL - THE LEVELS AS DESIGNED");
    return;
  }
  // A fight or a ride can't be saved, so it can't be rebuilt either: the
  // level changes the moment it could be saved again.
  mRefitHard = true;
  notice(mHard ? "HARD - THE EXTRA ENEMIES COME AFTER THIS FIGHT OR RIDE"
               : "NORMAL - THE EXTRA ENEMIES GO AFTER THIS FIGHT OR RIDE");
}

bool Game::refitDifficulty()
{
  if (!campaign() || mBonusOnly)
    return true; // bonus levels have no extras
  // In a bonus level the level parked behind it changes.
  if (mMainWorld)
    return refitWorld(mMainWorld, mMainLevel, mMainLevelPath);
  return refitWorld(mWorld, mLevel, mLevelPath);
}

bool Game::refitWorld(std::unique_ptr<World>& world, std::shared_ptr<const Level>& level, const std::string& path)
{
  // Between levels the next one simply loads with the new difficulty.
  if (!world || !level || level->hard == mHard || path.empty())
    return true;
  if (!world->canSave())
    return false;
  std::shared_ptr<const Level> next;
  try
  {
    const std::string full = path[0] == '/' ? path : dataDir() + "/" + path;
    Level lv = Level::loadFile(full);
    if (mHard)
      addHardModeEnemies(lv);
    next = std::make_shared<const Level>(std::move(lv));
  }
  catch (const std::exception& e)
  {
    std::fprintf(stderr, "cannot reload %s: %s\n", path.c_str(), e.what());
    return true; // the level carries on as it was
  }
  if (next->name != level->name)
    return true;
  auto fresh = std::make_unique<World>(next, world->characterIndex(), theme(), *mArt);
  prepareWorld(*fresh);
  // A save of the level as it is, with the extras put in or taken out.
  if (!fresh->restore(world->snapshotFor(*fresh)))
  {
    std::fprintf(stderr, "difficulty change: the save did not fit %s\n", path.c_str());
    return true;
  }
  fresh->settleView(); // the pause menu draws it before it ticks
  world = std::move(fresh);
  level = std::move(next);
  return true;
}

void Game::tickSettings(bool ok, bool cancel, int dir, int side)
{
  const auto items = settingsItems();
  const int n = int(items.size());
  mSettingsCursor = (mSettingsCursor + dir + n) % n;
  const int item = items[std::size_t(mSettingsCursor)];
  if (cancel || (ok && item == kBack))
  {
    if (ok)
      sound(Sfx::MenuSelect);
    if (mControlsReturn == Menu::Pause)
      openMenu(Menu::Pause);
    else
      closeMenu();
    return;
  }
  // Left and right switch the difficulty too, as on runner select.
  if (item == kDifficulty && side != 0)
  {
    switchDifficulty();
    return;
  }
  if (!ok)
    return;
  sound(Sfx::MenuSelect);
  switch (item)
  {
    case kDifficulty:
      switchDifficulty();
      break;
    case kControls:
      openControls();
      break;
    case kAssists:
    case kPlayOptions:
      mMenu = Menu::Assists;
      mAssistPage = item == kAssists ? 0 : 1;
      mAssistCursor = 0;
      break;
    case kFullscreen:
      mFullscreenToggle = true;
      break;
    default:
      break;
  }
}

void Game::renderSettings()
{
  auto& r = mRenderer;
  const auto& t = theme();
  // Over the title screen's own menu the see-through panel reads as
  // clutter: dim the screen further first.
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(0, 0, 0, 150));
  r.draw(mMenuPanel, 380, 120);
  r.drawText("SETTINGS", 640, 140, {48.0f, t.accentA, kInk, true}, Align::Center);
  const auto items = settingsItems();
  const int n = int(items.size());
  const float step = 46.0f;
  const float top = 220.0f + (6 - float(n)) * step * 0.5f;
  for (int i = 0; i < n; ++i)
  {
    const bool sel = i == mSettingsCursor;
    const int item = items[std::size_t(i)];
    const float y = top + float(i) * step;
    tapTarget(400, y + 15 - step * 0.5f, 480, step, [this, i](float, float) {
      mSettingsCursor = i;
      tapPress(&Input::confirm);
    });
    if (sel)
    {
      r.fillRect(420, y - 6, 440, step - 6, withAlpha(t.accentA, 60));
      drawGlow(r, *mArt, 640, y + 15, 160, t.accentA, 0.25f);
    }
    std::string label;
    Color color = sel ? t.accentA : t.hudText;
    switch (item)
    {
      case kDifficulty:
        label = sel ? (mHard ? "DIFFICULTY:  <  HARD  >" : "DIFFICULTY:  <  NORMAL  >")
                    : (mHard ? "DIFFICULTY: HARD" : "DIFFICULTY: NORMAL");
        if (mHard)
          color = rgb(255, 96, 72);
        break;
      case kControls:
        label = "CONTROLS";
        break;
      case kAssists:
        label = "ASSISTS";
        break;
      case kPlayOptions:
        label = "PLAY OPTIONS";
        break;
      case kFullscreen:
        label = mFullscreenNow ? "FULLSCREEN: ON" : "FULLSCREEN: OFF";
        break;
      default:
        label = "BACK";
        break;
    }
    r.drawText(label, 640, y, {26.0f, color, kInk, sel}, Align::Center);
  }
  const int sel = items[std::size_t(std::clamp(mSettingsCursor, 0, n - 1))];
  const TextStyle hint{16.0f, rgb(190, 188, 214), kInk};
  r.drawText(kSettingsHelp[sel], 640, 528, {16.0f, t.accentB, kInk}, Align::Center);
  r.drawText(mOptions.touch ? "TAP to choose   BACK to leave" : "UP/DOWN choose   ENTER / A select   ESC / B back", 640,
    566, hint, Align::Center);
}

} // namespace gr
