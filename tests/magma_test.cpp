// Level 33's mechanics, played by hand in small rooms: the rising lava (its
// delay, its rate and its cap), a touch costing two hearts and popping the
// runner up to a floor above, Turbo walking on it, a checkpoint's purge
// holding it, a respawn setting it 12 blocks under the checkpoint, the
// magma gutter, the Heat Sink sucking a Lava Daemon's fireball in and
// throwing everything back as one fireball (the only thing that hurts a
// Firewall), a Firewall's dip at a port and its knockback, a Fan Bot's
// gusts and its blades, the corrupted rack's dust, the RATINGS server
// opening the bonus, the DO NOT REBOOT rack, and Overclock's three frames
// to one. Then the bot clears Overclock with every runner.

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

struct Wall
{
  int x0, y0, x1, y1;
  char c = '#';
};

// A 40 x h room in the volcano: ceiling row 0, floor rows h-2 and h-1,
// walls at x 0 and 39, the runner at (px, py), plus `walls`, `entities`
// and extra header lines.
std::string room(int px, int py, const std::string& entities, const std::vector<Wall>& walls = {}, int h = 24,
  const std::string& header = "weapon=heat_sink\n")
{
  std::vector<std::string> map(std::size_t(h), std::string(40, '.'));
  for (int x = 0; x < 40; ++x)
    map[std::size_t(h - 2)][std::size_t(x)] = map[std::size_t(h - 1)][std::size_t(x)] = map[0][std::size_t(x)] = '#';
  for (int y = 0; y < h; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][39] = '#';
  for (const auto& w : walls)
    for (int y = w.y0; y <= w.y1; ++y)
      for (int x = w.x0; x <= w.x1; ++x)
        map[std::size_t(y)][std::size_t(x)] = w.c;
  map[std::size_t(py)][std::size_t(px)] = 'P';
  std::string s = "name=MAGMA TEST\nepisode=5\ntheme=volcano_datacenter\n" + header + "[map]\n";
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

Enemy* firstOf(World& w, EnemyKind kind)
{
  for (const auto& e : w.enemies())
    if (e.kind == kind)
      return const_cast<Enemy*>(&e);
  return nullptr;
}

int shotsOf(const World& w, ShotKind kind)
{
  int n = 0;
  for (const auto& pr : w.projectiles())
    n += pr.alive && pr.kind == kind;
  return n;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc < 2)
    return 2;
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);
    Keys none, fire, left, right, jump;
    fire.fire = true;
    jump.jump = true;
    left.left = true;
    right.right = true;

    auto make = [&](const std::string& text, int character = 0) {
      return World(std::make_shared<const Level>(Level::parse(text)), character, theme, art);
    };
    // A 40 x 40 room: the floor g38, a ledge g28 over x 4-16, the lava from row 38.
    const std::string lava = "@ fluid kind=rising rect=0,38,39,38 rate=1/20 delay=150 cap=20\n";
    const std::vector<Wall> ledge{{4, 28, 16, 28}};

    {
      World w = make(room(10, 37, lava, ledge, 40));
      idle(w, 150);
      const int a = w.magma().surface;
      idle(w, 200);
      const int b = w.magma().surface;
      check(a == 76 && b == 66, "the lava waits 150 frames, then rises a cell every 20 frames");
    }
    {
      // On the floor: a touch costs two hearts and pops the runner up onto the ledge.
      World w = make(room(10, 37, "@ fluid kind=rising rect=0,38,39,38 rate=1/20 delay=0 cap=20\n", ledge, 40));
      const int hp = w.player().hp;
      idle(w, 25);
      const auto& p = w.player();
      check(p.hp == hp - 2 && p.y == 55 && p.state == PlayerState::OnGround && w.magma().pops == 1,
        "touching the lava costs two hearts and pops the runner up to the floor above");
    }
    {
      // Turbo walks on it.
      World w = make(room(10, 37, "@ fluid kind=rising rect=0,38,39,38 rate=1/20 delay=0 cap=20\n", {}, 40));
      w.cheat(Cheat::Turbo);
      const int hp = w.player().hp;
      idle(w, 100);
      const auto& p = w.player();
      if (std::getenv("MAGMA_DEBUG"))
        std::printf("turbo: hp %d/%d y %d surface %d pops %d turbo %d\n", p.hp, hp, p.y, w.magma().surface, w.magma().pops, p.turbo);
      check(p.hp == hp && p.y == w.magma().surface - 1 && w.magma().pops == 0, "Turbo stands on the lava");
    }
    {
      // A checkpoint's purge holds it 150 frames; then it rises again.
      World w = make(room(20, 37, "@ fluid kind=rising rect=0,38,39,38 rate=1/20 delay=0 cap=20\n@ purge 22 37\n",
        {{22, 37, 22, 37, 'c'}}, 40));
      Keys prev;
      hold(w, right, 6, prev);
      const int a = w.magma().surface;
      idle(w, 140);
      const int b = w.magma().surface;
      idle(w, 60);
      const int c = w.magma().surface;
      check(a == b && c < b && w.magma().purges[0].fired, "touching a checkpoint purges: the lava holds for 150 frames");
    }
    {
      // A respawn at the checkpoint puts the lava 12 blocks under it and purges again.
      World w = make(room(20, 17, "@ fluid kind=rising rect=0,58,39,58 rate=1/20 delay=0 cap=20\n@ purge 22 17\n",
        {{22, 17, 22, 17, 'c'}, {2, 18, 37, 18}}, 60));
      Keys prev;
      hold(w, right, 6, prev);
      w.cheat(Cheat::God);
      w.cheat(Cheat::God);
      idle(w, 10);
      // Kill the runner: walk into the lava's way by raising it.
      for (int i = 0; i < 400 && w.player().state != PlayerState::Dying; ++i)
      {
        const_cast<MagmaState&>(w.magma()).surface = std::min(w.magma().surface, w.player().y);
        idle(w, 1);
      }
      for (int i = 0; i < 400 && w.player().state == PlayerState::Dying; ++i)
        idle(w, 1);
      check(w.magma().surface == 35 + 24 && w.magma().pause > 0, "a respawn puts the lava 12 blocks under the checkpoint");
    }
    {
      // The gutter: a heart every 15 frames, no pop.
      World w = make(room(10, 21, "@ fluid kind=gutter rect=10,22,14,22\n", {{10, 22, 14, 22, '.'}}));
      const int hp = w.player().hp;
      idle(w, 31);
      check(w.player().hp == hp - 3 && w.player().y == 45, "the magma gutter burns a heart every 15 frames");
    }
    {
      // The Heat Sink: hold to suck a Lava Daemon's fireball in, release
      // to throw everything back; the runner walks at half speed holding.
      World w = make(room(6, 21, "@ lava_daemon 14 21\n"));
      w.cheat(Cheat::Ammo);
      auto& p = const_cast<Player&>(w.player());
      p.ammo = 3;
      Keys prev;
      Keys suck = fire;
      suck.right = true;
      hold(w, suck, 2, prev);
      int got = 0;
      for (int i = 0; i < 120 && w.player().ammo < 4; ++i)
        step(w, fire, prev);
      got = w.player().ammo;
      const int x0 = w.player().x;
      Keys walk = fire;
      walk.left = true;
      hold(w, walk, 10, prev);
      const int slow = x0 - w.player().x;
      if (std::getenv("MAGMA_DEBUG"))
        std::printf("sink: got %d slow %d hp %d/%d weapon %d proto %d\n", got, slow, w.player().hp, w.player().maxHp, int(w.player().weapon), w.player().proto);
      check(got == 4 && slow > 0 && slow <= 6 && w.player().hp == w.player().maxHp,
        "holding the Heat Sink sucks a fireball in (no harm) and walks at half speed");
      hold(w, right, 2, prev);
      step(w, none, prev);
      bool ball = false;
      for (const auto& pr : w.projectiles())
        ball = ball || (pr.alive && pr.kind == ShotKind::Rocket && pr.proto == int(ProtoId::HeatSink) && pr.damage == 4);
      check(ball && w.player().weapon == Weapon::Normal, "releasing throws one fireball of everything stored; empty, it's the blaster");
    }
    {
      // A Firewall shrugs off blaster shots; a fireball of 6 kills it.
      World w = make(room(4, 21, "@ firewall 12 21 x0=10 x1=16\n"));
      Keys prev;
      for (int i = 0; i < 8; ++i)
      {
        step(w, fire, prev);
        hold(w, none, 6, prev);
      }
      Enemy* fw = firstOf(w, EnemyKind::Firewall);
      const int hpAfterShots = fw ? fw->hp : -1;
      w.cheat(Cheat::Ammo);
      hold(w, fire, 3, prev);
      step(w, none, prev);
      idle(w, 40);
      fw = firstOf(w, EnemyKind::Firewall);
      if (std::getenv("MAGMA_DEBUG"))
        std::printf("firewall: hp after shots %d alive %d hp %d\n", hpAfterShots, fw ? fw->alive : -1, fw ? fw->hp : -1);
      check(hpAfterShots == 6 && fw && !fw->alive, "only a Heat Sink fireball hurts a Firewall (6 stored kills it)");
    }
    {
      // At a port its flames dip for 30 frames after an 8-frame flicker;
      // contact otherwise hurts and knocks the runner back 2 blocks.
      World w = make(room(4, 21, "@ firewall 10 21 x0=10 x1=30 ports=14\n"));
      Enemy* fw = firstOf(w, EnemyKind::Firewall);
      int flick = -1, dip = -1;
      for (int i = 0; i < 40 && dip < 0; ++i)
      {
        idle(w, 1);
        if (flick < 0 && fw->tell > 0)
          flick = i;
        if (dip < 0 && fw->dive)
          dip = i;
      }
      int dipLen = 0;
      while (fw->dive && dipLen < 60)
      {
        idle(w, 1);
        ++dipLen;
      }
      check(flick >= 0 && dip - flick == 8 && fw->x == 28 && dipLen == 30, "a Firewall flickers for 8 frames, then dips for 30 at a port");
      World v = make(room(14, 21, "@ firewall 20 21 x0=10 x1=24 dir=l\n"));
      const int hp = v.player().hp;
      int x = v.player().x;
      for (int i = 0; i < 30 && v.player().hp == hp; ++i)
      {
        x = v.player().x;
        idle(v, 1);
      }
      if (std::getenv("MAGMA_DEBUG"))
        std::printf("knock: hp %d/%d x %d -> %d\n", v.player().hp, hp, x, v.player().x);
      check(v.player().hp == hp - 1 && v.player().x <= x - 3, "its flames hurt a heart and knock the runner back");
    }
    {
      // A Fan Bot's gust pushes; its blades turn shots from the front.
      World w = make(room(20, 21, "@ fan_bot 1 21 dir=r\n"));
      idle(w, 1);
      const int x0 = w.player().x;
      idle(w, 59);
      const int x1 = w.player().x;
      Enemy* fan = firstOf(w, EnemyKind::FanBot);
      Keys prev;
      Keys turn = left;
      step(w, turn, prev);
      for (int i = 0; i < 4; ++i)
      {
        step(w, fire, prev);
        hold(w, none, 8, prev);
      }
      check(x1 > x0 + 20 && fan && fan->hp == 6, "a Fan Bot's gust pushes the runner; its blades turn shots from the front");
    }
    {
      // The corrupted rack's dust infects while it blows; four shots stop it.
      World w = make(room(12, 21, "@ virusrack 20 21 hp=4 on=60 off=45 rect=10,18,19,21\n"));
      idle(w, 3);
      const bool sick = w.player().virus > 0;
      World v = make(room(4, 21, "@ virusrack 20 21 hp=4 on=60 off=45 rect=10,18,19,21\n"));
      Keys prev;
      step(v, right, prev);
      for (int i = 0; i < 5; ++i)
      {
        step(v, fire, prev);
        hold(v, none, 8, prev);
      }
      check(sick && v.magma().racks[0].hp == 0, "the corrupted rack's dust infects; four shots stop it for good");
    }
    {
      // The RATINGS server: shot, it pays 10000 and opens the bonus entrance.
      World w = make(room(4, 21, "@ treasure 14 21 score=10000 art=server_ratings\n", {{30, 21, 30, 21, 'B'}}));
      bool dormant = false;
      for (const auto& pr : w.props())
        dormant = dormant || (pr.kind == PropKind::BonusDoor && pr.dormant);
      const int score = w.stats().score;
      Keys prev;
      step(w, right, prev);
      step(w, fire, prev);
      idle(w, 20);
      bool open = false;
      for (const auto& pr : w.props())
        open = open || (pr.kind == PropKind::BonusDoor && !pr.dormant);
      check(dormant && open && w.stats().score >= score + 10000, "shooting the RATINGS server opens the bonus entrance");
    }
    {
      // DO NOT REBOOT: shot, the lava waits 90 frames.
      World w = make(room(4, 37, "@ fluid kind=rising rect=0,38,39,38 rate=1/20 delay=0 cap=20\n@ reboot 14 37\n", {}, 40));
      Keys prev;
      step(w, right, prev);
      step(w, fire, prev);
      idle(w, 4);
      const int a = w.magma().surface;
      idle(w, 80);
      const int b = w.magma().surface;
      check(w.magma().rebooted && a == b, "shooting the DO NOT REBOOT rack holds the lava for its reboot");
    }

    // Overclock: three logic frames to one; its Firewall only pushes.
    const auto bonus = std::make_shared<const Level>(Level::loadFile(argv[1]));
    {
      World w(bonus, 0, theme, art);
      const int f0 = w.stats().frames;
      idle(w, 10);
      check(w.stats().frames - f0 == 30, "Overclock runs three logic frames for every one");
    }
    for (int character = 0; character < 6; ++character)
    {
      World w(bonus, character, theme, art);
      Bot bot;
      const int f = botPlay(w, 450, bot);
      std::printf("     runner %d: %d frames\n", character, f);
      check(w.state() != WorldState::Playing && !w.bonusFailed(), "the bot clears Overclock");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures == 0 ? "all magma tests passed\n" : "%d magma test(s) failed\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
