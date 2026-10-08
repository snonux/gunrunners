#pragma once

#include "assets/art.hpp"
#include "audio/audio.hpp"
#include "data/level.hpp"
#include "frontend/bot.hpp"
#include "game/input.hpp"
#include "game/world.hpp"

#include <memory>
#include <string>

namespace gr
{

struct GameOptions
{
  int theme = 0;
  int character = 0;
  bool autoplay = false;
  bool skipMenu = false;
  bool quitAfterClear = false;
  bool trace = false;
  std::string levelPath;
};

// Top-level mode management (character select -> level -> bonus tally), the
// equivalent of RigelEngine's frontend/game.cpp + GameMode classes.
class Game
{
public:
  Game(const GameOptions& options, Renderer& renderer, Audio* audio);

  // Advances one fixed 60 Hz tick. Returns false once the game wants to quit.
  bool tick(const Input& input);
  void render();
  void cycleTheme();

private:
  enum class Mode
  {
    Select,
    Play,
    Bonus,
  };

  void setMode(Mode m);
  void startLevel();
  void tickPlay(const Input& raw);
  void tickBonus(const Input& in);
  void renderSelect();
  void renderPlayOverlay();
  void renderBonus();
  float renderAlpha() const;
  void buildPanels();
  void sound(Sfx s);
  const Theme& theme() const { return themeByIndex(mThemeIndex); }

  GameOptions mOptions;
  Renderer& mRenderer;
  Audio* mAudio;
  int mThemeIndex;
  std::unique_ptr<Art> mArt;
  Level mLevel;
  std::unique_ptr<World> mWorld;
  Mode mMode = Mode::Select;
  int mModeTicks = 0;
  int mFrame = 0;
  int mCursor = 0;
  Bot mBot;
  Input mPrev;

  // 15 Hz logic clock and input latching.
  int mSubTick = 0;
  PlayerInput mLatched;
  Input mBotInput;
  Input mPrevLogicInput;

  // Bonus tally.
  std::vector<Bonus> mBonuses;
  int mScoreBeforeBonus = 0;
  int mShownScore = 0;
  int mBonusesShown = 0;

  Texture mCardPanel;
  Texture mCardPanelSelected;
  Texture mBannerPanel;
  Texture mBonusPanel;
};

} // namespace gr
