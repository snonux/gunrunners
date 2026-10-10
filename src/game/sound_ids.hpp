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
  Crash,     // a boulder landing, a chute's flaps slamming
  Hiss,      // a Pit Snake about to rear
  EngineOn,  // climbing into a vehicle
  EngineOff, // climbing out
  Cannon,    // the tank's gun
  Torpedo,   // a submarine's torpedo
  Stomp,     // a mech landing hard
  Crunch,    // something crushed under a tank
  Count,
};

} // namespace gr
