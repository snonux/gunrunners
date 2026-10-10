// Level 25's mechanics, played by hand in small rooms: the day and dusk
// platforms, a bud the Chrono Mister opens, a seed it sprouts, a Soldier Ant
// column turning (and a sick one infecting), a Jumping Spider's leap (and
// missing a runner who moves), a Bombardier Beetle's jet, the pond, the
// dandelion going to seed (misted, or at the sweep), the ladybug waking the
// bonus door, the rare butterfly, the narrator. Then the bot plays the whole
// level starting at dusk, and Dandelion Drift.

#include "assets/art.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "frontend/bot.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

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

// A 40 x 24 room: ceiling row 0, floor rows 22-23, walls at x 0 and 39, the
// runner at (x, 21), plus whatever `rows` and `entities` say.
std::string room(int px, const std::vector<std::pair<int, std::string>>& rows, const std::string& entities)
{
  std::vector<std::string> map(24, std::string(40, '.'));
  for (int x = 0; x < 40; ++x)
    map[0][std::size_t(x)] = map[22][std::size_t(x)] = map[23][std::size_t(x)] = '#';
  for (int y = 0; y < 24; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][39] = '#';
  for (const auto& r : rows)
    for (std::size_t i = 0; i < r.second.size(); ++i)
      if (r.second[i] != ' ')
        map[std::size_t(r.first)][1 + i] = r.second[i];
  map[21][std::size_t(px)] = 'P';
  std::string s = "name=GARDEN TEST\nepisode=4\ntheme=macro_garden\nweapon=chrono_mister\n[map]\n";
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

void hold(World& w, Keys pad, int frames, Keys& prev)
{
  for (int f = 0; f < frames; ++f)
    step(w, pad, prev);
}

void idle(World& w, int frames)
{
  Keys prev;
  for (int f = 0; f < frames; ++f)
    step(w, Keys{}, prev);
}

int firstOf(const World& w, EnemyKind kind)
{
  for (std::size_t i = 0; i < w.enemies().size(); ++i)
    if (w.enemies()[i].kind == kind)
      return int(i);
  return -1;
}

const Prop* bonusDoor(const World& w)
{
  for (const auto& pr : w.props())
    if (pr.kind == PropKind::BonusDoor)
      return &pr;
  return nullptr;
}

// The bot plays `w` for at most `frames`; returns the frames it took.
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
    Keys mist;
    mist.fire = true;

    {
      // Day platforms are there for the first half of the cycle, dusk ones
      // for the second.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, {},
        "@ layer day_0 rect=10,15,12,15 tile== driver=timer on=405 off=405 phase=0\n"
        "@ layer dusk_0 rect=20,15,22,15 tile== driver=timer on=405 off=405 phase=405\n")));
      World w(level, 0, theme, art);
      idle(w, 5);
      check(w.garden().cycle && !w.gardenDusk(), "the garden starts at day");
      check(w.map().block(10, 15) == Tile::Platform && w.map().block(20, 15) == Tile::Empty, "day: the flower, no mushroom");
      idle(w, kGardenHalf + 5);
      check(w.gardenDusk(), "half a cycle on it is dusk");
      check(w.map().block(10, 15) == Tile::Empty && w.map().block(20, 15) == Tile::Platform, "dusk: the mushroom, no flower");
    }
    {
      // The mister opens a bud into a platform for 150 frames, and sprouts a
      // seed into a stalk that stays.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, {}, "@ bud 8 20 w=3\n@ seed x=8 y=21 h=6\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      check(w.garden().buds.size() == 1 && w.garden().seeds.size() == 1, "the bud and the seed are set up");
      const int ammo = w.player().ammo;
      Keys prev;
      hold(w, mist, 8, prev);
      hold(w, Keys{}, 1, prev);
      check(w.player().ammo == ammo - 2, "the mister takes a unit of ammo every 4 frames");
      check(w.garden().buds[0].open > 0 && w.map().block(8, 20) == Tile::Platform, "the misted bud opens");
      idle(w, kSeedSprout + 2);
      check(w.garden().seeds[0].grown && w.map().block(8, 16) == Tile::Ladder && w.map().block(8, 21) == Tile::Ladder,
        "the misted seed grows a stalk");
      idle(w, kBudOpen);
      check(w.garden().buds[0].open == 0 && w.map().block(8, 20) == Tile::Empty, "the bud closes again");
      check(w.map().block(8, 16) == Tile::Ladder, "the stalk stays");
    }
    {
      // A Soldier Ant column marches harmlessly; walked into, it halts with
      // its antennae up, then turns on the runner.
      const auto level = std::make_shared<const Level>(Level::parse(room(4, {},
        "@ antcolumn AC path=10,21;20,21 count=3 period=200\n")));
      World w(level, 0, theme, art);
      idle(w, 30);
      const auto& c = w.garden().columns[0];
      int out = 0;
      for (int a : c.ants)
        out += !w.enemies()[std::size_t(a)].hidden;
      check(out >= 2 && w.player().hp == w.player().maxHp, "the column marches, harmless");
      const int hp = w.player().hp;
      Keys prev, right;
      right.right = true;
      bool tell = false;
      for (int f = 0; f < 60 && !tell; ++f)
      {
        step(w, right, prev);
        tell = w.garden().columns[0].tell > 0;
      }
      check(tell, "touched, the column halts (antennae up)");
      check(w.player().hp == hp, "the touch doesn't hurt");
      hold(w, Keys{}, kAntTell + 30, prev);
      check(w.garden().columns[0].hostile > 0 && w.player().hp < hp, "then the column bites");
    }
    {
      // Out half the period, in the anthill the other half.
      const auto level = std::make_shared<const Level>(Level::parse(room(4, {},
        "@ antcolumn AC path=10,21;20,21 count=3 period=200\n")));
      World w(level, 0, theme, art);
      idle(w, 110);
      int out = 0;
      for (int a : w.garden().columns[0].ants)
        out += !w.enemies()[std::size_t(a)].hidden;
      check(out == 0, "the column goes back into the anthill for half its period");
    }
    {
      // A sick column: a hostile ant clings and infects.
      const auto level = std::make_shared<const Level>(Level::parse(room(4, {},
        "@ antcolumn AC path=10,21;20,21 count=3 period=200 carrier=1\n")));
      World w(level, 0, theme, art);
      idle(w, 30);
      Keys prev, right;
      right.right = true;
      for (int f = 0; f < 60 && w.garden().columns[0].tell == 0; ++f)
        step(w, right, prev);
      hold(w, Keys{}, kAntTell + 30, prev);
      check(w.player().virus > 0, "a sick ant infects the runner");
    }
    {
      // A Jumping Spider crouches when it sees the runner, then leaps where
      // the runner stood.
      const auto level = std::make_shared<const Level>(Level::parse(room(20, {}, "@ jumping_spider JS 28 21\n")));
      World w(level, 0, theme, art);
      const int s = firstOf(w, EnemyKind::JumpingSpider);
      const int hp = w.player().hp;
      bool crouched = false;
      for (int f = 0; f < 30 && !crouched; ++f)
      {
        idle(w, 1);
        crouched = w.enemies()[std::size_t(s)].attach == 1;
      }
      check(crouched, "the spider crouches");
      idle(w, 40);
      check(w.player().hp < hp, "and lands on a runner who stood still");
    }
    {
      // Moving while it crouches makes it miss.
      const auto level = std::make_shared<const Level>(Level::parse(room(20, {}, "@ jumping_spider JS 28 21\n")));
      World w(level, 0, theme, art);
      const int s = firstOf(w, EnemyKind::JumpingSpider);
      const int hp = w.player().hp;
      for (int f = 0; f < 30 && w.enemies()[std::size_t(s)].attach != 1; ++f)
        idle(w, 1);
      Keys prev, left;
      left.left = true;
      hold(w, left, 12, prev);
      hold(w, Keys{}, 25, prev);
      check(w.enemies()[std::size_t(s)].attach == 0 && w.player().hp == hp, "a runner who moved is missed");
    }
    {
      // A Bombardier Beetle jets a runner behind it.
      const auto level = std::make_shared<const Level>(Level::parse(room(11, {}, "@ bombardier_beetle BB 14 21 dir=r\n")));
      World w(level, 0, theme, art);
      const int hp = w.player().hp;
      idle(w, 30);
      check(w.player().hp < hp, "the beetle's jet hurts a runner behind it");
    }
    {
      // But not one in front of it.
      const auto level = std::make_shared<const Level>(Level::parse(room(19, {}, "@ bombardier_beetle BB 14 21 dir=r\n")));
      World w(level, 0, theme, art);
      const int hp = w.player().hp;
      idle(w, 20);
      check(w.player().hp == hp, "a runner in front of it is safe");
    }
    {
      // The pond: a heart, and back on the bank.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, {{22, "         ....."}},
        "@ fluid kind=pond rect=10,22,14,22\n")));
      World w(level, 0, theme, art);
      const int hp = w.player().hp;
      Keys prev, right;
      right.right = true;
      bool splashed = false;
      for (int f = 0; f < 40 && !splashed; ++f)
      {
        step(w, right, prev);
        splashed = w.player().hp < hp;
      }
      check(splashed && w.player().hp == hp - 1, "falling in the pond costs a heart");
      check(w.player().x < 20 && w.player().state == PlayerState::OnGround, "and puts the runner back on the bank");
    }
    {
      // The dandelion: misted at its base, the head goes to seed and the
      // seed head comes.
      const auto level = std::make_shared<const Level>(Level::parse(room(16, {},
        "@ layer day_0 rect=30,15,31,15 tile== driver=timer on=405 off=405 phase=0\n"
        "@ dandelion dl x=20 y=21\n"
        "@ platform 22 21 seedhead w=2 path=22,21;22,10 speed=1/2 mode=pingpong wait=rider\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      check(w.garden().seedhead >= 0 && w.platforms()[std::size_t(w.garden().seedhead)].hidden, "the seed head waits, hidden");
      Keys prev;
      hold(w, mist, 4, prev);
      hold(w, Keys{}, kDandelionSeed + 2, prev);
      check(w.garden().seeded && !w.platforms()[std::size_t(w.garden().seedhead)].hidden, "misted, the dandelion goes to seed");
    }
    {
      // Unmisted, it goes to seed at the next sweep.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, {},
        "@ layer day_0 rect=30,15,31,15 tile== driver=timer on=405 off=405 phase=0\n"
        "@ dandelion dl x=20 y=21\n"
        "@ platform 22 21 seedhead w=2 path=22,21;22,10 speed=1/2 mode=pingpong wait=rider\n")));
      World w(level, 0, theme, art);
      idle(w, kGardenHalf - kGardenSweep - 5);
      check(!w.garden().seeded, "not before the sweep");
      idle(w, kDandelionSeed + 10);
      check(w.garden().seeded, "at the sweep the dandelion goes to seed by itself");
    }
    {
      // The ladybug in front of the bonus door: misted, her spots spell
      // G-U-N and the door wakes; misted again, 4-2.
      const auto level = std::make_shared<const Level>(Level::parse(room(6, {{21, "          B"}}, "@ ladybug 9 19\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      check(bonusDoor(w) && bonusDoor(w)->dormant, "the bonus door sleeps behind the ladybug");
      Keys prev;
      hold(w, mist, 3, prev);
      hold(w, Keys{}, 3, prev);
      check(w.garden().ladybug == 1 && !bonusDoor(w)->dormant, "misted, G-U-N, and the door wakes");
      hold(w, mist, 3, prev);
      hold(w, Keys{}, 3, prev);
      check(w.garden().ladybug == 2, "misted again, 4-2");
    }
    {
      // The rare butterfly: the mister catches it.
      const auto level = std::make_shared<const Level>(Level::parse(room(6, {},
        "@ prize kind=rare_butterfly path=9,20;9,20 score=10000\n")));
      World w(level, 0, theme, art);
      w.cheat(Cheat::Ammo);
      const int score = w.stats().score;
      Keys prev;
      hold(w, mist, 3, prev);
      check(w.garden().bflyState == 1 && w.stats().score >= score + 10000, "the mister catches the rare butterfly");
    }
    {
      // Any other shot scares it off for good.
      const auto level = std::make_shared<const Level>(Level::parse(room(6, {},
        "@ prize kind=rare_butterfly path=12,20;12,20 score=10000\n")));
      World w(level, 0, theme, art);
      Keys prev;
      hold(w, mist, 1, prev);
      hold(w, Keys{}, 12, prev);
      check(w.garden().bflyState == 2, "a blaster shot scares it off");
    }
    {
      // The narrator: a line on entering its rect, and the aside for a
      // runner who stands still for 30 seconds.
      const auto level = std::make_shared<const Level>(Level::parse(room(5, {},
        "@ narrate n1 rect=1,1,10,21 runner=any line=\"DAWN IN THE TINY KINGDOM\"\n"
        "@ narrate n2 rect=1,1,10,21 runner=rocco line=\"NOT FOR DASH\"\n")));
      World w(level, 0, theme, art);
      idle(w, 2);
      check(w.garden().subtitle > 0 && w.garden().subtitleText == "DAWN IN THE TINY KINGDOM", "the narrator speaks");
      check(!w.garden().lines[1].said, "a line for another runner stays unsaid");
      idle(w, kStillLine);
      check(w.garden().stillSaid, "standing still for 30 seconds gets the aside");
    }
    {
      // Dandelion Drift: the bot drifts through to the exit.
      const auto level = std::make_shared<const Level>(Level::loadFile(argv[2]));
      for (int ch = 0; ch < 6; ++ch)
      {
        World w(level, ch, theme, art);
        Bot bot;
        const int f = botPlay(w, 90 * 15, bot);
        std::printf("     dandelion drift, runner %d: %d gems, %d bounces, %d frames\n", ch, w.stats().gems, w.glide().bounces, f);
        check(w.state() != WorldState::Playing && !w.bonusFailed() && w.stats().gems >= 20, "the bot drifts through with 20 gems");
      }
    }
    {
      // A gust lifts the runner; a thorn bounces it back 4 blocks.
      std::string s = "name=GLIDE TEST\nepisode=4\ntheme=macro_garden\nrules=glide\ntimer=90\ngoal=exit\n[map]\n";
      for (int y = 0; y < 24; ++y)
      {
        std::string row(40, '.');
        if (y == 23)
          row = std::string(40, '#');
        if (y == 12)
          row[2] = 'P';
        if (y >= 10 && y <= 14)
          row[20] = '^';
        if (y == 12)
          row[38] = 'X';
        s += row + "\n";
      }
      s += "[entities]\n@ wind rect=6,2,8,20 dir=u push=1\n";
      const auto level = std::make_shared<const Level>(Level::parse(s));
      World w(level, 0, theme, art);
      const int y0 = w.player().y;
      idle(w, 16);
      check(w.player().x >= 2 * kCellsPerTile + 7, "the seed head drifts right");
      idle(w, 10);
      check(w.player().y < y0 - 2, "a gust lifts it");
      for (int f = 0; f < 120 && w.glide().bounces == 0; ++f)
      {
        Keys prev, down;
        down.down = w.player().y < y0;
        step(w, down, prev);
      }
      check(w.glide().bounces == 1, "a thorn bounces it");
      idle(w, kGlideBounce + 1);
      check(w.player().x < 20 * kCellsPerTile - 4 * kCellsPerTile, "back 4 blocks");
    }
    {
      // The whole level from dusk: the bot waits out half a cycle at the
      // start, then crosses the ditch and the pond by the dusk mushrooms.
      const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
      World w(level, 0, theme, art);
      idle(w, kGardenHalf);
      Bot bot;
      bot.setTakeBonus(false);
      const int f = botPlay(w, 15 * 240, bot);
      std::printf("     tiny kingdom from dusk: %d frames, %d deaths\n", f, w.stats().deaths);
      check(w.state() != WorldState::Playing && w.stats().deaths == 0, "the bot clears the level starting at dusk");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all garden tests passed" : "SOME TESTS FAILED");
  return gFailures == 0 ? 0 : 1;
}
