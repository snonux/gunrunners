// Level 41's mechanics, played by hand in small rooms: a ballot at a vote
// checkpoint (the next segment's enemies asleep until it is over), the
// style score and its double friendly option, the Applause Cannon's meter,
// a Warm-up Comic's rubber chicken, an Audience Plant standing up and
// sitting back down, a Cue Card Drone's swoop, a turntable, every rule on
// the ballot (BIG HEADS, DOUBLE GEMS, LOW GRAVITY, TURBO FOR EVERYONE,
// MIRROR MODE, ???, SLIPPERY FLOORS, PARTY HATS, VIRUS FOR THE RUNNER), the
// seat's ladder and the letter G waking the bonus entrance; Paintball's
// paint. MIRROR MODE and BIG HEADS as drawn: the scene flipped under an
// unflipped HUD, the lens round the runner's head, both together, and the
// same read back from the screen when the renderer refuses the capture; a
// capture begun only for them (none for a plain frame or a dying runner).
// Then the bot clears Sweeps Week under each rule, takes the bonus entrance,
// and clears Sweeps Week and Paintball with every runner.

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
#include <cstring>
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
  const std::string& extra = "weapon=applause_cannon\n")
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
  std::string s = "name=SWEEPS TEST\nepisode=6\ntheme=sweeps_week\n" + extra + "[map]\n";
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

// Walks right into the vote at block 5 and waits out the ballot.
void vote(World& w, unsigned rule)
{
  w.forceSweepsRule(rule);
  Keys prev, right;
  right.right = true;
  for (int i = 0; i < 20 && w.sweeps().ballot < 0 && w.sweeps().cur == 0; ++i)
    step(w, right, prev);
  idle(w, kSwSwing + kSwShow + 2);
}

// The highest the runner's feet get in a jump from where they stand.
int jumpTop(World& w)
{
  Keys prev, jump;
  jump.jump = true;
  int best = w.player().y;
  step(w, jump, prev);
  for (int i = 0; i < 40; ++i)
  {
    step(w, jump, prev);
    best = std::min(best, w.player().y);
  }
  return best;
}

int botPlay(World& w, int frames, Bot& bot)
{
  Input prev;
  int f = 0;
  const bool trace = std::getenv("GR_SW_TRACE") != nullptr;
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
    if (trace)
    {
      const auto& pl = w.player();
      std::printf("f%d %d,%d st%d hp%d seg%d rules%u in %c%c%c%c%c%c\n", f, pl.x, pl.y, int(pl.state), pl.hp,
                  w.sweeps().cur, w.sweeps().rules, in.left ? 'L' : '.', in.right ? 'R' : '.', in.up ? 'U' : '.',
                  in.down ? 'D' : '.', in.jump ? 'J' : '.', in.fire ? 'F' : '.');
    }
    w.update(p);
  }
  return f;
}

const char* const kVote = "@ vote c1 5 21 virus=15,19\n";

// A 20 x 11 room, all of it in the picture: walls at x 0 and 19, the floor
// on rows 9 and 10, the runner at (3, 8), a vote at block 5. With `pit` the
// floor has a bottomless pit at blocks 9..11.
std::string stage(bool pit = false)
{
  std::vector<std::string> map(11, std::string(20, '.'));
  for (int x = 0; x < 20; ++x)
    if (!pit || x < 9 || x > 11)
      map[9][std::size_t(x)] = map[10][std::size_t(x)] = '#';
  for (int y = 0; y < 11; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][19] = '#';
  map[8][3] = 'P';
  std::string s = "name=SWEEPS TEST\nepisode=6\ntheme=sweeps_week\nweapon=applause_cannon\n[map]\n";
  for (const auto& row : map)
    s += row + "\n";
  return s + "[entities]\n@ vote c1 5 8 virus=15,8\n";
}

using Frame = std::vector<Uint32>;

// The picture a world draws on the 1280 x 720 surface.
Frame frameOf(const World& w, Renderer& r, SDL_Surface* surface)
{
  r.beginFrame();
  r.clear(rgb(0, 0, 0));
  w.draw(r, 100, 1.0f);
  SDL_RenderPresent(r.sdl());
  Frame f(std::size_t(kScreenW) * std::size_t(kScreenH));
  for (int y = 0; y < kScreenH; ++y)
    std::memcpy(&f[std::size_t(y) * std::size_t(kScreenW)],
      static_cast<const unsigned char*>(surface->pixels) + y * surface->pitch, std::size_t(kScreenW) * 4);
  return f;
}

Uint32 pixel(const Frame& f, int x, int y)
{
  return f[std::size_t(y) * std::size_t(kScreenW) + std::size_t(x)];
}

// Pixels that differ between a and b (b flipped left to right if asked) in
// the rows y0..y1 and the columns x0..x1 (a's), under the jumbotron and HUD.
int differ(const Frame& a, const Frame& b, bool flip, int x0 = 0, int x1 = kScreenW - 1, int y0 = 330,
  int y1 = kScreenH - 1)
{
  int n = 0;
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x)
      n += pixel(a, x, y) != pixel(b, flip ? kScreenW - 1 - x : x, y) ? 1 : 0;
  return n;
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
    Keys right, left, fire, jump;
    right.right = true;
    left.left = true;
    fire.fire = true;
    jump.jump = true;
    auto make = [&](const std::string& text, int character = 0) {
      return World(std::make_shared<const Level>(Level::parse(text)), character, theme, art);
    };
    {
      // A ballot: three options, the runner keeps moving, the next segment's
      // Comic sleeps until the winner is in.
      World w = make(room(3, 21, std::string(kVote) + "@ comic 19 21\n"));
      const int style = w.sweepsStyle();
      Keys prev;
      for (int i = 0; i < 20 && w.sweeps().ballot < 0; ++i)
        step(w, right, prev);
      const auto& s = w.sweeps();
      const bool up = s.ballot == 0;
      const bool kinds = s.options[0].cat == SwCat::Friendly && s.options[1].cat == SwCat::Neutral &&
        (s.options[2].cat == SwCat::Nasty || s.options[2].cat == SwCat::Secret);
      const int x0 = w.player().x;
      hold(w, right, 10, prev);
      const bool moves = w.player().x > x0;
      const bool asleep = w.enemies()[0].stun > 0 && w.enemies()[0].active;
      idle(w, kSwSwing + kSwShow);
      hold(w, right, 20, prev);
      std::printf("     style %d, ballot: %s / %s / %s, won %d, rules %u\n", style, sweepsOptionName(s.options[0]).c_str(),
                  sweepsOptionName(s.options[1]).c_str(), sweepsOptionName(s.options[2]).c_str(), s.winner, s.rules);
      check(style == 70, "a clean segment without enemies scores 70");
      check(up && kinds, "the vote checkpoint shows a friendly, a neutral and a nasty option");
      check(moves && asleep, "the runner moves during the ballot; the next segment sleeps");
      check(s.cur == 1 && s.ballot < 0 && s.rules == s.options[s.winner].rules && w.enemies()[0].stun == 0,
            "the winner rules the next segment, its enemies wake");
    }
    {
      // The Applause Cannon: four hits in a row raise the meter; a kill and
      // the meter's step lift the style score to 80, a double friendly option.
      World w = make(room(3, 21, "@ comic 14 21\n", {{2, 21, 2, 21, '^'}}));
      w.cheat(Cheat::God);
      w.cheat(Cheat::Ammo);
      Keys prev;
      const int hp = w.enemies()[0].hp;
      tap(w, fire, prev);
      idle(w, 12);
      const bool one = w.enemies()[0].hp == hp - 1;
      for (int i = 0; i < 3; ++i)
      {
        tap(w, fire, prev);
        idle(w, 12);
      }
      const auto& s = w.sweeps();
      std::printf("     comic hp %d -> %d, meter x%d, streak %d, style %d\n", hp, w.enemies()[0].hp, s.meter, s.streak,
                  w.sweepsStyle());
      check(one && s.meter == 2, "four Applause Cannon hits in a row: x2");
      check(!w.enemies()[0].alive && w.sweepsStyle() == 80, "no damage, x2, every enemy down: style 80");
      // A step into the hot stage lights: back to x1.
      w.cheat(Cheat::God);
      for (int i = 0; i < 20 && w.player().hp == w.player().maxHp; ++i)
        step(w, left, prev);
      idle(w, 2);
      check(s.meter == 1 && s.streak == 0, "damage resets the meter to x1");
    }
    {
      World w = make(room(3, 21, std::string(kVote) + "@ comic 14 21\n"));
      w.cheat(Cheat::God);
      w.cheat(Cheat::Ammo);
      Keys prev;
      for (int i = 0; i < 4; ++i)
      {
        tap(w, fire, prev);
        idle(w, 12);
      }
      vote(w, 0);
      const auto& o = w.sweeps().options[0];
      std::printf("     style %d: %s\n", w.sweeps().style, sweepsOptionName(o).c_str());
      check(w.sweeps().style >= 80 && __builtin_popcount(o.rules) == 2, "style 80: the friendly option is a double");
    }
    {
      // A rubber chicken: a heart and 4 cells of knockback (Rocco 2).
      auto knock = [&](int character) {
        World w = make(room(10, 21, "@ comic 18 21\n"), character);
        int dx = 0, hp = w.player().hp;
        for (int i = 0; i < 200 && dx == 0; ++i)
        {
          const int x = w.player().x;
          idle(w, 1);
          if (w.player().hp < hp)
            dx = x - w.player().x;
        }
        return dx;
      };
      const int dash = knock(0), rocco = knock(1);
      std::printf("     knockback: Dash %d, Rocco %d\n", dash, rocco);
      check(dash >= kSwKnock && rocco >= kSwKnock / 2 && rocco < dash, "a rubber chicken knocks the runner back");
    }
    {
      // An Audience Plant stands for a runner within 10 cells, shoots, and
      // sits back down after 150 frames out of reach.
      World w = make(room(3, 21, "@ audience_plant 22 21\n"));
      const auto& a = w.sweeps().actors[0];
      Keys prev;
      idle(w, 20);
      const bool sits = a.stand == 0;
      for (int i = 0; i < 60 && a.stand == 0; ++i)
        step(w, right, prev);
      const bool stood = a.stand > 0;
      int shots = 0;
      for (int i = 0; i < 40 && shots == 0; ++i)
      {
        idle(w, 1);
        for (const auto& pr : w.projectiles())
          shots += pr.alive && pr.kind == ShotKind::Enemy ? 1 : 0;
      }
      hold(w, left, 40, prev);
      idle(w, kSwPlantSit + 10);
      check(sits && stood, "a Plant sits until the runner is close, then stands");
      check(shots > 0, "a standing Plant shoots");
      check(a.stand == 0, "out of reach, it sits back down");
    }
    {
      // A Cue Card Drone flips its card over the runner, then swoops.
      World w = make(room(20, 21, "@ cuedrone 12 15 path=12,15;30,15\n"));
      const auto& a = w.sweeps().actors[0];
      const int hp = w.player().hp;
      bool flipped = false, swooped = false;
      for (int i = 0; i < 200 && w.player().hp == hp; ++i)
      {
        idle(w, 1);
        flipped = flipped || a.flip > 0;
        swooped = swooped || (flipped && a.swoop == 1);
      }
      check(flipped && swooped && w.player().hp < hp, "a Cue Card Drone flips its card and swoops");
    }
    {
      // A turntable carries the runner a cell every other frame and turns round.
      World w = make(room(12, 21, "@ turntable 8 22 w=20 dir=r\n"));
      const int x0 = w.player().x;
      idle(w, 40);
      const int x1 = w.player().x;
      idle(w, 60);
      std::printf("     turntable: %d -> %d -> %d\n", x0, x1, w.player().x);
      check(x1 - x0 >= 16 && w.player().x < x1, "a turntable carries the runner, then turns round");
    }
    {
      // BIG HEADS (the rehearsal's rule): a shot over a Plant's head hits it;
      // without the rule it misses.
      auto shoot = [&](unsigned rule) {
        World w = make(room(4, 19, std::string(rule != 0 ? kVote : "") + "@ audience_plant 20 21\n", {{2, 20, 12, 21}}));
        w.cheat(Cheat::God);
        if (rule != 0)
        {
          vote(w, rule);
          Keys prev;
          hold(w, left, 4, prev);
        }
        w.cheat(Cheat::Ammo);
        Keys prev;
        hold(w, right, 1, prev);
        const int hp = w.enemies()[0].hp;
        tap(w, fire, prev);
        idle(w, 20);
        return hp - w.enemies()[0].hp;
      };
      const int big = shoot(0), small = shoot(kSwDoubleGems);
      std::printf("     over the head: BIG HEADS %d, DOUBLE GEMS %d\n", big, small);
      check(big > 0 && small == 0, "BIG HEADS: a shot over an enemy's head hits");
    }
    {
      // DOUBLE GEMS: a gem is worth two.
      World w = make(room(3, 21, kVote, {{14, 21, 14, 21, 'g'}}));
      vote(w, kSwDoubleGems);
      const int score = w.stats().score;
      Keys prev;
      hold(w, right, 20, prev);
      idle(w, 10);
      std::printf("     gem: +%d\n", w.stats().score - score);
      check(w.stats().gems == 1 && w.stats().score - score >= 1000, "DOUBLE GEMS: a gem counts twice");
    }
    {
      // LOW GRAVITY: a higher jump.
      World a = make(room(3, 21, kVote)), b = make(room(3, 21, kVote));
      vote(a, kSwLowGrav);
      vote(b, kSwMirror);
      const int low = jumpTop(a), mirror = jumpTop(b);
      std::printf("     jump top: low gravity %d, normal %d\n", low, mirror);
      check(low < mirror, "LOW GRAVITY: the runner jumps higher");
    }
    {
      // TURBO FOR EVERYONE: the runner covers more ground (and so does the rest).
      World a = make(room(3, 21, kVote)), b = make(room(3, 21, kVote));
      vote(a, kSwTurbo);
      vote(b, kSwHats);
      Keys pa, pb;
      const int ax = a.player().x, bx = b.player().x;
      hold(a, right, 20, pa);
      hold(b, right, 20, pb);
      std::printf("     20 frames: turbo %d cells, normal %d\n", a.player().x - ax, b.player().x - bx);
      check(a.player().x - ax > b.player().x - bx, "TURBO FOR EVERYONE runs the show faster");
    }
    {
      // MIRROR MODE: right is left; a Turbo pick-up puts it back.
      World w = make(room(3, 21, kVote));
      vote(w, kSwMirror);
      Keys prev;
      const int x0 = w.player().x;
      hold(w, right, 10, prev);
      const bool mirrored = w.player().x < x0 && w.sweepsMirrored();
      w.cheat(Cheat::Turbo);
      const int x1 = w.player().x;
      hold(w, right, 10, prev);
      check(mirrored, "MIRROR MODE: right walks left");
      check(w.player().x > x1 && !w.sweepsMirrored(), "Turbo puts the mirror back");
    }
    {
      // MIRROR MODE and BIG HEADS draw through the renderer's scene capture,
      // nothing read back from the GPU. The same moment under four rules:
      // DOUBLE GEMS draws no differently from a plain segment.
      auto frame = [&](unsigned rule) {
        World w = make(stage());
        vote(w, rule);
        return frameOf(w, renderer, surface);
      };
      const Frame plain = frame(kSwDoubleGems), mirror = frame(kSwMirror), big = frame(kSwBigHeads),
                  both = frame(kSwMirror | kSwBigHeads);
      const int flipped = differ(mirror, plain, true), straight = differ(mirror, plain, false);
      const int lens = differ(big, plain, false, 0, 520), away = differ(big, plain, false, 640);
      const int bothFlipped = differ(both, big, true);
      std::printf("     MIRROR MODE vs flipped: %d px (unflipped %d); BIG HEADS lens %d px, away %d; both vs flipped %d\n",
        flipped, straight, lens, away, bothFlipped);
      check(flipped == 0 && straight > 10000, "MIRROR MODE: the scene is the plain one flipped left to right");
      const Uint32 tag = rgb(255, 60, 90); // the red edge of TONIGHT'S RULE's tag (drawSweepsHud)
      check(pixel(mirror, 18, 160) == tag && pixel(mirror, kScreenW - 19, 160) != tag,
        "MIRROR MODE: the HUD goes over it the right way round");
      check(lens > 2000 && away == 0, "BIG HEADS: the lens round the runner's head, the rest as drawn");
      check(bothFlipped == 0, "MIRROR MODE with BIG HEADS: the lens mirrored with the scene");
      check(!renderer.capturing(), "the capture is over when the world is drawn");

      // A capture begins only for a frame that needs one: under MIRROR
      // MODE, or for BIG HEADS' lens (copied from it on the GPU; without
      // SDL_RenderGeometry, SDL older than 2.0.18, the lens is read back
      // anyway and begins none). None for a plain frame, nor for BIG HEADS
      // while the runner dies in a pit: no lens then, as while hidden,
      // somersaulting or on the ceiling.
#if SDL_VERSION_ATLEAST(2, 0, 18)
      const std::size_t lensBegins = 1;
#else
      const std::size_t lensBegins = 0;
#endif
      auto begunBy = [&](const World& w) {
        const std::size_t before = renderer.capturesBegun();
        frameOf(w, renderer, surface);
        return renderer.capturesBegun() - before;
      };
      auto begunFor = [&](unsigned rule) {
        World w = make(stage());
        vote(w, rule);
        return begunBy(w);
      };
      World dying = make(stage(true));
      vote(dying, kSwBigHeads);
      Keys prevDying;
      for (int i = 0; i < 200 && dying.player().state != PlayerState::Dying; ++i)
        step(dying, right, prevDying);
      const std::size_t begunPlain = begunFor(kSwDoubleGems), begunDying = begunBy(dying);
      const std::size_t begunMirror = begunFor(kSwMirror), begunLens = begunFor(kSwBigHeads);
      std::printf("     captures begun: plain %d, BIG HEADS dying %d, MIRROR MODE %d, BIG HEADS %d\n", int(begunPlain),
        int(begunDying), int(begunMirror), int(begunLens));
      check(dying.player().state == PlayerState::Dying && begunPlain == 0 && begunDying == 0,
        "no capture without MIRROR MODE or BIG HEADS' lens (a plain frame, the runner dying)");
      check(begunMirror == 1 && begunLens == lensBegins, "MIRROR MODE and BIG HEADS' lens begin one capture a frame");

      // A renderer that refuses the capture (no render targets, or no
      // texture that big) reads the picture back instead: MIRROR MODE still
      // mirrors it, as the controls are still swapped; BIG HEADS keeps its
      // lens.
      renderer.refuseCaptures(true);
      const Frame mirrorRead = frame(kSwMirror), bigRead = frame(kSwBigHeads), bothRead = frame(kSwMirror | kSwBigHeads);
      renderer.refuseCaptures(false);
      const int flippedRead = differ(mirrorRead, plain, true), lensRead = differ(bigRead, plain, false, 0, 520);
      const int awayRead = differ(bigRead, plain, false, 640), bothFlippedRead = differ(bothRead, bigRead, true);
      std::printf("     read back: MIRROR MODE vs flipped %d px; BIG HEADS lens %d px, away %d; both vs flipped %d\n",
        flippedRead, lensRead, awayRead, bothFlippedRead);
      check(flippedRead == 0 && pixel(mirrorRead, 18, 160) == tag,
        "MIRROR MODE, capture refused: the scene read back and flipped, the HUD the right way round");
      check(lensRead > 2000 && awayRead == 0 && bothFlippedRead == 0, "BIG HEADS, capture refused: the lens read back");
    }
    {
      // SLIPPERY FLOORS: the runner slides on after letting go.
      auto slide = [&](unsigned rule) {
        World w = make(room(3, 21, kVote));
        vote(w, rule);
        Keys prev;
        hold(w, right, 10, prev);
        const int x = w.player().x;
        idle(w, 20);
        return w.player().x - x;
      };
      const int slip = slide(kSwSlippery), dry = slide(kSwMirror);
      std::printf("     after letting go: slippery %d cells, dry %d\n", slip, dry);
      check(slip >= 3 && dry <= 1, "SLIPPERY FLOORS: the runner slides on");
    }
    {
      // PARTY HATS: every enemy of the segment a hit more.
      World w = make(room(3, 21, std::string(kVote) + "@ comic 30 21\n"));
      const int hp = w.enemies()[0].hp;
      vote(w, kSwHats);
      check(w.enemies()[0].hp == hp + 1 && w.sweeps().actors[0].hat, "PARTY HATS: +1 HP and a hat");
    }
    {
      // ???: every enemy a confetti cannon: still, harmless, pops for its score + 500.
      World w = make(room(3, 21, std::string(kVote) + "@ comic 16 21\n"));
      vote(w, kSwConfetti);
      const int hp = w.player().hp, x0 = w.enemies()[0].x;
      idle(w, 60);
      const bool harmless = w.player().hp == hp && w.enemies()[0].x == x0 && w.sweeps().chickens.empty();
      w.cheat(Cheat::Ammo);
      const int score = w.stats().score;
      Keys prev;
      tap(w, fire, prev);
      idle(w, 20);
      std::printf("     pop: +%d\n", w.stats().score - score);
      check(harmless && w.sweeps().actors[0].popper, "???: the Comic is a still confetti cannon");
      check(!w.enemies()[0].alive && w.stats().score - score >= 1000, "one hit pops it for its score + 500");
    }
    {
      // VIRUS FOR THE RUNNER: the V comes in and drifts at the runner; shot, it's gone.
      World w = make(room(3, 21, kVote));
      vote(w, kSwVirus);
      const Item* v = nullptr;
      for (const auto& it : w.items())
        if (it.kind == ItemKind::Virus && !it.taken)
          v = &it;
      const int vx = v ? v->x : 0;
      idle(w, 8);
      const bool drifts = v && v->x < vx;
      idle(w, 16);
      w.cheat(Cheat::Ammo);
      Keys prev;
      tap(w, fire, prev);
      idle(w, 10);
      check(v && drifts, "VIRUS FOR THE RUNNER: the V comes in and drifts at the runner");
      check(v && v->taken && !w.player().virus, "a shot V is gone");
    }
    {
      // The seat: two hits break it and its block over the hollow turns ladder;
      // the letter G wakes the bonus entrance.
      World w = make(room(10, 21, "@ breakable rect=20,20,20,21 hp=2 by=any\n@ seatladder 20 21\n",
                          {{20, 20, 20, 21}, {14, 21, 14, 21, '1'}, {30, 19, 30, 19, 'B'}}));
      bool dormant = false;
      for (const auto& pr : w.props())
        dormant = dormant || (pr.kind == PropKind::BonusDoor && pr.dormant);
      w.cheat(Cheat::Ammo);
      Keys prev;
      for (int i = 0; i < 2; ++i)
      {
        tap(w, fire, prev);
        idle(w, 12);
      }
      idle(w, 2);
      const bool ladder = w.map().block(20, 21) == Tile::Ladder;
      hold(w, right, 10, prev);
      bool awake = false;
      for (const auto& pr : w.props())
        awake = awake || (pr.kind == PropKind::BonusDoor && !pr.dormant);
      check(ladder, "the broken seat leaves a ladder down");
      check(dormant && awake, "the letter G wakes the bonus entrance");
    }
    {
      // Paintball: the floor under the feet and shots paint; 80 % wins.
      World w = make(room(3, 21, "", {}, "rules=paint\ntimer=60\ngoal=paint:80\n"));
      idle(w, 2);
      const auto& s = w.sweeps();
      const int under = s.painted;
      Keys prev;
      hold(w, left, 2, prev);
      tap(w, fire, prev);
      idle(w, 10);
      int walls = 0;
      for (const auto c : s.paints)
        walls += c == 4 ? 1 : 0;
      for (int i = 0; i < 200 && w.state() == WorldState::Playing; ++i)
        step(w, right, prev);
      std::printf("     painted %d of %d, walls %d\n", s.painted, s.tops, walls);
      check(under >= 1 && walls >= 1, "the runner's feet and shots paint");
      check(w.state() != WorldState::Playing && !w.bonusFailed(), "80 % of the floor painted wins");
    }

    // The bot clears Sweeps Week under every rule on the ballot.
    const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
    const auto bonus = std::make_shared<const Level>(Level::loadFile(argv[2]));
    const int only = std::getenv("GR_SW_RUNNER") ? std::atoi(std::getenv("GR_SW_RUNNER")) : -1;
    const int onlyRule = std::getenv("GR_SW_RULE") ? std::atoi(std::getenv("GR_SW_RULE")) : -1;
    const bool skipRules = std::getenv("GR_SW_NO_RULES") != nullptr;
    const bool skipLevel = std::getenv("GR_SW_BONUS_ONLY") != nullptr;
    for (int r = 0; r < kSwRuleCount && !skipRules && !skipLevel; ++r)
    {
      if (onlyRule >= 0 && r != onlyRule)
        continue;
      const unsigned rule = 1u << r;
      // The Virus with the lowest jump too.
      for (int character : {0, 1})
      {
        if ((character == 1 && rule != kSwVirus) || (only >= 0 && character != only))
          continue;
        World w(level, character, theme, art);
        w.forceSweepsRule(rule);
        w.setSweepsQuack(true);
        Bot bot;
        const int f = botPlay(w, 9000, bot);
        std::printf("     %s, runner %d: %d frames, %d deaths, segment %d, at %d,%d\n", sweepsRuleName(rule), character,
                    f, w.stats().deaths, w.sweeps().cur, w.player().x, w.player().y);
        check(w.state() != WorldState::Playing, "the bot clears Sweeps Week under the rule");
      }
    }
    // The bonus entrance: the seat, the hollow, the letter G.
    for (int character = 0; character < 6 && !skipLevel; ++character)
    {
      if (only >= 0 && character != only)
        continue;
      World w(level, character, theme, art);
      Bot bot;
      bot.setTakeBonus(true);
      const int f = botPlay(w, 9000, bot);
      std::printf("     runner %d: %d frames, letters %s\n", character, f, w.stats().letters.c_str());
      check(w.bonusRequested(), "the bot finds G and takes the bonus entrance");
    }
    // The level and the bonus with every runner.
    for (int character = 0; character < 6 && !skipLevel; ++character)
    {
      if (only >= 0 && character != only)
        continue;
      World w(level, character, theme, art);
      Bot bot;
      const int f = botPlay(w, 9000, bot);
      std::printf("     runner %d: %d frames, %d deaths, at %d,%d\n", character, f, w.stats().deaths, w.player().x,
                  w.player().y);
      check(w.state() != WorldState::Playing, "the bot clears Sweeps Week");
    }
    for (int character = 0; character < 6; ++character)
    {
      if (only >= 0 && character != only)
        continue;
      World w(bonus, character, theme, art);
      Bot bot;
      const int f = botPlay(w, 900, bot);
      std::printf("     runner %d: %d frames, painted %d of %d\n", character, f, w.sweeps().painted, w.sweeps().tops);
      check(w.state() != WorldState::Playing && !w.bonusFailed(), "the bot clears Paintball");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures == 0 ? "all sweeps tests passed\n" : "%d sweeps test(s) failed\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
