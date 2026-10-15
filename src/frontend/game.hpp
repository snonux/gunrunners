#pragma once

#include "assets/art.hpp"
#include "audio/audio.hpp"
#include "data/level.hpp"
#include "frontend/bindings.hpp"
#include "frontend/bot.hpp"
#include "frontend/cutscene.hpp"
#include "frontend/cutscene_voices.hpp"
#include "frontend/runner_editor.hpp"
#include "frontend/voices.hpp"
#include "game/input.hpp"
#include "game/run_gesture.hpp"
#include "game/profile.hpp"
#include "game/s3.hpp"
#include "game/savegame.hpp"
#include "game/world.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <future>
#include <functional>
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
  S3Config s3;           // optional remote backing for savegame files
  std::string dataDir;   // holds levels/ and cutscenes/
  int startLevel = 0;    // campaign: start at this level (with skipMenu)
  bool noCutscenes = false;
  std::string cutscene; // play just this cutscene, then quit
  bool cheats = false;  // the pause menu's CHEATS item from the start
  bool window = false;  // a real window (not headless): offer FULLSCREEN
  bool touch = false;   // on-screen gamepad: offer its TOUCH PAD size
  bool voiceLog = false; // print the runners' lines as they trigger and play
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
  // WORLD SELECT is on screen.
  bool worldSelect() const { return mMode == Mode::Worlds && mMenu == Menu::None; }
  int touchSize() const { return mProfile.touchSize; }
  // The app is going to the background (Android may close it there): the
  // pause menu opens over a running level and the profile is saved.
  void suspend();
  // The window lost focus or a gamepad dropped out: a running level pauses.
  void pauseLevel();

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

  // Direct touch in the menus (taps.cpp): a tap at a point of the 1280x720
  // picture picks what is drawn there, and a drag scrolls (in the map it
  // pans). Each screen marks its tap targets as it draws itself.
  void tapAt(float x, float y);
  void dragBy(float dx, float dy);
  // The on-screen menu pad (d-pad, OK, BACK in menus), kept in the profile.
  bool menuPad() const;
  void setMenuPad(bool on);
  // In a level: the thumbs scheme (left thumb moves and jumps, right thumb
  // shoots and aims) or the Jump and Fire buttons. CONTROLS switches it;
  // thumbs by default.
  bool touchThumbs() const;
  // Where the runner is on the screen (the middle of its body), for a touch
  // that shoots at the spot it lands on; false outside a level.
  bool runnerOnScreen(float& x, float& y) const;
  // The level being played (the bonus level while one runs), for tests.
  const World* world() const { return mWorld.get(); }

private:
  enum class Mode
  {
    Title,
    Select,
    Cutscene,
    Play,
    Bonus, // the tally
    Arsenal,
    List,      // an episode's levels, bonus channel, reruns, extras
    Worlds,    // WORLD SELECT: the episodes, DEEP SPACE and EXTRAS
    Cloud,     // copy/paste S3 configuration
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
  // WORLD SELECT (worlds.cpp): one card per episode, DEEP SPACE and EXTRAS;
  // a card opens its levels in the list screen.
  void openWorlds();
  void tickWorlds(const Input& in);
  void renderWorlds();
  // An episode's levels that can be played now (built and reached), in order.
  std::vector<int> worldLevels(int episode) const;
  // Why a WORLD SELECT card is shut, empty when it is open.
  std::string worldLock(int card) const;
  void tickList(const Input& in);
  void renderList();
  void renderContinued();
  void tickCloud(const Input& in);
  void renderCloud();
  float renderAlpha() const;
  void buildPanels();
  void sound(Sfx s);

  // taps.cpp: what the last frame drew that a tap can hit, newest on top.
  struct TapTarget
  {
    float x, y, w, h;
    std::function<void(float, float)> act; // gets the tap's position
  };
  void clearTapTargets();
  void tapTarget(float x, float y, float w, float h, std::function<void(float, float)> act);
  // A press for the next tick, made by a tap or a drag step.
  void tapPress(bool Input::*button);
  Input takeTapInput();

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
    Assists, // ASSISTS AND OPTIONS, from CONTROLS
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
  void beginCloudRefresh();
  void queueCloudUpload(const std::string& path);
  void pollCloud();
  bool loadSave(SaveGame s, const std::string& message);
  void switchRunner(int index);
  void switchRandomRunner(); // any runner but the current one
  void setTheme(int index);
  void useTheme(int index);
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
  // The ASSISTS AND OPTIONS rows (kept in the profile; none is a cheat).
  bool playOption(const char* key, bool fallback = false) const;
  bool assistOn(const char* key) const;
  World::PlayOptions playOptions() const;
  void tickAssists(bool ok, bool cancel, int dir);
  void renderAssists();
  int mAssistCursor = 0;
  int mSlowCount = 0;       // GAME SPEED 75%: every fourth logic frame is skipped
  bool mHeldFrame = false;  // the last logic frame was skipped: draw the newest state
  // The palette in use. Worlds, panels and cutscene clips hold references to
  // it and to the art, so a new look is copied into these same objects.
  const Theme& theme() const { return mLook; }

  // Campaign.
  Profile mProfile;
  int mLevelNumber = 0;       // campaign level being played, 0 = stand-alone
  std::string mLevelPath;     // relative to the data dir in the campaign
  bool mCampaign = false;
  bool mBonusOnly = false;    // replaying a bonus level from the Bonus Channel
  std::unique_ptr<World> mMainWorld; // parked while its bonus level plays
  std::shared_ptr<const Level> mMainLevel;
  Bot mMainBot;
  VoiceDirector mMainVoice; // the level's lines while its bonus level runs
  int mBonusScore = 0, mBonusGems = 0;
  bool mBonusWon = false;
  bool mBonusCheated = false;
  std::unique_ptr<CutscenePlayer> mCutscene;
  std::unique_ptr<ClipKit> mClipKit;
  std::deque<std::string> mCutQueue;
  After mAfter = After::Title;
  Mode mCutsceneReturn = Mode::Title; // where a skipped-into-nothing queue lands
  int mTitleCursor = 0;
  // The announcer shouts GUNRUNNERS as the banner slams in, once per launch.
  int mTitleShoutFrame = -1; // mFrame when it started (-1: not yet)
  void titleShout();
  int mCloudCursor = 0;
  int mPendingLevel = 1;    // runner select leads into this campaign level
  bool mPendingNewGame = false;
  int mArsenalCursor = 0;
  ListKind mListKind = ListKind::Levels;
  std::vector<std::pair<std::string, std::string>> mListItems; // id, label
  int mListCursor = 0;
  int mListEpisode = 1;       // whose levels ListKind::Levels shows
  int mWorldCursor = 0;       // WORLD SELECT's card
  bool mSelectFromList = false; // runner select backs out to the level list
  // SWAP RUNNER ON DEATH: the runners still sitting out, handed from a
  // cleared campaign level to the next one (see World::bench).
  std::vector<int> mBench;

  GameOptions mOptions;
  Renderer& mRenderer;
  Audio* mAudio;
  int mThemeIndex;
  Theme mLook;
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

  // Direct touch: the targets, the screen they were drawn for, the presses
  // they made for the next tick, and drag distance not yet turned into steps.
  std::vector<TapTarget> mTapTargets;
  int mTapScreen = -1;
  Input mTapInput;
  float mDragX = 0.0f, mDragY = 0.0f;
  int mDragStepsX = 0, mDragStepsY = 0;
  int tapScreen() const { return int(mMode) * 16 + int(mMenu); }

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
  // The runners' spoken lines (voices.cpp) and the RUNNER VOICES setting.
  VoiceBank mVoiceBank;
  VoiceDirector mVoice;
  std::string voicesDir() const;
  float voiceVolume() const;
  void beginVoices();
  void renderSubtitle();
  CutsceneVoices mCutsceneVoices; // the cutscenes' speech (voices/cutscenes)
  int mNoticeTicks = 0;
  std::future<std::string> mCloudRefresh;
  struct CloudUpload
  {
    std::uint64_t generation = 0;
    std::future<std::string> result;
  };
  std::vector<CloudUpload> mCloudUploads;
  std::shared_ptr<std::atomic<std::uint64_t>> mCloudGeneration =
    std::make_shared<std::atomic<std::uint64_t>>(0);
  std::shared_ptr<std::mutex> mCloudCommitMutex = std::make_shared<std::mutex>();
  std::uint64_t mCloudRefreshGeneration = 0;
  bool mCloudRefreshPending = false;
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
  RunGesture mRun; // double tap and hold left or right: run
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
