#include "data/weapons.hpp"

#include <array>

namespace gr
{

namespace
{

using M = FireMode;

// Numbers from SPEC.md section 10, one row per level.
const std::array<ProtoDef, kProtoCount> kProtos{{
  {ProtoId::PulsePistol, "pulse_pistol", "PULSE PISTOL", 1, M::Tap, 0, 3, 2, 20, 40, rgb(255, 60, 200),
   "SHOOT ON THE BEAT FOR DOUBLE DAMAGE"},
  {ProtoId::SparkDisc, "spark_disc", "SPARK DISC", 2, M::Tap, 0, 2, 2, 12, 24, rgb(120, 230, 255),
   "DISCS RIDE WALLS AND CEILINGS"},
  {ProtoId::BassCannon, "bass_cannon", "BASS CANNON", 3, M::Tap, 10, 0, 2, 12, 24, rgb(190, 90, 255),
   "A WALL OF SOUND: KNOCKS BACK, SHATTERS GLASS"},
  {ProtoId::FlareGun, "flare_gun", "FLARE GUN", 4, M::Tap, 0, 3, 1, 6, 18, rgb(255, 120, 40),
   "FLARES STICK AND LIGHT UP THE DARK"},
  {ProtoId::BubbleGun, "bubble_gun", "BUBBLE GUN", 5, M::Tap, 0, 1, 1, 10, 20, rgb(140, 220, 255),
   "TRAPS ENEMIES IN BUBBLES YOU CAN STAND ON"},
  {ProtoId::ArcCaster, "arc_caster", "ARC CASTER", 6, M::Tap, 10, 0, 2, 15, 30, rgb(160, 200, 255),
   "LIGHTNING THAT JUMPS FROM TARGET TO TARGET"},
  {ProtoId::LockOnRockets, "lock_on_rockets", "LOCK-ON ROCKETS", 7, M::Charge, 0, 2, 3, 8, 16, rgb(255, 90, 60),
   "HOLD TO PAINT TARGETS, RELEASE TO FIRE"},
  {ProtoId::Boomerang, "boomerang", "BOOMERANG", 8, M::Tap, 0, 2, 2, 12, 24, rgb(200, 150, 70),
   "COMES BACK; CUTS ROPES"},
  {ProtoId::SnareBolas, "snare_bolas", "SNARE BOLAS", 9, M::Tap, 0, 2, 1, 10, 20, rgb(170, 120, 60),
   "TOPPLES AND TANGLES WALKERS"},
  {ProtoId::SunstoneLance, "sunstone_lance", "SUNSTONE LANCE", 10, M::Hold, 0, 4, 1, 30, 60, rgb(255, 220, 90),
   "A SUNBEAM THAT BOUNCES OFF MIRRORS"},
  {ProtoId::BlastingCaps, "blasting_caps", "BLASTING CAPS", 11, M::Tap, 8, 2, 8, 8, 16, rgb(230, 70, 50),
   "BOUNCING DYNAMITE WITH A SHORT FUSE"},
  {ProtoId::SerpentSpear, "serpent_spear", "SERPENT SPEAR", 12, M::Tap, 0, 3, 2, 8, 16, rgb(90, 200, 120),
   "STICKS IN WALLS: A STEP YOU CAN STAND ON"},
  {ProtoId::FanDarts, "fan_darts", "FAN DARTS", 13, M::Auto, 4, 3, 1, 40, 80, rgb(120, 220, 160),
   "THREE DARTS IN A FAN, WHILE YOU RUN"},
  {ProtoId::JadeBow, "jade_bow", "JADE BOW", 14, M::Charge, 0, 4, 4, 16, 32, rgb(80, 220, 150),
   "FULL DRAW PIERCES EVERYTHING"},
  {ProtoId::BreachCharge, "breach_charge", "BREACH CHARGE", 15, M::Place, 0, 2, 8, 8, 16, rgb(255, 170, 40),
   "STICK IT ON, BLOW IT UP"},
  {ProtoId::FreezeRay, "freeze_ray", "FREEZE RAY", 16, M::Tap, 0, 3, 1, 12, 24, rgb(150, 230, 255),
   "FROZEN ENEMIES ARE BLOCKS YOU CAN STAND ON"},
  {ProtoId::HedgeTrimmer, "hedge_trimmer", "HEDGE TRIMMER", 17, M::Hold, 3, 0, 1, 40, 80, rgb(120, 230, 90),
   "SPINNING BLADES FOR THORN WALLS"},
  {ProtoId::RecoilCannon, "recoil_cannon", "RECOIL CANNON", 18, M::Tap, 10, 3, 3, 10, 20, rgb(255, 200, 90),
   "SHOOT DOWN IN THE AIR FOR A SECOND JUMP"},
  {ProtoId::GravGrenade, "grav_grenade", "GRAV GRENADE", 19, M::Tap, 0, 2, 2, 8, 16, rgb(170, 110, 255),
   "A LITTLE BLACK HOLE THAT PULLS ENEMIES IN"},
  {ProtoId::DeflectorBracer, "deflector_bracer", "DEFLECTOR BRACER", 20, M::Hold, 0, 2, 1, 20, 40, rgb(90, 200, 255),
   "HOLD TO RAISE A SHIELD THAT BOUNCES SHOTS BACK"},
  {ProtoId::PhaseRifle, "phase_rifle", "PHASE RIFLE", 21, M::Tap, 0, 3, 2, 20, 40, rgb(200, 120, 255),
   "SHOOTS THROUGH ONE BLOCK OF WALL"},
  {ProtoId::SixShooter, "six_shooter", "SIX-SHOOTER", 22, M::Tap, 0, 4, 2, 18, 36, rgb(220, 200, 160),
   "SIX SHOTS, THEN RELOAD; RICOCHETS OFF METAL"},
  {ProtoId::SilverCrossbow, "silver_crossbow", "SILVER CROSSBOW", 23, M::Tap, 10, 3, 2, 10, 20, rgb(210, 220, 240),
   "PINS GHOSTS TO THE SPOT"},
  {ProtoId::PepperGrinder, "pepper_grinder", "PEPPER GRINDER", 24, M::Auto, 2, 3, 1, 40, 80, rgb(240, 90, 60),
   "SPIN IT UP; ENEMIES SNEEZE"},
  {ProtoId::ChronoMister, "chrono_mister", "CHRONO MISTER", 25, M::Hold, 6, 0, 1, 30, 60, rgb(150, 255, 200),
   "FAST-FORWARDS WHATEVER IT SPRAYS"},
  {ProtoId::AtomicBreath, "atomic_breath", "ATOMIC BREATH", 26, M::Hold, 3, 0, 1, 30, 60, rgb(90, 255, 200),
   "A KAIJU BEAM; UP AND DOWN SWEEP IT"},
  {ProtoId::Silencer, "silencer", "SILENCER", 27, M::Tap, 12, 3, 1, 12, 24, rgb(150, 150, 170),
   "SILENT; TRIPLE DAMAGE ON UNAWARE ENEMIES"},
  {ProtoId::PortableHole, "portable_hole", "PORTABLE HOLE", 28, M::Place, 0, 2, 0, 6, 12, rgb(40, 30, 50),
   "THROW A HOLE INTO A WALL OR FLOOR"},
  {ProtoId::TriToneBlaster, "tri_tone_blaster", "TRI-TONE BLASTER", 29, M::Tap, 0, 2, 1, 20, 40, rgb(255, 80, 80),
   "UP + FIRE CHANGES THE COLOUR"},
  {ProtoId::HarpoonLine, "harpoon_line", "HARPOON LINE", 30, M::Tap, 0, 3, 2, 12, 24, rgb(220, 230, 240),
   "TWO HARPOONS MAKE A LINE YOU CAN WALK OR HANG ON"},
  {ProtoId::TeslaRod, "tesla_rod", "TESLA ROD", 31, M::Tap, 0, 3, 1, 6, 12, rgb(170, 200, 255),
   "A LIGHTNING ROD YOU CAN THROW"},
  {ProtoId::SignalJammer, "signal_jammer", "SIGNAL JAMMER", 32, M::Tap, 15, 0, 1, 10, 20, rgb(255, 220, 120),
   "ROBOTS IT HITS TURN ON EACH OTHER"},
  {ProtoId::HeatSink, "heat_sink", "HEAT SINK", 33, M::Hold, 0, 1, 1, 3, 6, rgb(255, 120, 40),
   "SUCK UP FIREBALLS, THROW THEM BACK"},
  {ProtoId::GrappleGun, "grapple_gun", "GRAPPLE GUN", 34, M::Tap, 0, 4, 1, 20, 40, rgb(200, 170, 90),
   "HOOK BRASS RINGS AND SWING"},
  {ProtoId::SatelliteSwarm, "satellite_swarm", "SATELLITE SWARM", 35, M::Tap, 0, 2, 1, 15, 30, rgb(160, 255, 255),
   "THREE DRONES THAT SHOOT WITH YOU"},
  {ProtoId::MagnetGun, "magnet_gun", "MAGNET GUN", 36, M::Hold, 0, 3, 2, 30, 60, rgb(255, 60, 60),
   "HOLD TO PULL METAL, TAP TO PUSH IT"},
  {ProtoId::DecoyLauncher, "decoy_launcher", "DECOY LAUNCHER", 37, M::Tap, 0, 2, 2, 8, 16, rgb(240, 200, 170),
   "A MANNEQUIN EVERYONE SHOOTS AT INSTEAD"},
  {ProtoId::RedPen, "red_pen", "RED PEN", 38, M::Tap, 0, 3, 2, 20, 40, rgb(230, 30, 40),
   "STRIKES OUT WORDS IN THE SCRIPT"},
  {ProtoId::EchoMic, "echo_mic", "ECHO MIC", 39, M::Tap, 0, 2, 2, 20, 40, rgb(255, 230, 120),
   "EVERY SHOT FIRES AGAIN A SECOND LATER"},
  {ProtoId::PyroRig, "pyro_rig", "PYRO RIG", 40, M::Hold, 4, 0, 1, 40, 80, rgb(255, 150, 40),
   "FLAMES; DOWN + FIRE TO FLY"},
  {ProtoId::ApplauseCannon, "applause_cannon", "APPLAUSE CANNON", 41, M::Tap, 0, 2, 1, 30, 60, rgb(255, 210, 60),
   "HIT STREAKS RAISE THE DAMAGE METER"},
  {ProtoId::Encore, "encore", "THE ENCORE", 42, M::Tap, 0, 3, 2, 30, 30, rgb(255, 255, 255),
   "EVERY PROTOTYPE YOU FOUND; UP + FIRE TO SWITCH"},
  // Episode 7, DEEP SPACE.
  {ProtoId::GooGun, "goo_gun", "GOO GUN", 44, M::Tap, 4, 2, 1, 12, 24, rgb(150, 255, 90),
   "GOO PATCHES ON WALLS YOU CAN CLING TO; GLUES ALIENS"},
  {ProtoId::BileBlaster, "bile_blaster", "BILE BLASTER", 45, M::Tap, 5, 2, 2, 16, 32, rgb(210, 240, 70),
   "SHOTS RIDE THE GULLET TUBES AND COME OUT THE FAR END"},
  {ProtoId::SwapRifle, "swap_rifle", "SWAP RIFLE", 46, M::Tap, 6, 3, 1, 14, 28, rgb(230, 130, 255),
   "TRADE PLACES WITH WHAT IT HITS; BOUNCES OFF WALLS ONCE"},
  {ProtoId::SilkShooter, "silk_shooter", "SILK SHOOTER", 47, M::Tap, 8, 2, 1, 12, 24, rgb(240, 236, 220),
   "FIRES DOWN AHEAD; WHERE IT HITS ROCK, A SILK LINE TO RIDE"},
  {ProtoId::TamersWhip, "tamers_whip", "TAMER'S WHIP", 48, M::Tap, 7, 0, 2, 30, 60, rgb(230, 170, 90),
   "A CRACK THAT STUNS ALIENS AND CALLS A BOUNDER TO YOU"},
  {ProtoId::StarSeed, "star_seed", "STAR SEED", 49, M::Charge, 8, 1, 2, 16, 32, rgb(255, 236, 140),
   "HOLD FIRE TO CHARGE A SLOW STAR THAT GOES THROUGH EVERY ALIEN"},
}};

} // namespace

const ProtoDef& protoDef(ProtoId id) { return kProtos[std::size_t(id)]; }

const ProtoDef& protoDef(int index) { return kProtos[std::size_t(index)]; }

int protoIndex(const std::string& key)
{
  for (const auto& p : kProtos)
    if (key == p.key)
      return int(p.id);
  return -1;
}

} // namespace gr
