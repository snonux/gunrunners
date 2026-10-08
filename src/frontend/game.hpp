#pragma once

#include "assets/art.hpp"
#include "data/level.hpp"
#include "frontend/bot.hpp"
#include "game/input.hpp"
#include "game/world.hpp"

#include <memory>
#include <string>

namespace td
{

struct GameOptions
{
  int theme = 0;
  int character = 0;
  bool autoplay = false;
  bool skipMenu = false;
  bool quitAfterClear = false;
  std::string levelPath;
};

// Top-level mode management (character select -> level -> results), the
// equivalent of RigelEngine's frontend/game.cpp + GameMode classes.
class Game
{
public:
  explicit Game(const GameOptions& options);

  // Advances one fixed 60 Hz tick. Returns false once the game wants to quit.
  bool tick(const Input& input);
  void render(Canvas& canvas) const;
  void cycleTheme();

private:
  enum class Mode
  {
    Select,
    Play,
    Clear,
  };

  void setMode(Mode m);
  void startLevel();
  void renderSelect(Canvas& c) const;
  void renderPlayOverlay(Canvas& c) const;
  void renderClear(Canvas& c) const;
  const Theme& theme() const { return themeByIndex(mThemeIndex); }

  GameOptions mOptions;
  int mThemeIndex;
  std::unique_ptr<Art> mArt;
  Level mLevel;
  std::unique_ptr<World> mWorld;
  Mode mMode = Mode::Select;
  int mModeTicks = 0;
  int mFrame = 0;
  int mCursor = 0;
  int mAttempt = 1;
  Bot mBot;
  Input mPrev;
};

} // namespace td
