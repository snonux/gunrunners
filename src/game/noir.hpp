#pragma once

#include "game/collision.hpp"

#include <string>
#include <vector>

namespace gr
{

// Level 27, The Long Rain (world_noir.cpp, world_noir_draw.cpp): stealth in
// a black and white city. The runner is hidden in shadows and dark rooms
// (out of the lamps' light); Flatfeet see along their flashlights, the club
// spotlight sweeps the floor, Crows caw at a runner close by. A runner seen
// for 8 frames is spotted: the whistle blows and Goons come out of the
// nearest door for the alarm's 15 seconds. The Silencer is never heard and
// hits an unaware enemy three times as hard. The Silent Picture bonus
// (rules=silent_film) is FilmState, also in world_noir.cpp.

constexpr int kAlarmFrames = 225;  // 15 s
constexpr int kSpotFill = 8;       // frames in a cone before the runner is spotted
constexpr int kGoonEvery = 30;     // frames between Goons out of a door
constexpr int kGoonMax = 2;
constexpr int kDoorReach = 32;     // cells: Goons come out of a door this close
constexpr int kHearReach = 20;     // cells: a loud shot is heard this far
constexpr int kHearTurn = 60;      // frames a Flatfoot looks toward a sound
constexpr int kCrowReach = 12;     // cells: a Crow caws at a runner this close
constexpr int kFlashLen = 16;      // cells: a flashlight's reach
constexpr int kFlashHalf = 20;     // degrees either side of a flashlight's aim
constexpr int kBurstShots = 3;
constexpr int kBurstGap = 3;       // frames between a burst's shots
constexpr int kCleanCase = 2000;
constexpr int kCaseClosed = 25000;
constexpr int kRainCoat = 300;     // frames in the rain for the trench coat

// `@ light x y r=`: a lamp; r in blocks.
struct NoirLight
{
  int x = 0, y = 0, r = 4; // cells
};

// `@ spotlight x= y= sweep=x0,x1 period= cone=width,length`: its lit spot
// sweeps along the floor and back.
struct NoirSpot
{
  int x = 0, y = 0;           // cells: the lamp
  int x0 = 0, x1 = 0;         // cells: the sweep's ends on the floor
  int floor = 0;              // cells: the row the spot moves along
  int period = 120;
  int half = 10;              // degrees either side of its aim
  int len = 20;               // cells
};

// `@ door x y`: where Goons come out during an alarm.
struct NoirDoor
{
  int x = 0, y = 0; // cells: a Goon's bottom-left
};

struct NoirState
{
  bool on = false;
  std::vector<CellBox> shadows, darks, rains;
  std::vector<NoirLight> lights;
  std::vector<NoirSpot> spots;
  std::vector<NoirDoor> doors;
  // This frame: hidden, in a cone (and whose), the "?" over the runner.
  bool hidden = false;
  bool inCone = false;
  int suspicion = 0;
  // The alarm.
  int alarm = 0;          // frames left
  int alarms = 0;         // alarms all level (Case Closed: none)
  bool segmentAlarm = false; // an alarm since the last checkpoint
  int whistleCool = 0;    // after an alarm, frames before anyone may whistle again
  int goonIn = 0;         // frames to the next Goon
  int burstCool = 0;      // the Goons' shared burst timer
  int burstBy = -1;       // the Goon firing (enemy index)
  int burstShots = 0;
  int alarmFlash = 0;
  // A loud shot heard (cells), and how long ago.
  int noiseX = 0, noiseY = 0, noiseT = -1;
  // Props (blocks).
  int jukeX = -1, jukeY = 0;
  bool jukebox = false;   // playing in a minor key
  int noteX = -1, noteY = 0;
  bool noteRead = false;
  int paintX = -1, paintY = 0;
  bool paintOpen = false;
  int ladderX = -1, ladderY0 = 0, ladderY1 = 0; // the safe's ladder up into the booth
  int alarmX = -1, alarmY = 0;                  // the office alarm (shootable)
  int shutterX = -1, shutterY0 = 0, shutterY1 = 0;
  bool shutterOpen = false;
  int prizeX = -1, prizeY = 0, prizeScore = 2000;
  bool prizeTaken = false;
  std::string prizeKind;
  // F0: the alley's scripted Flatfoot (an enemy index), out once the
  // runner reaches the trigger (cells).
  int f0 = -1, f0Trigger = 0;
  bool f0Out = false;
  // The smoke cloud (the Virus skinned as smoke) drifting on its path.
  int smoke = -1;         // item index
  int smokeX0 = 0, smokeX1 = 0, smokeEvery = 4, smokeDir = 1;
  std::string smokeSkin;
  int smokeAt = -1;       // x*1000+y of the Virus it skins (blocks), until linked
  // The trench coat: frames stood in the rain.
  int rainFrames = 0;
  bool coat = false;
  int coatT = 0; // frames since the coat went on (drawing)
};

// The Silent Picture bonus (rules=silent_film).
struct FilmHat
{
  float x = 0.0f, y = 0.0f, vx = 0.0f, vy = 0.0f; // cells
  int t = 0;
};

struct FilmState
{
  bool on = false;
  bool odd = false;     // every other frame runs twice (1.5x speed)
  std::vector<CellBox> peels;
  int slide = 0;        // cells of a banana-peel slide left
  int slideDir = 1;
  int fallTop = -1;     // the highest row (cells) of the runner's fall, -1 on the ground
  int pratfall = 0;     // frames of the bounce left
  int card = 0;         // frames of a title card left
  std::string cardText;
  int room = -1;
  std::vector<int> roomX; // cells: where each room starts
  std::vector<std::string> cards;
  // The pram: rolls down the street's steps and off the end, then again.
  int pramX0 = -1, pramX1 = 0, pramEvery = 90; // cells
  float pramX = 0.0f, pramV = 0.0f;
  int pramY = 0;        // cells: its bottom row
  int pramT = 0;        // frames to the next pram
  bool pramOn = false;
  // The ladder gag: climb to the top and it tips over into a bridge.
  int gagX = -1, gagY0 = 0, gagY1 = 0, gagLen = 0; // blocks: the ladder, and the bridge's length
  int gagState = 0;     // 0 standing, 1 tipping, 2 a bridge
  int gagT = 0;
  std::vector<FilmHat> hats;
};

} // namespace gr
