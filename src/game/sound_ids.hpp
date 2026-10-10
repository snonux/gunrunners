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
  Count,
};

} // namespace gr
