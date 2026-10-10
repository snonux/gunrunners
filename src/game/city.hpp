#pragma once

#include "game/collision.hpp"

#include <string>
#include <vector>

namespace gr
{

// Level 26, Kaiju Tonight (world_city.cpp, world_city_draw.cpp): a model
// city where the runner is the monster. Model buildings break as a whole
// (jumped into, walked into by Rocco, burnt by the Atomic Breath, or stood
// on for too long), towers topple into bridges, power lines snap and black
// out their sector's Maser Trucks, Model Tank squads fire in turn, Toy Jet
// formations strafe, and Gorgazilla waits on the harbor piers. The
// Top-Down Rampage bonus (rules=topdown) is in world_topdown.cpp.

constexpr int kCrumbleFrames = 10; // a building turning to debris
constexpr int kRoofCrack = 20;     // frames stood on a roof before it breaks
constexpr int kBurnFrames = 6;     // frames of breath on a building before it breaks
constexpr int kTowerLean = 8;
constexpr int kTowerFall = 15;
constexpr int kLineSparks = 8;
constexpr int kBreathLen = 16;     // cells: the Atomic Breath's beam
constexpr int kBreathStepEvery = 3; // frames per 15-degree step of a sweep
constexpr int kBreathAmmoEvery = 4;
constexpr int kBreathDamageEvery = 3;
constexpr int kJetIntro = 30;      // frames a formation grows in the backdrop
constexpr int kJetGap = 45;        // frames between a formation's runs
constexpr int kMaserBeam = 40;     // cells: a Maser Truck's beam
constexpr int kMaserShow = 6;      // frames a beam is drawn
constexpr int kSquadCool = 30;     // frames between a squad's shots
constexpr int kRoarLen = 30;       // frames of Gorgazilla's roar; the dish's ring grows over the last 22
constexpr int kRoarLock = 22;      // the frame the dish locks onto the runner's row
constexpr int kSwipeTell = 12;     // his tail coils
constexpr int kUnder = 60;         // frames he stays submerged

enum class BuildingKind
{
  House,
  Block,
  Roof,  // breaking it breaks the street slab under it too
  Tower, // topples into a bridge instead of crumbling
  Prize, // a secret: a big rating
};

enum class BuildingState
{
  Standing,
  Crumbling, // turning to debris (non-solid already)
  Down,      // debris
  Leaning,   // a tower creaking over
  Falling,
  Lying,     // a tower lying as a bridge
};

// `@ building ID rect= kind=house|block|roof|tower|prize rating= under= fall=l|r`.
struct Building
{
  std::string id;
  BuildingKind kind = BuildingKind::House;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
  int rating = 100;
  int ux0 = -1, uy0 = 0, ux1 = -1, uy1 = 0; // roof: the street slab under it (blocks)
  int fall = 1;                             // tower: the way it topples
  int bx0 = 0, bx1 = -1, by0 = 0, by1 = 0;  // tower: the bridge it lies as (blocks)
  BuildingState state = BuildingState::Standing;
  int t = 0;      // frames into the state
  int cracks = 0; // frames a runner has stood on its roof
  int burn = 0;   // frames of breath on it in a row
  bool burnNow = false; // the breath is on it this frame
  bool standing() const { return state == BuildingState::Standing; }
  bool covers(int bx, int by) const { return bx >= x0 && bx <= x1 && by >= y0 && by <= y1; }
};

// `@ powerline ID path=x0,y;x1,y sector=S`.
struct PowerLine
{
  std::string id, sector;
  int x0 = 0, x1 = 0, y = 0; // cells: the wire's ends and row
  bool snapped = false;
  int sparks = 0;
};

// `@ sector ID rect=`: a part of the city a power line feeds.
struct CitySector
{
  std::string id;
  int x0 = 0, x1 = 0; // blocks
  bool dark = false;
};

// `@ squad kind=model_tank count=4 rect=x0,y,x1,y`: tanks that patrol the
// rect and fire in turn.
struct TankSquad
{
  std::vector<int> tanks; // enemy indices
  int x0 = 0, x1 = 0;     // cells: the patrol's ends
  int cool = 0;
  int next = 0;           // the tank whose turn it is
};

// `@ formation kind=toy_jet count=3 trigger=X row=Y`.
struct JetFormation
{
  std::vector<int> jets; // enemy indices
  int trigger = 0;       // cells: passing it calls the formation in
  int row = 0;           // cells: the jets' row
  int state = 0;         // 0 waiting, 1 growing in the backdrop, 2 a run, 3 between runs, 4 gone
  int t = 0;
  int run = 0;
  int dir = 1;
};

enum class LizardState
{
  Waiting, // before the runner is in the arena
  Idle,
  Roar,
  Swipe,
  Dive,    // wading in
  Under,
  Stagger,
  Sit,     // the zipper opens
  Leave,   // a tired man walks off set
  Done,
};

// Gorgazilla (`@ lizard x y arena= door= dish= drops= turbo=`).
struct Lizard
{
  bool on = false;
  LizardState state = LizardState::Waiting;
  int phase = 1;
  int hp = 20;            // the current phase's
  float x = 0.0f, prevX = 0.0f; // cells: the body's left
  int floor = 0;          // cells: the row under his feet
  int dir = -1;
  int t = 0;
  int cool = 30;
  int attacks = 0;
  int flash = 0;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;           // blocks: the arena
  int doorX = 0, doorY0 = 0, doorY1 = 0;        // blocks
  bool doorShut = false;
  int dishX = 0, dishY = 0;                     // cells: the harbor Maser dish
  int beamRow = -1;                             // cells: the row the dish fires along
  int beam = 0;                                 // frames the dish's beam shows
  bool beamHit = false;
  int splashX = -1;                             // cells: where he surfaces
  int piers[2][2] = {{0, 0}, {0, 0}};           // cells: west and east pier x ranges
  int dropX[2] = {0, 0}, dropY = 0;             // blocks: the phase drops (west, east)
  int turboX = -1, turboY = 0;                  // blocks: the crate's Turbo
  bool turboDropped = false;
  int startX = 0;
  float manX = 0.0f;                            // cells: the man walking off
  int surfaced = 0;                             // frames of the splash showing
  static constexpr int kW = 4, kH = 6;
  static constexpr int kHp = 20;
};

struct CityState
{
  bool on = false;
  std::vector<Building> buildings;
  std::vector<PowerLine> lines;
  std::vector<CitySector> sectors;
  std::vector<TankSquad> squads;
  std::vector<JetFormation> formations;
  Lizard lizard;
  int rating = 0;      // the damage meter
  int ratingFlash = 0;
  int fullhealX = -1, fullhealY = -1; // blocks
  bool fullhealUsed = false;
  int diner = -1;      // the diner (a building index): reaching dinerX with it standing is a secret
  int dinerX = -1;     // cells
  bool dinerSaid = false;
  int wave = 0;        // frames of the diner figurines waving
  // The Atomic Breath.
  int breath = 0;      // frames held
  int breathAng = 0;   // 15-degree steps, -3 (up) to 3 (down)
  float beamX0 = 0.0f, beamY0 = 0.0f, beamX1 = 0.0f, beamY1 = 0.0f; // cells
  int beamDir = 1;
  std::vector<CellBox> ponds; // the canal's and the bay's water: a heart, and back on dry land
  int photoX = -1, photoY = 0, photoSwing = 0; // cells: the trailer's EMPLOYEE OF THE MONTH photo
};

// The Top-Down Rampage bonus.
struct TopDownState
{
  bool on = false;
  int fx = 1, fy = 0; // the way the runner faces (shots go that way)
  int broken = 0;
};

} // namespace gr
