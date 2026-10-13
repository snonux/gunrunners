#include "data/enemies.hpp"

#include <array>

namespace gr
{

namespace
{

using K = EnemyKind;
using L = EnemyLook;

const EnemyDef kEnemies[] = {
  // The PoC's classic three (map characters w, f, t).
  {"walker", "WALKER BOT", K::Walker, L::Walker, 3, 3, 3, 250, 2, 0, 0, 0, 0, 0},
  {"flyer", "FLYER DRONE", K::Flyer, L::Flyer, 3, 3, 2, 500, 1, 24, 0, 0, 0, 0},
  {"turret", "TURRET", K::Turret, L::Turret, 3, 2, 4, 1000, 0, 20, 0, 22, 0, 0},
  {"candid_camera", "CANDID CAMERA", K::Camera, L::Camera, 2, 2, 1, 5000, 0, 0, 0, 0,
   kEnemyHarmless | kEnemyNoTally, 0},

  // Episode 1.
  {"chrome_cop", "CHROME COP", K::Walker, L::Walker, 3, 3, 3, 250, 2, 0, 0, 0, 0, 0, 3},
  {"hover_lens", "HOVER LENS", K::Flyer, L::Flyer, 3, 3, 2, 500, 1, 30, 8, 0, kEnemyBeatDive, 0},
  {"roof_turret", "ROOF TURRET", K::Turret, L::Turret, 3, 2, 4, 1000, 0, 30, 8, 22, 0, 0},
  // Level 2. Crawler: range is how far its spark flies. Sniper: range is
  // how many rows below it it wakes; tell is the tracking time (it then
  // holds still 8 frames).
  {"glass_crawler", "GLASS CRAWLER", K::Crawler, L::Styled, 3, 2, 2, 300, 2, 30, 10, 6, 0, 0, 0},
  {"squeegee_drone", "SQUEEGEE DRONE", K::Rider, L::Styled, 4, 4, 5, 400, 1, 90, 15, 0, kEnemyHarmless, 0, 0},
  {"penthouse_sniper", "PENTHOUSE SNIPER", K::Sniper, L::Styled, 3, 4, 4, 1500, 0, 45, 15, 14, 0, 0},
  // Level 3. Bouncer: range is how far either side of his post he guards.
  // Raver: range is how far he throws.
  {"bouncer", "BOUNCER", K::Bouncer, L::Styled, 4, 6, 8, 800, 2, 30, 10, 6, 0, 0, 2},
  {"disco_drone", "DISCO DRONE", K::Disco, L::Styled, 3, 3, 4, 600, 0, 30, 8, 0, 0, 0},
  {"glow_raver", "GLOW RAVER", K::Raver, L::Styled, 3, 4, 2, 350, 0, 30, 15, 20, 0, 0},
  {"cardboard_bouncer", "CARDBOARD BOUNCER", K::Stepper, L::Styled, 4, 6, 2, 300, 0, 0, 0, 0, 0, 0},
  // Level 4. Stalker: range is the lunge in cells. Looter: range is how far
  // (cells) it spots a pick-up.
  {"night_stalker", "NIGHT STALKER", K::Stalker, L::Styled, 3, 5, 4, 700, 1, 30, 8, 4, 0, 0},
  {"looter", "LOOTER", K::Looter, L::Styled, 3, 4, 2, 400, 1, 0, 0, 24, kEnemyHarmless, 0},
  {"grid_leech", "GRID LEECH", K::Leech, L::Styled, 2, 2, 3, 500, 2, 0, 8, 0, kEnemyCarrier, 0},
  // Level 5. Gator: range is how close (cells) a runner must be for a
  // lunge. Rats come out of rat pipes and are not part of the tally.
  {"sludge_gator", "SLUDGE GATOR", K::Gator, L::Styled, 4, 2, 3, 600, 2, 45, 15, 6, 0, 0},
  {"pipe_rat", "PIPE RAT", K::Walker, L::Styled, 2, 1, 1, 100, 1, 0, 0, 0, kEnemyNoTally, 0},
  {"valve_keeper", "VALVE KEEPER", K::Keeper, L::Styled, 3, 4, 10, 900, 2, 0, 30, 0, 0, 0, 3},
  // Level 6: Maglev Express.
  {"track_hopper", "TRACK HOPPER", K::Hopper, L::Styled, 3, 4, 4, 500, 2, 45, 10, 12, 0, 0},
  {"rail_drone", "RAIL DRONE", K::RailDrone, L::Styled, 4, 2, 3, 600, 1, 45, 10, 0, 0, 0},
  {"decoupler", "DECOUPLER", K::Decoupler, L::Styled, 3, 4, 6, 800, 0, 75, 15, 24, 0, 0},
  // Level 7: Chopper Down. Troopers: range is how far they see you (cells).
  {"rappel_trooper", "RAPPEL TROOPER", K::Trooper, L::Styled, 3, 5, 3, 500, 2, 30, 10, 20, 0, 0, 2},
  {"hover_biker", "HOVER BIKER", K::Biker, L::Styled, 5, 3, 5, 700, 1, 45, 10, 0, 0, 0, 3},
  {"shield_trooper", "SHIELD TROOPER", K::Shield, L::Styled, 3, 5, 6, 900, 2, 40, 10, 20, 0, 0, 2},
  // Pilot Seat's cardboard targets: they only run along their roofs.
  {"cardboard_runner", "CARDBOARD RUNNER", K::Walker, L::Styled, 3, 5, 2, 500, 1, 0, 0, 0, kEnemyHarmless | kEnemyNoTally, 0},
  {"cardboard_truck", "CARDBOARD TRUCK", K::Walker, L::Styled, 8, 4, 6, 1500, 1, 0, 0, 0, kEnemyHarmless | kEnemyNoTally, 0},
  // Level 8: Canopy Road. Howler: range is how far (cells) it throws.
  // Viper: range is how far to the side (cells) it notices a runner below.
  {"howler", "HOWLER", K::Howler, L::Styled, 3, 3, 2, 300, 0, 40, 12, 24, 0, 0},
  {"viper", "CANOPY VIPER", K::Viper, L::Styled, 2, 2, 2, 450, 0, 45, 12, 4, 0, 0},
  {"cutter", "BRIDGE CUTTER", K::Cutter, L::Styled, 3, 5, 3, 600, 1, 30, 10, 20, 0, 0, 2},
  // Level 9: Hall of Traps. Guardian: range is how far (cells) it notices
  // you on its floor. Dart Face: cooldown is its rhythm. Scarabs: hp is the
  // beetle count (set from count=), score is per beetle.
  {"guardian", "STONE GUARDIAN", K::Guardian, L::Styled, 4, 6, 6, 1200, 4, 40, 14, 24, 0, 0, 3},
  {"dartface", "DART FACE", K::DartFace, L::Styled, 2, 2, 4, 500, 0, 30, 10, 0, 0, 0},
  {"scarabs", "SCARAB TIDE", K::Scarabs, L::Styled, 8, 1, 8, 50, 2, 15, 0, 0, 0, 0},
  {"treasure_hunter", "TREASURE HUNTER", K::Hunter, L::Styled, 3, 5, 2, 200, 2, 0, 0, 0, kEnemyHarmless, 0},
  {"wraith", "SHADE WRAITH", K::Wraith, L::Styled, 3, 5, 6, 800, 4, 30, 8, 0, 0, 0, 0},
  {"monk", "MIRROR MONK", K::Monk, L::Styled, 3, 5, 4, 700, 2, 30, 10, 2, 0, 0},
  {"moth", "SUN MOTH", K::Moth, L::Styled, 2, 2, 1, 100, 2, 0, 0, 24, kEnemyHarmless | kEnemyNoTally, 0, 0},
  // Level 11: Idol Mines. Bandit: rides its rail (4 x 5 with the cart). Bat:
  // one of a cloud. Mole: range is how far (cells) from its home it surfaces.
  {"cartbandit", "CART BANDIT", K::Bandit, L::Styled, 4, 5, 6, 800, 1, 30, 10, 0, 0, 0},
  {"bat", "CAVE BAT", K::Bat, L::Styled, 2, 2, 1, 40, 1, 0, 0, 0, kEnemyNoTally, 0, 0},
  {"mole", "ROCK MOLE", K::Mole, L::Styled, 3, 3, 3, 500, 2, 45, 15, 16, 0, 0},
  {"toad", "MAGMA TOAD", K::Toad, L::Styled, 3, 3, 3, 400, 1, 60, 15, 20, 0, 0},
  {"wisp", "EMBER WISP", K::Wisp, L::Styled, 2, 2, 2, 300, 4, 20, 15, 4, kEnemyHarmless, 0},
  {"crab", "BASALT CRAB", K::Crab, L::Styled, 4, 3, 6, 900, 4, 30, 10, 3, 0, 0},
  // Level 44: Crash Garden. Skitter: range is how far ahead (cells) it
  // notices you before the leap. Spitpod: range is how far it lobs. Gloop:
  // range is how close you must be before it hops after you; the small ones
  // are its halves.
  {"skitter", "SKITTER", K::Skitter, L::Styled, 3, 2, 2, 300, 1, 30, 8, 14, 0, 0},
  {"spitpod", "SPITPOD", K::Spitpod, L::Styled, 3, 4, 3, 400, 0, 45, 12, 22, 0, 0},
  {"gloop", "GLOOP", K::Gloop, L::Styled, 3, 3, 2, 300, 1, 18, 8, 18, 0, 0},
  {"gloop_small", "GLOOPLET", K::Gloop, L::Styled, 2, 2, 1, 100, 1, 14, 6, 18, kEnemyNoTally, 0},
  // Level 45: Hive Gullets. Hive Mite: range is how far (cells) it notices
  // you. Polyp: range is how far in front it pulls. Drone Warden: range is
  // how far either side of home it patrols.
  {"hive_mite", "HIVE MITE", K::Mite, L::Styled, 2, 2, 1, 100, 1, 0, 0, 40, kEnemyNoTally, 0},
  {"polyp", "POLYP", K::Polyp, L::Styled, 4, 4, 4, 500, 0, 75, 12, 16, 0, 0},
  {"drone_warden", "DRONE WARDEN", K::Warden, L::Styled, 4, 3, 8, 1500, 2, 160, 14, 18, 0, 0},
  // Level 43: Starfall. Void Ray: range is how close (cells) it gets before
  // it lights its fins and dives. Rock Leech: range is how far it spits.
  {"void_ray", "VOID RAY", K::VoidRay, L::Styled, 4, 2, 3, 800, 0, 40, 10, 18, 0, 0},
  {"rock_leech", "ROCK LEECH", K::RockLeech, L::Styled, 2, 2, 2, 500, 0, 60, 12, 30, 0, 0},
  // Level 46: Crystal Drift. Blinker: range is how far (cells) it notices
  // you; Shard Golem: stepEvery is its plod; Prism Bat: range is how wide
  // its figure eight is.
  {"blinker", "BLINKER", K::Blinker, L::Styled, 3, 4, 3, 700, 3, 50, 12, 22, 0, 0},
  {"shard_golem", "SHARD GOLEM", K::ShardGolem, L::Styled, 4, 5, 5, 1200, 4, 0, 0, 0, 0, 0},
  {"prism_bat", "PRISM BAT", K::PrismBat, L::Styled, 3, 2, 2, 400, 0, 0, 0, 7, 0, 0},
  // Level 47: Silk Canyon. Loom Spider: range is how far (cells) down its
  // line from the top it walks, tell its warning before a cut. Cocoon Pod:
  // range is how far (cells) to either side of it you set it off.
  {"loom_spider", "LOOM SPIDER", K::LoomSpider, L::Styled, 3, 3, 3, 600, 2, 0, 8, 10, 0, 0},
  {"cocoon_pod", "COCOON POD", K::CocoonPod, L::Styled, 2, 3, 3, 300, 0, 30, 0, 3, 0, 0},
  {"dropling", "DROPLING", K::Dropling, L::Styled, 2, 2, 1, 100, 0, 0, 8, 0, kEnemyNoTally, 0},
  // Level 48: Bounder Plains. Thorn Hog: range is how near (cells, on its
  // floor) sets it charging. Sky Gulper: range is how far (cells) it sways
  // to either side of where it starts; it hurts no one, it swallows.
  {"thorn_hog", "THORN HOG", K::ThornHog, L::Styled, 4, 3, 3, 400, 2, 30, 10, 16, 0, 0},
  {"sky_gulper", "SKY GULPER", K::SkyGulper, L::Styled, 4, 4, 5, 700, 0, 40, 0, 10, kEnemyHarmless, 0},
  // Level 49: The Hive Mother. Egg Guard: an egg (hp 2) until you come
  // within range cells (or the tell runs out), then a guard that charges
  // you along its floor. Spore Nurse: heals the Hive Mother every cooldown
  // frames while it lives.
  {"egg_guard", "EGG GUARD", K::EggGuard, L::Styled, 3, 3, 3, 300, 2, 20, 60, 14, 0, 0},
  {"spore_nurse", "SPORE NURSE", K::SporeNurse, L::Styled, 3, 3, 2, 400, 0, 180, 0, 8, kEnemyHarmless, 0},
  // Her brood in the fight: the same, laid or called by her (no tally).
  {"brood_egg", "EGG GUARD", K::EggGuard, L::Styled, 3, 3, 3, 100, 2, 20, 60, 14, kEnemyNoTally, 0},
  {"brood_nurse", "SPORE NURSE", K::SporeNurse, L::Styled, 3, 3, 2, 200, 0, 300, 0, 8, kEnemyHarmless | kEnemyNoTally, 0},
  {"thornbush", "THORNBUSH", K::Thornbush, L::Styled, 3, 2, 1, 50, 0, 0, 0, 0, kEnemyNoTally, 0},
  // Level 13. Spear Runner: range is how close (cells) behind it you get
  // before it turns to throw. Pit Snake: range is how near (cells) wakes it.
  // Totem Stack: hp is 2 per head, score per head (each head scores as it goes).
  {"spearrunner", "SPEAR RUNNER", K::SpearRunner, L::Styled, 3, 5, 2, 400, 4, 0, 10, 12, 0, 0},
  {"pitsnake", "PIT SNAKE", K::PitSnake, L::Styled, 2, 2, 2, 300, 0, 45, 12, 6, 0, 0},
  {"cultist", "CULTIST", K::Walker, L::Styled, 3, 5, 1, 100, 3, 0, 0, 0, kEnemyHarmless, 0},
  {"drummer", "WAR DRUMMER", K::Drummer, L::Styled, 4, 5, 5, 700, 0, 0, 0, 0, 0, 0},
  {"coinbeetle", "COIN BEETLE", K::CoinBeetle, L::Styled, 2, 2, 1, 200, 1, 30, 8, 0, kEnemyNoTally, 0},
  {"templecat", "TEMPLE CAT", K::Walker, L::Styled, 4, 4, 1, 100, 2, 0, 0, 0, kEnemyHarmless | kEnemyNoTally, 0},
  // Level 15. Loader Mech: range is how far (cells) it throws; hp is the
  // body's (its legs take 4 more). Weld Drone: cooldown is the pause
  // between runs, tell the torch's flare. Tether Pair: tell is the
  // survivor's wobble before each ram.
  {"loader_mech", "LOADER MECH", K::Loader, L::Styled, 6, 8, 8, 1200, 4, 40, 12, 24, 0, 0},
  {"weld_drone", "WELD DRONE", K::WeldDrone, L::Styled, 2, 2, 2, 400, 1, 15, 8, 0, 0, 0},
  {"tether_pair", "TETHER DRONE", K::Tether, L::Styled, 2, 2, 2, 500, 2, 0, 8, 0, 0, 0},
  // Level 16. Puck Drone: cooldown is its rest after 3 bounces, tell the
  // spin-up. Sleeper Pod: range is how close (cells) the runner comes before
  // its frost melts (over `tell` frames). Mutant: range is its lunge
  // (cells). Lab Arm: stepEvery is its ride along the rail, tell the red
  // lamp before it drops.
  {"puck_drone", "PUCK DRONE", K::Puck, L::Styled, 4, 2, 2, 400, 1, 30, 10, 0, 0, 0},
  {"sleeper_pod", "SLEEPER POD", K::SleeperPod, L::Styled, 4, 6, 99, 0, 0, 0, 15, 8, kEnemyHarmless | kEnemyNoTally, 0},
  {"pod_mutant", "POD MUTANT", K::Mutant, L::Styled, 3, 5, 4, 600, 3, 30, 10, 4, 0, 0},
  {"lab_arm", "LAB ARM", K::LabArm, L::Styled, 4, 4, 6, 800, 2, 45, 12, 0, kEnemyHarmless, 0},
  // Level 17. Spore Puffer: cooldown between puffs, tell the bulb swelling.
  // Snapjaw: range is how close (cells) wakes it. Glob: stepEvery its hop's
  // frames per cell, cooldown its rest between hops, tell the squash.
  {"spore_puffer", "SPORE PUFFER", K::Puffer, L::Styled, 4, 4, 4, 400, 0, 30, 10, 0, 0, 0},
  {"snapjaw", "SNAPJAW", K::Snapjaw, L::Styled, 4, 4, 3, 500, 0, 20, 10, 4, 0, 0},
  {"glob", "GLOB", K::Glob, L::Styled, 4, 4, 2, 300, 2, 20, 8, 0, 0, 0},
  {"glob_medium", "GLOB", K::Glob, L::Styled, 3, 3, 2, 150, 2, 20, 8, 0, 0, 0},
  {"glob_small", "GLOB", K::Glob, L::Styled, 2, 2, 1, 75, 2, 20, 8, 0, 0, 0},
  // Level 18. Space Barnacle: range is how close (cells) opens it. EVA Ram:
  // stepEvery its drift to your row, range its ram (cells). Rivet Mites: one
  // swarm of five (hp), range how close a mite hops at you.
  {"space_barnacle", "SPACE BARNACLE", K::Barnacle, L::Styled, 4, 3, 4, 500, 0, 45, 12, 8, 0, 0},
  {"eva_ram", "EVA RAM", K::EvaRam, L::Styled, 3, 3, 4, 600, 2, 45, 15, 24, 0, 0},
  {"rivet_mites", "RIVET MITES", K::Mites, L::Styled, 2, 1, 5, 100, 2, 30, 10, 2, kEnemyHarmless, 0},
  {"sentinel", "GLYPH SENTINEL", K::Sentinel, L::Styled, 4, 4, 6, 600, 0, 45, 24, 0, kEnemyHarmless, 0},
  {"totem", "TOTEM STACK", K::Totem, L::Styled, 2, 8, 8, 250, 0, 45, 10, 0, 0, 0},
  // Deep water (world_sea.cpp). Fish and Angler: range is how close (cells)
  // something must come before they go for it. Sea Mine: range is how close
  // sets it off.
  {"piranha", "PIRANHA", K::Fish, L::Styled, 3, 2, 2, 300, 2, 20, 0, 18, 0, 0},
  {"jellyfish", "JELLYFISH", K::Jelly, L::Styled, 3, 3, 3, 400, 2, 0, 0, 0, 0, 0},
  {"sea_mine", "SEA MINE", K::SeaMine, L::Styled, 2, 2, 1, 200, 0, 0, 6, 4, kEnemyHarmless, 0},
  {"angler", "ANGLERFISH", K::Angler, L::Styled, 6, 4, 12, 2000, 1, 30, 10, 22, 0, 0},
  // Level 19. Flip Walker: tell the turn to face you before it walks at you.
  // Gravity Probe: stepEvery its hover, cooldown between shots, tell the core
  // glowing, range its field (cells). Test Subject: tell the crouch before a
  // copied jump.
  {"flip_walker", "FLIP WALKER", K::FlipWalker, L::Styled, 3, 4, 4, 500, 2, 0, 8, 0, 0, 0},
  {"gravity_probe", "GRAVITY PROBE", K::Probe, L::Styled, 4, 4, 5, 700, 4, 30, 12, 16, 0, 0},
  {"test_subject", "TEST SUBJECT", K::TestSubject, L::Styled, 3, 5, 4, 600, 2, 0, 8, 0, 0, 0},
  // Level 20. Conduit Spark: rides its wire (world_reactor.cpp sets the
  // speed). Shield Drone: stepEvery its hover. Isotope Imp: cooldown between
  // lunges, tell its glow before one.
  {"conduit_spark", "CONDUIT SPARK", K::Spark, L::Styled, 2, 2, 2, 300, 1, 0, 0, 0, 0, 0},
  {"shield_drone", "SHIELD DRONE", K::ShieldDrone, L::Styled, 4, 3, 6, 800, 2, 0, 0, 0, kEnemyHarmless | kEnemyRobot, 0},
  {"isotope_imp", "ISOTOPE IMP", K::Imp, L::Styled, 3, 2, 3, 500, 2, 20, 10, 0, 0, 0},
  // Level 21. Lattice Turret: stepEvery along its rail, cooldown between
  // shots down, tell the barrel glowing. Repair Swarm: stepEvery its drift.
  // Echo: replays the runner (world_zero.cpp moves it).
  {"lattice_turret", "LATTICE TURRET", K::LatticeTurret, L::Styled, 4, 3, 6, 800, 2, 30, 10, 0, kEnemyRobot, 0},
  {"repair_swarm", "REPAIR SWARM", K::RepairSwarm, L::Styled, 3, 3, 3, 400, 2, 0, 0, 0, kEnemyRobot, 0},
  {"echo", "ECHO", K::Echo, L::Styled, 3, 5, 4, 1000, 1, 0, 8, 0, kEnemyNoTally | kEnemyHarmless, 0},
  // Level 22. Duelist: cooldown from his shot to his next, tell the draw
  // glint. Tumble Mine: stepEvery its roll (1 a frame in a gust); it
  // explodes itself on contact. Window Bandit: cooldown his cycle.
  {"duelist", "DUELIST", K::Duelist, L::Styled, 3, 5, 4, 1000, 0, 45, 8, 0, 0, 0},
  {"tumble_mine", "TUMBLE MINE", K::TumbleMine, L::Styled, 4, 4, 1, 300, 2, 0, 8, 0, kEnemyHarmless, 0},
  {"window_bandit", "WINDOW BANDIT", K::WindowBandit, L::Styled, 3, 3, 2, 400, 0, 40, 12, 0, 0, 0},
  // Level 23. Portrait Ghost: cooldown between lunges (they come back, so
  // they are not counted); Haunted Armor: between swings (only the swing
  // hurts); Poltergeist:
  // between throws, range in cells.
  {"portrait_ghost", "PORTRAIT GHOST", K::PortraitGhost, L::Styled, 4, 4, 3, 600, 2, 40, 10, 0, kEnemyNoTally, 0},
  {"haunted_armor", "HAUNTED ARMOR", K::HauntedArmor, L::Styled, 3, 6, 8, 900, 2, 36, 12, 0, kEnemyHarmless, 0},
  {"poltergeist", "POLTERGEIST", K::Poltergeist, L::Styled, 2, 2, 3, 700, 0, 30, 12, 16, kEnemyHarmless, 0},
  // Level 24. Meatball: stepEvery its roll (contact is world_kitchen.cpp's:
  // a small one bounces the runner back, bigger ones hurt); Cleaver Bot:
  // cooldown between lobs, tell its raised arm; Egg Timer: its walk (it
  // only hurts when it pops).
  {"meatball", "MEATBALL", K::Meatball, L::Styled, 3, 3, 2, 300, 2, 0, 0, 0, kEnemyHarmless, 0},
  {"cleaver_bot", "CLEAVER BOT", K::CleaverBot, L::Styled, 3, 5, 5, 700, 0, 36, 12, 24, 0, 0},
  {"egg_timer", "EGG TIMER", K::EggTimer, L::Styled, 3, 3, 4, 500, 2, 0, 12, 0, kEnemyHarmless, 0},
  // Level 25. Soldier Ant: harmless until its column turns (the bite is
  // world_garden.cpp's); Jumping Spider: cooldown after a leap, tell its
  // crouch (only its leap hurts); Bombardier Beetle: only its jet hurts, tell the swelling.
  {"soldier_ant", "SOLDIER ANT", K::SoldierAnt, L::Styled, 3, 2, 1, 150, 2, 30, 10, 0, kEnemyHarmless, 0},
  {"jumping_spider", "JUMPING SPIDER", K::JumpingSpider, L::Styled, 4, 3, 4, 600, 2, 40, 15, 0, kEnemyHarmless, 0},
  {"bombardier_beetle", "BOMBARDIER BEETLE", K::BombardierBeetle, L::Styled, 4, 3, 5, 500, 4, 30, 12, 0, kEnemyHarmless, 0},
  {"model_tank", "MODEL TANK", K::ModelTank, L::Styled, 3, 2, 2, 200, 2, 30, 10, 0, kEnemyHarmless, 0},
  {"toy_jet", "TOY JET", K::ToyJet, L::Styled, 3, 2, 1, 300, 1, 45, 30, 0, kEnemyHarmless | kEnemyNoTally, 0},
  {"maser_truck", "MASER TRUCK", K::MaserTruck, L::Styled, 6, 4, 10, 1200, 4, 45, 22, 0, kEnemyHarmless, 0},
  // Level 27, The Long Rain (world_noir.cpp). Flatfoot: tell is the "?"
  // filling, cooldown the wait before he may whistle again after an alarm.
  // Goon: tell the tommy gun's click, cooldown between bursts (shared).
  // Crow: tell the head turn before a caw.
  {"flatfoot", "FLATFOOT", K::Flatfoot, L::Styled, 3, 5, 3, 500, 2, 45, 8, 0, 0, 0},
  {"goon", "GOON", K::Goon, L::Styled, 3, 5, 3, 400, 2, 45, 12, 0, kEnemyNoTally, 0},
  {"crow", "CROW", K::Crow, L::Styled, 2, 2, 1, 100, 0, 0, 10, 0, kEnemyHarmless, 0},
  {"grinning_bomb", "GRINNING BOMB", K::GrinningBomb, L::Styled, 3, 3, 1, 300, 1, 0, 10, 0, kEnemyHarmless, 0},
  {"piano_bird", "PIANO BIRD", K::PianoBird, L::Styled, 4, 3, 3, 500, 1, 45, 22, 0, kEnemyHarmless | kEnemyNoTally, 0},
  {"rubber_goon", "RUBBER GOON", K::RubberGoon, L::Styled, 4, 6, 6, 600, 2, 30, 12, 0, 0, 0},
  // Level 29. Static Swarm: tell the hiss before it latches. Test Card: tell
  // the quadrant's aim line, cooldown between beams. Ghost Image: tell its
  // after-images snapping back, cooldown between shots.
  {"static_swarm", "STATIC SWARM", K::StaticSwarm, L::Styled, 4, 4, 2, 40, 2, 15, 8, 0, kEnemyHarmless, 0},
  {"test_card", "TEST CARD", K::TestCard, L::Styled, 8, 8, 9, 900, 0, 45, 15, 0, kEnemyHarmless, 0},
  {"ghost_image", "GHOST IMAGE", K::GhostImage, L::Styled, 3, 5, 4, 600, 2, 30, 12, 0, 0, 0},
  // Level 30. Anglerfish: tell the lure going still before a lunge. Box
  // Jelly: the column is set up in world_trench.cpp. Cable Eel: tell the
  // box sparking, cooldown back in its box.
  {"anglerfish", "ANGLERFISH", K::Anglerfish, L::Styled, 6, 4, 5, 600, 0, 30, 10, 0, 0, 0},
  {"box_jelly", "BOX JELLY", K::BoxJelly, L::Styled, 4, 4, 2, 300, 0, 15, 8, 0, 0, 0},
  {"cable_eel", "CABLE EEL", K::CableEel, L::Styled, 3, 12, 4, 700, 0, 40, 12, 0, kEnemyHarmless, 0},
  // Level 31. Storm Gull: tell the scream before a dive (spawned, so not in
  // the tally). Kite Drone: tell the rotor's pitch rising before it turns at
  // you. Foghorn: tell the bellows swelling before a blast.
  {"storm_gull", "STORM GULL", K::StormGull, L::Styled, 4, 3, 2, 300, 0, 45, 12, 0, kEnemyNoTally, 0},
  {"kite_drone", "KITE DRONE", K::KiteDrone, L::Styled, 4, 4, 4, 600, 0, 30, 8, 0, kEnemyRobot, 0},
  {"foghorn", "FOGHORN", K::Foghorn, L::Styled, 6, 6, 8, 800, 0, 45, 22, 0, kEnemyHarmless, 0},
  // Level 32. Sand Shark: tell the fin stopping and the sand bulging before
  // it breaches. Vulture Drone: the scrap's shadow shrinking as it falls.
  // Scorpion Mech: its aim line turning toward you, then locking.
  {"sand_shark", "SAND SHARK", K::SandShark, L::Styled, 6, 4, 4, 700, 1, 40, 12, 0, 0, 0},
  {"vulture_drone", "VULTURE DRONE", K::VultureDrone, L::Styled, 6, 3, 3, 400, 1, 45, 22, 0, kEnemyRobot | kEnemyHarmless, 0},
  {"scorpion_mech", "SCORPION MECH", K::ScorpionMech, L::Styled, 8, 6, 10, 1200, 4, 45, 38, 0, kEnemyRobot, 0, 3},
  // Level 33. Firewall: its flames flicker for 8 frames, then dip for 30 at
  // a port. Lava Daemon: its arm glows and winds up (12 frames). Fan Bot:
  // the blades spin up with a rising whine (15 frames).
  {"firewall", "FIREWALL", K::Firewall, L::Styled, 4, 6, 6, 900, 2, 0, 8, 0, kEnemyHarmless | kEnemyNoTally, 0},
  {"lava_daemon", "LAVA DAEMON", K::LavaDaemon, L::Styled, 3, 4, 3, 600, 1, 40, 12, 0, 0, 0},
  {"fan_bot", "FAN BOT", K::FanBot, L::Styled, 4, 6, 6, 500, 1, 45, 15, 0, kEnemyHarmless | kEnemyRobot, 0},
  // Level 34. Sky Pirate: the blade goes up (10 frames) before a cutlass
  // swing; the arm levels with a glint (12) before a pistol shot. Peck
  // Parrot: a squawk and a head bob (8) before its dive. Balloon Mine:
  // flashes red for 8 frames, then bursts.
  {"sky_pirate", "SKY PIRATE", K::SkyPirate, L::Styled, 3, 5, 5, 800, 2, 40, 10, 0, 0, 0},
  {"peck_parrot", "PECK PARROT", K::PeckParrot, L::Styled, 3, 3, 2, 400, 1, 45, 8, 0, 0, 0},
  {"balloon_mine", "BALLOON MINE", K::BalloonMine, L::Styled, 4, 4, 1, 300, 4, 0, 8, 0, kEnemyHarmless | kEnemyNoTally, 0},
  // Level 35. Mast Climber: the wrench goes up (10 frames) before the swing.
  // Relay Drone: a muzzle flash (10) before each shot. Feedback Node: glows
  // white with a rising hum (22) before its ring pushes. The Copycat
  // bonus's dummies copy the runner and only bump.
  {"mast_climber", "MAST CLIMBER", K::MastClimber, L::Styled, 3, 5, 5, 500, 2, 30, 10, 0, 0, 0},
  {"relay_drone", "RELAY DRONE", K::RelayDrone, L::Styled, 4, 3, 3, 400, 1, 40, 10, 0, kEnemyHarmless | kEnemyRobot, 0},
  {"feedback_node", "FEEDBACK NODE", K::FeedbackNode, L::Styled, 4, 4, 8, 900, 0, 45, 22, 0, kEnemyHarmless | kEnemyRobot, 0},
  {"copycat_dummy", "COPYCAT DUMMY", K::MastClimber, L::Styled, 3, 5, 99, 0, 1, 0, 0, 0, kEnemyHarmless | kEnemyNoTally, 0},
  // Level 36. Forklift Bot: forks lower and the engine revs (15) before a
  // charge; it does its own contact damage (world_warehouse.cpp). Stagehand:
  // white gloves flash as it leans out (12). Prop Mimic: the lid opens on
  // its teeth (10) before it lunges. The Throw Everything bonus's dummies
  // only walk.
  {"forklift", "FORKLIFT BOT", K::Forklift, L::Styled, 6, 4, 8, 800, 2, 45, 15, 0, kEnemyHarmless | kEnemyRobot, 0},
  {"stagehand", "STAGEHAND", K::Stagehand, L::Styled, 3, 5, 4, 500, 2, 30, 12, 0, kEnemyHarmless, 0},
  {"mimic", "PROP MIMIC", K::Mimic, L::Styled, 4, 4, 3, 700, 0, 30, 10, 0, 0, 0},
  // Level 37. Mannequin: creaks and twitches (8) before it moves off; it
  // only moves while nobody faces it. Powder Puffer: bellows squeeze (12),
  // then a cloud; harmless to touch. Rack Runner: hangers rattle (15)
  // before each roll; its own contact (world_wardrobe.cpp).
  {"mannequin", "MANNEQUIN", K::Mannequin, L::Styled, 3, 5, 4, 500, 2, 15, 8, 0, kEnemyHarmless, 0},
  {"powder_puffer", "POWDER PUFFER", K::PowderPuffer, L::Styled, 4, 4, 3, 400, 0, 45, 12, 0, kEnemyHarmless, 0},
  {"rackrunner", "RACK RUNNER", K::RackRunner, L::Styled, 6, 6, 6, 600, 1, 30, 15, 0, kEnemyHarmless, 0},
  // Level 38. Typo: crawls to struck words (world_writers.cpp), infects on
  // contact. Deadline: speeds up each lap, the bell rings first (12).
  // Writer's Block: scrapes (15) before each slide; bullets don't hurt it.
  // Sketch guard: raises its pencil rifle (12), fires every 30 frames.
  {"typo", "TYPO", K::Typo, L::Styled, 2, 2, 1, 200, 2, 30, 8, 0, kEnemyCarrier | kEnemyNoTally, 0},
  {"deadline", "DEADLINE", K::Deadline, L::Styled, 4, 5, 6, 700, 1, 15, 12, 0, kEnemyHarmless, 0},
  {"writersblock", "WRITER'S BLOCK", K::WritersBlock, L::Styled, 4, 4, 99, 1000, 1, 45, 15, 0, kEnemyHarmless, 0},
  {"sketch_guard", "SKETCH GUARD", K::SketchGuard, L::Styled, 3, 5, 4, 500, 0, 30, 12, 30, 0, 0},
  {"floormgr", "FLOOR MANAGER", K::FloorManager, L::Styled, 3, 5, 6, 800, 2, 45, 22, 24, 0, 0},
  {"wrangler", "CABLE WRANGLER", K::CableWrangler, L::Styled, 4, 5, 4, 500, 2, 30, 10, 0, 0, 0},
  {"boomop", "BOOM OP", K::BoomOp, L::Styled, 4, 5, 5, 600, 0, 45, 10, 0, 0, 0},
  {"practice_dummy", "PRACTICE DUMMY", K::Walker, L::Styled, 3, 5, 3, 0, 3, 0, 0, 0, kEnemyHarmless | kEnemyNoTally, 0},
};

constexpr int kEnemyCount = int(sizeof(kEnemies) / sizeof(kEnemies[0]));

} // namespace

int enemyIndex(const std::string& key)
{
  for (int i = 0; i < kEnemyCount; ++i)
    if (key == kEnemies[i].key)
      return i;
  return -1;
}

const EnemyDef& enemyDef(int index) { return kEnemies[index]; }

int enemyDefCount() { return kEnemyCount; }

} // namespace gr
