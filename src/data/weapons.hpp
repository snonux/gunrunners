#pragma once

#include "render/color.hpp"

#include <string>

namespace gr
{

// The 42 prototype weapons (docs/LEVELS.md, SPEC.md section 4), plus
// Episode 7's own (one per DEEP SPACE level, after the 42). Each level
// hides its own prototype in green W boxes; it only works in that level and
// is logged to the Arsenal once found. The numbers are data; what makes
// each one special lives in the world code, keyed by ProtoId.
enum class ProtoId
{
  PulsePistol,
  SparkDisc,
  BassCannon,
  FlareGun,
  BubbleGun,
  ArcCaster,
  LockOnRockets,
  Boomerang,
  SnareBolas,
  SunstoneLance,
  BlastingCaps,
  SerpentSpear,
  FanDarts,
  JadeBow,
  BreachCharge,
  FreezeRay,
  HedgeTrimmer,
  RecoilCannon,
  GravGrenade,
  DeflectorBracer,
  PhaseRifle,
  SixShooter,
  SilverCrossbow,
  PepperGrinder,
  ChronoMister,
  AtomicBreath,
  Silencer,
  PortableHole,
  TriToneBlaster,
  HarpoonLine,
  TeslaRod,
  SignalJammer,
  HeatSink,
  GrappleGun,
  SatelliteSwarm,
  MagnetGun,
  DecoyLauncher,
  RedPen,
  EchoMic,
  PyroRig,
  ApplauseCannon,
  Encore,
  // Episode 7, DEEP SPACE (docs/DEEP_SPACE.md).
  GooGun,
  BileBlaster,
  SwapRifle,
  SilkShooter,
  TamersWhip,
  Count,
};

constexpr int kProtoCount = int(ProtoId::Count);

enum class FireMode
{
  Tap,    // one shot per press
  Auto,   // every `cooldown` frames while held
  Hold,   // continuous beam or spray while held
  Charge, // hold to charge, release fires
  Place,  // puts an object down; a second press triggers it
};

struct ProtoDef
{
  ProtoId id;
  const char* key;  // as in the level header's weapon=
  const char* name; // shown in the HUD and the Arsenal
  int level;        // the level that hides it
  FireMode mode;
  int cooldown; // frames between shots (tap refire or auto interval)
  int speed;    // cells per frame
  int damage;
  int boxAmmo;
  int maxAmmo;
  Color color;
  const char* blurb; // one line for the pick-up message and the Arsenal
};

const ProtoDef& protoDef(ProtoId id);
const ProtoDef& protoDef(int index);
// -1 if the key is unknown.
int protoIndex(const std::string& key);

} // namespace gr
