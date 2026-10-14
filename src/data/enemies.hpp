#pragma once

#include "render/color.hpp"

#include <string>

namespace gr
{

// What an enemy does each frame. Most of the campaign's enemies are one of
// these behaviours with their own numbers (SPEC.md section 5); the ones that
// need more get their own kind.
enum class EnemyKind
{
  Walker, // patrol: turns at walls and ledges
  Flyer,  // hover over the player's head and dive
  Turret, // static, aimed shots in 8 directions
  Camera, // the candid camera: a harmless shootable prop
  Crawler, // cling: creeps along walls and ceilings, spits sparks
  Rider,   // ride: sweeps along a rail (Squeegee Drone)
  Sniper,  // static: tracks you with a laser line, then fires along it
  Bouncer, // patrols his post; shots from the front only stagger him
  Disco,   // static: eight light shards on beat 1 of every bar
  Raver,   // static: lobs glowsticks that leave a puddle
  Stepper, // moves only on the beat (Step on the Beat's cardboard Bouncers)
  Stalker, // chases in the dark, freezes in any light; shots pass through it in the dark
  Looter,  // grabs a loose pick-up and runs off to the stash
  Leech,   // crawls along a power cable to pull its breaker
  Gator,   // swims under the sludge, lunges up out of it
  Keeper,  // walks to its valve and floods the zone; then guards it
  Hopper,  // leaps car to car toward you; its landing sends a shockwave along the roof
  RailDrone, // hovers alongside the train and drops caltrops
  Decoupler, // climbs out of a coupling and unhooks it
  Trooper,   // slides down a rope from the gunship, then patrols and shoots
  Biker,     // charges along its roof, turns at the end, revs and charges back
  Shield,    // patrols behind a riot shield that stops shots from the front
  Howler,    // sits on a branch and lobs fruit that rolls along the ground
  Viper,     // coiled on a branch; drops to hang and strikes at a runner below
  Cutter,    // runs to its rope bridge's far anchor and chops it down
  Guardian,  // patrols (or stands sentry), turns to face you, swings a club; shots from the front spark off
  DartFace,  // a face in the ceiling that drops a dart on its own rhythm
  Scarabs,   // a carpet of beetles flowing along its floor toward you
  Hunter,    // Trapmaster's cultists: walk to the idol, take a coin, leave
  Wraith,    // drifts through walls toward you; only light hurts it
  Monk,      // patrols behind a mirror shield: sends shots and sunbeams back
  Moth,      // drifts to the nearest sunbeam; a few of them block it
  Bandit,    // rides its own rail level with you and shoots from the cart
  Bat,       // one of a Bat Cloud: swarms along its tunnel on a sine path
  Mole,      // burrows through rock, surfaces near you and lobs a rock
  Toad,      // leaps out of the lava onto your stone, sits, dives back
  Wisp,      // drifts toward you; swells and bursts when close
  Crab,      // patrols its stone; armored top and front, flips when hit from behind
  // Episode 7, DEEP SPACE (world_space.cpp).
  Skitter,   // scuttles along its floor and up goo; clicks, then leaps at you
  Spitpod,   // a rooted plant: its bulb swells, then it lobs acid in an arc
  Gloop,     // hops toward you; splits in two when killed and splats goo on a wall
  Mite,      // a small fast biter out of a wall pore, in threes (level 45, world_hive.cpp)
  Polyp,     // a mouth in the wall that breathes in, pulling you toward it, and nips
  Warden,    // an armoured flyer patrolling its wing that calls mites down
  VoidRay,   // level 43: a glowing manta that sweeps across in a wave; its fins light up, then it dives (world_starfall.cpp)
  RockLeech, // level 43: clings to an asteroid and spits a slow glob at you as you pass
  Blinker,   // level 46: blinks to a spot near you (a shimmer shows where first), then lunges (world_crystal.cpp)
  ShardGolem, // level 46: a slow armoured plodder; shots only hurt it in its glowing back
  PrismBat,  // level 46: flutters in a figure eight; a shot through it splits in three
  LoomSpider, // level 47: walks its Silk Line; cuts it under you after a tell, spins it again (world_silk.cpp)
  CocoonPod, // level 47: hangs on a web; drops its Dropling on you as you pass under
  Dropling,  // level 47: falls out of its pod on a thread, bites, climbs back up
  ThornHog,  // level 48: paces the plains, lowers its tusks and charges (world_plains.cpp)
  SkyGulper, // level 48: a floating mouth; swallows a runner on foot and spits them back
  Thornbush, // level 48: a bush in the grass; carrier=1 is the green one with the Virus
  EggGuard,  // level 49: an egg that hatches as you come near, then charges you (world_mother.cpp)
  SporeNurse, // level 49: drifts about; heals the Hive Mother until it is shot
  SpearRunner, // flees ahead of you, turns once to throw a spear back
  PitSnake,  // waits in a floor hole, rears and strikes when you come close
  Totem,     // a stack of spitting heads; a solid column that shrinks as heads go
  Drummer,   // drums on a dais: enemies near it move faster while it plays
  CoinBeetle, // crawls at you out of a coin heap, hops; drops two gems
  Sentinel,  // a glyph carved in the wall: its row of glyphs lights up, then beams
  // Episode 3, STATION ZERO (world_station.cpp).
  Loader,    // a Loader Mech: patrols on magnetic feet, throws crates from a pile
  WeldDrone, // crawls its floor (or up a wall) in runs, leaving a hot seam
  Tether,    // one of a Tether Pair: two drones with a beam strung between them
  Puck,      // a Puck Drone: slides along the floor in straight lines, bouncing
  SleeperPod, // a cryo pod in the back wall: lets a mutant out as you come near
  Mutant,    // out of a Sleeper Pod: staggers after you, lunges to grab
  LabArm,    // a claw on a ceiling rail: drops on you, lifts you and lets go
  Puffer,    // a Spore Puffer: a bulb on a floor or ceiling that puffs spore clouds
  Snapjaw,   // a plant mouth on a floor, wall or ceiling: bites what comes close
  Glob,      // hops at you; splits in two when it dies (big, medium, small)
  Barnacle,  // clings to the hull; a ring of six spikes when you come close
  EvaRam,    // hovers to your row, then rams along it
  Mites,     // a swarm of five Rivet Mites (one enemy, world_hull.cpp moves them)
  Fish,      // deep water: patrols, then chases whatever is in the water with it
  Jelly,     // deep water: pulses up and drifts down, stings on contact
  SeaMine,   // deep water: bobs on its chain, blows up when something comes close
  Angler,    // deep water: waits behind its lure, then lunges
  // Level 19, Gravity Lab (world_grav.cpp).
  FlipWalker,  // patrols on its chamber's down; falls when the chamber turns over
  Probe,       // a Gravity Probe: hovers its distance, aimed shots, a field that pulls you in
  TestSubject, // chases on its chamber's down; crouches and copies your jumps
  // Level 20, Reactor Core (world_reactor.cpp).
  Spark,       // a Conduit Spark: rides its wire, never leaves it
  ShieldDrone, // hovers over its group; nothing in its bubble can be hurt
  Imp,         // an Isotope Imp: scuttles after you, faster after every pulse; lunges
  // Level 21, ZERO (world_zero.cpp).
  LatticeTurret, // rides its ceiling rail above you and fires straight down
  RepairSwarm,   // a cloud of nanobots: hovers to what was broken and builds it back
  Echo,          // a hologram replaying your own moves and shots from a while ago
  // Level 22, Dry Gulch (world_west.cpp).
  Duelist,       // stands and faces you; the bell rings twice, then he draws
  TumbleMine,    // rolls with the wind, rolls over short gaps, blows up on contact
  WindowBandit,  // pops up in a window to fire, ducked he can't be hit
  PortraitGhost, // leaves its portrait, chases through walls, lunges; pinned it is a platform
  HauntedArmor,  // wakes as you pass, walks after you, swings its halberd
  Poltergeist,   // lives in a glowing object and throws things at you
  // Level 24, Too Many Cooks (world_kitchen.cpp).
  Meatball,      // rolls along belts, grows in sauce, bounces small runners back
  CleaverBot,    // stands on a rim and lobs cleavers that stick in walls
  EggTimer,      // walks at you and pops when its dial runs out
  // Level 25, Tiny Kingdom (world_garden.cpp).
  SoldierAnt,       // marches in a column; hit or touched, the column turns on you
  JumpingSpider,    // stalks at a distance, crouches, leaps where you stood
  BombardierBeetle, // patrols; a boiling jet out of its back at a runner behind it
  // Level 26, Kaiju Tonight (world_city.cpp).
  ModelTank,  // patrols in a squad; the squad's tanks fire up at 45 degrees in turn
  ToyJet,     // strafes in a formation of three, dropping a shot over the runner
  MaserTruck, // patrols; a ring grows on its dish, then a beam along its row
  // Level 27, The Long Rain (world_noir.cpp).
  Flatfoot,   // patrols with a flashlight; spots a runner who is not hidden and blows the whistle
  Goon,       // comes out of a door during an alarm, chases, fires a three-shot burst
  Crow,       // sits on a ledge; caws (an alarm) at a runner who is not hidden close by
  GrinningBomb, // chases; close by it frowns, lunges and goes off on contact
  PianoBird,  // flies east along a row and drops a piano where the runner will be
  RubberGoon, // patrols; punches with an arm that stretches; squashed flat by every hit
  StaticSwarm, // drifts at the runner through walls; latches on (a carrier infects)
  TestCard,   // hovers and turns; a quadrant beams at the runner; only the signal's colour is hurt
  GhostImage, // patrols with two after-images; a tinted halo only the matching tint gets through
  Anglerfish, // waits in the dark behind its lure; lunges at a runner close by
  BoxJelly,   // rises up its column, holds, sinks back; drifts with currents
  CableEel,   // lunges out of its junction box at a runner close by
  // Level 31, Storm Relay (world_storm.cpp).
  StormGull,  // wheels high over the runner, screams, then a V-dive through them
  KiteDrone,  // flies a figure eight on its tether; turns at a runner close by
  Foghorn,    // static: a cone of sound that blows a runner back
  SandShark,  // swims under `@ sand` (only its fin shows), breaches and bites
  VultureDrone, // circles high over its stretch and drops scrap on the runner
  ScorpionMech, // patrols; its tail's aim line turns toward the runner, locks, and a beam fires
  Firewall,   // a wall of flame sweeping its floor; it dips at each port (only Heat Sink fireballs hurt it)
  LavaDaemon, // leaps between perches on its floor and lobs fireballs
  FanBot,     // on the shaft wall: blows gusts across; its blades turn shots from the front
  SkyPirate,  // Level 34: jets in on a trigger, lands on your ship: cutlass and pistol
  PeckParrot, // Level 34: sits on a balloon and sinks the ship; shooed, it dives
  BalloonMine, // Level 34: rises from the clouds; close by or shot, it bursts
  MastClimber, // Level 35: climbs ladders after you, swings a wrench
  RelayDrone,  // Level 35: circles the mast, fires at you on its front half
  FeedbackNode, // Level 35: bolted to the mast, glows, then pushes you along
  Forklift,     // Level 36: patrols, then charges; lifts the runner on its forks and carries them off
  Stagehand,    // Level 36: carries a flat, sets it down in front of you, shoots from behind it
  Mimic,        // Level 36: a prop box that bites (or infects) when you come close
  Mannequin,    // Level 37: creeps up only while you look away
  PowderPuffer, // Level 37: hovers, puffs a powder cloud in front of it
  RackRunner,   // Level 37: a costume rack on casters, rolls its aisle end to end
  Typo,         // Level 38: crawls out of an inkwell, rewrites struck words
  Deadline,     // Level 38: a clock on legs, faster every lap of its minute hand
  WritersBlock, // Level 38: slides into your way and sits; only striking its word kills it
  SketchGuard,  // Level 38: a pencil sentry with a pencil rifle
  FloorManager, // Level 39: counts 3-2-1, then his whole room fires
  CableWrangler, // Level 39: drags a cable that trips the runner
  BoomOp,       // Level 39: sweeps a boom mic over its head
  Followspot,   // Level 40: a stage light whose beam follows the runner
  Rigger,       // Level 40: a sniper on a catwalk, aiming where the light is
  SandbagDrop,  // Level 40: cuts sandbags down onto the runner
};

enum EnemyFlag : unsigned
{
  kEnemyHarmless = 1u << 0,  // no contact damage
  kEnemyBeatDive = 1u << 1,  // flyer that dives only on beat 1, once per bar
  kEnemyNoTally = 1u << 2,   // not counted for "all enemies destroyed"
  kEnemyCarrier = 1u << 3,   // attacks infect with the Virus instead of hurting
  kEnemyRobot = 1u << 4,     // the Signal Jammer can turn it
};

// Which baked art an enemy uses.
enum class EnemyLook
{
  Walker,
  Flyer,
  Turret,
  Camera,
  Styled, // drawn by its own art routine (assets/enemy_art.cpp, keyed by `key`)
};

struct EnemyDef
{
  const char* key;
  const char* name;
  EnemyKind kind;
  EnemyLook look;
  int w, h; // cells
  int hp;
  int score;
  int stepEvery; // frames per cell moved (walkers 2 = 1/2 cell per frame)
  int cooldown;  // frames between attacks
  int tell;      // telegraph frames before an attack
  int range;     // turrets: horizontal reach in cells
  unsigned flags;
  Color tint; // 0 = the theme's enemy colours
  int weight = 1; // on weighted platforms (level 2's pulleys)
};

// Index into the enemy table, -1 if unknown.
int enemyIndex(const std::string& key);
const EnemyDef& enemyDef(int index);
int enemyDefCount();

} // namespace gr
