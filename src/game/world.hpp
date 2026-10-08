#pragma once

#include "assets/art.hpp"
#include "base/math.hpp"
#include "data/characters.hpp"
#include "data/enemies.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "data/weapons.hpp"
#include "engine/camera.hpp"
#include "game/collision.hpp"
#include "game/input.hpp"
#include "game/savegame.hpp"
#include "game/sound_ids.hpp"
#include "render/renderer.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace gr
{

// The game logic runs at Duke Nukem II's 15 frames per second; rendering
// runs at 60 and interpolates in between, like RigelEngine's motion
// smoothing.
constexpr int kTicksPerLogicFrame = 4;
constexpr int kTurboFramesTotal = 15 * 15; // Turbo Mode: 15 seconds of logic frames
constexpr int kVirusFramesTotal = 15 * 8;  // Virus: 8 seconds
constexpr float kViewCellsW = float(kScreenW) / (kPixelScale * float(kCellSize));
constexpr float kViewCellsH = float(kScreenH) / (kPixelScale * float(kCellSize));

// --- Player ------------------------------------------------------------------

enum class PlayerState
{
  OnGround,
  Jumping,
  Falling,
  Recovering, // one frame after landing at full speed
  Ladder,
  Pipe,
  Jetpack,
  Dying,
  Teleporting, // leaving through the exit
};

// What the player looks like; also decides the hit box (RigelEngine's
// VisualState).
enum class PlayerVisual
{
  Standing,
  Walking,
  LookingUp,
  Crouching,
  Coiling, // about to jump, or absorbing a hard landing
  Jumping,
  Somersault,
  Falling,
  FallingFull,
  ClimbingLadder,
  Hanging,
  MovingOnPipe,
  AimingDownOnPipe,
  PullingLegsUp,
  Jetpack,
  Dying,
};

enum class Stance
{
  Regular,
  Crouched,
  Up,
  Down,
  Jetpack,
};

struct Player
{
  int x = 0; // bottom-left cell
  int y = 0;
  int prevX = 0;
  int prevY = 0;
  int facing = 1;
  PlayerState state = PlayerState::OnGround;
  PlayerVisual visual = PlayerVisual::Standing;
  Stance stance = Stance::Regular;
  int frames = 0; // frames spent in the current state (jump arc index etc.)
  bool fromLadder = false;
  int somersault = -1; // frame of the somersault, -1 if none
  int deathPhase = 0;
  bool jumpRequested = false;
  int coyote = 0; // frames after the ground went away in which a jump still works
  bool rapidFiredLastFrame = false;
  bool oddFrame = false;
  bool hidden = false;

  int hp = 9;
  int maxHp = 9;
  int mercy = 0;
  Weapon weapon = Weapon::Normal;
  int proto = -1; // ProtoId while weapon == Weapon::Proto
  int ammo = 0;
  int shotCooldown = 0; // frames until the prototype can fire again
  int charge = 0;       // frames fire has been held (charge and hold modes)
  int rapidFire = 0; // frames left
  int turbo = 0;     // frames of Turbo Mode left: every stat maxed
  int virus = 0;     // frames of infection left: slower and weaker
  bool hasKey = false;

  int walkFrame = 0;
  int climbFrame = 0;
  int pipeFrame = 0;
  int recoil = 0;
  int muzzleTicks = 0; // 60 Hz
  Stance muzzleStance = Stance::Regular;

  static constexpr int kWidth = 3;
  static constexpr int kHeight = 5;
  int height() const;
  CellBox box() const { return boxAt(x, y, kWidth, height()); }
  CellBox hitBox() const;
};

// --- Actors ------------------------------------------------------------------

struct Enemy
{
  EnemyKind kind;
  int def = 0; // index into the enemy table (data/enemies.hpp)
  int x = 0, y = 0; // bottom-left cell
  int prevX = 0, prevY = 0;
  int w = 3, h = 3;
  int hp = 1;
  int dir = -1;
  int timer = 0;
  int flash = 0; // 60 Hz
  int dive = 0;
  int tell = 0;     // frames left of the attack telegraph
  int lastDive = -1; // bar of the last beat dive
  int id = 0;
  // Clingers: the side their surface is on (-1 left wall, 1 right wall,
  // 2 ceiling, 0 floor). Riders: their rail. Snipers: where the laser aims.
  int attach = 0;
  int railX0 = 0, railX1 = 0;
  int aimX = 0, aimY = 0;
  int platform = -1; // spawned onto this platform (a spawner's cop)
  // Eased draw position in cells. Walkers step a cell every other logic
  // frame; easing turns that stop-and-go into a steady glide.
  float drawX = 0.0f, drawY = 0.0f;
  bool drawSnap = true;
  bool alive = true;
  bool active = false;
  bool carrier = false; // carrier=1: spreads the Virus
  CellBox box() const { return boxAt(x, y, w, h); }
  unsigned flags() const { return enemyDef(def).flags | (carrier ? unsigned(kEnemyCarrier) : 0u); }
};

enum class ShotKind
{
  Normal,
  Laser,
  Rocket,
  Flame,
  Enemy,
  Proto, // a prototype weapon's shot; see Projectile::proto
};

struct Projectile
{
  ShotKind kind;
  int x = 0, y = 0; // top-left cell
  int prevX = 0, prevY = 0;
  int dx = 1, dy = 0;
  int speed = 2;
  int w = 2, h = 1;
  int damage = 1;
  bool pierce = false;
  int pierceLeft = 0; // enemies it still passes through (pierce-one shots)
  int proto = -1;     // ProtoId for ShotKind::Proto
  bool strong = false; // on-the-beat and other powered-up shots look bigger
  bool carrier = false; // an infected enemy's shot: infects instead of hurting
  int range = -1;       // cells left before it fizzles, -1 unlimited
  int sx = 0, sy = 0;   // surface riders: the side the surface is on
  int ride = -1;        // surface riders: frames of riding left
  // Shots along any angle (snipers, crawler sparks): float position and a
  // unit step, `speed` steps per frame.
  bool precise = false;
  float fx = 0.0f, fy = 0.0f, vx = 0.0f, vy = 0.0f;
  bool alive = true;
  int age = 0;
  std::vector<int> hit; // enemies a piercing shot already damaged
  CellBox box() const { return {x, y, w, h}; }
};

enum class ItemKind
{
  Health,
  Merch,
  Laser,
  Rocket,
  Flame,
  RapidFire,
  Key,
  Gem,
  LetterG,
  LetterU,
  LetterN,
  Turbo,
  Virus, // a hazard, not a reward: touching it infects you
  Proto, // this level's prototype weapon (green W box)
  Duck,  // the level's rubber duck
};

// Shootable crate that releases an item, Duke Nukem II style. White boxes
// hold gadgets, blue ones health and merchandise, green ones weapons.
struct ItemBox
{
  ItemKind content;
  int x = 0, y = 0; // bottom-left, 2x2 cells
  int variant = 0;
  bool alive = true;
  int flash = 0;
  CellBox box() const { return boxAt(x, y, 2, 2); }
};

// 0 white, 1 blue, 2 green: which colour of box holds this kind of item.
int boxColor(ItemKind content);

struct Item
{
  ItemKind kind;
  int x = 0, y = 0; // bottom-left, 2x2 cells
  int prevX = 0, prevY = 0;
  int variant = 0;
  int frames = 0;
  int pickupDelay = 0;
  bool floating = false; // placed in the level: hovers instead of falling
  int vx = 0;            // sideways drift while popping out (gem caches)
  bool taken = false;
  CellBox box() const { return boxAt(x, y, 2, 2); }
};

// A rectangle of tiles that turns solid and empty (SPEC 3.1). While empty
// it draws as a ghost outline.
enum class LayerDriver
{
  Beat,   // solid on some beats of the 120 BPM music clock
  Timer,  // on/off/phase in frames
  Switch, // follows a switch
  Script, // changed by level logic
  Lit,    // always solid; only its lighting follows the beats
};

struct Layer
{
  std::string id;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks, inclusive
  Tile tile = Tile::Solid;
  LayerDriver driver = LayerDriver::Beat;
  std::array<bool, 8> beats{}; // beats 1-4 (or 1-8 over two bars)
  int bars = 1;
  int on = 0, off = 0, phase = 0;
  std::string switchId;
  int switchState = 1;
  bool scriptSolid = true;
  bool solid = true;
  bool buzzing = false; // about to go dark: flicker and buzz
  int style = 0;        // 0 neon sign, 1 plain
  Color color = 0;
};

// Things that are drawn and sometimes touched but are not enemies or items:
// decorations, the bonus entrance, the gem cache, easter eggs.
enum class PropKind
{
  Deco42,        // the number 42, hidden somewhere in every level
  Billboard,     // foreground board that fades while the player is behind it
  TextSign,      // a sign with text on it (easter eggs)
  UfoFlyby,      // a tiny UFO crossing the sky when triggered
  BonusDoor,     // B: a patch of TV static leading to the bonus level
  GemCache,      // $: five gems burst out when touched
  Reflection,    // the backdrop glass mirrors you; stand still in the box and it waves
};

struct Prop
{
  PropKind kind;
  int x = 0, y = 0, w = 2, h = 2; // cells, top-left
  std::string text;
  int timer = -1; // running animation frame, -1 idle
  int hold = 0;   // trigger counter
  bool used = false;
  CellBox box() const { return {x, y, w, h}; }
};

// Rectangles that push or pull the player (SPEC 3.4).
enum class ZoneKind
{
  Wind, // push `num/den` cells per frame along dir
};

struct Zone
{
  ZoneKind kind;
  CellBox box;
  int dx = 0, dy = 0;
  int num = 1, den = 1;
};

// Moving and weighted platforms (SPEC 3.2): one-way tops the player and
// walkers ride.
enum class PlatformMode
{
  Path,   // follows `path` (loop or ping-pong)
  Pulley, // two gondolas on one cable: the heavier side sinks
};

struct Platform
{
  std::string id;
  PlatformMode mode = PlatformMode::Path;
  int x = 0, y = 0, w = 6, h = 2; // cells, top-left; y is the top surface
  int prevX = 0, prevY = 0;
  int startY = 0; // where it rests (pulley: rehome target)
  int homeY = 0;  // pulley: the centre of its travel
  int travel = 4; // pulley: cells either way of home
  int pair = -1;  // pulley: the other gondola
  bool sideA = true;
  int slack = 8, slackLeft = 0, balance = 0;
  int rehome = 30, idle = 0;
  bool brake = false, braked = false;
  int moveTick = 0;
  int shudder = 0; // frames left of the creak telegraph
  // Path mode, in cells.
  std::vector<std::pair<int, int>> path;
  int target = 1;
  bool pingpong = false;
  int step = 1;
  int speedNum = 1, speedDen = 1;
  CellBox box() const { return {x, y, w, h}; }
};

// A trapdoor in a slab that turns into ladder once you have stood on the
// slab (level 2's spine).
struct Hatch
{
  int tx = 0, ty = 0; // block
  bool open = false;
  Tile tile = Tile::Ladder; // what it turns into (`tile==`: a one-way top)
};

// Terrain that breaks when shot (SPEC 3.7).
struct Breakable
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks, inclusive
  int hp = 1;
  int by = 0; // 0 any, 1 explosion, 2 heavy
  bool broken = false;
};

// Brings a new enemy in when its condition holds (level 2's cop door).
struct Spawner
{
  int def = -1;
  int x = 0, y = 0; // cells, bottom-left
  int platform = -1; // only while this platform is at rest and empty
  std::string onto;  // the platform's id
  int cooldown = 0;
};

struct Checkpoint
{
  int x = 0, y = 0; // bottom-left, 2x4 cells
  bool active = false;
  CellBox box() const { return boxAt(x, y, 2, 4); }
};

// --- Effects (60 Hz, world pixels) -------------------------------------------

struct Particle
{
  Vec2 pos;
  Vec2 vel;
  int life = 0;
  int maxLife = 1;
  Color color = 0;
  int size = 1;
  bool gravity = true;
  bool glow = true;
};

struct FloatingText
{
  Vec2 pos;
  std::string text;
  Color color;
  int life = 0;
};

struct Flash
{
  Vec2 pos;
  float radius;
  Color color;
  int life = 0;
  int maxLife = 1;
};

enum class WorldState
{
  Playing,
  Exiting,
  Done,
};

struct WorldStats
{
  int score = 0;
  int gems = 0;
  int gemsTotal = 0;
  int kills = 0;
  int enemiesTotal = 0;
  int merch = 0;
  int merchTotal = 0;
  int weaponsCollected = 0; // bit per Weapon
  int weaponsTotal = 0;
  std::string letters; // in pickup order
  bool tookDamage = false;
  int deaths = 0;
  int frames = 0; // 15 Hz logic frames; also the music clock
  bool protoFound = false;
  bool duck = false;
  bool camera = false; // shot the candid camera
  int protoKills = 0;   // kills with the prototype, for the Arsenal
};

// The music clock (SPEC 3.1): 120 BPM, a bar is 30 logic frames and its four
// beats are 8, 7, 8 and 7 frames long.
int beatOfFrame(int frame);      // 0..3
int beatStartFrame(int beat);    // first frame of beat 0..3 within the bar
int framesIntoBeat(int frame);   // 0.. within the current beat

struct Bonus
{
  const char* name;
  int points;
};

// Centre of a cell box in world pixels (effects positions).
inline Vec2 cellCenter(const CellBox& b)
{
  return {(float(b.x) + float(b.w) * 0.5f) * kCellSize, (float(b.y) + float(b.h) * 0.5f) * kCellSize};
}

// Owns the running level: player, enemies, projectiles, items and effects.
// Plays the role of RigelEngine's GameWorld (game_logic/game_world.cpp),
// with plain entity lists instead of entityx components.
class World
{
public:
  World(std::shared_ptr<const Level> level, int characterIndex, const Theme& theme, const Art& art);

  // One 15 Hz logic frame.
  void update(const PlayerInput& input);
  // One 60 Hz render tick: particles, shake, flashes.
  // 60 Hz effects and camera easing; alpha is the next render's alpha.
  void tickEffects(float alpha);
  // alpha: 0..1 progress between the previous and the current logic frame.
  void draw(Renderer& r, int frame, float alpha) const;

  WorldState state() const { return mState; }
  int stateFrames() const { return mStateFrames; }
  const WorldStats& stats() const { return mStats; }
  std::vector<Bonus> bonuses() const;
  const Player& player() const { return mPlayer; }
  const CharacterDef& character() const { return *mCharacter; }
  const std::vector<Enemy>& enemies() const { return mEnemies; }
  const std::vector<ItemBox>& boxes() const { return mBoxes; }
  const std::vector<Item>& items() const { return mItems; }
  const std::vector<Projectile>& projectiles() const { return mProjectiles; }
  const Level& level() const { return *mLevel; }
  const CollisionMap& map() const { return mMap; }
  const Camera& camera() const { return mCamera; }

  // Sounds triggered since the last call; the frontend plays them.
  std::vector<Sfx> takeSounds();

  // Savegames (world_save.cpp). Saving is refused while dying or leaving.
  bool canSave() const;
  SaveGame snapshot() const;
  // Puts a fresh world for the same level into the saved state. Returns
  // false (and changes nothing) if the save does not fit this level.
  bool restore(const SaveGame& save);
  // Swaps in another runner on the spot, keeping position, weapon, ammo and
  // items; health keeps the same share of the new runner's hearts. Returns
  // false while dying or leaving through the exit.
  bool switchCharacter(int characterIndex);
  int characterIndex() const { return mCharacterIndex; }
  // Shows a line of text under the HUD, like the pickup messages.
  void notify(const std::string& text) { showMessage(text); }

  // The player stands in the bonus entrance and pressed up: the frontend
  // takes over (sting, bonus level, and back). Cleared by the frontend.
  bool bonusRequested() const { return mBonusRequested; }
  void clearBonusRequest() { mBonusRequested = false; }
  // Bonus levels: the timer and goal from the header.
  int bonusFramesLeft() const { return mBonusFramesLeft; }
  bool bonusFailed() const { return mBonusFailed; }
  // Adds what was collected in a bonus level to this level's run.
  void addBonusReward(int score, int gems, bool star);
  bool bonusStar() const { return mBonusStar; }
  const std::vector<Layer>& layers() const { return mLayers; }
  const std::vector<Prop>& props() const { return mProps; }
  const std::vector<Platform>& platforms() const { return mPlatforms; }
  const std::vector<Hatch>& hatches() const { return mHatches; }
  int clock() const { return mStats.frames; }
  // The planner bot simulates copies of the world: no effects or sounds.
  std::unique_ptr<World> cloneForSim() const;

private:
  // player.cpp: port of RigelEngine's game_logic/player.cpp
  void updatePlayer(const PlayerInput& input);
  void updatePlayerMovement(int mvX, int mvY, const Button& jump, const Button& fire);
  void updateLadderAttachment(int mvX, int mvY);
  void updateJumpMovement(int mvX, bool jumpPressed);
  void updateHorizontalMovementInAir(int mvX);
  void updateShooting(const Button& fire);
  void updateDeathAnimation();
  MoveResult moveVerticallyInAir(int amount, bool& attached);
  bool tryAttachToClimbable();
  bool canFire() const;
  void fireShot();
  void jump();
  void jumpFromLadder(int mvX);
  void startFalling();
  void startFallingDelayed();
  void landOnGround(bool needRecoveryFrame);
  void switchOrientation();
  void switchOrientationWithPositionChange();
  void setVisual(PlayerVisual v) { mPlayer.visual = v; }
  void hurtPlayer(int amount);
  void startTurbo();
  void infect();
  const std::array<int, 8>& jumpArc() const;
  int horizontalSteps() const; // cells per frame: 2 in turbo, 0 or 1 when infected
  void killPlayer();
  void respawnPlayer();
  void updatePlayerInteractions();

  // world_level.cpp: level entities, music clock, layers, props
  void setupEntities();
  void spawnEnemy(int def, int x, int y);
  void updateLayers(bool force);
  bool layerWantsSolid(const Layer& l, int frame) const;
  void applyLayer(Layer& l, bool solid);
  void updateProps(const PlayerInput& input);
  void updateBonusRules(const PlayerInput& input);
  void drawLayers(Renderer& r, float camX, float camY, int frame) const;
  void drawProps(Renderer& r, float camX, float camY, int frame, bool foreground) const;
  void drawBeatHud(Renderer& r, int frame) const;

  // world_platforms.cpp: moving platforms, hatches, breakables, spawners
  void setupPlatform(const EntityDef& e);
  void linkPlatforms();
  void updatePlatforms();
  bool standsOn(const CellBox& feet, const Platform& pl) const;
  int platformWeight(const Platform& pl, bool& player) const;
  bool movePlatform(Platform& pl, int dx, int dy);
  void syncPlatformCollision();
  void updateHatches();
  bool hitBreakable(const CellBox& shot, int damage, int kind);
  void updateSpawners();
  void drawPlatforms(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void updateFreeFall(int mvX, int mvY);

  // world_actors.cpp: the campaign's enemy behaviours
  void placeClinger(Enemy& e);
  void updateCrawler(Enemy& e, const EnemyDef& def);
  void updateRider(Enemy& e, const EnemyDef& def);
  void updateSniper(Enemy& e, const EnemyDef& def);
  bool lineOfFire(int x0, int y0, int x1, int y1, int& hitX, int& hitY) const;
  void shootAt(Enemy& e, int fromX, int fromY, int speed, int range);
  void touchPlayer(const Enemy& e);

  // world_proto.cpp: prototype weapons
  void fireProto(int ox, int oy, int dx, int dy);
  bool stepSurfaceShot(Projectile& pr);
  void takeProto(const Vec2& at);
  void updateProtoShooting(const Button& fire);
  bool onTheBeat() const;

  // world.cpp
  void updateEnemies();
  void updateProjectiles();
  void updateItems();
  void spawnProjectile(ShotKind kind, int x, int y, int dx, int dy);
  void damageEnemy(Enemy& e, int damage);
  void killEnemy(Enemy& e);
  void destroyBox(ItemBox& b);
  void collectItem(Item& it);
  void explodeAt(int cx, int cy, int radius, int damage);
  bool isOnScreen(const CellBox& b, int margin) const;
  void addScore(int points, Vec2 at);
  void showMessage(const std::string& text);
  void playSound(Sfx s)
  {
    if (!mSimulation)
      mSounds.push_back(s);
  }
  Camera::Target cameraTarget() const;

  // effects
  void burst(Vec2 at, Color a, Color b, int count, float speed, bool glow = true);
  void flashAt(Vec2 at, float radius, Color c, int life);

  // world_draw.cpp
  void drawTiles(Renderer& r, float camX, float camY, int frame) const;
  void drawPlayer(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawHud(Renderer& r, int frame) const;

  std::shared_ptr<const Level> mLevel; // immutable; copies of the world share it
  CollisionMap mMap;
  const CharacterDef* mCharacter;
  int mCharacterIndex;
  const Theme& mTheme;
  const Art& mArt;

  Player mPlayer;
  int mRespawnX = 0, mRespawnY = 0;
  int mSafeX = 0, mSafeY = 0; // last spot standing on safe ground: saves put you here
  std::vector<Enemy> mEnemies;
  std::vector<Projectile> mProjectiles;
  std::vector<ItemBox> mBoxes;
  std::vector<Item> mItems;
  std::vector<Checkpoint> mCheckpoints;
  std::vector<Layer> mLayers;
  std::vector<std::uint8_t> mLayerMask; // per block: 1 if a layer draws it
  std::vector<Prop> mProps;
  std::vector<Zone> mZones;
  std::vector<Platform> mPlatforms;
  std::vector<std::string> mPlatformPairs; // setup only: each platform's pair=
  std::vector<Hatch> mHatches;
  std::vector<Breakable> mBreakables;
  std::vector<Spawner> mSpawners;
  std::size_t mLevelEnemyCount = 0; // the level's own; spawned ones follow
  bool mFreeFall = false; // bonus rule: no ground until the net
  std::vector<CellBox> mRopes; // free fall: window-cleaner ropes that bounce you
  int mStall = 0;         // free fall: frames the fall is stalled after a bounce
  int mLevelProto = -1; // the header's weapon=
  bool mHasBeat = false; // level uses the music clock: show the equalizer
  bool mBonusRequested = false;
  bool mBonusLevel = false;
  int mBonusFramesLeft = 0;
  bool mBonusFailed = false;
  bool mBonusStar = false;
  bool mAirJump = false; // bonus rule: jump again in mid-air
  bool mSimulation = false;
  std::vector<Particle> mParticles;
  std::vector<FloatingText> mTexts;
  std::vector<Flash> mFlashes;
  std::vector<Sfx> mSounds;
  Camera mCamera{kViewCellsW, kViewCellsH};
  int mManualScroll = 0;
  int mLookFrames = 0;
  int mBaseCamY = 0;
  Rng mRng{42u};      // effects only
  Rng mLogicRng{7u};  // game logic: stays in step when the bot simulates without effects
  WorldState mState = WorldState::Playing;
  int mStateFrames = 0;
  WorldStats mStats;
  std::string mMessage;
  int mMessageTicks = 0;
  int mFieldFlash = 0;
  int mTickCount = 0;
  std::array<Vec2, 5> mTrail{}; // recent player draw positions, for Turbo afterimages
};

} // namespace gr
