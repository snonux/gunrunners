#pragma once

#include "data/level.hpp"
#include "game/collision.hpp"

#include <string>
#include <vector>

namespace gr
{

// Level 38, Writers' Room (world_writers.cpp, world_writers_draw.cpp): the
// script of the show, three floors that zigzag upward. Handwritten words
// sit on the things they name, and a Red Pen shot strikes a word out: the
// WALL, the SPIKES, the GUARD, a Writer's BLOCK, the CRUSHED press, the
// LANCE WINS door are gone. Typos crawl out of inkwells and rewrite struck
// words; load-bearing ones (FLOOR, LADDER) sketch themselves back. Red
// strings between corkboard pins are walkable slopes, the typewriter's keys
// type the bridge sentence, Deadlines speed up with every lap of their
// minute hand. The Breakout bonus (rules=breakout) is in the same files.

// Words.
constexpr int kWrStrikeLine = 8;     // frames the red line takes to draw
constexpr int kWrStrikeFade = 8;     // frames the target takes to fade out
constexpr int kWrRewrite = 30;       // frames a Typo takes to rewrite a word
constexpr int kWrRewriteShow = 8;    // the letters are all back this long before the target
constexpr int kWrResketchWait = 150; // frames before a load-bearing word sketches itself back
constexpr int kWrResketch = 22;      // frames of pencil
// Typos.
constexpr int kWrTypoEvery = 300;    // frames between Typos from one inkwell
constexpr int kWrTypoReach = 40;     // cells: how far a Typo goes for a struck word
constexpr int kWrTypoTell = 8;       // frames of letter scramble before it turns on the runner
constexpr int kWrTypoSight = 20;     // cells
// Deadline: cells a frame, x4, for each lap of its minute hand.
constexpr int kWrDeadlineSpeeds[5] = {2, 3, 4, 6, 8};
constexpr int kWrDeadlineLap = 60;
constexpr int kWrDeadlineBell = 12;  // frames of alarm bell before a speed-up
// Writer's Block.
constexpr int kWrBlockSlide = 16;    // cells: its longest slide (8 blocks)
constexpr int kWrBlockCool = 45;     // frames between slides
constexpr int kWrBlockTell = 15;     // frames of scraping before a slide
constexpr int kWrBlockSight = 24;    // cells
// Sketch guard.
constexpr int kWrGuardRange = 30;    // cells
// The crusher press.
constexpr int kWrCrusherPeriod = 60;
constexpr int kWrCrusherTell = 15;
// The Red Pen's boxes come back for a runner who is nearly out.
constexpr int kWrPenRespawn = 150;
constexpr int kWrPenLow = 10;
// Easter eggs.
constexpr int kWrEggHatch = 45;

enum class WordState
{
  Intact,
  Striking,  // the red line, then the target fades
  Struck,    // gone (t counts toward a load-bearing word's re-sketch)
  Rewriting, // a Typo writes it back in (t to kWrRewrite)
  Sketching, // a load-bearing word sketches itself back (t to kWrResketch)
  Waiting,   // written back; the target waits for the runner to leave
};

enum class WordTarget
{
  Rect,    // blocks of the map (a wall, spikes, a floor, a ladder, a door)
  Enemy,   // the GUARD and every Writer's BLOCK: striking kills it
  Crusher, // the press
  Egg,     // the EASTER EGG sticky note
};

// `@ word ID x y text= rect=x0,y0,x1,y1 target=crusher|egg load=0|1`: a
// handwritten word, (x, y) its left block; a block per two letters.
struct Word
{
  std::string id, text;
  int x = 0, y = 0, w = 2; // cells: top-left and width; 2 cells tall
  WordTarget target = WordTarget::Rect;
  int enemy = -1;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // blocks: the rect it names
  std::vector<Tile> tiles;            // what the rect holds
  bool load = false;
  WordState state = WordState::Intact;
  int t = 0;
  int typo = -1; // the Typo rewriting it (an enemy index)
  CellBox box() const { return {x, y, w, 2}; }
  bool gone() const { return state == WordState::Struck || state == WordState::Rewriting || state == WordState::Sketching || state == WordState::Waiting || (state == WordState::Striking && t >= kWrStrikeLine + kWrStrikeFade); }
};

// `@ inkwell x y`: Typos crawl out of it, two at most at a time.
struct Inkwell
{
  int x = 0, y = 0; // cells: bottom-left, feet row
  int timer = 0;
  int typos[2] = {-1, -1}; // enemy slots
};

struct Typo
{
  int enemy = -1;
  int word = -1;   // the struck word it goes for
  int tell = 0;
  int wander = 1;
};

struct Deadline
{
  int enemy = -1;
  int x0 = 0, x1 = 0; // cells: its patrol
  int step = 0;       // 0..4: its speed
  int lap = 0;        // frames into the minute hand's lap
  int part = 0;       // quarter cells carried
};

struct WritersBlock
{
  int enemy = -1;
  int word = -1;
  int slide = 0;            // cells left of this slide (sign: direction)
  int stampX = -1, stampY = -1; // blocks: where its solid blocks are (-1 none)
};

// `@ crusher x row w= to=`: a press over x..x+w-1, its face resting at
// `row`; it slams down to `to` every kWrCrusherPeriod frames.
struct Crusher
{
  int x0 = 0, w = 4;   // blocks
  int rest = 0, to = 0; // blocks: the face's row up and down
  int t = 0;            // frames into the cycle
  int face = 0;         // cells: the face's bottom row now
  bool gone = false;
  CellBox box() const { return {x0 * kCellsPerTile, 4, w * kCellsPerTile, face - 3}; }
};

// `@ key x y ch=`: a typewriter key, 2 blocks wide and 1 tall, solid.
struct TypeKey
{
  int x = 0, y = 0; // blocks
  char ch = 'A';
  int press = 0;    // frames of the key going down
};

// `@ bridge rect=x0,row,x1,row text=`: each correct letter typed adds a block.
struct WordBridge
{
  int x0 = 0, x1 = 0, row = 0; // blocks
  std::string text;
  int typed = 0;
  int wrong = 0; // frames of the wrong key's clack shown
};

// `@ card x y text=`: a rejected pitch pinned to the corkboard; shots spin it.
struct PitchCard
{
  int x = 0, y = 0; // cells
  std::string text;
  int spin = 0;
};

// The EASTER EGG: the note becomes an egg, falls, hatches, the chick follows.
struct WritersEgg
{
  int state = 0;   // 0 note, 1 falling, 2 hatching, 3 a chick, 4 gone home
  float x = 0, y = 0, vy = 0;
  int t = 0;
  int cheep = 0;
  int checkpoints = 0; // checkpoints touched when it hatched
};

// The bot's way through (`@ step x y act=strike|type|go arg=`).
struct WritersStep
{
  int x = 0, y = 0; // cells: feet
  std::string act, arg;
  int dir = 1;
};

// Breakout (rules=breakout): bricks, the ball-shot, the runner as paddle.
struct Brick
{
  int x = 0, y = 0; // cells: top-left, 4 x 2
  char ch = ' ';
  int hp = 1;
  bool red = false, gem = false;
  bool alive = true;
};

struct WritersState
{
  bool on = false;
  std::vector<Word> words;
  std::vector<Inkwell> inkwells;
  std::vector<Typo> typos;
  std::vector<Deadline> deadlines;
  std::vector<WritersBlock> blocks;
  std::vector<int> guards; // sketch guards (enemy indices)
  std::vector<Crusher> crushers;
  std::vector<TypeKey> keys;
  WordBridge bridge;
  std::vector<PitchCard> cards;
  std::vector<int> pins; // cells: x, top, x, top... along the strings
  std::vector<int> penBoxes;  // boxes that come back (indices into the boxes)
  std::vector<int> penTimers;
  WritersEgg egg;
  int pitchX = -1, pitchY = -1; // the original pitch on the top shelf (cells)
  bool pitchTaken = false;
  std::vector<WritersStep> route;
  int routeAt = 0;
  bool wasGrounded = true; // the runner stood on something last frame
  int scramble = 0;        // frames a HUD word stays scrambled (a Typo's joke)
  // Breakout.
  bool breakout = false;
  std::vector<Brick> bricks;
  bool ball = false;
  float bx = 0, by = 0, bvx = 0, bvy = 0; // cells
  int balls = 3;
  int broken = 0;
};

} // namespace gr
