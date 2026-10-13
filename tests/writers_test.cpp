// Level 38's mechanics, played by hand in small rooms: a Red Pen shot
// strikes a WALL out (a blaster shot doesn't); a Typo from an inkwell
// rewrites it; a load-bearing FLOOR sketches itself back, but not while the
// runner is under it; walking up and down a red string; typing the bridge
// by landing on keys (a wrong key types nothing); the crusher and its
// CRUSHED sign; a Writer's Block (bullets tink off it, it is solid, its
// word kills it); a Deadline speeding up; a pen box coming back for a
// runner nearly out; Breakout's ball. Then the bot clears the Writers' Room
// with every runner, and Breakout.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "frontend/bot.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace gr;

namespace
{

int gFailures = 0;

void check(bool ok, const char* what)
{
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok)
    ++gFailures;
}

struct Wall
{
  int x0, y0, x1, y1;
  char c = '#';
};

// A 40 x 24 room with walls, a floor on rows 22 and 23, the runner at
// (px, py), plus `walls` (a '.' wall clears) and `entities`.
std::string room(int px, int py, const std::string& entities, const std::vector<Wall>& walls = {},
  const std::string& extra = "weapon=red_pen\n")
{
  const int h = 24;
  std::vector<std::string> map(std::size_t(h), std::string(40, '.'));
  for (int y = 0; y < h; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][1] = map[std::size_t(y)][38] = map[std::size_t(y)][39] = '#';
  for (int x = 0; x < 40; ++x)
    map[std::size_t(h - 2)][std::size_t(x)] = map[std::size_t(h - 1)][std::size_t(x)] = '#';
  for (const auto& w : walls)
    for (int y = w.y0; y <= w.y1; ++y)
      for (int x = w.x0; x <= w.x1; ++x)
        map[std::size_t(y)][std::size_t(x)] = w.c;
  map[std::size_t(py)][std::size_t(px)] = 'P';
  std::string s = "name=WRITERS TEST\nepisode=6\ntheme=writers_room\n" + extra + "[map]\n";
  for (const auto& row : map)
    s += row + "\n";
  s += "[entities]\n" + entities;
  return s;
}

struct Keys
{
  bool left = false, right = false, down = false, jump = false, fire = false, up = false;
};

void step(World& w, Keys pad, Keys& prev)
{
  PlayerInput p;
  p.left = pad.left;
  p.right = pad.right;
  p.up = pad.up;
  p.down = pad.down;
  p.jump = {pad.jump, pad.jump && !prev.jump};
  p.fire = {pad.fire, pad.fire && !prev.fire};
  w.update(p);
  prev = pad;
}

void idle(World& w, int frames)
{
  Keys prev;
  for (int f = 0; f < frames; ++f)
    step(w, Keys{}, prev);
}

void hold(World& w, Keys pad, int frames, Keys& prev)
{
  for (int f = 0; f < frames; ++f)
    step(w, pad, prev);
}

void tap(World& w, Keys pad, Keys& prev)
{
  step(w, pad, prev);
  step(w, Keys{}, prev);
}

int botPlay(World& w, int frames, Bot& bot)
{
  Input prev;
  int f = 0;
  for (; f < frames && w.state() == WorldState::Playing; ++f)
  {
    const Input in = bot.play(w);
    PlayerInput p;
    p.left = in.left;
    p.right = in.right;
    p.up = in.up;
    p.down = in.down;
    p.jump = {in.jump, in.jump && !prev.jump};
    p.fire = {in.fire, in.fire && !prev.fire};
    prev = in;
    w.update(p);
  }
  return f;
}

const Enemy* firstOf(const World& w, EnemyKind kind)
{
  for (const auto& e : w.enemies())
    if (e.alive && e.kind == kind)
      return &e;
  return nullptr;
}

const Word* word(const World& w, const char* id)
{
  for (const auto& wd : w.writers().words)
    if (wd.id == id)
      return &wd;
  return nullptr;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc < 3)
    return 2;
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    Keys none, right, left, up, fire, down, jump;
    right.right = true;
    left.left = true;
    up.up = true;
    fire.fire = true;
    down.down = true;
    jump.jump = true;
    auto make = [&](const std::string& text, int character = 0) {
      return World(std::make_shared<const Level>(Level::parse(text)), character, theme, art);
    };
    const std::string wallWord = "@ word w1 12 20 text=WALL rect=12,14,13,21\n";
    const std::vector<Wall> wall = {{12, 14, 13, 21}};

    {
      // A blaster shot passes a word by; a Red Pen shot strikes it, and
      // 16 frames later the wall it names is gone.
      World w = make(room(6, 21, wallWord, wall));
      Keys prev;
      tap(w, fire, prev);
      idle(w, 20);
      const bool blaster = word(w, "w1")->state == WordState::Intact && w.map().block(12, 18) == Tile::Solid;
      w.cheat(Cheat::Ammo);
      tap(w, fire, prev);
      idle(w, 6);
      const bool striking = word(w, "w1")->state == WordState::Striking && w.map().block(12, 18) == Tile::Solid;
      idle(w, kWrStrikeLine + kWrStrikeFade);
      check(blaster, "a blaster shot passes a word by");
      check(striking, "a Red Pen shot strikes the word out (the wall stays while the line draws)");
      check(word(w, "w1")->state == WordState::Struck && w.map().block(12, 18) == Tile::Empty,
        "the struck WALL is gone");
      hold(w, right, 40, prev);
      check(w.player().x > 30, "the runner walks on through where it stood");
    }
    {
      // A Typo crawls out of the inkwell, goes to the struck word and
      // rewrites it: the wall comes back (once the runner is clear of it).
      World w = make(room(6, 21, wallWord + "@ inkwell 20 21\n", wall));
      w.cheat(Cheat::Ammo);
      Keys prev;
      tap(w, fire, prev);
      idle(w, kWrStrikeLine + kWrStrikeFade + 2);
      const bool gone = w.map().block(12, 18) == Tile::Empty;
      bool rewriting = false;
      for (int f = 0; f < 400 && word(w, "w1")->state != WordState::Intact; ++f)
      {
        idle(w, 1);
        rewriting = rewriting || word(w, "w1")->state == WordState::Rewriting;
      }
      std::printf("     word state %d, typo %d\n", int(word(w, "w1")->state), firstOf(w, EnemyKind::Typo) != nullptr);
      check(gone && rewriting, "a Typo crawls out of the inkwell and rewrites the struck word");
      check(word(w, "w1")->state == WordState::Intact && w.map().block(12, 18) == Tile::Solid, "the wall is back");
    }
    {
      // A Typo touching the runner infects them.
      World w = make(room(16, 21, "@ inkwell 20 21\n"));
      idle(w, 160);
      check(w.player().virus > 0, "a Typo's touch gives the runner the Virus");
    }
    {
      // A load-bearing FLOOR: struck, the runner falls; it sketches itself
      // back 150 frames later, but not while the runner is under it.
      const std::string ents = "@ word w4 14 15 text=FLOOR rect=12,16,19,16 load=1\n";
      World w = make(room(16, 15, ents, {{12, 16, 19, 16, '='}}));
      w.cheat(Cheat::Ammo);
      Keys prev;
      // Off the plank to its left end and a jump: a shot from the air strikes it.
      const int y0 = w.player().y;
      step(w, left, prev);
      step(w, down, prev);
      w.cheat(Cheat::Ammo);
      for (auto f = 0; f < 3; ++f)
        tap(w, Keys{false, false, true, false, true, false}, prev);
      idle(w, kWrStrikeLine + kWrStrikeFade + 30);
      const bool fell = w.player().y > y0 + 6 && word(w, "w4")->state == WordState::Struck;
      idle(w, kWrResketchWait + kWrResketch + 10);
      const bool held = word(w, "w4")->state == WordState::Struck; // the runner is under it
      hold(w, left, 40, prev);
      hold(w, right, 0, prev);
      idle(w, kWrResketch + 10);
      std::printf("     fell %d held %d, now %d\n", fell, held, int(word(w, "w4")->state));
      check(fell, "striking the FLOOR drops the runner");
      check(held, "it doesn't sketch itself back over the runner");
      check(word(w, "w4")->state == WordState::Intact && w.map().block(15, 16) == Tile::Platform,
        "with the runner out from under it, the FLOOR sketches itself back");
    }
    {
      // A red string: up it a cell at a time, and back down it.
      World w = make(room(3, 21, "@ string pins=5,22,25,16\n"));
      Keys prev;
      const int y0 = w.player().y;
      hold(w, right, 30, prev);
      const int y1 = w.player().y;
      const bool walking = w.player().state == PlayerState::OnGround;
      hold(w, left, 30, prev);
      std::printf("     string: y %d -> %d -> %d\n", y0, y1, w.player().y);
      check(y1 < y0 - 6 && walking, "the runner walks up a red string");
      check(w.player().y == y0 && w.player().state == PlayerState::OnGround, "and back down it");
    }
    {
      // The typewriter: landing on a key types it; a wrong key types
      // nothing; the sentence builds the bridge a block a letter.
      std::string ents;
      const char* keys = "AB ";
      for (int i = 0; i < 3; ++i)
      {
        const int kx = 8 + 4 * i;
        ents += "@ key " + std::to_string(kx) + " 21 ch=" + (keys[i] == ' ' ? std::string("SPACE") : std::string(1, keys[i])) + "\n";
      }
      ents += "@ bridge rect=24,22,27,22 text=\"A BA\"\n";
      std::vector<Wall> walls = {{8, 21, 9, 21}, {12, 21, 13, 21}, {16, 21, 17, 21}, {24, 22, 27, 23, '.'}};
      World w = make(room(5, 21, ents, walls));
      Keys prev;
      auto landOn = [&](int kx) {
        // Hop toward the key and steer onto it.
        const int tx = kx * 2;
        for (int f = 0; f < 120; ++f)
        {
          const auto& p = w.player();
          if (f > 2 && p.state == PlayerState::OnGround && p.y + 1 == 42 && p.x + 1 >= tx && p.x + 1 <= tx + 3)
            break;
          Keys k;
          k.jump = p.state == PlayerState::OnGround ? !prev.jump : p.state == PlayerState::Jumping;
          k.right = p.x < tx;
          k.left = p.x > tx;
          step(w, k, prev);
        }
        idle(w, 2);
      };
      landOn(12); // B: wrong
      const int wrong = w.writers().bridge.typed;
      landOn(8); // A
      const int one = w.writers().bridge.typed;
      landOn(16); // space
      landOn(12); // B
      landOn(8);  // A
      std::printf("     typed %d %d -> %d\n", wrong, one, w.writers().bridge.typed);
      check(wrong == 0 && one == 1, "a wrong key types nothing, the right one types its letter");
      check(w.writers().bridge.typed == 4 && w.map().block(24, 22) == Tile::Solid && w.map().block(27, 22) == Tile::Solid,
        "typing the sentence builds the bridge");
    }
    {
      // The crusher kills a runner under it; struck out (its sign), it stops.
      const std::string ents = "@ crusher 16 13 w=4 to=21\n@ word w10 8 20 text=CRUSHED target=crusher\n";
      World w = make(room(17, 21, ents));
      idle(w, kWrCrusherPeriod + 10);
      check(w.player().state == PlayerState::Dying || w.stats().deaths > 0, "the crusher press kills a runner under it");
      World w2 = make(room(4, 21, ents));
      w2.cheat(Cheat::Ammo);
      Keys prev;
      tap(w2, fire, prev);
      idle(w2, kWrStrikeLine + kWrStrikeFade + 2);
      hold(w2, right, 26, prev);
      idle(w2, kWrCrusherPeriod * 2);
      std::printf("     crusher gone %d, runner %d at %d\n", w2.writers().crushers[0].gone, int(w2.player().state), w2.player().x);
      check(w2.writers().crushers[0].gone && w2.stats().deaths == 0 && w2.player().state != PlayerState::Dying,
        "struck out, the crusher is gone and the runner stands under it safe");
    }
    {
      // A Writer's Block: solid where it sits, bullets tink off it, its word kills it.
      World w = make(room(6, 21, "@ writersblock 16 21\n"));
      Keys prev;
      const bool solid = w.map().block(16, 21) == Tile::Solid && w.map().block(17, 20) == Tile::Solid;
      tap(w, fire, prev);
      idle(w, 20);
      const Enemy* b = firstOf(w, EnemyKind::WritersBlock);
      const bool tink = b && b->hp == enemyDef(b->def).hp;
      w.cheat(Cheat::Ammo);
      tap(w, fire, prev);
      idle(w, kWrStrikeLine + kWrStrikeFade + 4);
      check(solid, "a sitting Writer's Block is solid");
      check(tink, "bullets don't hurt a Writer's Block");
      check(!firstOf(w, EnemyKind::WritersBlock) && w.map().block(16, 21) == Tile::Empty && w.stats().score >= 1000,
        "striking BLOCK kills it (and scores)");
    }
    {
      // A Writer's Block slides into the way of a runner coming at it.
      World w = make(room(4, 21, "@ writersblock 16 21\n"));
      Keys prev;
      hold(w, right, 3, prev);
      const int x0 = firstOf(w, EnemyKind::WritersBlock)->x;
      idle(w, kWrBlockTell + 20);
      const int x1 = firstOf(w, EnemyKind::WritersBlock)->x;
      std::printf("     block %d -> %d (runner %d)\n", x0, x1, w.player().x);
      check(x1 < x0 && x1 % 2 == 0 && w.map().block(x1 / 2, 21) == Tile::Solid, "a Writer's Block slides into the runner's way and sits");
    }
    {
      // A Deadline speeds up with every lap of its minute hand.
      World w = make(room(4, 21, "@ deadline 14 21 lane=10,20\n"));
      idle(w, 2);
      const auto& d = w.writers().deadlines[0];
      const int s0 = d.step;
      idle(w, kWrDeadlineLap * 2 + 2);
      std::printf("     deadline step %d -> %d\n", s0, d.step);
      check(s0 == 0 && d.step == 2, "a Deadline speeds up every lap of its minute hand");
    }
    {
      // A pen box comes back for a runner nearly out of Red Pen.
      World w = make(room(6, 21, "@ penbox 10 21\n", {{10, 21, 10, 21, 'W'}}));
      Keys prev;
      tap(w, Keys{false, false, true, false, true, false}, prev); // a crouched shot opens the box
      idle(w, 12);
      hold(w, right, 12, prev);
      idle(w, 4);
      const bool took = w.player().weapon == Weapon::Proto && w.player().proto == int(ProtoId::RedPen);
      hold(w, left, 12, prev);
      for (int i = 0; i < 16; ++i)
      {
        tap(w, fire, prev);
        idle(w, 4);
      }
      idle(w, kWrPenRespawn + 4);
      std::printf("     took %d ammo %d box %d\n", took, w.player().ammo, w.boxes()[0].alive);
      check(took && w.boxes()[0].alive, "a pen box comes back for a runner nearly out");
    }
    {
      // Breakout: fire launches the ball; it breaks a brick and comes back.
      std::string ents = "@ brick 18 4 ch=T\n@ brick 20 4 ch=H gem=1\n";
      World w = make(room(19, 21, ents, {}, "rules=breakout\ntimer=90\ngoal=collect:60\n"));
      Keys prev;
      tap(w, fire, prev);
      const bool launched = w.writers().ball;
      Bot bot;
      botPlay(w, 900, bot);
      std::printf("     broken %d balls %d\n", w.writers().broken, w.writers().balls);
      check(launched, "fire launches the ball");
      check(w.writers().broken == 2, "the ball breaks the bricks");
    }

    // The bot clears the Writers' Room and Breakout with every runner.
    const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
    const auto bonus = std::make_shared<const Level>(Level::loadFile(argv[2]));
    const int only = std::getenv("GR_WRITERS_RUNNER") ? std::atoi(std::getenv("GR_WRITERS_RUNNER")) : -1;
    for (int character = 0; character < 6; ++character)
    {
      if (only >= 0 && character != only)
        continue;
      World w(level, character, theme, art);
      Bot bot;
      const int f = botPlay(w, 9000, bot);
      std::printf("     runner %d: %d frames, %d deaths, route %d\n", character, f, w.stats().deaths, w.writers().routeAt);
      check(w.state() != WorldState::Playing, "the bot clears the Writers' Room");
    }
    for (int character = 0; character < 6; ++character)
    {
      if (only >= 0 && character != only)
        continue;
      World w(bonus, character, theme, art);
      Bot bot;
      const int f = botPlay(w, 1350, bot);
      std::printf("     runner %d: %d frames, %d bricks\n", character, f, w.writers().broken);
      check(w.state() != WorldState::Playing && !w.bonusFailed(), "the bot clears Breakout");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures == 0 ? "all writers tests passed\n" : "%d writers test(s) failed\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
