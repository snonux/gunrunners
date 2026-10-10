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
#include "game/space.hpp"
#include "game/station.hpp"
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
  Swing,       // holding a swing vine (level 8, world_jungle.cpp)
  Cling,       // stuck to a goo wall, sliding down (level 44, world_space.cpp)
  Swim,        // in deep water without a submarine (world_sea.cpp)
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
  Clinging, // on a goo wall, back to it
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
  int cart = -1;   // riding this mine cart (level 11)
  bool cartDuck = false;
  int vine = -1;   // Swing: the vine held
  int vineAt = 0;  // Swing: cells from the anchor to the hands
  int fling = 0;   // cells a frame sideways until landing (let go of a vine)
  bool vineArc = false; // this jump is a vine launch (its own arc)
  int wall = 0;         // Cling: the goo wall's side (-1 left, 1 right)
  int kick = 0;         // cells still to be pushed off a goo wall (sign: direction)
  bool kickArc = false; // this jump is a kick off a goo wall (always the full arc)
  int tube = -1;        // inside this Gullet Tube (level 45)
  int tubeBranch = 0;
  int tubeS = 0;        // cells along it
  int vehicle = -1; // driving this vehicle (world_vehicle.cpp)

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
  int variant = 0;      // a look from the level (the gator's sunglasses)
  int stun = 0;         // frames left dazed (out of a popped bubble)
  bool trapped = false; // held in a Bubble Gun bubble
  int tangle = 0;       // frames left lying in the Snare Bolas' cords
  int tangleRoll = 0;   // cells left to roll first (sign: direction)
  bool hidden = false;  // out of sight and out of reach (the camera in a dark mirror)
  int ox = 0, oy = 0;   // where a leap started (level 12's toads)
  int cool = 0;         // frames until it can attack again
  CellBox box() const { return boxAt(x, y, w, h); }
  unsigned flags() const { return enemyDef(def).flags | (carrier ? unsigned(kEnemyCarrier) : 0u); }
};

// Lock-On Rockets' targets: an enemy's index, or a part of Black Halo
// (kBossTarget - part).
constexpr int kNoTarget = -1000;
constexpr std::size_t kChopperSave = 14; // fixed part of SaveGame::chopper (world_save.cpp)
constexpr int kBossTarget = -10;

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
  float gy = 0.0f;   // precise shots: added to vy every frame (lobbed glowsticks)
  bool flare = false; // sticks where it hits and lights up the dark
  bool lob = false;  // leaves a puddle where it lands
  int target = kNoTarget; // Lock-On Rockets: steers for this (see World::targetBox)
  int radius = 0;    // explodes with this radius (cells) where it hits (the gunship's rockets)
  int footRow = -1;  // Serpent Spear: the row its foothold's top takes (under the thrower's feet)
  bool spear = false; // a Spear Runner's spear (Fan Darts break it)
  bool vehicle = false; // fired by a vehicle: breaks `by=vehicle` walls
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
  int restY = -1; // floats up off this row on rising sludge (level 5)
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
  int heldBy = -1; // carried off by this enemy (a Looter)
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
  Decks,         // a DJ deck: shoot both on the beat for a change of music
  DanceFloor,    // lit tiles; crouch on it for 16 seconds to breakdance
  Graffiti,      // glow-in-the-dark credits that show once a flare lights them
  Cat,           // a pair of eyes in the dark: shoot near it and it meows off
  Interior,      // a back wall behind an underground room (no sky in a garage)
  DevNull,       // a pipe mouth that swallows whatever floats into it, with a bloop
  Waterfall,     // sludge pouring out of the outflow pipe
  Log,           // Duck Rapids: a log floating on the river (over a solid block)
  LowPipe,       // Duck Rapids: a pipe hanging from the roof (over solid blocks)
  CarInterior,   // Maglev Express: a car's back wall with its windows
  Seat,          // Maglev Express: a row of seats (over a solid block)
  Crate,         // Maglev Express: luggage (over solid blocks)
  Sleeper,       // Maglev Express: the passenger who sleeps through everything
  HiScore,       // Maglev Express: a billboard through the window with your level 2 score
  Girder,        // Chopper Down: a steel I-beam drawn over its blocks
  Sheet,         // Chopper Down: hanging plastic sheeting (cover from the searchlight)
  Awning,        // Chopper Down: a canvas awning over a one-way row
  Office,        // Chopper Down: the site office's back wall
  Lattice,       // Chopper Down: the crane's counter-jib lattice
  SpareShip,     // Chopper Down: the parked spare gunship (the bonus door is its hatch)
  Radio,         // Chopper Down: the cab radio; stand in the cab and it plays the theme as elevator music
  Mixer,         // Chopper Down: the cement mixer's drum (drawn over its breakable)
  Trunk,         // Canopy Road: a tree trunk behind the canopy (over its rect)
  Gate,          // Canopy Road: the temple gate around the exit
  Nest,          // Canopy Road: the nest on the tallest tree
  Skeleton,      // Hall of Traps: an explorer who didn't make it (press up beside him)
  Glyph,         // Hall of Traps: a carved glyph over a wing's door (its text)
  Torch,         // Hall of Traps: a wall torch
  Idol,          // Trapmaster: the idol the hunters walk to
  Serpent,       // Lava Heart: a carved serpent head on the wall, breathing smoke
  Marshmallow,   // Lava Heart: press up to take it on a stick, roast it at the hearth
};

struct Prop
{
  PropKind kind;
  int x = 0, y = 0, w = 2, h = 2; // cells, top-left
  std::string text;
  int timer = -1; // running animation frame, -1 idle
  int hold = 0;   // trigger counter
  bool used = false;
  int ride = -1;        // carried by this platform (the passer's bonus patch)
  int rideDx = 0, rideDy = 0; // cells from the platform's top-left
  bool dormant = false; // not there yet (the patch before the camera is shot)
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
  Sink,   // level 12: a stone that sinks into the lava under a load (a ferry with a path)
  Rise,   // level 12: a bridge segment that comes up out of the lava when triggered
};

struct Platform
{
  std::string id;
  bool frozen = false; // Golden Touch (level 14's bonus): turned gold where it was
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
  int powered = -1; // a lift: only moves while this breaker is on
  bool once = false;   // mode=once: hidden until a script starts it, gone at the end
  int latch = -1;      // a hook: held by this latch until it is shot, then runs once you stand on it
  std::string latchId;
  bool running = false;
  bool hidden = false;
  // wait=rider (level 11's mine elevator): parks at each end until someone
  // stands on it (a bell, then it goes) or calls it from the far landing.
  bool waitRider = false;
  bool parked = false;
  int bell = 0;
  // Level 12 (world_lava.cpp). Sink: rests at homeY, sinks under a load
  // down to floorY (a block under the lava), rises back when free; `acc`
  // counts fifteenths of a cell. A ferry (dock=1) crosses its path once
  // boarded (ferry: 1 docked, 2 rumbling, 3 crossing). Rise: starts under
  // the lava at startY and comes up to homeY once triggered.
  int acc = 0;
  int floorY = 0;
  int ferry = 0;
  int ferryWait = 0;
  int riseX0 = 0, riseX1 = -1; // cells: standing here triggers a Rise segment
  int riseDelay = 0;
  int riseAt = -1;             // clock it was triggered (bubbles first), -1 waiting
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
  int by = 0; // 0 any, 1 explosion, 2 heavy, 3 sound (the Bass Cannon)
  int look = 0; // 0 cracked glass, 1 mirror ball, 2 speaker cabinet
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

// Level 3's subwoofers: a speaker cone in the floor that bumps you on every
// beat and launches you on its drop hit (SPEC 03).
struct Pad
{
  std::string id;
  int x = 0, y = 0, w = 6; // cells; y is the floor's top row
  int bump = 2;            // cells, every beat
  int launchX10 = 25;      // launch height in tenths of the rider's jump
  int fire = 1;            // drop hit (1-3) that launches; 0 only bumps
};

// A ceiling fan of laser beams that sweeps during the chorus.
struct LaserFan
{
  int x = 0, y = 0;            // cells, the hub
  int a0 = 200, a1 = 340;      // degrees, maths convention (270 = down)
  int len = 20;                // cells
};

// A glowstick's puddle: hurts on touch until it fades.
struct Puddle
{
  int x = 0, y = 0, w = 6; // cells; y is the row it lies on
  int life = 30;
};

// The Bass Cannon's wall of sound.
struct Cone
{
  int x = 0, y = 0, dir = 1; // origin cell (centre of the mouth), facing
  int life = 3;
  int damage = 2;
  std::vector<int> hit;
};

// Level 4's power cuts (SPEC 04, world_dark.cpp). A sector is dark until
// its breaker is thrown; then it relights as a wave from the breaker.
struct DarkSector
{
  std::string id;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks, inclusive
  int breaker = -1;                   // -1: only flares ever light it
  bool contains(int cx, int cy) const
  {
    return cx >= x0 * 2 && cx < (x1 + 1) * 2 && cy >= y0 * 2 && cy < (y1 + 1) * 2;
  }
};

struct LightSource
{
  int x = 0, y = 0; // cells, the centre
  int r = 12;       // cells
};

struct Breaker
{
  std::string id;
  int x = 0, y = 0; // cells, bottom-left of its 2x4 box
  int sector = -1;
  bool on = false;
  int thrownAt = -100000; // clock when it was last thrown (the relight wave)
  int throwing = 0;       // frames left of the lever animation
  bool leechKilled = false; // its cable's Leech is dead: no more come
  int leechIn = -1;         // frames until a new Leech starts, -1 none
  CellBox box() const { return boxAt(x, y, 2, 4); }
};

// A roller shutter (or a floor hatch) that opens while its breaker is on.
struct Door
{
  std::string id;
  int x0 = 0, y0 = 0, w = 1, h = 4; // blocks
  int breaker = -1;
  int opentime = 15;
  int open = 0; // frames of opening done
  bool solid = true;
};

struct Flare
{
  float x = 0.0f, y = 0.0f; // cells
  int life = 120;
  int enemy = -1;           // stuck to this enemy (index), else to a wall
  float ox = 0.0f, oy = 0.0f; // offset from the enemy's bottom-left
  int burnLeft = 3;
};

// A power cable a Grid Leech crawls along to its breaker.
struct Cable
{
  std::string id;
  std::vector<std::pair<int, int>> path; // cells, from the nest to the breaker
  int breaker = -1;
};

// Echo Room: a sonar ring from a shot.
struct Ping
{
  float x = 0.0f, y = 0.0f; // cells
  int age = 0;
};

// Level 5's sludge (SPEC 3.3 and 05, world_sludge.cpp). The surface is the
// top cell row of the sludge: tides move it between low and high on one
// 300-frame clock, and a Valve Keeper's flood holds it high for a while.
struct Fluid
{
  std::string id;
  bool tide = false;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // cells, inclusive: where sludge can be
  int low = 0, high = 0;              // cells: the surface at low and at high tide
  int surface = 0;                    // cells: the surface now
  int floodAt = -100000;              // clock when its valve was turned
  int current = 0;                    // drift east, quarter cells per frame
  bool covers(int cx) const { return cx >= x0 && cx <= x1; }
  bool wet(int cx, int cy) const { return covers(cx) && cy >= surface && cy <= y1; }
};

struct Valve
{
  std::string id;
  int x = 0, y = 0; // cells, bottom-left of its 4x4 wheel
  int fluid = -1;
  bool locked = false; // padlocked, or its Keeper is dead
  int hold = 0;        // frames up has been held beside it
  CellBox box() const { return boxAt(x, y, 4, 4); }
};

// Spits a line of Pipe Rats when a runner comes close.
struct RatPipe
{
  int x = 0, y = 0, dir = -1; // cells: the mouth's bottom-left
  int count = 5;
  int rattle = 0; // frames left of the tell
  int left = 0;   // rats still to come in this burst
  int next = 0;   // frames to the next rat
  int idle = 0;   // frames off screen since the burst
  bool armed = true;
};

// A Bubble Gun bubble with an enemy in it, the camera's bubble, or the raft.
// Floats to the sludge surface and rides it; a one-way top to stand on.
struct Bubble
{
  int x = 0, y = 0, w = 4, h = 4; // cells, top-left
  int prevX = 0, prevY = 0;
  int enemy = -1; // trapped enemy index
  int life = 150; // -1: never pops on its own
  int stood = 0;  // frames a runner has stood on it
  int hp = 0;     // > 0: pops when shot (the camera's)
  int drift = 0;  // quarter cells of current
  bool raft = false;
  CellBox box() const { return {x, y, w, h}; }
};

// Level 6's gantries (SPEC 06, world_maglev.cpp): a hazard band that sweeps
// over the train from the front, at `speed` cells a frame.
enum class GantryKind
{
  Low,   // rows 7-9: crouch
  Tall,  // rows 10-11: jump
  Tall4, // rows 8-11: Nova jumps it, the others take a hatch
  Mouth, // rows 0-11: the tunnel mouth, be inside a car
  Ring,  // rows 0-11: a tunnel ring
};

struct Gantry
{
  std::string id;
  GantryKind kind = GantryKind::Low;
  int trigger = 0; // cells: the runner first reaching this x sets it off
  int speed = 3;
  int warn = 30;
  bool fired = false;
  int x = -1;     // cells: the band's left edge while it sweeps, -1 idle
  int prevX = -1;
  int bandTop() const { return kind == GantryKind::Low ? 14 : kind == GantryKind::Tall ? 20 : kind == GantryKind::Tall4 ? 16 : 0; }
  int bandBottom() const { return kind == GantryKind::Low ? 19 : 23; }
};

// A Rail Drone's caltrop: falls onto a roof, slides back with the wind
// (green ones stick and infect).
struct Caltrop
{
  int x = 0, y = 0; // cells, top-left of its 2x1 box
  int prevX = 0, prevY = 0;
  int life = 150;
  int slide = 0;     // quarter cells of wind
  bool green = false;
  bool landed = false;
  CellBox box() const { return {x, y, 2, 1}; }
};

// A Track Hopper's landing: a ripple along the roof, 1 cell high.
struct Shockwave
{
  int x = 0, y = 0; // cells: its front, and the row it runs along (feet row)
  int dir = 1;
  int left = 12;    // cells still to run
};

// The Arc Caster's lightning, for drawing (logic frames).
struct ArcBolt
{
  float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f; // cells
  int life = 3;
  int seed = 0;
};

// Light Trail: a block of neon under the runner's feet that fades.
struct TrailBlock
{
  int x = 0, y = 0; // cells: left end, the row you stand on
  int life = 45;
};

// Level 7's hunter (SPEC 07, world_chopper.cpp): the gunship in the
// backdrop. Its searchlight spot drifts toward the runner; a spot on an
// exposed runner for `lock` frames starts the beep, and `salvo` frames later
// it fires `rockets` rockets at where it last saw them.
struct Hunter
{
  bool on = false;
  int zoneX0 = 0, zoneX1 = 0; // cells: where it hunts
  int r = 6;                  // cells: the spot's radius
  int lock = 15, salvo = 22, rockets = 3, damage = 1, cooldown = 60, speed = 1;
  float sx = 0.0f, sy = 0.0f;  // cells: the spot's centre
  float prevSx = 0.0f, prevSy = 0.0f;
  int locking = 0;  // frames the spot has held an exposed runner
  int beep = 0;     // frames left before the salvo
  int left = 0;     // rockets still to fire in this salvo
  int next = 0;     // frames to the next rocket
  int cool = 0;     // frames of cooldown left
  int seenX = 0, seenY = 0; // cells: the runner's centre when last seen
  int demoTrigger = -1, demoX = 0, demoY = 0; // the yard's demonstration salvo
  bool demoDone = true;
  bool demo = false;   // this salvo is the demonstration
  bool random = false; // Black Halo's light is out: slow random salvos
};

// A gunship rocket on its way down: lands on (x, y) in t frames.
struct Strike
{
  int x = 0, y = 0; // cells: where it lands
  int t = 8;
  int r = 4;        // cells: blast radius
  int damage = 1;
  float fromX = 0.0f, fromY = 0.0f; // cells: where it was fired from (drawing)
};

// Where the gunship drops Rappel Troopers.
struct RappelZone
{
  CellBox zone;
  int max = 2, period = 150;
  int next = 0; // clock of the next drop
};

// A latch that holds a hook until it is shot.
struct Latch
{
  std::string id;
  int x = 0, y = 0; // cells, top-left of its 2x2 box
  bool open = false;
  CellBox box() const { return {x, y, 2, 2}; }
};

// Black Halo (SPEC 07): three phases, each with its own weak spots.
enum class BossPhase
{
  Waiting, // the runner has not reached the arena yet
  Strafe,  // gun pods, chaingun sweeps
  Drop,    // belly hatch, trooper pods, searchlight salvos
  Ram,     // tail rotor, rams along the jib
  Falling, // spinning into the bay
  Done,
};

enum class BossPart
{
  NosePod,
  TailPod,
  Hatch,
  Light,
  Rotor,
  Count,
};

struct Boss
{
  bool on = false;
  CellBox arena{0, 0, 0, 0}; // cells
  int deckY = 19;            // cells: the row the runner's feet are on
  int exitX = 0, exitY = 0;  // blocks: where the exit drops to
  BossPhase phase = BossPhase::Waiting;
  int x = 0, y = 0;          // cells: the body's top-left (16 x 6)
  int prevX = 0, prevY = 0;
  int facing = -1;           // -1: nose to the left
  int t = 0;                 // frames into the current cycle
  std::array<int, 5> hp{14, 14, 30, 8, 24};
  int open = 0;              // frames the current weak spot stays open
  int sweepX = -1;           // cells: the chaingun's front while it sweeps
  int prevSweepX = -1;
  int passDamage = 0;        // rotor damage taken this pass
  int ram = 0;               // 0 hovering, 1 lining up, 2 ramming, 3 turning over the cab
  int podX = -1, podY = 0;   // cells: a trooper pod falling (its shadow shows first)
  int podT = 0;
  int flash = 0;
  int fallT = 0;             // frames of the fall into the bay
  int exitT = -1;            // frames of the exit dropping from the cab, -1 not yet
  static constexpr int kW = 16, kH = 6;
  CellBox body() const { return {x, y, kW, kH}; }
  int total() const;
};

// Level 8's jungle (SPEC 08, world_jungle.cpp).
// A swing vine: anchored at (ax, ay), hanging len cells, swinging as a
// pendulum amp degrees either way of straight down (0 is the right end of
// the swing). Pumping on it raises the swing, letting it be lowers it back
// to its resting amp.
struct Vine
{
  std::string id;
  int ax = 0, ay = 0;  // cells: the anchor
  int len = 14;        // cells
  int rest = 20;       // degrees: the resting swing
  int amp = 20;        // degrees now
  int t = 0;           // frames into the period
  int period = 26;
  int pump = 0;        // frames of this half-swing the runner pushed along it
  int halves = 0;      // half-swings held without letting go (the yell egg)
  bool yelled = false;
  int cool = 0;        // frames before the runner can grab it again
  float angle() const; // degrees, + to the right
  int swingDir() const { return t < period / 2 ? -1 : 1; } // where it is heading
  // Where the point `at` cells down the vine is now (cells).
  void point(int at, float& x, float& y) const;
};

// A rope bridge: one-way planks at block row y. It snaps under a heavy
// weight, a while after the last runner left it, or at a Bridge Cutter's
// third chop.
struct Bridge
{
  std::string id;
  int x0 = 0, x1 = 0, y = 0; // blocks
  int snap = 0;   // frames of weight 2+ to snap (0: never)
  int after = 0;  // frames after the last runner steps off (0: never)
  int cut = 0;    // chops to fall (0: can't be cut)
  int heavy = 0;  // frames of weight 2+ so far
  int left = -1;  // frames left to snap after the runner stepped off, -1 not counting
  bool stood = false;
  int chops = 0;
  int creak = 0;  // frames left of the creak before it drops
  int drop = 0;   // frames of the drop shown so far
  bool down = false;
  int sag = -1;   // cells: where the weight is (drawing), -1 none
};

// Something hanging on a rope over the ravine: a log that drops across its
// notches as a bridge, or a cage of gems that breaks where it lands.
struct Load
{
  std::string id;
  bool cage = false;
  int x = 0, y = 0;   // cells: centre top, hanging
  int len = 6;        // blocks (logs)
  int landX0 = 0, landX1 = 0, landRow = 0; // blocks (logs)
  int gems = 0;       // cages
  int state = 0;      // 0 hanging, 1 falling, 2 down, 3 broken
  int fall = 0;       // frames fallen
  int fy = 0;         // cells: the cage's top while it falls
};

// A rope a load hangs from, from its tie (x0, y0) to (x1, y1). Any shot
// wears it through; the Boomerang cuts it outright.
struct JungleRope
{
  std::string id;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // cells
  int hp = 3;
  int load = -1;
  bool cut = false;
  bool hits(const CellBox& b) const;
};

// A Howler's fruit: lobbed, then rolls along what it lands on.
struct Fruit
{
  float x = 0.0f, y = 0.0f; // cells: the centre
  float vx = 0.0f, vy = 0.0f;
  float prevX = 0.0f, prevY = 0.0f;
  int dir = 1;
  int roll = -1; // frames of rolling left, -1 still in the air
  bool carrier = false;
  bool banana = false; // the Dash Howler's: harmless
  CellBox box() const { return {int(std::floor(x)) - 1, int(std::floor(y)) - 1, 2, 2}; }
};

// --- Level 9: Hall of Traps (world_temple.cpp) --------------------------------

// A pressure plate: a red glyph in the floor. Anything standing on it
// presses it (the runner, an enemy, a beetle); rolling stones don't.
struct Plate
{
  std::string id;
  int x = 0, y = 0, w = 2; // cells: left, the floor row it is set in, width
  bool down = false;
  int presses = 0;
  int pressedAt = -1000; // frame of the last press
};

enum class TrapKind
{
  Stone,  // a 2 x 2 block stone that rolls along its groove
  Blade,  // a blade in a wall slit: one sweep
  Spikes, // a spike strip: tips peek, then up
};

struct Trap
{
  std::string id;
  TrapKind kind = TrapKind::Stone;
  int plate = -1;           // fired by this plate; -1: on a cycle
  int cycle = 0, phase = 0; // cycle traps: period and offset (frames)
  int startX = -1;          // cycle traps: the cycle starts when the runner passes this cell column
  int startAt = 0;          // frame the cycle started, -1 not yet
  int tell = 10, active = 6, rearm = 30; // frames
  CellBox box{0, 0, 0, 0};  // blades and spikes: the cells they cover
  int x0 = 0, x1 = 0, row = 0, dir = -1; // stones: the groove (cells), the stone's bottom row, roll direction
  int state = 0;            // 0 armed, 1 telegraph, 2 firing, 3 re-arming (a stone in its catch slot)
  int t = 0;                // frames into the state
  int sx = 0;               // stones: the stone's left cell
  std::vector<int> hit;     // enemies hurt by this firing
  bool hitRunner = false;
  CellBox stoneBox() const { return {sx, row - 3, 4, 4}; }
  CellBox reach() const { return kind == TrapKind::Stone ? stoneBox() : box; }
};

// A floor tile that cracks under you, falls, and comes back.
struct CollapseTile
{
  int tx = 0, ty = 0;
  int state = 0; // 0 whole, 1 cracked, 2 fallen
  int t = 0;
};

struct StoneKey
{
  std::string id;
  int x = 0, y = 0; // cells, top-left of its block
  bool taken = false;
};

// A 1 x 3 slab that sinks once the runner arrives with every stone key.
struct KeyDoor
{
  int tx = 0, ty = 0, h = 3; // blocks
  int keys = 3;
  int sink = -1; // frames of sinking, -1 shut
  bool open = false;
  int nag = 0;   // frames before it says again how many keys it wants
};

// Wall blocks a plate opens without a sound (and the bonus patch it wakes).
struct SecretDoor
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
  int plate = -1;
  int presses = 1;
  bool open = false;
  bool bonus = false; // wakes the bonus patch instead of opening a wall
};

// Trapmaster (Level 9's bonus): the traps' glyph cursor.
struct TrapCursor
{
  int at = 0;  // the plate selected
  int pan = 0; // cells the camera has panned
  bool up = false, down = false; // last frame's keys, for edges
};

// --- Level 10: Sun Mirrors (world_light.cpp) --------------------------------

// Eight directions, 45 degrees apart, counter-clockwise from east.
constexpr int kDirX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
constexpr int kDirY[8] = {0, -1, -1, -1, 0, 1, 1, 1};

// A sunbeam coming in through a roof slit or a sun pipe.
struct SunBeam
{
  std::string id;
  int x = 0, y = 0; // blocks
  int dir = 6;
};

// A statue whose head is a one-sided mirror. Shots turn it.
struct Mirror
{
  std::string id;
  int x = 0, y = 0; // blocks: the head (the pedestal is under it)
  int angle = 0;    // where its face points
  int to = 0;       // the angle it is turning to
  int turn = 0;     // frames of turning left
  bool lit = false; // a sunbeam reflects off it this frame
  // A beam travelling in direction d leaves in this direction, or -1 if it
  // meets the mirror's back.
  static int reflect(int angle, int d)
  {
    const int rel = ((d - angle) % 8 + 8) % 8;
    if (rel < 3 || rel > 5)
      return -1;
    return ((2 * angle - d + 4) % 8 + 8) % 8;
  }
};

// A slab (or floor hatch) that opens after 15 lit frames in a row.
struct SunDoor
{
  std::string id;
  int x = 0, y = 0, w = 1, h = 3; // blocks
  bool latch = true;              // stays open once opened
  bool crack = false;             // the vault's cracked disc
  int opens = -1;                 // the door it opens instead of itself (the stone sun)
  std::string opensId;
  int lit = 0;                    // lit frames in a row
  bool litNow = false;
  bool open = false;
  int hold = 0;                   // latch=0: frames it stays open after the light goes
  std::vector<Tile> under;        // the map's tiles under the slab, for when it opens
  bool covers(int bx, int by) const { return bx >= x && bx < x + w && by >= y && by < y + h; }
};

// A beam's path for drawing: corners in blocks.
struct BeamPath
{
  std::vector<std::pair<int, int>> pts;
  bool lance = false;
};

// Dome floor 2's green-rimmed mirror: stare into it and catch the virus.
struct CursedMirror
{
  int x = 0, y = 0; // blocks: its top
  int need = 30;
  int stare = 0;
};

// --- Level 11: Idol Mines (world_mine.cpp) ---------------------------------

// A mine track: a line of points in cells (x, and the top row of the ground
// the track lies on), with gaps where the track is missing. A branch leaves
// another rail at its first point while its lever is in `state`.
struct Rail
{
  std::string id;
  std::vector<std::pair<int, int>> pts;  // cells
  std::vector<std::pair<int, int>> gaps; // cells: x ranges with no track
  std::string leverId;
  int lever = -1;
  int state = 1;
  bool resets = false; // the loop: puts its lever back to 0 at its end
  int len = 0;         // eighths of a cell, each segment counted by its longer side
};

// A mine cart (2 x 1.5 blocks) on a rail. Positions are the middle of its
// bottom edge, in cells, on the track's row.
struct Cart
{
  std::string id;
  int rail = -1;
  int s = 0;     // eighths of a cell along the rail
  int dir = 1;   // +1 along the rail's points, -1 back
  int speed = 0; // eighths of a cell a frame; 0 standing
  int hop = 0;   // frame of a hop, 0 on the track
  bool falling = false;
  float fx = 0.0f, fy = 0.0f, vx = 0.0f, vy = 0.0f;
  float prevFx = 0.0f, prevFy = 0.0f;
  float angle = 0.0f, prevAngle = 0.0f; // radians, the track's slope (0 flat)
  int lost = 0; // frames until a lost cart is back at its dock
  int dockRail = -1, dockS = 0;
  bool painted = false; // the 42 on its side
  static constexpr int kW = 4, kH = 3;
  CellBox box() const;
};

// A lever by the track: shoot it to throw the junction.
struct Lever
{
  std::string id;
  int x = 0, y = 0; // cells, top-left of its 2 x 4 box
  int state = 0;
  int states = 2;
  int cool = 0;
  CellBox box() const { return {x, y, 2, 4}; }
};

// Level 12: a lava pool (`@ fluid kind=lava`). Touching it costs 2 hearts
// and pops the runner back to the last solid ground (shallow: 1 heart);
// a wading shelf costs a heart every 20 frames and holds you.
struct Lava
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // cells, inclusive; y0 is the surface
  bool shallow = false;
  bool wade = false;
  bool hearth = false; // the forge's hearth: roasts a marshmallow
  bool wet(int cx, int cy) const { return cx >= x0 && cx <= x1 && cy >= y0 && cy <= y1; }
  CellBox box() const { return {x0, y0, x1 - x0 + 1, y1 - y0 + 1}; }
};

// A Serpent Spear stuck in a wall: a 2 x 2 cell foothold, a one-way top.
struct Foothold
{
  int x = 0, y = 0; // cells, top-left
  int life = 150;
  int dir = 1; // the way it was thrown (the wall is on that side)
};

// Level 13: a boulder rolling along the corridor's floor (`@ boulder`). It
// waits in a ceiling hole (or on the floor), drops when the runner crosses
// its trigger, rolls right and ends in its chute.
struct Boulder
{
  enum State
  {
    Wait,
    Dust,   // dust trickling from the hole: it is coming
    Drop,   // falling out of the hole
    Roll,
    Halt,   // stopped a while (the stagehand's door)
    Lip,    // at its chute's lip, waiting for the runner in the shelter
    Teeter, // teetering at the lip: the bonus patch is on its flank
    Chute,  // falling into its chute
    Fly,    // off the end of the floor
    Gone,
    Surf,   // the bonus level's boulder, ridden (updateSurf)
  };
  int x = 0, y = 0; // cells, top-left
  int prevX = 0, prevY = 0;
  int size = 14; // cells
  int homeX = 0, homeY = 0;
  int num = 3, den = 4; // cells per frame
  int acc = 0;
  int vy = 0;
  int state = Wait;
  int timer = 0;
  int trigger = -1; // cells: crossing it sets the drop off
  int wake = -1;    // an alcove: standing in its shelter sets it rolling, `delay` frames later
  int delay = 0;
  int chute = -1;   // the chute that takes it
  int haltX = -1;   // cells: where it stops for `haltFrames`
  int haltFrames = 0;
  bool halted = false;
  int teeter = -1;  // an alcove: it waits at the chute's lip while the runner shelters there
  int shelter = 0;  // frames the runner has sheltered since it rolled over that alcove
  int camera = -1;  // the candid camera riding on it (an enemy index)
  float angle = 0.0f, prevAngle = 0.0f; // radians, for drawing
  CellBox box() const { return {x, y, size, size}; }
  bool rolling() const { return state >= Dust && state <= Fly && state != Chute; }
};

// Boulder Surfing (level 13's bonus, rules=boulder_surf): the runner rides
// on top of a boulder and rolls it along with left and right.
struct Surf
{
  int boulder = -1;    // which of mBoulders is ridden
  float x = 0.0f;      // the boulder's left edge (cells)
  float y = 0.0f;      // and its top
  float v = 0.0f;      // roll speed, cells a frame
  float vy = 0.0f;     // falling speed when the floor drops away
  float drift = 0.0f;  // the runner's offset from the top centre (cells)
  float carry = 0.0f;  // after a jump: the roll's speed the runner keeps
  float carried = 0.0f;
  bool mounted = true;
  bool free = false;   // off the boulder for good, on the exit ledge
  int fall = 0;        // frames left of a fall before the restart
  int falls = 0;
  int exitX = 0;       // cells: ground from here on is the exit ledge
  std::vector<int> restarts; // cells: where a fall puts the boulder back (else every screen)
};

// Level 14, The Idol Awakens (SPEC 14, world_sanctum.cpp).
// An altar (`@ altar`, 2 x 1 blocks, solid): hold up beside an offering
// altar to give gems back; the false one drops its trapdoor into the
// treasury, or with no gems held, wakes the bonus entrance.
struct Altar
{
  std::string id;
  int bx = 0, by = 0; // blocks: its left block
  bool offer = true;
  int tx0 = 0, ty0 = 0, tx1 = -1, ty1 = 0; // blocks: the false altar's trapdoor
  bool open = false;  // the trapdoor dropped (or the bonus woke)
  bool dropped = false; // the false altar went down with its trapdoor
  int stand = 0;      // frames stood on it
  int flare = 0;      // the incense flaring after an offering
  int offered = 0;
};

// A coin heap (`@ coinheap`): Coin Beetles crawl out as the runner passes.
struct CoinHeap
{
  int x = 0, y = 0; // cells: bottom-left
  int waves = 2, count = 2;
  int cool = 0;
};

// A Glyph Sentinel's row of glyphs, carved in the wall (non-solid).
struct GlyphRow
{
  int enemy = -1;      // the master glyph (an enemy index)
  int x0 = 0, x1 = 0;  // blocks
  int row = 0;
  int heads = 1;       // segments that fire in turn
  int t = 0;           // frames into the cycle (0: waiting)
  int cool = 0;
};

enum class GolemPhase
{
  Seated,  // the idol, until the runner is in the arena
  Rise,    // 60 frames of standing up
  Stomp,   // phase 1
  Break,   // 30 frames of falling apart into three heads
  Heads,   // phase 2
  Rebuild, // 30 frames of building itself again out of the walls
  Sweep,   // phase 3
  Crumble, // 60 frames into a pile of gold
  Done,
};

// Kaan-Tolok, the Idol Golem (`@ golem`).
struct Golem
{
  bool on = false;
  GolemPhase phase = GolemPhase::Seated;
  CellBox arena{0, 0, 0, 0};          // cells: the inner arena
  int doorX0 = 0, doorY0 = 0, doorX1 = 0, doorY1 = 0; // blocks
  int floor = 0;                      // cells: the row under the runner's feet
  float x = 0.0f;                     // cells: the body's left
  float prevX = 0.0f;
  int t = 0;                          // frames into the phase
  int cycle = 0;                      // frames into the attack cycle
  int hp = 32;                        // the current phase's
  int plates = 0, plateHp = 0;        // armor over the chest gem
  int open = 0;                       // frames the chest gem stays open
  int sweeps = 0;
  int tell = 0;                       // frames of the current telegraph left
  int sweep = 0;                      // frames of a sweep left; sweepHigh
  int sweepDir = 1;                   // the side it sweeps
  bool sweepHigh = false;
  int flash = 0;
  int wallL = 0, wallR = 0;           // blocks: the walls (phase 3 slides them in)
  int wallL0 = 0, wallR0 = 0;         // blocks: where they start
  int slide = 0;                      // frames of a wall slide (dust, then moving)
  int slid = 0;                       // slides so far
  bool wink = false;
  bool lid = false;                   // the gold lid over the left eye is open
  bool away = false;                  // the runner respawned outside: it waits for them
  int stomps = 0;
  int camera = -1;                    // the candid camera in its eye (an enemy index)
  int exitX = 0, exitY = 0;           // blocks
  static constexpr int kW = 16, kH = 20;
  struct Head
  {
    float x = 0.0f, y = 0.0f, vx = 1.0f, vy = 0.0f; // cells: x its left, y its bottom row
    float prevX = 0.0f, prevY = 0.0f;
    int hp = 8;
    int bounce = 10; // cells high
    bool down = true; // on the floor this frame (a landing)
    int stop = 0;    // rumbling before a charge
    bool charge = false;
    bool alive = true;
    int flash = 0;
  };
  std::array<Head, 3> heads;
  int charger = 0;
  struct Wave
  {
    float x = 0.0f;
    int dir = 1;
  };
  std::vector<Wave> waves;
  struct Rock
  {
    int x = 0;  // cells: its left column
    int t = 0;  // frames until it lands
    float y = 0.0f;
  };
  std::vector<Rock> rocks;
  CellBox body() const { return {int(x), floor + 1 - kH, kW, kH}; }
  // The gem is set low in the chest, where a jump shot reaches it.
  CellBox chest() const { return {int(x) + 6, floor - 10, 4, 4}; }
};

// Golden Touch (level 14's bonus, world_golden.cpp): a gold door (or the
// exit gate) one block wide. A door opens as the runner comes near; touched
// or shot first, it is gold and never opens. The gate opens once enough of
// the marked blocks are gold, unless the runner touched it first.
struct GoldDoor
{
  int bx = 0, by0 = 0, by1 = 0; // blocks
  bool gate = false;
  int open = 0;      // frames into opening (kDoorFrames: open)
  bool gold = false; // touched or shot shut
};

// A chute in the floor (`@ chute`): its flaps open for its boulder and
// shut 15 frames after it has gone.
struct Chute
{
  int bx0 = 0, bx1 = 0, row = 0; // blocks: the flaps
  int x0 = 0, x1 = 0;            // cells
  bool open = false;
  int shut = -1; // frames until the flaps shut
};

// An alcove (`@ alcove`): a 4-block shelter under the roll line and a
// 2-block step out of it.
struct Alcove
{
  std::string id;
  int x0 = 0, x1 = 0; // cells: the shelter
  int feet = 0;       // cells: a runner's feet row standing in it
};

// Level 13's secrets: a crack that opens once you have stood still in an
// alcove (`@ crack`), and floor hatches that open only with a lead on the
// boulder (`@ leadhatch`).
struct Crack
{
  int bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
  int alcove = -1;
  int still = 30;
  bool open = false;
  int glint = 0;
};
struct LeadHatch
{
  int bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
  int lead = 0, near = 16; // cells
  int ladderX = -1;        // block: this column turns to ladder when open
  bool open = false;
};

// A Blasting Cap: lobbed, bounces twice, rolls, and goes off after its fuse.
struct Cap
{
  float fx = 0.0f, fy = 0.0f, vx = 0.0f, vy = 0.0f; // cells: the middle of its bottom
  float prevFx = 0.0f, prevFy = 0.0f;
  int fuse = 30;
  int bounces = 0;
  int rail = -1; // rolling along this rail
  int rs = 0, rdir = 1;
  bool alive = true;
};

// A trapdoor in a floor that gives way under a heavy runner (Rocco).
struct Trapdoor
{
  int x0 = 0, x1 = 0, y = 0; // blocks
  bool open = false;
};

// Pinball Mine (rules=pinball): the runner is the ball.
struct PinSeg
{
  float x0, y0, x1, y1; // cells
};

struct PinBumper
{
  float x, y, r; // cells
  int flash = 0;
};

struct PinLamp
{
  float x, y; // cells
  bool lit = false;
  int flash = 0;
};

struct Pinball
{
  float x = 0.0f, y = 0.0f, vx = 0.0f, vy = 0.0f; // the ball's middle, cells
  float prevX = 0.0f, prevY = 0.0f;
  int flipL = 0, flipR = 0;  // 0 down .. kFlipSteps up
  int holdL = 0, holdR = 0;  // frames the button has been held
  float lx = 0.0f, ly = 0.0f, rx = 0.0f, ry = 0.0f; // the flippers' pivots
  float flipLen = 7.0f;
  std::vector<PinSeg> segs;
  std::vector<PinBumper> bumpers;
  std::vector<PinLamp> lamps;
  float plungerX = 0.0f, plungerY = 0.0f;
  bool inPlunger = true;
  int plungeWait = 0;
  int pull = 0; // frames the plunger has been pulled back (held jump or fire)
  float gateX = 0.0f, gateY = 0.0f;
  bool gateOpen = false;
  CellBox drain{0, 0, 0, 0};
  int drains = 0;
  static constexpr int kFlipSteps = 3;
};

struct Checkpoint
{
  int x = 0, y = 0; // bottom-left, 2x4 cells
  bool active = false;
  CellBox box() const { return boxAt(x, y, 2, 4); }
};


// --- Vehicles (world_vehicle.cpp) ----------------------------------------------
//
// Parked in a level with `@ vehicle kind=...`: the runner climbs in with USE
// (or up), drives it with the usual controls and climbs out with USE (or
// down + jump). A vehicle has its own armour, which takes every hit while
// the runner is inside, and its own weapons.
enum class VehicleKind
{
  Tank,  // slow, heavy cannon (aim up with up), crushes small enemies, spike-proof
  Heli,  // free flight on limited fuel, chaingun, down + fire drops a bomb
  Bike,  // hoverbike: fast, long jumps, floats over spikes, a blaster
  Sub,   // submarine: moves freely in deep water, torpedoes, you never run out of air
  Ship,  // space ship: zero-gravity flight with momentum, twin lasers
  Mech,  // walker: huge jet jumps, landing stomps break floors, arm cannon
  Count,
};

struct VehicleDef
{
  const char* key;
  const char* name;
  int w, h; // cells
  int hp;
  int fuel; // frames of flight (the helicopter), 0 unlimited
  Color color;
};
const VehicleDef& vehicleDef(VehicleKind k);
VehicleKind vehicleKindForKey(const std::string& key, bool* ok = nullptr);

struct Vehicle
{
  VehicleKind kind = VehicleKind::Tank;
  std::string id;
  int x = 0, y = 0; // cells, bottom-left
  int prevX = 0, prevY = 0;
  int w = 8, h = 5;
  int homeX = 0, homeY = 0, homeFacing = 1; // where it comes back to (moves to a checkpoint you drive past)
  int facing = 1;
  int hp = 10;
  int fuel = 0;
  int vx = 0, vy = 0; // sixteenths of a cell a frame (the bike, the ship, a helicopter on its way down)
  int ax = 0, ay = 0; // sixteenths not yet moved
  int air = -1;       // bike, mech: frame of the jump arc, -1 on the ground
  int fallen = 0;     // mech: cells fallen since it left the ground (a stomp at 6)
  int aim = 0;        // tank: 0 ahead, 1 up at 45 degrees; mech: 1 up
  int cool = 0, cool2 = 0; // frames until the guns fire again
  int mercy = 0;      // frames without damage after a hit
  int wreck = 0;      // destroyed: frames until it is back at home
  int flash = 0;      // 60 Hz hit flash
  int step = 0;       // tread, rotor and leg animation
  int barrel = 0;     // which gun fired last
  bool occupied = false;
  bool bot = false;           // bot=1: the autopilot drives it
  int dropX = -1, dropY = -1; // cells: where the autopilot climbs out
  CellBox box() const { return boxAt(x, y, w, h); }
};

// Deep water (world_sea.cpp): `@ sea rect=...`, its surface the rect's top
// row. A runner in it swims and runs out of air; a submarine drives in it.
struct Sea
{
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // cells, inclusive
  bool contains(int cx, int cy) const { return cx >= x0 && cx <= x1 && cy >= y0 && cy <= y1; }
};
constexpr int kAirFrames = 15 * 20; // 20 seconds of breath

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
  bool cheated = false; // a cheat was used: no bonuses, nothing recorded
};

// The pause menu's secret cheats (world_cheats.cpp).
enum class Cheat
{
  God,    // toggle: no damage (falling off the map still counts)
  Health, // full hearts
  Ammo,   // the level's prototype with full ammo
  Turbo,  // Turbo Mode now
  Cure,   // the Virus is gone
  Card,   // the access card
  Rapid,  // rapid fire
  Exit,   // beam out through the exit
  Count,
};

// The music clock (SPEC 3.1): 120 BPM, a bar is 30 logic frames and its four
// beats are 8, 7, 8 and 7 frames long.
int beatOfFrame(int frame);      // 0..3
int beatStartFrame(int beat);    // first frame of beat 0..3 within the bar
int framesIntoBeat(int frame);   // 0.. within the current beat
// Level 3's 16-bar phrases (480 frames): bars 0-7 verse, 8-13 chorus,
// 14-15 riser; the drop is a triple hit on beat 1 of bars 0, 1 and 2.
constexpr int kPhraseFrames = 480;
int phraseBar(int frame);        // 0..15
int dropHit(int frame);          // 1..3 on a drop hit, else 0

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
  const CharacterDef& character() const { return mCharacter; }
  const std::vector<Enemy>& enemies() const { return mEnemies; }
  const std::vector<ItemBox>& boxes() const { return mBoxes; }
  const std::vector<Item>& items() const { return mItems; }
  const std::vector<Projectile>& projectiles() const { return mProjectiles; }
  const Level& level() const { return *mLevel; }
  const CollisionMap& map() const { return mMap; }
  const Camera& camera() const { return mCamera; }

  // Sounds triggered since the last call; the frontend plays them.
  std::vector<Sfx> takeSounds();

  // The map (world_map.cpp): the blocks the runners have had on screen,
  // filled in as they go and saved with the game. Dark sectors only count
  // where they are lit or right around the runner.
  bool explored(int tx, int ty) const;
  // Bakes the explored blocks bx0..bx0+bw-1, by0..by0+bh-1 as a map at
  // `scale` pixels a block; an empty texture if none of them are explored.
  Texture bakeMap(Renderer& r, float scale, int bx0, int by0, int bw, int bh) const;
  // The live marks over a map whose block (0, 0) is at (x, y): the runner,
  // the checkpoints and the exit once seen.
  void drawMapMarks(Renderer& r, float x, float y, float scale, int frame) const;

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
  // Applies a cheat and says what it did; false if it can't right now
  // (dying, already leaving).
  bool cheat(Cheat c);
  bool godMode() const { return mGod; }
  void markCheated() { mStats.cheated = true; } // a cheat in its bonus level

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
  const std::vector<Pad>& pads() const { return mPads; }
  const std::vector<LaserFan>& fans() const { return mFans; }
  const std::vector<Breakable>& breakables() const { return mBreakables; }
  const std::vector<Breaker>& breakers() const { return mBreakers; }
  const std::vector<Door>& doors() const { return mDoors; }
  bool exitPowered() const;
  const std::vector<Fluid>& fluids() const { return mFluids; }
  const std::vector<Bubble>& bubbles() const { return mBubbles; }
  const std::vector<Valve>& valves() const { return mValves; }
  // Level 6: the gantries, and whether something about to sweep over the
  // runner would knock them off where they stand (the planner waits it out).
  const std::vector<Gantry>& gantries() const { return mGantries; }
  bool trainDanger() const;
  bool trainBusy() const; // the train is braking or a gantry is sweeping
  bool lightTrail() const { return mLightTrail; }
  // Level 7: the hunter, its rockets on the way and Black Halo.
  const Hunter& hunter() const { return mHunter; }
  const std::vector<Strike>& strikes() const { return mStrikes; }
  const Boss& boss() const { return mBoss; }
  bool bossFight() const; // the runner is in the arena and Black Halo is up
  bool bossTarget(BossPart part, CellBox& box) const; // where a part is; false if it can't be hit now
  int bossHp() const { return mBoss.on ? mBoss.total() : 0; }
  bool flight() const { return mFlight; } // Pilot Seat: you fly the gunship
  bool latchedPlatform(const Platform& pl) const; // a hook still held by its latch
  // Level 8: the vines (the bot swings them), the bridges, the Boomerang out.
  const std::vector<Vine>& vines() const { return mVines; }
  const std::vector<Bridge>& bridges() const { return mBridges; }
  bool bounce() const { return mBounce; }
  // Would letting go now launch (high enough and near a turn)?
  bool vineLaunchReady() const { return vineLaunchDir() != 0; }
  // Which way letting go now would launch the runner (-1, 1), 0 for no launch.
  int vineLaunchDir() const;
  const std::vector<Load>& loads() const { return mLoads; }
  const std::vector<JungleRope>& jungleRopes() const { return mJRopes; }
  bool inWater() const; // Canopy Road's stream: wading at half speed
  // Level 9: plates, traps and the stone keys (the bot fetches them).
  const std::vector<Plate>& plates() const { return mPlates; }
  const std::vector<Trap>& traps() const { return mTraps; }
  const std::vector<StoneKey>& stoneKeys() const { return mStoneKeys; }
  const std::vector<KeyDoor>& keyDoors() const { return mKeyDoors; }
  const std::vector<SecretDoor>& secretDoors() const { return mSecretDoors; }
  const std::vector<CollapseTile>& collapseTiles() const { return mCollapse; }
  int stoneKeysHeld() const;
  bool trapmaster() const { return mTrapmaster; }
  const std::vector<SunBeam>& sunBeams() const { return mSunBeams; }
  const std::vector<Mirror>& mirrors() const { return mMirrors; }
  const std::vector<SunDoor>& sunDoors() const { return mSunDoors; }
  const std::vector<BeamPath>& beamPaths() const { return mBeamPaths; }
  bool lanceOn() const { return mLanceOn; }
  bool negative() const { return mNegative; }
  // Level 11: tracks, carts, levers, caps and Pinball Mine.
  const std::vector<Rail>& rails() const { return mRails; }
  const std::vector<Cart>& carts() const { return mCarts; }
  const std::vector<Lever>& levers() const { return mLevers; }
  const std::vector<Cap>& caps() const { return mCaps; }
  const std::vector<Trapdoor>& trapdoors() const { return mTrapdoors; }
  bool pinball() const { return mPinball; }
  const Pinball& pin() const { return mPin; }
  // Level 12: lava, spear footholds and The Floor Is Lava.
  const std::vector<Lava>& lavas() const { return mLavas; }
  const std::vector<Foothold>& footholds() const { return mFootholds; }
  bool floorLava() const { return mFloorLava; }
  // Vehicles and deep water (world_vehicle.cpp, world_sea.cpp).
  const std::vector<Vehicle>& vehicles() const { return mVehicles; }
  const Vehicle* riding() const
  {
    return mPlayer.vehicle >= 0 ? &mVehicles[std::size_t(mPlayer.vehicle)] : nullptr;
  }
  int vehicleInReach() const; // the empty vehicle USE would board now, -1 for none
  bool canLeaveVehicle() const;
  // Pressing up next to a vehicle boards it too (people; the bot uses USE).
  void setUpBoards(bool on) { mUpBoards = on; }
  // How the USE action is labelled in prompts ("E / LB").
  void setUseLabel(const std::string& s) { mUseLabel = s; }
  const std::vector<Sea>& seas() const { return mSeas; }
  bool inSea(int cx, int cy) const;
  int seaSurface(int cx) const; // top row of the deep water in this column, -1 for none
  int air() const { return mAir; }
  // Where a vehicle of this size could be: inside the map, nothing solid in
  // it, and (a submarine) in deep water.
  bool vehicleFits(VehicleKind k, int x, int y) const;
  int headBounces() const { return mHeadBounces; }
  bool surfing() const { return mSurfing; }
  // Level 14: Gold Fever and the golem.
  bool greedOn() const { return mGreedOn; }
  int greed() const { return mGreed; }
  const std::vector<Altar>& altars() const { return mAltars; }
  const Golem& golem() const { return mGolem; }
  bool besideAltar(int index) const; // the runner can hold up at it
  // Level 14's bonus, Golden Touch.
  bool golden() const { return mGolden; }
  int goldAt(int bx, int by) const; // 0 unmarked, 1 marked, 2 gilded, 3 a statue
  int goldMarked() const { return mGoldMarked; }
  int goldDone() const { return mGoldDone; }
  bool goldReached() const { return mGoldDone * 100 >= mGoldMarked * mGoldGoal; }
  const std::vector<GoldDoor>& goldDoors() const { return mGoldDoors; }
  // Episode 3, STATION ZERO: hull panels and vents, rails, crates, charges.
  const StationState& station() const { return mStation; }
  bool holdingRail() const;
  // The open panel pulling at cell (cx, cy), -1 for none; tx, ty its middle.
  int ventPulling(int cx, int cy, int& tx, int& ty) const;
  CellBox tetherBeam(const Enemy& low) const;
  bool golemFight() const;
  int golemHp() const;
  const Surf& surf() const { return mSurf; }
  // Level 13: the boulders and the alcoves.
  const std::vector<Boulder>& boulders() const { return mBoulders; }
  const std::vector<Alcove>& alcoves() const { return mAlcoves; }
  bool inShelter(int alcove) const;
  int lavaPops() const { return mLavaPops; }
  int lavaAt(int cx, int cy) const; // the pool covering that cell, -1 for none
  // Episode 7 (world_space.cpp): goo on walls (level 44).
  bool hasGoo() const { return mSpace.goo; }
  bool gooAt(int cx, int cy) const; // a solid cell coated in goo
  bool gooBlockAt(int tx, int ty) const; // a block coated for good
  const std::vector<GooPatch>& gooPatches() const { return mSpace.patches; }
  // Level 45: the Gullet Tubes; whether a mouth is open now; where a tube
  // spits you out (cells, the runner's feet) and how far that is from s.
  bool hasHive() const { return mSpace.hive; }
  const std::vector<GulletTube>& gulletTubes() const { return mSpace.tubes; }
  bool mouthOpen(const GulletTube& t) const;
  CellBox mouthTrigger(const GulletTube& t) const;
  void tubeExit(const GulletTube& t, int branch, int& x, int& y) const;
  static constexpr int kBreathPeriod = 90; // the hive breathes in for 40 frames of every 90
  bool inLava(const CellBox& b) const;
  // Fifteenths of a cell a sink platform goes down per frame with its load now.
  int sinkRate(const Platform& pl) const;
  // The track's row (cells) under x on this rail, and whether there is
  // track there at all (false in a gap or past its ends).
  bool railY(const Rail& r, float x, float& y) const;
  int negativePhase() const; // 0 sun, 1 moon
  // Level 10, for the bot: where a sunbeam from (x, y) going `dir` ends up
  // with these mirror angles (doors as they are, no Monks or moths): the
  // sun door it lights, -1 for none, or -2 at the first mirror not yet
  // `fixed` (its index in *stop).
  int traceBeamFor(int x, int y, int dir, const std::vector<int>& angles, const std::vector<char>& fixed,
    int* stop) const;
  const TrapCursor& trapCursor() const { return mCursor; }
  // Level 6's billboard shows the best level 2 score, if there is one.
  void setHiScore(int score) { mHiScore = score; }
  const std::vector<TrailBlock>& trail() const { return mTrailBlocks; }
  // Level 5: in sludge (feet at or under its surface), and not in Turbo.
  bool wading() const;
  bool autorun() const { return mAutorun; } // Duck Rapids: the duck moves you
  // Level 4: is this cell in light (a lit sector, a lamp or a flare)?
  bool litAt(int cx, int cy) const;
  const std::string& musicOverride() const { return mMusicOverride; }
  // Tracks this level may switch to mid-run (the club's chiptune, the cab
  // radio's cover), so they can be rendered before they are needed.
  std::vector<std::string> musicVariants() const;
  // Standing on a pad that launches: frames until it does, else -1.
  int framesToNextLaunch() const;
  bool launching() const { return mLaunch > 0; }
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
  // Level 3 (world_club.cpp).
  int jumpHeight() const;
  int padUnder(const CellBox& b) const;
  void startLaunch(int cells);
  void updateLaunch(int mvX);
  void updateClub();
  void updateFans();
  void updateCones();
  void fireCone(int ox, int oy, int dir, int damage);
  void knockBack(Enemy& e, int dir, int cells);
  bool shotHitsEnemy(Enemy& e, int dir, int damage);
  void updateBouncer(Enemy& e, const EnemyDef& def);
  void updateDisco(Enemy& e, const EnemyDef& def);
  void updateRaver(Enemy& e, const EnemyDef& def);
  void updateStepper(Enemy& e, const EnemyDef& def);
  void shotAtProps(const CellBox& b);
  PlayerInput beatStepInput(const PlayerInput& in);
  void drawClub(Renderer& r, float camX, float camY, int frame) const;
  void drawClubHud(Renderer& r, int frame) const;
  // Level 4 (world_dark.cpp).
  bool setupDarkEntity(const EntityDef& e);
  bool darkAt(int cx, int cy) const;     // in a sector that is (still) dark here
  float lightLevel(int cx, int cy) const; // 0 dark .. 1 lit, for drawing
  void throwBreaker(Breaker& b);
  void cutBreaker(Breaker& b);
  void applyDoor(Door& d, bool solid);
  void updateDark(const PlayerInput& input);
  void updateFlares();
  void stickFlare(const Projectile& pr, int enemy);
  void updateStalker(Enemy& e, const EnemyDef& def);
  void updateLooter(Enemy& e, const EnemyDef& def);
  void updateLeech(Enemy& e, const EnemyDef& def);
  void dropLoot(const Enemy& e);
  void drawDark(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void sonarPing(int x, int y);
  // Level 5 (world_sludge.cpp).
  bool setupSludgeEntity(const EntityDef& e);
  void setupSludgeEnemy(Enemy& en, const EntityDef& e);
  int fluidAt(int cx, int cy) const; // the fluid whose sludge fills this cell, -1
  int fluidSurface(const Fluid& f) const; // where its surface is at this clock
  int wadeFluid() const;             // the fluid at the runner's feet, -1
  bool buoyed() const;               // under the surface: it pushes you up
  bool wadeStep(int dir);            // step up a block out of sludge
  void updateSludge(const PlayerInput& input);
  void syncFloats();
  void floatItem(Item& it) const;
  void updateGator(Enemy& e, const EnemyDef& def);
  void updateKeeper(Enemy& e, const EnemyDef& def);
  void updateRatPipes();
  void updateBubbles();
  bool trapEnemy(Enemy& e);
  void popBubble(std::size_t i, bool stun);
  bool shotAtSludge(const Projectile& pr);
  bool shotAtBubbles(const CellBox& b);
  void drawSludgeBack(Renderer& r, float camX, float camY, int frame) const;
  void drawSludgeFront(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawTideHud(Renderer& r, int frame) const;
  // Level 6 (world_maglev.cpp).
  bool setupMaglevEntity(const EntityDef& e);
  void setupMaglevEnemy(Enemy& en, const EntityDef& e);
  void updateMaglev(const PlayerInput& input);
  void fireGantry(Gantry& g);
  void updateGantries();
  void updateTunnel();
  void updateCaltrops();
  void updateLightTrail(const PlayerInput& input);
  void updateHopper(Enemy& e, const EnemyDef& def);
  void updateRailDrone(Enemy& e, const EnemyDef& def);
  void updateDecoupler(Enemy& e, const EnemyDef& def);
  void resetTrain(); // after a respawn: gantries gone, Decouplers back in their couplings
  void fireArc(int ox, int oy, int dir, int damage);
  int roofTopBelow(int cx, int cy) const; // first solid top at or below cy, -1
  bool sameRoof(const CellBox& a, const CellBox& b) const;
  float trainSpeed() const; // 1 at full speed, 0 stopped at the station
  void drawMaglevBack(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawMaglevFront(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawTrainProp(Renderer& r, const Prop& pr, float x, float y, float w, float h, int frame, bool foreground) const;
  // Level 7 (world_chopper.cpp).
  bool setupChopperEntity(const EntityDef& e);
  void setupChopperEnemy(Enemy& en, const EntityDef& e);
  void updateChopper(const PlayerInput& input);
  bool covered() const; // under cover from the searchlight
  void updateHunter();
  void startSalvo(bool demo);
  void fireStrike(int tx, int ty);
  void updateStrikes();
  void updateRappel();
  bool dropTrooper(int x, int feetY, int fall);
  void updateTrooper(Enemy& e, const EnemyDef& def);
  void updateBiker(Enemy& e, const EnemyDef& def);
  void updateShield(Enemy& e, const EnemyDef& def);
  void updateBoss();
  void bossPhase(BossPhase phase);
  void damageBoss(BossPart part, int damage, Vec2 at);
  bool shotAtBoss(Projectile& pr);
  void explodeAtBoss(int cx, int cy, int radius, int damage);
  void resetBossCycle(); // after a respawn
  void updateFlight(const PlayerInput& input);
  void updateCardboard();
  void updateRadio();
  bool targetBox(int target, CellBox& box) const;
  void paintTarget();
  void launchRockets();
  void steerRocket(Projectile& pr);
  void drawChopperBack(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawChopperFront(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawChopperProp(Renderer& r, const Prop& pr, float x, float y, float w, float h, int frame, bool foreground) const;
  void drawBoss(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawFlightShip(Renderer& r, float camX, float camY, int frame, float alpha) const;
  // Level 8 (world_jungle.cpp).
  bool setupJungleEntity(const EntityDef& e);
  void setupJungleEnemy(Enemy& en, const EntityDef& e);
  void updateJungle(const PlayerInput& input);
  void updateVines();
  bool tryGrabVine();
  void placeOnVine();
  void updateSwing(int mvX, int mvY, const Button& jump);
  void letGoOfVine(bool launch, int dir);
  void updateBridges();
  int bridgeWeight(const Bridge& b, bool& runner) const;
  void dropBridge(Bridge& b);
  void updateLoads();
  void cutRope(JungleRope& rope);
  void updateFruits();
  void lobFruit(const Enemy& e);
  void updateHowler(Enemy& e, const EnemyDef& def);
  void updateViper(Enemy& e, const EnemyDef& def);
  void updateCutter(Enemy& e, const EnemyDef& def);
  bool shotAtJungle(Projectile& pr);
  bool stepBoomerang(Projectile& pr);
  void resetJungle(); // after a respawn: bridges rebuilt
  void updateBounce(int mvX, int mvY, const PlayerInput& in);
  void drawJungleBack(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawJungleFront(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawJungleProp(Renderer& r, const Prop& pr, float x, float y, float w, float h, int frame, bool foreground) const;
  // Level 9 (world_temple.cpp).
  bool setupTempleEntity(const EntityDef& e);
  void setupTempleEnemy(Enemy& en, const EntityDef& e);
  void updateTemple(const PlayerInput& input);
  void updatePlates();
  void fireTrap(Trap& t);
  void updateTraps();
  void trapHits(Trap& t);
  void updateCollapse();
  void updateKeys();
  void updateGuardian(Enemy& e, const EnemyDef& def);
  void updateDartFace(Enemy& e, const EnemyDef& def);
  void updateScarabs(Enemy& e, const EnemyDef& def);
  bool tangled(Enemy& e); // the Snare Bolas: true while it lies tangled (no AI)
  void tangle(Enemy& e, int dir);
  bool shotAtTemple(Projectile& pr, Enemy& e); // true: the shot is used up on it
  void killBeetles(Enemy& e, int n);
  void resetTemple(); // after a respawn
  void updateTrapmaster(const PlayerInput& input);
  void drawTempleBack(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawTempleFront(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawTempleProp(Renderer& r, const Prop& pr, float x, float y, float w, float h, int frame, bool foreground) const;
  void drawTempleHud(Renderer& r, float x, float top) const;
  // Level 10 (world_light.cpp).
  bool setupLightEntity(const EntityDef& e);
  void setupLightEnemy(Enemy& en, const EntityDef& e);
  void updateLight(const PlayerInput& input);
  void traceBeam(int x, int y, int dir, int maxSteps, int bounces, bool lance, BeamPath& path);
  void updateSunDoors();
  void setSunDoor(SunDoor& d, bool open);
  void updateWraith(Enemy& e, const EnemyDef& def);
  void updateMonk(Enemy& e, const EnemyDef& def);
  void updateMoth(Enemy& e, const EnemyDef& def);
  bool shotAtLight(Projectile& pr, const CellBox& b); // mirrors: true if the shot is used up
  int shotAtLightEnemy(Projectile& pr, Enemy& e); // Wraiths, Monks, moths: 0 not handled, 1 used up, 2 passes through
  bool lightHides(const Enemy& e) const; // the camera in an unlit mirror's pedestal
  void drawNegativeLayer(Renderer& r, const Layer& l, float x, float y, float w, float h, int frame) const;
  void drawLightBack(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawLightFront(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawLightHud(Renderer& r, int frame) const;
  // Level 11 (world_mine.cpp).
  bool setupMineEntity(const EntityDef& e);
  void setupMineEnemy(Enemy& en, const EntityDef& e);
  void linkMine();
  void updateMine(const PlayerInput& input);
  void updateRide(int mvX, int mvY, const PlayerInput& in);
  void stepCart(Cart& c, bool ridden);
  void placeCart(Cart& c);
  void railPoint(const Rail& r, int s, float& x, float& y, float& angle, int* seg = nullptr) const;
  int railNear(float x, float y, int skip, int& s) const;
  bool overGap(const Rail& r, float x) const;
  void loseCart(Cart& c);
  void dockCart(Cart& c);
  void leaveCart(bool jump, int fling);
  void placeRider();
  void boardCarts();
  void resetMine(); // after a respawn
  void throwCap(int ox, int oy);
  void updateCaps();
  void blowCap(Cap& cap);
  bool shotAtMine(Projectile& pr, const CellBox& b);
  void updateBandit(Enemy& e, const EnemyDef& def);
  void updateBat(Enemy& e, const EnemyDef& def);
  void updateMole(Enemy& e, const EnemyDef& def);
  void crashed();
  void setupPinball();
  void updatePinball(const PlayerInput& input);
  void drawMineBack(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawMineFront(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawCart(Renderer& r, const Cart& c, float camX, float camY, float alpha, bool front) const;
  void drawPinball(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawMineHud(Renderer& r, int frame) const;
  // Level 12 (world_lava.cpp).
  bool setupLavaEntity(const EntityDef& e);
  void setupLavaPlatform(Platform& pl, const EntityDef& e);
  void setupLavaEnemy(Enemy& en, const EntityDef& e);
  void linkLava();
  void updateLava(const PlayerInput& input);
  void updateSinkPlatform(Platform& pl);
  void updateRisePlatform(Platform& pl);
  void updateLavaPlatforms();
  void lavaPop(int hearts);
  void stickSpear(Projectile& pr);
  bool shotAtLava(Projectile& pr);
  int shotAtCrab(Projectile& pr, Enemy& e); // 0 not handled, 1 used up
  void updateToad(Enemy& e, const EnemyDef& def);
  void updateWisp(Enemy& e, const EnemyDef& def);
  void updateCrab(Enemy& e, const EnemyDef& def);
  void updateFloorLava(const PlayerInput& input);
  bool setupSanctumEntity(const EntityDef& e);
  void setupSanctumEnemy(Enemy& en, const EntityDef& e);
  void linkSanctum();
  void updateSanctum(const PlayerInput& input);
  void updateGolem();
  void golemPhase(GolemPhase phase);
  void hurtGolem(int damage, bool full);
  bool shotAtGolem(Projectile& pr, const CellBox& b);
  bool shotAtGlyph(Projectile& pr, const CellBox& b);
  void updateDrummer(Enemy& e, const EnemyDef& def);
  void updateCoinBeetle(Enemy& e, const EnemyDef& def);
  void updateSentinel(Enemy& e, const EnemyDef& def);
  void offerGem(Altar& a);
  void gainGreed();
  void setWall(int bx, bool solid);
  void drawSanctumBack(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawSanctumFront(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawSanctumHud(Renderer& r, int frame) const;
  void resetSanctum();
  bool drummed(const Enemy& e) const;
  void dropGems(int x, int y, int n);
  void setupSurf();
  bool setupGoldenEntity(const EntityDef& e);
  bool setupStationEntity(const EntityDef& e);
  void setupStationEnemy(Enemy& en, const EntityDef& e);
  void linkStation();
  void resetStation();
  int panelAt(const CellBox& b) const;
  void breachPanel(int index);
  void ventOut(int kind, float x, float y, int variant);
  bool pullBox(int& x, int& y, int w, int h, int tx, int ty, int cells) const;
  void updateVents();
  void breakCrate(Crate& c);
  bool shotAtStation(Projectile& pr, const CellBox& b);
  void updateCrates();
  void placeCharge(int ox, int oy);
  void blastAt(int cx, int cy);
  void detonateCharges();
  void updateCharges();
  bool breachTrigger(const Button& fire);
  void updateLoader(Enemy& e, const EnemyDef& def);
  void updateWeldDrone(Enemy& e, const EnemyDef& def);
  void updateTether(Enemy& e, const EnemyDef& def);
  void updateStation(const PlayerInput& input);
  bool stationCanSave() const;
  void drawStationBack(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawStationFront(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawStationHud(Renderer& r, int frame) const;
  void linkGolden();
  void updateGolden();
  void gild(int bx, int by);
  void gildBox(const CellBox& b);
  bool shotAtGolden(Projectile& pr, const CellBox& b);
  void gildEnemy(Enemy& e);
  void setGoldDoor(GoldDoor& d, bool solid);
  void drawGolden(Renderer& r, float camX, float camY, int frame) const;
  void drawGoldenHud(Renderer& r, int frame) const;
  void updateSurf(const PlayerInput& input);
  void placeSurf(float x);
  void moveSurfBoulder();
  void syncFootholds();
  void drawLavaBack(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawLavaFront(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawLavaProp(Renderer& r, const Prop& pr, float x, float y, float w, float h, int frame) const;
  // Episode 7, DEEP SPACE (world_space.cpp).
  bool setupSpaceEntity(const EntityDef& e);
  void updateSpace(const PlayerInput& input);
  bool tryCling(int mvX);
  void updateCling(int mvX, int mvY);
  void addGoo(int x, int y0, int y1, int side, int life);
  void splatGoo(int cx, int cy, int reach); // on the nearest wall face beside (cx, cy)
  bool shotAtSpace(Projectile& pr);           // a shot hit a wall: the Goo Gun splats
  bool shotAtAlien(Projectile& pr, Enemy& e); // true: the shot is used up on it
  void alienKilled(const Enemy& e);
  void updateSkitter(Enemy& e, const EnemyDef& def);
  void updateSpitpod(Enemy& e, const EnemyDef& def);
  void updateGloop(Enemy& e, const EnemyDef& def);
  void resetSpace(); // after a respawn
  void drawSpaceBack(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawSpaceFront(Renderer& r, float camX, float camY, int frame, float alpha) const;
  // Level 45 (world_hive.cpp).
  bool setupHiveEntity(const EntityDef& e);
  void updateHive();
  void updateTubeRide();
  void swallowPlayer(int tube);
  bool shotAtHive(Projectile& pr); // a Bile Blaster shot at a mouth goes down the tube
  bool shotAtValve(Projectile& pr);
  void updateMite(Enemy& e, const EnemyDef& def);
  void updatePolyp(Enemy& e, const EnemyDef& def);
  void updateWarden(Enemy& e, const EnemyDef& def);
  void resetHive();
  void drawHiveBack(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawHiveFront(Renderer& r, float camX, float camY, int frame, float alpha) const;
  // Level 13 (world_boulder.cpp).
  bool setupBoulderEntity(const EntityDef& e);
  void setupBoulderEnemy(Enemy& en, const EntityDef& e);
  void linkBoulders();
  void updateBoulders(const PlayerInput& input);
  void updateBoulder(Boulder& b);
  void resetBoulders();
  void syncTotems();
  int rollFloor(int x, int top, int size) const;
  bool shotAtTotem(Projectile& pr, const CellBox& b);
  void updateSpearRunner(Enemy& e, const EnemyDef& def);
  void updatePitSnake(Enemy& e, const EnemyDef& def);
  void updateTotem(Enemy& e, const EnemyDef& def);
  void setChute(Chute& c, bool open);
  void drawBoulderBack(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawBoulderFront(Renderer& r, float camX, float camY, int frame, float alpha) const;

  // world_vehicle.cpp: vehicles.
  bool setupVehicleEntity(const EntityDef& e);
  void updateVehicles(const PlayerInput& input);
  bool tryBoard(const PlayerInput& input);
  void boardVehicle(int index);
  void leaveVehicle(bool thrown);
  void updateDrive(int mvX, int mvY, const PlayerInput& input);
  void driveTank(Vehicle& v, int mvX, int mvY, const PlayerInput& input);
  void driveHeli(Vehicle& v, int mvX, int mvY, const PlayerInput& input);
  void driveBike(Vehicle& v, int mvX, int mvY, const PlayerInput& input);
  void driveSub(Vehicle& v, int mvX, int mvY, const PlayerInput& input);
  void driveShip(Vehicle& v, int mvX, int mvY, const PlayerInput& input);
  void driveMech(Vehicle& v, int mvX, int mvY, const PlayerInput& input);
  bool vehicleFall(Vehicle& v, int cells); // false once it is on the ground
  bool vehicleStep(Vehicle& v, int dx, int dy); // moves a free mover a cell, true if it moved
  void stomp(Vehicle& v);
  void vehicleContacts(Vehicle& v);
  void damageVehicle(Vehicle& v, int amount);
  void wreckVehicle(Vehicle& v);
  bool shotAtVehicle(const CellBox& b); // an enemy shot hits the ridden vehicle
  void placeDriver();
  void resetVehicles(); // after a respawn
  Projectile& vehicleShot(ShotKind kind, float x, float y, float vx, float vy, int speed, int damage);
  void drawVehicles(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawVehicle(Renderer& r, const Vehicle& v, float camX, float camY, int frame, float alpha) const;
  void drawVehicleHud(Renderer& r, int frame) const;
  // world_sea.cpp: deep water, swimming and the sea's residents.
  bool setupSeaEntity(const EntityDef& e);
  void updateSea();
  bool updateSwim(int mvX, int mvY, const PlayerInput& input); // false: not in the water
  void updateFish(Enemy& e, const EnemyDef& def);
  void updateJelly(Enemy& e, const EnemyDef& def);
  void updateSeaMine(Enemy& e, const EnemyDef& def);
  void updateAngler(Enemy& e, const EnemyDef& def);
  void blowSeaMine(Enemy& e);
  bool waterCell(int cx, int cy) const; // deep water and not solid
  void drawSeaBack(Renderer& r, float camX, float camY, int frame) const;
  void drawSeaFront(Renderer& r, float camX, float camY, int frame) const;
  void drawAirHud(Renderer& r, int frame) const;

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
  // world_map.cpp
  void markExplored();
  std::vector<int> exploredRuns() const;
  void restoreExplored(const std::vector<int>& runs);

  // effects
  void burst(Vec2 at, Color a, Color b, int count, float speed, bool glow = true);
  void flashAt(Vec2 at, float radius, Color c, int life);

  // world_draw.cpp
  void drawTiles(Renderer& r, float camX, float camY, int frame) const;
  void drawPlayer(Renderer& r, float camX, float camY, int frame, float alpha) const;
  void drawHud(Renderer& r, int frame) const;

  std::shared_ptr<const Level> mLevel; // immutable; copies of the world share it
  CollisionMap mMap;
  CharacterDef mCharacter; // a copy: the roster can change under a running level
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
  // Per block: 1 once explored. Shared by copies; simulations have none.
  std::shared_ptr<std::vector<std::uint8_t>> mExplored;
  std::vector<Prop> mProps;
  std::vector<Zone> mZones;
  std::vector<Platform> mPlatforms;
  std::vector<std::string> mPlatformPairs; // setup only: each platform's pair=
  std::vector<Hatch> mHatches;
  std::vector<Breakable> mBreakables;
  std::vector<Spawner> mSpawners;
  std::size_t mLevelEnemyCount = 0; // the level's own; spawned ones follow
  // Level 3: subwoofers, laser fans, the Bass Cannon, beat-step rules.
  std::vector<Pad> mPads;
  std::vector<LaserFan> mFans;
  std::vector<Puddle> mPuddles;
  std::vector<Cone> mCones;
  int mLaunch = 0;          // cells of a pad launch still to rise
  int mLaunchBump = 0;      // the launch is a beat bump of this many cells, else 0
  bool mBeatStep = false;   // bonus rule: you move only on the beat
  int mQueuedDx = 0;
  bool mQueuedJump = false, mQueued = false;
  int mBeatDx = 0, mBeatMove = 0, mBeatJump = 0;
  std::string mMusicOverride; // an easter egg can change the track
  // Level 4: power cuts.
  std::vector<DarkSector> mSectors;
  std::vector<LightSource> mLights;
  std::vector<Breaker> mBreakers;
  std::vector<Door> mDoors;
  std::vector<Flare> mFlares;
  std::vector<Cable> mCables;
  std::vector<std::pair<CellBox, int>> mHiddenSpikes; // spikes that do not draw while their sector is dark
  int mAllLitAt = -1;  // clock when the last breaker relit the whole district
  bool mSonar = false; // bonus rule: nothing draws but the runner and the echoes
  std::vector<Ping> mPings;
  std::vector<int> mPinged; // per block: clock of the last echo off it
  std::vector<std::uint8_t> mChimed; // sonar: gems already announced
  // Level 5: the tide.
  std::vector<Fluid> mFluids;
  std::vector<Valve> mValves;
  std::vector<RatPipe> mRatPipes;
  std::vector<Bubble> mBubbles;
  std::vector<CellBox> mDevNull; // pipe mouths that swallow what floats in
  int mSludgeTicks = 0;          // frames in sludge since the last heart lost
  bool mDiving = false;          // holding down in sludge: sink instead of float
  bool mAutorun = false;         // bonus rule: the duck paddles on by itself
  int mBump = 0;                 // autorun: cells of a bump back still to go
  // Level 6: the train.
  bool mTrain = false;              // trainscroll: the backdrop rushes past
  std::array<int, 3> mScrollSpeeds{6, 12, 24}; // px a frame: sky, far, near
  float mScroll = 0.0f;             // frames of travel so far (braking slows it)
  int mBrakeX = -1, mBrakeAt = -1;  // cells: arrival trigger; clock when braking began
  int mStationLayer = -1;
  std::vector<Gantry> mGantries;
  int mTunnelX0 = -1, mTunnelX1 = -1; // cells
  int mTunnelRing = 90;
  int mTunnelState = 0; // 0 not yet, 1 inside (rings), 2 left
  int mTunnelNext = 0;  // clock of the next ring
  int mTunnelMouthX = -1, mTunnelExitX = -1; // cells: where the dark starts and ends (drawing)
  int mGapX = -1, mGapFrames = 160, mGapUntil = -1, mPasser = -1;
  bool mGapDone = false;
  std::vector<Caltrop> mCaltrops;
  std::vector<Shockwave> mWaves;
  std::size_t mLevelGantries = 0; // the level's own; tunnel rings follow
  std::vector<ArcBolt> mArcs;
  bool mLightTrail = false; // bonus rule: your feet draw a neon bridge
  std::vector<TrailBlock> mTrailBlocks;
  int mPry = 0; // frames down has been held on a loose floor panel
  int mHiScore = -1;
  // Level 7: the hunter, cover, rappel drops, the hook and Black Halo.
  Hunter mHunter;
  std::vector<Strike> mStrikes;
  std::vector<CellBox> mCovers;
  std::vector<RappelZone> mRappels;
  std::vector<Latch> mLatches;
  Boss mBoss;
  std::vector<int> mPaint;     // Lock-On Rockets: painted targets
  int mNextTarget = kNoTarget; // the target of the rocket fireProto launches next
  int mRadio = 0;              // frames in the crane cab (the elevator music)
  CellBox mCab{0, 0, 0, 0};    // cells: the crane cab
  bool mFlight = false;        // bonus rule: you fly Black Halo
  int mFlightGun = 0, mFlightRocket = 0; // frames to the next chaingun round, rocket
  int mGemScore = 0;           // Pilot Seat: points toward the next gem
  int mPopNext = 0, mTruckNext = 0, mPops = 0, mTrucks = 0;
  // Level 8: vines, rope bridges, ropes and their loads, fruit, Bounce House.
  std::vector<Vine> mVines;
  std::vector<Bridge> mBridges;
  std::vector<Load> mLoads;
  std::vector<JungleRope> mJRopes;
  std::vector<Fruit> mFruits;
  std::vector<CellBox> mWater; // shallow water: half speed, harmless
  bool mBounce = false;  // bonus rule: every surface is a trampoline
  int mBounceH = 8;      // cells: the next bounce's height
  int mBounceKick = 0;   // frames of a wall's bounce back left (sign: direction)
  // Level 9: plates, traps, collapsing floors, stone keys, the key door,
  // the secret walls, the drum egg and Trapmaster.
  std::vector<Plate> mPlates;
  std::vector<Trap> mTraps;
  std::vector<CollapseTile> mCollapse;
  std::vector<StoneKey> mStoneKeys;
  std::vector<KeyDoor> mKeyDoors;
  std::vector<SecretDoor> mSecretDoors;
  std::vector<int> mDrumPlates;  // the final hall's entry plates, in rhythm order
  std::vector<int> mDrumTaps;    // frames they were pressed, in order
  int mDrumEgg = 0;              // frames left of the traps playing drums
  bool mTrapmaster = false;      // bonus rule: you work the traps
  TrapCursor mCursor;
  int mWave = 0, mWaveNext = 0;  // Trapmaster: hunters sent so far and when the next comes
  int mIdolX = -1;               // Trapmaster: cells, where the hunters walk to
  // Level 10: sunbeams, mirrors, sun doors, the Lance's beam, the cursed
  // mirror and Negative Space.
  std::vector<SunBeam> mSunBeams;
  std::vector<Mirror> mMirrors;
  std::vector<SunDoor> mSunDoors;
  std::vector<BeamPath> mBeamPaths; // this frame's beams, for drawing and the moths
  std::vector<CursedMirror> mCursed;
  std::vector<std::array<int, 3>> mReflects; // shots a Monk sends back next frame: x, y, dir
  bool mLanceOn = false;            // the Sunstone Lance's beam is out
  int mLanceDir = 0;
  bool mNegative = false;           // bonus rule: sun and moon blocks swap every 75 frames
  // Level 11: tracks and carts, levers, bumpers, lanterns, the cursed veins,
  // Blasting Caps, the W box that comes back, Rocco's trapdoor, the foreman's
  // rubble, the DAYS WITHOUT ACCIDENT sign, and Pinball Mine.
  std::vector<Rail> mRails;
  std::vector<Cart> mCarts;
  std::vector<Lever> mLevers;
  std::vector<CellBox> mBumpers;
  std::vector<std::pair<int, int>> mLanterns; // cells: where each lantern's post stands
  std::vector<CellBox> mVeins;
  std::vector<Cap> mCaps;
  std::vector<std::array<int, 3>> mRespawns; // box index, frames, frames left
  std::vector<Trapdoor> mTrapdoors;
  std::vector<std::array<int, 4>> mRubble; // breakable, block x, y, placed
  CellBox mDaysSign{0, 0, 0, 0};
  int mDays = 41, mDaysFrames = 0;
  bool mPinball = false; // bonus rule: you are the ball
  Pinball mPin;
  // Level 12: lava pools, spear footholds, where lava pops you back to, the
  // wading shelf's clock, the marshmallow, and The Floor Is Lava (the last
  // head you bounced on).
  std::vector<Lava> mLavas;
  std::vector<Foothold> mFootholds;
  int mLavaSafeX = 0, mLavaSafeY = 0;
  int mLavaTicks = 0;
  int mRoast = 0;          // frames the marshmallow has been held to the hearth
  bool mOnStick = false;   // carrying the marshmallow
  bool mFloorLava = false; // bonus rule: every floor is lava, bounce on heads
  int mHeadX = 0, mHeadY = 0;
  int mHeadBounces = 0, mLavaPops = 0; // for the bot's look-ahead
  // Episode 7, DEEP SPACE.
  SpaceState mSpace;
  // Episode 3, STATION ZERO.
  StationState mStation;
  // Level 13: boulders, chutes, alcoves, the crack and the lead hatches;
  // frames the runner has stood still; the WRONG WAY sign.
  std::vector<Boulder> mBoulders;
  bool mSurfing = false; // bonus rule: ride the boulder
  // Golden Touch: per block 0 unmarked, 1 marked, 2 gilded, 3 a statue.
  bool mGolden = false;
  int mGoldGoal = 80; // percent of the marked blocks that opens the gate
  std::vector<std::uint8_t> mGold;
  int mGoldMarked = 0, mGoldDone = 0;
  std::vector<GoldDoor> mGoldDoors;
  bool mGateTold = false;
  // Level 14: Gold Fever (the greed meter and the altars), the coin heaps,
  // the glyph rows, the drummers' beat and Kaan-Tolok.
  bool mGreedOn = false;
  int mGreed = 0;
  int mOffered = 0;
  std::vector<Altar> mAltars;
  std::vector<CoinHeap> mCoinHeaps;
  std::vector<GlyphRow> mGlyphRows;
  std::vector<std::string> mSentinelIds;
  Golem mGolem;
  bool mBowFull = false; // the Jade Bow shot being fired is a full draw
  int mRefillX = -1, mRefillY = 0; // cells: the jade basin (`@ refill`), bottom-left
  bool mRefillUsed = false;        // this life
  Surf mSurf;
  std::vector<Chute> mChutes;
  std::vector<Alcove> mAlcoves;
  std::vector<Crack> mCracks;
  std::vector<LeadHatch> mLeadHatches;
  int mStill = 0;
  bool mWrongWay = false;
  std::vector<std::string> mAlcoveNames, mChuteNames, mChuteIds; // while linking
  std::vector<std::pair<int, int>> mPopups; // cells: where cardboard runners pop up
  int mStreetY = -1;           // cells: the street the trucks drive along
  CellBox mStash{0, 0, 0, 0};        // where Looters' takings end up
  int mManholeX = -1, mManholeY = -1; // cells: where Looters run off to
  bool mBreakdance = false;
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
  // Vehicles and deep water.
  std::vector<Vehicle> mVehicles;
  std::vector<Sea> mSeas;
  int mAir = kAirFrames;
  int mDrown = 0;          // frames since the last heart lost to drowning
  bool mUpBoards = false;  // up next to a vehicle boards it (not for the bot)
  bool mUpHeld = false;    // up was held last frame (boarding wants a fresh press)
  std::string mUseLabel = "USE";
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
  bool mGod = false; // the God Mode cheat
  std::string mMessage;
  int mMessageTicks = 0;
  int mFieldFlash = 0;
  int mTickCount = 0;
  std::array<Vec2, 5> mTrail{}; // recent player draw positions, for Turbo afterimages
};

} // namespace gr
