#pragma once

// Level 23, Fright Night Manor (SPEC.md 23): the Mirror World. The level has
// a side, the real manor or its reflection; tiles, enemies, boxes and items
// tagged `side=` are there only on their side. Mirrors flip it (Up in front
// of one), Portrait Ghosts chase through walls and, pinned by a Silver
// Crossbow bolt, are a platform; Haunted Armors swing their halberds;
// Poltergeists throw things. The Both Sides bonus (rules=split_mirror)
// steers two runners at once, the bottom one with left and right swapped.
// See world_manor.cpp.

#include <cstdint>
#include <string>
#include <vector>

namespace gr
{

constexpr int kSideReal = 0;
constexpr int kSideMirror = 1;
constexpr int kMirrorShimmer = 12;  // frames of the step through (no control)
constexpr int kPinFrames = 75;      // a Silver Crossbow bolt holds what it hits
constexpr int kGhostReturn = 90;    // frames until a Portrait Ghost comes back out
constexpr int kGhostWake = 10;      // blocks: a ghost leaves its portrait
constexpr int kGhostLeash = 18;     // blocks: farther, it drifts back in
constexpr int kGhostTell = 10;
constexpr int kGhostLunge = 18;
constexpr int kGhostReach = 14; // cells from the runner where a ghost stops to tell
constexpr int kArmorTell = 12;
constexpr int kPolterTell = 12;
constexpr int kLightningEvery = 300; // +- 60 frames
constexpr int kBoltRespawn = 300;
constexpr int kThirdWorld = 15;     // frames a shot broken mirror shows the crypt
constexpr int kBackwardsHold = 30;  // the reflection that stands still (easter egg)

enum class MirrorKind : std::uint8_t
{
  Normal, // flips the side
  Broken, // shot: shows the third world for kThirdWorld frames; Up then: to the crypt
  Crypt,  // the crypt's mirror: back to its broken mirror
};

struct ManorMirror
{
  std::string id;
  int x = 0, y = 0; // blocks, top-left (2 x 3)
  MirrorKind kind = MirrorKind::Normal;
  int third = 0;    // Broken: frames of the third world left
  int to = -1;      // Broken / Crypt: the mirror it leads to
  int toX = -1, toY = -1; // Broken: where the runner lands (cells, bottom-left)
};

struct ManorLever
{
  std::string id;
  int x = 0, y = 0; // blocks
  int state = 0;
  int swing = 0;    // frames of the throw animation
};

// A layer that is solid on one side only (index into World::mLayers).
struct SideLayer
{
  int layer = -1;
  int side = kSideReal;
};

// The ballroom door: a layer solid until all its levers are thrown.
struct LeverDoor
{
  int layer = -1;
  std::vector<int> levers;
};

// One step of the bot's way through the manor: a mirror to step through to
// reach a side, or a lever to throw.
struct ManorStep
{
  int mirror = -1, side = kSideReal;
  int lever = -1;
};

// Scenery the art draws (portraits, chandeliers, the front door...).
struct ManorDeco
{
  std::string kind, text;
  int side = -1;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks
};

// The twin in Both Sides: the bottom runner (see World::mTwin).
struct SplitMirror
{
  bool on = false;
  int divider = 11;          // blocks: rows divider, divider + 1 between the halves
  int twinX = 0, twinY = 0;  // cells: where the bottom runner starts
  bool topOn = false, botOn = false; // standing on their exits
  int topExitX = 0, topExitY = 0;     // blocks
  int botExitX = 0, botExitY = 0;
  int lastTopX = 0, lastTopY = 0;     // cells: the last floor each stood on
  int lastBotX = 0, lastBotY = 0;
};

struct ManorState
{
  bool on = false;
  int side = kSideReal;
  int flip = 0;            // frames of the shimmer left (no control)
  int flipTo = -1;         // the mirror being stepped into
  bool backwards = false;  // stepped in with Down (easter egg)
  int reflection = 0;      // frames the reflection stands still on the other side
  int reflX = 0, reflY = 0; // cells: where it stands
  std::vector<ManorMirror> mirrors;
  std::vector<ManorLever> levers;
  std::vector<SideLayer> layers;
  std::vector<LeverDoor> doors;
  std::vector<int> enemySide;  // per enemy index: -1 both, else its side
  std::vector<int> boxSide;    // per box index
  std::vector<int> parkedBoxes; // put away while their side is gone (items: Item::parked)
  int exitSide = -1;
  std::vector<ManorDeco> decos;
  bool prevUp = false, prevDown = false;
  int lastFlip = -1;       // the mirror stepped through last
  bool stepFresh = true;   // the route's step just became current
  // Secret 2: Lance's portrait in the reflected west gallery.
  int lanceX = -1, lanceY = -1, lanceScore = 10000, lanceSide = -1;
  bool lanceShot = false;
  int lanceWink = 0;
  int coffinW = 4;
  // The gallery hole the bot bridges with pinned ghosts (blocks).
  int bridgeEdge = -1, bridgeLanding = -1, bridgeRow = -1;
  int lightning = kLightningEvery; // frames to the next flash
  int flash = 0;           // frames of the flash left
  std::uint32_t seed = 23;
  // Silver Crossbow pins on Portrait Ghosts: the platform tiles they put down.
  struct GhostPlatform
  {
    int enemy = -1;
    int bx = 0, by = 0;    // blocks: the left block of the 2-block top
    int set = 0;           // bit k: block bx + k was put down (was empty)
  };
  std::vector<GhostPlatform> platforms;
  struct Portrait
  {
    int x = 0, y = 0;      // blocks
    int ghost = -1;        // enemy index
    int back = 0;          // frames until it comes out again (after a kill)
    bool out = false;
  };
  std::vector<Portrait> portraits;
  int boltBox = -1;        // the refill box (box index) and its timer
  int boltT = 0;
  int coffin = 0;          // frames of the coffin's lid lifting
  bool coffinWoke = false;
  int coffinX = -1, coffinY = -1, signX = -1, signY = -1; // blocks
  std::vector<ManorStep> route;
  int step = 0;
  int checkpoints = -1;
  int cpSide = kSideReal;
  int plates = 0;          // P2's plates thrown (every third infects)
  SplitMirror split;
  bool anyFlip() const { return flip > 0; }
};

} // namespace gr
