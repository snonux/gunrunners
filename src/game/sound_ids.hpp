#pragma once

namespace gr
{

// Every sound effect the game can trigger. The audio module synthesizes them
// at startup; the game logic only emits these ids.
enum class Sfx
{
  Jump,
  Land,
  Shot,
  LaserShot,
  RocketShot,
  FlameShot,
  EnemyShot,
  Hit,
  Explosion,
  SmallExplosion,
  BoxBreak,
  Gem,
  Item,
  Health,
  WeaponPickup,
  Letter,
  LettersComplete,
  Hurt,
  Death,
  AttachClimbable,
  Key,
  ForceFieldOff,
  Checkpoint,
  Teleport,
  MenuMove,
  MenuSelect,
  Tally,
  TurboOn,
  VirusOn,
  EffectEnd,
  Siren,     // the tide is about to rise
  Splash,    // into sludge, a gator surfacing
  Bubble,    // a bubble shot, a bubble popping
  Bloop,     // gone down the /dev/null pipe
  Klaxon,    // a Valve Keeper turning its wheel
  Rattle,    // a rat pipe about to burst
  Quack,     // the Duck Rapids duck
  Warn,      // a gantry's warning lights
  Chime,     // the maglev's station chime, the passing train
  Clunk,     // a coupling, a caltrop bay
  Zap,       // the Arc Caster
  Beep,      // the hunter has a lock
  Whistle,   // a gunship rocket coming down
  Scream,    // Black Halo's engine as it lines up a ram
  Rev,       // a Hover Biker revving
  Creak,     // a rope bridge about to go
  Chop,      // a Bridge Cutter's machete, a rope cut
  Hoot,      // a Howler winding up a throw
  Rustle,    // a Canopy Viper about to strike
  Whoosh,    // the Boomerang in the air
  Yell,      // the jungle yell (ten swings on one vine)
  Boing,     // Bounce House: a trampoline landing
  Rumble,    // a rolling stone about to go
  Slice,     // a trap blade's sweep
  Click,     // a pressure plate
  Drum,      // the Hall of Traps' drum egg
  Dart,      // a Dart Face's dart
  Skitter,   // a Scarab Tide on the move
  Bell,      // the mine elevator setting off
  Fuse,      // a Blasting Cap's fuse
  Flap,      // a Bat Cloud's wingbeats
  Dig,       // a Rock Mole breaking out of the rock
  Rail,      // a cart's wheels hitting the track
  Sizzle,    // something hits the lava
  Magma,     // a lava bubble's plop, a Magma Toad leaping out
  Sink,      // a basalt stone grinding down into the lava
  Croak,     // a Magma Toad landing
  Snap,      // a Basalt Crab's claws
  Squelch,   // goo: sticking to a goo wall, a Goo Gun splat, a Gloop splitting
  Chitter,   // a Skitter's click before it leaps
  Spit,      // a Spitpod lobbing acid
  Gulp,      // a Gullet Tube swallowing or spitting out
  Inhale,    // a Polyp breathing in
  Screech,   // a Drone Warden calling its mites
  Swap,      // a Swap Crystal ringing as you trade places with it
  Blink,     // a Blinker's shimmer before it appears
  Zip,       // grabbing a Silk Line and sliding off down it
  Snip,      // a Loom Spider cutting a Silk Line: a twang and a snap
  Crack,     // the Tamer's Whip
  Crash,     // a boulder landing, a chute's flaps slamming
  Hiss,      // a Pit Snake about to rear
  EngineOn,  // climbing into a vehicle
  EngineOff, // climbing out
  Cannon,    // the tank's gun
  Torpedo,   // a submarine's torpedo
  Stomp,     // a mech landing hard
  Crunch,    // something crushed under a tank
  Freeze,    // the Freeze Ray icing something over
  Tink,      // a shot off a block of ice, a frozen block shattering
  Whine,     // a Puck Drone spinning up
  Cheer,     // a faint crowd far away (the goal vent, Air Hockey goals)
  Trim,      // the Hedge Trimmer's blades (a short buzz, repeated while held)
  Puff,      // a Spore Puffer letting go of a cloud
  Squish,    // a Glob landing, splitting
  LampOn,    // a grow lamp buzzing on
  Grow,      // Growth Spurt: one size up (down is the same, played lower)
  Recoil,    // the Recoil Cannon: a deep thump with a whoosh of exhaust
  Thrust,    // an EVA Ram's thruster firing (its ram)
  Spikes,    // a Space Barnacle's shell snapping open, spikes out
  Rivet,     // a rivet working loose and popping out (a metallic ping)
  Hop,       // a Rivet Mite or the runner on a planetoid leaping (a tiny chirp)
  FlagUp,    // a flag planted: a flap of cloth and a little fanfare
  GravFlip,  // a chamber turning over: a rising whoosh with a klaxon blip
  Thud,      // landing after a flip (a soft body thump)
  Vortex,    // the Grav Grenade's vortex opening (a deep swirling hum)
  ProbeShot, // a Gravity Probe's aimed shot (a hollow electronic blip)
  CoreHum,   // the reactor core winding up for a pulse (a rising hum, 3 s)
  CorePulse, // a Core Pulse ring going off (a deep electric boom with a ring)
  BracerUp,  // the Deflector Bracer's shield raised (a quick shimmering hum)
  Deflect,   // a shot bounced off the Bracer, a pulse soaked, a drone's bubble taking a hit
  ValveTurn, // a coolant valve squeaking round
  ValveShut, // a valve shut tight (a heavy clank and a hiss)
  Rewind,    // DO NOT PRESS: the victory fanfare played backwards (6 s)
  Bulkhead,  // a bulkhead sliding (a heavy hydraulic grind ending in a clunk)
  Monitor,   // the monitors switching to the next schematic (a CRT blip)
  PhaseShot, // the Phase Rifle (a thin rising zip that goes hollow through a wall)
  Glint,     // a fake wall or hidden pocket glinting (a tiny chime)
  Rebuild,   // a Repair Swarm finishing a rebuild (a rising ratchet and a ping)
  EchoIn,    // an Echo stepping out of its pad (a reversed shimmer)
  EyeCharge, // ZERO's eye charging (a rising whine, 1.5 s)
  EyeBeam,   // its beam on the grille (a crackling buzz)
  Shutter,   // its shutter closing or opening (a camera-iris clack)
  WallFall,  // the painted flat tipping over (a creak and a huge flat slap)
  LightClunk, // a bank of studio lights switching on
  Applause,  // the studio audience cheering
  FuseLit,   // a fuse catching (a match strike and a fizz)
  BarrelHiss, // a TNT barrel about to blow (a sharp hiss, 12 frames)
  Ricochet,  // a Six-Shooter bullet off metal (a whining ping)
  Reload,    // the Six-Shooter's cylinder spinning (a ratchet and a click)
  Piano1,    // the saloon piano's four keys (honky-tonk, rising)
  Piano2,
  Piano3,
  Piano4,
  HonkyTonk, // the first bar of the theme on the saloon piano
  PosterSpin, // a wanted poster spinning on its nail
  Gust,      // a gust down the mine tunnel
  DuelDraw,  // a Duelist drawing (a holster slap and a hammer click)
  MirrorFlip, // stepping through a mirror (a glassy shimmer, inverted)
  Lever,     // a heavy iron lever thrown (a creak and a clunk)
  GhostHiss, // a Portrait Ghost leaving its frame or lunging (a cold hiss)
  ArmorClank, // a Haunted Armor waking (a rattle of plate)
  Swoosh,    // a halberd swing
  Throw,     // a Poltergeist throwing something
  BoltThunk, // a Silver Crossbow bolt pinning something
  GlassChime, // the broken mirror shot (a cracked chime)
  CoffinSlam, // the coffin lid lifting and slamming
  Thunder,   // lightning over the manor
  BellDing,  // a chef's bell rung (Level 24's belts flip after it)
  Sneeze,    // an enemy sneezing from the Pepper Grinder
  EggRing,   // an Egg Timer ringing out its last frames
  GrinderWhir, // the Pepper Grinder spinning up
  Toaster,   // the toaster popping
  CleaverThunk, // a cleaver sticking in a wall
  Mist,      // the Chrono Mister's spray (a soft, airy hiss)
  AntAlarm,  // a Soldier Ant column alarmed (a rattle of chittering clicks)
  BeetleJet, // a Bombardier Beetle firing (a boiling pop and a scalding hiss)
  Narrator,  // a narrator's subtitle (breathy blips around 200 Hz)
  Roar,      // Gorgazilla's roar (a growl through a megaphone, rising and cracking)
  BuildingCrumble, // a model building collapsing (a balsa crunch and a rattle of bits)
  BreathBeam, // the Atomic Breath (a rising crackling electric whoosh)
  MaserHum,  // a Maser dish charging (a pulsing hum climbing in pitch)
  SlideWhistle, // Level 28: a cartoon slide whistle falling (the "uh-oh" drop, Buddy into the ink)
  Bonk,      // running into painted rock (a wood block knock with a wobbling boing)
  HoleGulp,  // a Portable Hole opening (a cartoon gulp: a hollow bloop swallowed down)
  PianoCrash, // a piano landing (a smash of wood with every string jangling)
  PieSplat,  // a cream pie in the face (a wet splat and a soft plop)
  PencilScratch, // the giant pencil drawing (a dry graphite scratch)
  EraserRub, // the eraser rubbing a floor out (a squeaking rubber scrub)
  Honk,      // Buddy Boom hit (a squeeze-bulb horn honk)
  RubberBoing, // a flat Rubber Goon bouncing a runner (a long twanging boing)
  DialTurn,  // Level 29: an RGB dial clicking round, a signal change on its way (a knob notch, a blip of static, two rising notes)
  ChannelFlip, // the change landing, every bar swapping (a CRT's thump with a crackle)
  TintTurn,  // the Tri-Tone turning its tint (a selector's detent and a pure blip)
  SnowHiss,  // a Static Swarm about to latch on (TV snow swelling up)
  SnowLatch, // a Static Swarm latching on (a burst of static clamping down)
  TestTone,  // the Test Card's tell (the 1 kHz test tone)
  CardBeam,  // the Test Card's beam (a hard bright buzz falling away)
  TuneIn,    // tuned in: the static narrowing into a clean tone that resolves into a chord
  SwimStroke, // Level 30: a swim stroke (a soft underwater whoosh and a burble of bubbles)
  AbyssSplash, // spat back out of the abyss (a deep muffled boom, bubbles rushing up)
  HarpoonPlant, // a harpoon planted in the floor (a dull thud and a metal ring, muffled)
  HarpoonStick, // a harpoon sticking in a wall or an anchor ring (a sharp thunk and a clang)
  LineTaut,  // the harpoon line pulled taut (a rope creak and a low twang)
  EelSpark,  // a Cable Eel's box sparking (crackling arcs and a rising hum)
  EelSnap,   // a Cable Eel lunging out (a zap and a wet snap of jaws)
  AnglerLunge, // an Anglerfish lunging (a rushing gulp and a clap of teeth)
  JellyPulse, // a Box Jelly contracting before a rise (a soft glassy pulse)
  BubblePop, // the Bubble Up bubble popping (a bright pop and a spray)
  RingPass,  // a Bubble Up ring passed (a rising bubbly chime)
  StormStrike, // Level 31: lightning striking close (a tearing crack, a boom and a rolling rumble)
  StormGlow, // metal glowing before a strike (a buzz of static climbing, crackling harder)
  RodThunk,  // a Tesla rod sticking in rock or a machine (a hard thunk, a copper ring, a zing)
  RodArc,    // a struck rod arcing to the enemies round it (a sharp zap and a sizzle)
  MachinePower, // a dead machine powered by a strike (a relay's clack, a motor spinning up to a hum)
  GullCry,   // a Storm Gull's cry before its dive (a harsh falling kee-yow)
  GullDive,  // a Storm Gull diving (wind rushing past folded wings)
  KiteWhine, // a Kite Drone's rotors climbing in pitch as it turns at you
  FogWheeze, // a Foghorn's bellows filling (a reedy wheezing inhale)
  FogBlast,  // a Foghorn's blast (a deep brassy diaphone groan)
  BungeeStretch, // Bungee's rope stretching (a rubbery creak)
  BungeeTwang, // let go: flung on the rope (a deep elastic twang)
  StormRing, // a storm ring taken (a crackle and a bright electric chime)
  DishGroan, // Level 32: a big dish about to turn (a deep steel groan, creaking bearings)
  DishTurn,  // a dish turning (its drive motors spinning up, gears ticking)
  JamBurst,  // the Signal Jammer firing (a warbling burst of radio noise)
  RobotJammed, // a robot jammed (garbled bleeps tumbling down, a fizz)
  SandBulge, // a Sand Shark about to breach (a low rumble, the sand hissing up)
  SharkBreach, // a Sand Shark breaching (a spray of sand and a snap of jaws)
  ScrapDrop, // a Vulture Drone letting go of scrap (a falling whistle)
  ScrapClank, // scrap landing (a tinny clank and a rattle)
  MechAim,   // a Scorpion Mech's stinger aiming (a charging whine climbing)
  MechBeam,  // a Scorpion Mech's laser (a searing zap, a hum)
  MiniDishTurn, // a minidish knocked round (a little servo whirr and a click)
  CactusHat, // the cactus's hat popping up (a cork pop and a slide whistle)
  SwapWarn,  // Heatstroke: left and right about to swap (a wobbling two-tone siren)
  LavaRumble, // Level 33: the lava rising (a deep magma rumble, slow bubbles glugging)
  SteamBurst, // a coolant purge venting (a blast of steam, a hissing roar tailing off)
  PurgeSiren, // the purge running out (a klaxon whooping)
  LavaPop,    // popped out of the lava (a searing sizzle, a fat pop up and away)
  GutterSizzle, // wading in the magma gutter (a short searing sizzle)
  SinkSuck,   // the Heat Sink held (a vacuum whoosh pulling in)
  SinkGulp,   // a shot swallowed by the Heat Sink (a deep glug and a ping)
  SinkThrow,  // the Heat Sink's fireball thrown (a whump and a rolling roar of flame)
  FirewallSputter, // a Firewall about to dip (flames sputtering, a glitchy buzz)
  FirewallRoar, // a Firewall flaring back up (a roaring whoosh)
  DaemonWindup, // a Lava Daemon winding up (a crackling charge rising)
  DaemonSpit, // a Lava Daemon's fireball lobbed (a fwoomp and a hiss)
  DaemonHop,  // a Lava Daemon leaping (a rocky scrape and a sizzle)
  FanWhine,   // a Fan Bot spinning up (a rising turbine whine)
  FanGust,    // a Fan Bot's gust (a rush of wind)
  RackGlitch, // the corrupted rack spinning up (digital chatter over a sick whine)
  RebootChime, // the DO NOT REBOOT rack shot (a power-down whir, then a startup chime)
  RatingsDing, // the RATINGS server shot (a rising arpeggio and a till's ding)
  Count,
};

} // namespace gr
