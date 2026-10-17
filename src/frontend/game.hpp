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
  bool hard = false; // HARD: more enemies in every level (data/hard_mode.hpp)
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
  // Ends any cloud save sync still running (cancelCloudSync) so quitting
  // does not wait out the S3 timeouts: about 3 s at most (see there).
  ~Game();

  // Advances one fixed 60 Hz tick. Returns false once the game wants to quit.
  bool tick(const Input& input);
  // Fullscreen is borderless at the desktop's resolution. The window code
  // applies it and reports back with setFullscreen, which saves it in the
  // profile (the campaign's; see saveProfile); the menus ask for a switch
  // through takeFullscreenToggle.
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
  // A device reset (recoverRender) leaves every texture dead. dropTextures
  // lets go of the game's, in this order: the art (emptied in place, as
  // worlds, the cutscene's clip kit and the runner editor keep references to
  // it), the menu panels, the map's textures, the cutscene clips' cache and
  // the runner editor's. Nothing may draw until rebakeTextures, which bakes
  // the art, the panels and the editor's panels again; the map, the clips
  // and the art's lazily baked runners and styled enemies are baked again
  // as they are next drawn. The window code runs both through
  // recoverRender (windowOwners), around its touch controls' and the
  // renderer's own. A new holder of textures in the game must be added to
  // both: Renderer::staleDraws only catches one left out if a test draws
  // its textures after a reset.
  void dropTextures();
  void rebakeTextures();

  // For the on-screen gamepad: a level is being played (its layout) rather
  // than a menu, cutscene or tally, and the TOUCH PAD size setting.
  bool inLevel() const;
  // A cloud refresh or upload is still running or not yet collected by
  // tick (pollCloud). Once a refresh's future is ready, every one of its
  // workers has returned, each having checked its file's dirty marker.
  bool cloudSyncing() const { return mCloudRefresh.valid() || !mCloudUploads.empty(); }
  // WORLD SELECT is on screen.
  bool worldSelect() const { return mMode == Mode::Worlds && mMenu == Menu::None; }
  int touchSize() const { return mProfile.touchSize; }
  // The app is going to the background (Android may close it there): the
  // pause menu opens over a running level and the profile is saved (in the
  // campaign; see saveProfile). False when the disk refused that write (the
  // window code has no one to tell; the tests check it); a stand-alone level
  // has nothing to write, which counts as saved.
  bool suspend();
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
  // The current theme's art, for tests.
  const Art& art() const { return *mArt; }

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
    Difficulty, // NORMAL / HARD over the cards (the campaign)
    Action, // EDIT / REMIX under the card
  };
  int selectCards() const;
  bool tickSelect(Input& in, const Input& raw);
  void renderSelect();
  // index: the runner to edit or remix; -1 a new one.
  void openEditor(int index);
  void tickEditor(const Input& in);
  // The editor's SAVE and DELETE: change the roster, write runners.txt and
  // tell the player how that went (a failed write says so, not the usual).
  void saveEditedRunner();
  void deleteEditedRunner();
  void renderEditor();
  void renderPlayOverlay();
  void renderBonus();
  void finishTally();

  // flow.cpp: the campaign around the levels.
  bool campaign() const { return mCampaign; }
  std::string dataDir() const;
  std::string saveDir() const;
  // Writes mProfile to saveDir(), the one way every setting and every bit of
  // progress gets there. A stand-alone level (--level) plays with a default
  // profile (the constructor does not load one, so the run does not depend on
  // the player's assists and Arsenal), so it never writes it either: that
  // would replace the player's real profile.txt (progress, Arsenal, stars,
  // bindings) with the defaults (task le3). Its settings last for the run.
  // False when the disk refused the write; nothing to write counts as done.
  bool saveProfile();
  void goTitle();
  void tickTitle(const Input& in);
  void renderTitle();
  std::vector<std::string> titleItems() const;
  void beginCampaignLevel(int number, bool fromNewGame);
  void playBriefing(int number, bool fromNewGame);
  // A level file, with the HARD enemies when hard is set. It only reads
  // (mLevel stays), so a save being loaded can try its level aside. nullptr,
  // after a notice, if it won't load; a failure also forgets the kept
  // level-file lookups (forgetDataFiles), as the file may have been deleted
  // since a menu looked it up. readLevelFile takes the file as it is
  // (one that already has the data dir in front); readLevel's path is
  // relative to the data dir unless absolute.
  std::shared_ptr<const Level> readLevelFile(const std::string& file, bool hard);
  std::shared_ptr<const Level> readLevel(const std::string& path, bool hard);
  // readLevel at the HARD setting, made the level being played (mLevel,
  // mLevelPath); false, with the old level kept, if it won't load.
  bool loadLevel(const std::string& path);
  // Makes level the level being played, read from path (see mLevelPath).
  // mLevel only ever changes through here, with the file it came from, so a
  // save being loaded can tell whether its level is the one loaded.
  void useLevel(std::shared_ptr<const Level> level, std::string path);
  void applyLevelLook(int episode, const std::string& themeKey);
  void playLevelMusic();
  void playCutscenes(std::vector<std::string> names, After after);
  void nextCutscene();
  void afterCutscenes();
  void tickCutscene(const Input& in);
  void enterBonus();
  void startBonusWorld();
  void leaveBonus();
  // The level a bonus level is entered from is parked behind it as one unit:
  // its world, level, file, bot and voice lines (mMainWorld ...). These three
  // are the only ways in and out, so a level is parked exactly when a world
  // is (goTitle drops the world first, then takes the level back).
  // parkMainLevel parks the level being played; the caller then makes the
  // bonus level the one played (useLevel).
  void parkMainLevel();
  // What was parked is played again (the bonus level is over), or on the
  // title the level and its file come back (QUIT TO TITLE from the bonus
  // level, its world already gone; the bot and voice lines come back too,
  // idle there until a level starts them anew). Nothing parked, nothing
  // changes.
  void unparkMainLevel();
  // What was parked goes: another level (a loaded save, the next campaign
  // level) ends the bonus level and the one parked behind it.
  void dropParkedLevel();
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
  // From the lists, played on their own and back to the list after: a
  // level's bonus level (the Bonus Channel) and an EXTRAS level.
  void playBonusChannel(int level);
  void playExtra(int index);
  // What the list screen's confirm does with the item under the cursor.
  void pickListItem(std::string id);
  void renderList();
  void renderContinued();
  void tickCloud(const Input& in);
  void renderCloud();
  float renderAlpha() const;
  void buildPanels();
  void dropPanels(); // every texture buildPanels makes
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
    Settings, // SETTINGS: difficulty, controls, assists, play options, fullscreen
    Controls,
    TouchEdit,
    Assists, // ASSISTS or PLAY OPTIONS (mAssistPage), from SETTINGS
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
  // For quitting: cancels the cloud refresh at once, gives uploads still
  // running a short grace period to finish, then cancels them too and waits
  // for every worker. A cancelled request aborts within about a second
  // (libcurl's progress callback), except that with libcurl older than 7.87
  // (no CURLOPT_QUICK_EXIT) one cancelled in its DNS lookup waits for
  // getaddrinfo to return.
  void cancelCloudSync();
  // Builds the save's world aside and swaps it in only once it fits; a
  // refused save (another level, a level that won't load, one that does
  // not fit) leaves the run in progress, its level and HARD as they were.
  bool loadSave(SaveGame s, const std::string& message);
  // The world a save restores into, built on level apart from the run in
  // progress; nullptr, after the refusal's notice, if the save is from
  // another level or does not fit it.
  std::unique_ptr<World> restoreAside(const SaveGame& s, const std::shared_ptr<const Level>& level);
  // The save fit: level, its level, becomes the one played. reread: it was
  // read again from file (the old level's preloads turn stale); otherwise
  // it is the level loaded, or the one parked behind a bonus level.
  void useSaveLevel(std::shared_ptr<const Level> level, bool reread, const std::string& file);
  // A loaded campaign save's level number and difficulty (the levels after
  // it follow them); a stand-alone level keeps none.
  void takeSaveProgress(const SaveGame& s);
  // The save fit: its restored world replaces the run in progress, in the
  // save's look (theme index). It ends a bonus level: the world and level
  // parked behind one go, and so does a Bonus Channel or EXTRAS run.
  // mLevel must already be the world's level: the voices and music read it.
  void startLoadedWorld(std::unique_ptr<World> world, int themeIndex, const std::string& message);
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
  // The ASSISTS and PLAY OPTIONS rows (kept in the profile; none is a cheat).
  bool playOption(const char* key, bool fallback = false) const;
  bool assistOn(const char* key) const;
  World::PlayOptions playOptions() const;
  void tickAssists(bool ok, bool cancel, int dir);
  void renderAssists();
  int mAssistCursor = 0;
  int mAssistPage = 0; // 0 ASSISTS, 1 PLAY OPTIONS
  // settings_menu.cpp: SETTINGS, from the title screen and the pause menu.
  // Everything in it takes effect at once, mid-level too.
  std::vector<int> settingsItems() const;
  void openSettings(Menu back);
  void tickSettings(bool ok, bool cancel, int dir, int side);
  void renderSettings();
  int mSettingsCursor = 0;
  // DIFFICULTY in SETTINGS: switches it and refits the level being played.
  void switchDifficulty();
  // The level being played (or parked behind a bonus level) takes the
  // difficulty now: rebuilt with or without HARD's extras through a save of
  // itself. False while it can't be saved (a fight or a ride): mRefitHard
  // then tries again every frame until it can.
  bool refitDifficulty();
  bool refitWorld(std::unique_ptr<World>& world, std::shared_ptr<const Level>& level, const std::string& path);
  bool mRefitHard = false;
  int mSlowCount = 0;       // GAME SPEED 75%: every fourth logic frame is skipped
  bool mHeldFrame = false;  // the last logic frame was skipped: draw the newest state
  // The palette in use. Worlds, panels and cutscene clips hold references to
  // it and to the art, so a new look is copied into these same objects.
  const Theme& theme() const { return mLook; }

  // Campaign.
  Profile mProfile;
  // The campaign level being played (also while its bonus level runs, and
  // the level whose bonus level the Bonus Channel plays). 0 in a stand-alone
  // level, the training stage and EXTRAS, and on the title, which keeps the
  // last level played (mLevel, mLevelPath; quit from a bonus level, the level
  // it was entered from): a save loaded there brings its number (loadSave).
  int mLevelNumber = 0;
  // The file mLevel was read from, as a campaign save names it (relative to
  // the data dir). Empty for a level no save names: a stand-alone level
  // (--level), whose saves have no file, and a bonus level (in its level or
  // on the Bonus Channel) or an EXTRAS level, which can't be saved in: a
  // campaign save loaded there always reads its level again (loadSave).
  std::string mLevelPath;
  bool mCampaign = false;
  bool mBonusOnly = false;    // a Bonus Channel or EXTRAS level, played on its own
  // The level a bonus level was entered from, parked while the bonus level
  // plays (parkMainLevel and its two partners keep them together): its
  // world, its level and that level's file, its bot and its voice lines.
  std::unique_ptr<World> mMainWorld;
  std::shared_ptr<const Level> mMainLevel;
  std::string mMainLevelPath;
  Bot mMainBot;
  VoiceDirector mMainVoice;
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
  // HARD: levels load with their extra enemies (data/hard_mode.hpp). Chosen
  // on runner select and kept in the profile (opt.hard); a save brings its own.
  bool mHard = false;
  void setHard(bool hard);
  void toggleHard(); // runner select: switch it and say what it means
  int mArsenalCursor = 0;
  ListKind mListKind = ListKind::Levels;
  std::vector<std::pair<std::string, std::string>> mListItems; // id, label
  int mListCursor = 0;
  int mListEpisode = 1;       // whose levels ListKind::Levels shows
  int mWorldCursor = 0;       // WORLD SELECT's card
  bool mSelectFromList = false; // runner select backs out to the level list
  // SWAP RUNNER ON DEATH: the runner the level started with, who starts it
  // again once every runner is down (see World::wipedOut).
  int mLevelRunner = 0;

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
  // Cloud save workers. A std::async future blocks in its destructor until
  // the task ends, so ~Game (cancelCloudSync) makes the requests stale first:
  // the workers' S3Active check then fails and libcurl aborts the request
  // instead of running into its 10 s connect / 30 s transfer timeouts.
  // mCloudGeneration bumps make every request stale, mCloudQuitting only
  // the refresh's. Both change under mCloudCommitMutex so a worker never
  // commits a file for a stale request.
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
  std::shared_ptr<std::atomic<bool>> mCloudQuitting =
    std::make_shared<std::atomic<bool>>(false);
  std::uint64_t mCloudRefreshGeneration = 0;
  bool mCloudRefreshPending = false;
  // Makes every cloud request stale when destroyed. Declared after the cloud
  // futures, so it is destroyed before them: if the constructor throws after
  // beginCloudRefresh, ~Game never runs, and without this the futures'
  // destructors would wait out the S3 timeouts. (After ~Game nothing is
  // left for it to cancel.)
  struct CloudCancel
  {
    std::shared_ptr<std::atomic<std::uint64_t>> generation;
    std::shared_ptr<std::mutex> commitMutex;
    ~CloudCancel();
  };
  CloudCancel mCloudCancel{mCloudGeneration, mCloudCommitMutex};
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
  Menu mControlsReturn = Menu::None; // where SETTINGS goes back to

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
