#pragma once

#include "assets/art.hpp"
#include "audio/audio.hpp"
#include "data/level.hpp"
#include "frontend/bindings.hpp"
#include "frontend/bot.hpp"
#include "frontend/cutscene.hpp"
#include "frontend/runner_editor.hpp"
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
  bool touch = false;   // on-screen gamepad: offer its TOUCH PAD size
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

  // For the on-screen gamepad: a level is being played (its layout) rather
  // than a menu, cutscene or tally, and the TOUCH PAD size setting.
  bool inLevel() const;
  int touchSize() const { return mProfile.touchSize; }
  // The app is going to the background (Android may close it there): the
  // pause menu opens over a running level and the profile is saved.
  void suspend();

  // The CONTROLS menu (controls_menu.cpp). The window code reads the
  // bindings, and while the menu listens for a new key or pad button it
  // hands over the next press instead of turning it into Input.
  const Bindings& bindings() const { return mBindings; }
  bool listening() const { return mListening; }
  void captureKey(int scancode);
  void capturePad(int code);
  // The on-screen gamepad's layout editor, and the layout itself.
  bool touchEditing() const { return mMenu == Menu::TouchEdit; }
  const std::vector<int>& touchLayout() const;
  void setTouchLayout(const std::vector<int>& offsets);
  // The runner editor's name entry takes typing from a real keyboard: the
  // window code hands over text, Backspace and Enter instead of keys.
  bool wantsText() const;
  void typeText(const std::string& text);
  void textBackspace();
  void textDone();

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
    Editor,    // the runner editor, from runner select
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
    Extras,
  };

  void setMode(Mode m);
  void startLevel();
  // Settings every new world takes from the frontend (vehicle prompts, up boards).
  void prepareWorld(World& world) const;
  void tickPlay(const Input& raw);
  void tickBonus(const Input& in);
  // select.cpp: runner select and the runner editor.
  enum class SelectFocus
  {
    Cards,
    Load,   // LOAD GAME (stand-alone levels)
    Action, // EDIT / REMIX under the card
  };
  int selectCards() const;
  bool tickSelect(Input& in, const Input& raw);
  void renderSelect();
  // index: the runner to edit or remix; -1 a new one.
  void openEditor(int index);
  void tickEditor(const Input& in);
  void renderEditor();
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
    Controls,
    TouchEdit,
    Map,
  };
  bool tickMenu(const Input& in);
  // The pause menu's items; CHEATS shows up once the code was entered.
  std::vector<int> pauseItems() const;
  void openMenu(Menu m);
  void closeMenu();
  void refreshSlots();
  void saveToSlot(int slot);
  bool loadFromSlot(int slot);
  // The quick save slot, apart from the five (F5 / F9 by default).
  void quickSave();
  bool quickLoad();
  bool saveTo(const std::string& path);
  bool loadSave(SaveGame s, const std::string& message);
  void switchRunner(int index);
  void setTheme(int index);
  void notice(const std::string& text);
  void renderMenu();
  void renderNotice();
  // map_view.cpp: the level map, a pause-style overlay.
  void openMap();
  void tickMap(const Input& in, bool close, bool ok);
  void renderMap();
  void freeMap();
  float mapFitScale() const;
  float mapScale() const;
  // controls_menu.cpp
  struct ControlRow;
  std::vector<ControlRow> controlRows() const;
  void openControls();
  void tickControls(const Input& in, bool ok, bool cancel, int dir, int side);
  void renderControls();
  void saveBindings();
  std::string keysHint() const;
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
  SelectFocus mSelectFocus = SelectFocus::Cards;
  float mSelectScroll = 0.0f; // the carousel's position, in cards
  std::unique_ptr<RunnerEditor> mEditor;
  int mEditorReturn = 0; // runner select's card when the editor opened
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
  void cycleTouchSize();
  std::string touchSizeLabel() const;

  // The map overlay: zoomed in (else the whole level), the block in the
  // middle of the view, and the textures baked when it opened.
  struct MapChunk
  {
    int bx = 0, by = 0;
    Texture tex;
  };
  bool mMapClose = true;
  float mMapX = 0.0f, mMapY = 0.0f;
  int mMapPanTicks = 0;
  const World* mMapWorld = nullptr; // what the textures show
  int mMapTheme = -1;
  bool mMapWholeBaked = false, mMapChunksBaked = false;
  Texture mMapWhole;
  std::vector<MapChunk> mMapChunks;

  // CONTROLS menu.
  Bindings mBindings = Bindings::defaults(); // the profile's, in the campaign
  int mControlsCursor = 0;
  int mControlsColumn = 0; // key 1, key 2, pad
  bool mListening = false;
  int mListenTicks = 0;
  bool mMenuHold = false;  // ignore the menu until every button is let go
  Menu mControlsReturn = Menu::None;

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
  Texture mActionButton;
  Texture mActionButtonFocus;
};

} // namespace gr
