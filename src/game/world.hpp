#pragma once

#include "assets/art.hpp"
#include "base/math.hpp"
#include "data/characters.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "engine/camera.hpp"
#include "game/collision.hpp"
#include "game/input.hpp"
#include "game/savegame.hpp"
#include "game/sound_ids.hpp"
#include "render/renderer.hpp"

#include <string>
#include <vector>

namespace gr
{

// The game logic runs at Duke Nukem II's 15 frames per second; rendering
// runs at 60 and interpolates in between, like RigelEngine's motion
// smoothing.
constexpr int kTicksPerLogicFrame = 4;
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
  bool rapidFiredLastFrame = false;
  bool oddFrame = false;
  bool hidden = false;

  int hp = 9;
  int maxHp = 9;
  int mercy = 0;
  Weapon weapon = Weapon::Normal;
  int ammo = 0;
  int rapidFire = 0; // frames left
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

enum class EnemyKind
{
  Walker,
  Flyer,
  Turret,
};

struct Enemy
{
  EnemyKind kind;
  int x = 0, y = 0; // bottom-left cell
  int prevX = 0, prevY = 0;
  int w = 3, h = 3;
  int hp = 1;
  int dir = -1;
  int timer = 0;
  int flash = 0; // 60 Hz
  int dive = 0;
  int id = 0;
  // Eased draw position in cells. Walkers step a cell every other logic
  // frame; easing turns that stop-and-go into a steady glide.
  float drawX = 0.0f, drawY = 0.0f;
  bool drawSnap = true;
  bool alive = true;
  bool active = false;
  CellBox box() const { return boxAt(x, y, w, h); }
};

enum class ShotKind
{
  Normal,
  Laser,
  Rocket,
  Flame,
  Enemy,
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
  bool taken = false;
  CellBox box() const { return boxAt(x, y, 2, 2); }
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
  int frames = 0; // 15 Hz logic frames
};

struct Bonus
{
  const char* name;
  int points;
};

// Owns the running level: player, enemies, projectiles, items and effects.
// Plays the role of RigelEngine's GameWorld (game_logic/game_world.cpp),
// with plain entity lists instead of entityx components.
class World
{
public:
  World(const Level& level, int characterIndex, const Theme& theme, const Art& art);

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
  const Level& level() const { return mLevel; }
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
  // Shows a line of text under the HUD, like the pickup messages.
  void notify(const std::string& text) { showMessage(text); }

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
  void killPlayer();
  void respawnPlayer();
  void updatePlayerInteractions();

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
  void playSound(Sfx s) { mSounds.push_back(s); }
  Camera::Target cameraTarget() const;

  // effects
  void burst(Vec2 at, Color a, Color b, int count, float speed, bool glow = true);
  void flashAt(Vec2 at, float radius, Color c, int life);

  // world_draw.cpp
  void drawTiles(Renderer& r, float camX, float camY, int frame) const;
  void drawPlayer(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawHud(Renderer& r, int frame) const;

  Level mLevel;
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
  std::vector<Particle> mParticles;
  std::vector<FloatingText> mTexts;
  std::vector<Flash> mFlashes;
  std::vector<Sfx> mSounds;
  Camera mCamera{kViewCellsW, kViewCellsH};
  int mManualScroll = 0;
  int mLookFrames = 0;
  int mBaseCamY = 0;
  Rng mRng{42u};
  WorldState mState = WorldState::Playing;
  int mStateFrames = 0;
  WorldStats mStats;
  std::string mMessage;
  int mMessageTicks = 0;
  int mFieldFlash = 0;
};

} // namespace gr
