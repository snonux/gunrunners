#pragma once

#include "assets/art.hpp"
#include "render/renderer.hpp"
#include "base/math.hpp"
#include "data/characters.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "engine/camera.hpp"
#include "game/input.hpp"

#include <vector>

namespace gr
{

constexpr int kViewW = 320;
constexpr int kViewH = 180;

struct Player
{
  Vec2 pos; // top-left of the 16x24 sprite
  Vec2 vel;
  bool onGround = false;
  int facing = 1;
  int hp = 3;
  int invulnerable = 0;
  int fireCooldown = 0;
  int coyote = 0;
  int jumpBuffer = 0;
  int knockback = 0;
  int animTicks = 0;
  int muzzleFlash = 0;
  bool jumpHeld = false;

  // Collision box inside the sprite.
  static constexpr float kBoxX = 4.0f;
  static constexpr float kBoxY = 3.0f;
  static constexpr float kBoxW = 8.0f;
  static constexpr float kBoxH = 21.0f;
  Rect box() const { return {pos.x + kBoxX, pos.y + kBoxY, kBoxW, kBoxH}; }
};

enum class EnemyKind
{
  Walker,
  Flyer,
  Turret,
};

struct Enemy
{
  EnemyKind kind;
  Vec2 pos;
  Vec2 vel;
  Vec2 home;
  int hp = 1;
  int dir = -1;
  int timer = 0;
  int flash = 0;
  bool alive = true;
  bool active = false;
  bool onGround = false;
  Rect box() const;
};

struct Bullet
{
  Vec2 pos;
  Vec2 vel;
  int life = 60;
  bool fromEnemy = false;
  int sprite = 0;
  bool alive = true;
  Rect box() const;
};

enum class PickupKind
{
  Gem,
  Health,
};

struct Pickup
{
  PickupKind kind;
  Vec2 pos;
  int variant = 0;
  bool taken = false;
};

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

enum class WorldState
{
  Playing,
  Cleared,
  Dead,
};

struct WorldStats
{
  int score = 0;
  int gems = 0;
  int gemsTotal = 0;
  int kills = 0;
  int enemiesTotal = 0;
  int ticks = 0;
};

// Owns the running level: player, enemies, projectiles, pickups and effects.
// Plays the role of RigelEngine's GameWorld (game_logic/game_world.cpp),
// with plain entity lists instead of entityx components.
class World
{
public:
  World(const Level& level, int characterIndex, const Theme& theme, const Art& art);

  void update(const Input& input);
  void draw(Renderer& r, int frame) const;

  WorldState state() const { return mState; }
  int stateTicks() const { return mStateTicks; }
  const WorldStats& stats() const { return mStats; }
  const Player& player() const { return mPlayer; }
  const CharacterDef& character() const { return *mCharacter; }
  const std::vector<Enemy>& enemies() const { return mEnemies; }
  const Level& level() const { return mLevel; }
  const Camera& camera() const { return mCamera; }

private:
  void updatePlayer(const Input& input);
  void firePlayerWeapon();
  void hurtPlayer(int amount, float fromX);
  void updateEnemies();
  void updateBullets();
  void updatePickups();
  void updateParticles();
  void killEnemy(Enemy& e);
  void damageCrate(int tx, int ty);
  void explode(Vec2 at, Color a, Color b, int count, float speed, bool glow = true);
  void moveBody(Vec2& pos, Vec2& vel, const Rect& boxOffset, bool& onGround);
  bool overlapsTile(const Rect& r, Tile t) const;
  bool isActive(const Rect& r) const;

  Level mLevel;
  const CharacterDef* mCharacter;
  int mCharacterIndex;
  const Theme& mTheme;
  const Art& mArt;

  Player mPlayer;
  std::vector<Enemy> mEnemies;
  std::vector<Bullet> mBullets;
  std::vector<Pickup> mPickups;
  std::vector<Particle> mParticles;
  Camera mCamera{kViewW, kViewH};
  float mBaseCamY = 0.0f;
  Rng mRng{42u};
  WorldState mState = WorldState::Playing;
  int mStateTicks = 0;
  WorldStats mStats;
};

} // namespace gr
