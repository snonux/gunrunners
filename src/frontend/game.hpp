#pragma once

#include "assets/art.hpp"
#include "audio/audio.hpp"
#include "data/level.hpp"
#include "frontend/bot.hpp"
#include "frontend/cutscene.hpp"
#include "game/profile.hpp"
#include "game/input.hpp"
#include "game/savegame.hpp"
#include "game/world.hpp"

#include <array>
#include <deque>
#include <memory>
#include <optional>
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
  std::string levelPath; // a stand-alone level; empty plays the campaign
  std::string saveDir;   // where the 5 savegame slots and the profile live
  std::string dataDir;   // holds levels/ and cutscenes/
  int startLevel = 0;    // campaign: start at this level (with skipMenu)
  bool noCutscenes = false;
  std::string cutscene; // play just this cutscene, then quit
  bool cheats = false;  // the pause menu's CHEATS item from the start
  bool window = false;  // a real window (not headless): offer FULLSCREEN
};

// Top-level mode management, the equivalent of RigelEngine's
// frontend/game.cpp + GameMode classes. The campaign runs title -> runner
// select -> briefing cutscene -> level (with its bonus level behind the B)
// -> tally -> episode cutscene -> next briefing; a stand-alone level file
// runs runner select -> level -> tally.
class Game
{
public:
  Game(const GameOptions& options, Renderer& renderer, Audio* audio);

  // Advances one fixed 60 Hz tick. Returns false once the game wants to quit.
  bool tick(const Input& input);
  // Fullscreen is borderless at the desktop's resolution. The window code
  // applies it and reports back with setFullscreen, which saves it in the
  // profile; the menus ask for a switch through takeFullscreenToggle.
  // showFullscreen only updates the menu label (a command-line flag
  // overrode the saved setting for this run).
  bool fullscreenSetting() const { return mProfile.fullscreen; }
  void setFullscreen(bool on);
  void showFullscreen(bool on) { mFullscreenNow = on; }
  bool takeFullscreenToggle()
  {
    const bool t = mFullscreenToggle;
    mFullscreenToggle = false;
    return t;
  }
  void render();
  void cycleTheme();

private:
  enum class Mode
  {
    Title,
    Select,
    Cutscene,
    Play,
    Bonus, // the tally
    Arsenal,
    List,      // level select, bonus channel, reruns
    Continued, // the campaign ran out of built levels
  };
  // What happens once the queued cutscenes have played (flow.cpp).
  enum class After
  {
    StartLevel,
    NextLevel,
    Title,
    EnterBonus,
    LeaveBonus,
    List, // back to the list screen it came from
    Quit,
  };
  enum class ListKind
  {
    Levels,
    BonusChannel,
    Reruns,
  };

  void setMode(Mode m);
  void startLevel();
  void tickPlay(const Input& raw);
  void tickBonus(const Input& in);
  void renderSelect();
  void renderPlayOverlay();
  void renderBonus();
  void finishTally();

  // flow.cpp: the campaign around the levels.
  bool campaign() const { return mCampaign; }
  std::string dataDir() const;
  std::string saveDir() const;
  void goTitle();
  void tickTitle(const Input& in);
  void renderTitle();
  std::vector<std::string> titleItems() const;
  void beginCampaignLevel(int number, bool fromNewGame);
  bool loadLevel(const std::string& path);
  void applyLevelLook(int episode, const std::string& themeKey);
  void playLevelMusic();
  void playCutscenes(std::vector<std::string> names, After after);
  void nextCutscene();
  void afterCutscenes();
  void tickCutscene(const Input& in);
  void enterBonus();
  void startBonusWorld();
  void leaveBonus();
  void recordClear();
  void tickArsenal(const Input& in);
  void renderArsenal();
  void openList(ListKind kind);
  void tickList(const Input& in);
  void renderList();
  void renderContinued();
  float renderAlpha() const;
  void buildPanels();
  void sound(Sfx s);

  // menus.cpp: pause menu, savegame slots, runner switching.
  enum class Menu
  {
    None,
    Pause,
    Slots,
    Runner,
    Cheats,
  };
  bool tickMenu(const Input& in);
  // The pause menu's items; CHEATS shows up once the code was entered.
  std::vector<int> pauseItems() const;
  void openMenu(Menu m);
  void closeMenu();
  void refreshSlots();
  void saveToSlot(int slot);
  bool loadFromSlot(int slot);
  void switchRunner(int index);
  void setTheme(int index);
  void notice(const std::string& text);
  void renderMenu();
  void renderNotice();
  const Theme& theme() const { return themeByIndex(mThemeIndex); }

  // Campaign.
  Profile mProfile;
  int mLevelNumber = 0;       // campaign level being played, 0 = stand-alone
  std::string mLevelPath;     // relative to the data dir in the campaign
  bool mCampaign = false;
  bool mBonusOnly = false;    // replaying a bonus level from the Bonus Channel
  std::unique_ptr<World> mMainWorld; // parked while its bonus level plays
  std::shared_ptr<const Level> mMainLevel;
  Bot mMainBot;
  int mBonusScore = 0, mBonusGems = 0;
  bool mBonusWon = false;
  bool mBonusCheated = false;
  std::unique_ptr<CutscenePlayer> mCutscene;
  std::unique_ptr<ClipKit> mClipKit;
  std::deque<std::string> mCutQueue;
  After mAfter = After::Title;
  Mode mCutsceneReturn = Mode::Title; // where a skipped-into-nothing queue lands
  int mTitleCursor = 0;
  int mPendingLevel = 1;    // runner select leads into this campaign level
  bool mPendingNewGame = false;
  int mArsenalCursor = 0;
  ListKind mListKind = ListKind::Levels;
  std::vector<std::pair<std::string, std::string>> mListItems; // id, label
  int mListCursor = 0;

  GameOptions mOptions;
  Renderer& mRenderer;
  Audio* mAudio;
  int mThemeIndex;
  std::unique_ptr<Art> mArt;
  std::shared_ptr<const Level> mLevel;
  std::unique_ptr<World> mWorld;
  Mode mMode = Mode::Select;
  int mModeTicks = 0;
  int mAutoWait = 0; // autoplay: ticks a cutscene panel has waited for a button
  int mFrame = 0;
  int mCursor = 0;
  Bot mBot;
  Input mPrev;

  // Menus.
  Menu mMenu = Menu::None;
  bool mSlotsForSave = false;
  bool mTitleFocusLoad = false; // title screen: the LOAD GAME button has focus
  int mMenuCursor = 0;
  int mSlotCursor = 0;
  int mRunnerCursor = 0;
  int mCheatCursor = 0;
  bool mCheatsUnlocked = false; // the code was entered on the pause menu
  int mCodeStep = 0;            // how much of the code has been entered
  std::array<std::optional<SaveGame>, kSaveSlots> mSlots;
  std::string mNotice;
  std::string mPlayingOverride; // the world's music override now playing
  int mNoticeTicks = 0;
  bool mQuit = false;
  bool mFullscreenToggle = false;
  bool mFullscreenNow = false;

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
  Texture mMenuPanel;
  Texture mSlotPanel;
  Texture mLoadButton;
  Texture mLoadButtonFocus;
};

} // namespace gr
