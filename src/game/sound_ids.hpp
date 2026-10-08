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
  Count,
};

} // namespace gr
