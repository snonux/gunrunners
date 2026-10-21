// Free aim: the plain guns shoot along the aim at any angle (the right
// stick's way, or a touched spot measured from the runner). Straight ahead
// it is the plain shot, to the cell; a diagonal flies its line; a spot is
// hit; aiming behind turns the runner; the right stick held keeps firing;
// a prototype keeps its own shots. A test room drives the real world logic.

#include "assets/art.hpp"
#include "data/characters.hpp"
#include "data/level.hpp"
#include "data/theme.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <SDL.h>

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace gr;

namespace
{

int gFailures = 0;

void check(bool ok, const std::string& what)
{
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
  if (!ok)
    ++gFailures;
}

// A 40 x 12 room: floor row 10, walls at x 0 and 39, the runner at (3, 9),
// plus whatever entities say.
std::shared_ptr<const Level> room(const std::string& entities, const std::string& header = "", char box = 0)
{
  std::vector<std::string> map(12, std::string(40, '.'));
  for (int x = 0; x < 40; ++x)
    map[0][std::size_t(x)] = map[10][std::size_t(x)] = map[11][std::size_t(x)] = '#';
  for (int y = 0; y < 12; ++y)
    map[std::size_t(y)][0] = map[std::size_t(y)][39] = '#';
  map[9][3] = 'P';
  if (box)
    map[9][6] = box;
  map[9][37] = 'X';
  std::string s = "name=AIM TEST\ntheme=neon_rooftops\nmusic=theme_synthwave\n" + header + "[map]\n";
  for (const auto& row : map)
    s += row + "\n";
  s += "[entities]\n" + entities;
  return std::make_shared<const Level>(Level::parse(s));
}

struct Driver
{
  World& w;
  bool prevFire = false;
  void step(bool fire, Aim aim = {}, bool hold = false, bool right = false, bool down = false)
  {
    PlayerInput p;
    p.right = right;
    p.down = down;
    p.fire = {fire, fire && !prevFire};
    p.aim = aim;
    p.aimHold = hold;
    prevFire = fire;
    w.update(p);
  }
};

Aim way(float x, float y)
{
  const float len = std::hypot(x, y);
  Aim a;
  a.on = true;
  a.x = x / len;
  a.y = y / len;
  return a;
}

// The runner's middle, as Game::runnerOnScreen measures a touch from (cells).
void middle(const World& w, float& x, float& y)
{
  const auto& p = w.player();
  x = float(p.x) + float(Player::kWidth) * 0.5f;
  y = float(p.y + 1) - float(p.height()) * 0.5f;
}

const Projectile* lastShot(const World& w)
{
  return w.projectiles().empty() ? nullptr : &w.projectiles().back();
}

} // namespace

int main()
{
  SDL_Init(0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScreenW, kScreenH, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_Renderer* sdl = SDL_CreateSoftwareRenderer(surface);
  {
    Renderer renderer(sdl);
    const Theme& theme = themeByIndex(0);
    const Art art = Art::build(theme, renderer);

    // Straight ahead, free aim is the plain shot to the cell.
    {
      World plain(room(""), 0, theme, art), aimed(room(""), 0, theme, art);
      Driver dp{plain}, da{aimed};
      for (int i = 0; i < 3; ++i)
      {
        dp.step(false);
        da.step(false);
      }
      check(aimed.aimsFreely(), "the blaster aims freely");
      dp.step(true);
      da.step(true, way(1.0f, 0.0f));
      const Projectile* a = lastShot(plain);
      const Projectile* b = lastShot(aimed);
      check(a && b && a->x == b->x && a->y == b->y && a->w == b->w && a->h == b->h,
        "aimed straight ahead, the shot starts where a plain one does");
      check(b && b->aimed && b->dx == 1 && b->dy == 0, "...flying right");
      for (int i = 0; i < 4; ++i)
      {
        dp.step(false);
        da.step(false);
      }
      a = lastShot(plain);
      b = lastShot(aimed);
      check(a && b && a->x == b->x && a->y == b->y, "...and stays level with it");
    }

    // A diagonal flies its line, from the end of the gun.
    {
      World w(room(""), 0, theme, art);
      Driver d{w};
      for (int i = 0; i < 3; ++i)
        d.step(false);
      d.step(true, way(0.6f, -0.8f));
      const Projectile* s = lastShot(w);
      check(s && s->precise && std::fabs(s->vx - 0.6f) < 1e-4f && std::fabs(s->vy + 0.8f) < 1e-4f,
        "a shot aimed up and right flies at that angle");
      const float fx = s ? s->fx : 0.0f, fy = s ? s->fy : 0.0f;
      d.step(false);
      s = lastShot(w);
      const int speed = s ? s->speed : 0;
      check(s && std::fabs(s->fx - fx - 0.6f * float(speed)) < 1e-3f && std::fabs(s->fy - fy + 0.8f * float(speed)) < 1e-3f,
        "...a cell a step along it");
    }

    // A touched spot is hit, wherever it is: a lens hovering up and to the
    // right, out of reach of a shot straight ahead.
    {
      const auto level = room("@ roof_turret 14 4\n");
      World w(level, 0, theme, art);
      Driver d{w};
      for (int i = 0; i < 3; ++i)
        d.step(false);
      const Enemy* t = nullptr;
      for (const auto& e : w.enemies())
        t = &e;
      check(t != nullptr, "a turret up and to the right");
      if (t)
      {
        const int hp = t->hp;
        // Straight ahead misses it.
        for (int i = 0; i < 30; ++i)
          d.step(i % 4 == 0);
        check(t->alive && t->hp == hp, "a shot straight ahead misses it");
        // Aimed at its middle (from the runner's middle), it is hit.
        for (int i = 0; i < 60 && t->alive && t->hp == hp; ++i)
        {
          float mx = 0.0f, my = 0.0f;
          middle(w, mx, my);
          const CellBox b = t->box();
          Aim a;
          a.on = a.spot = true;
          a.x = float(b.x) + float(b.w) * 0.5f - mx;
          a.y = float(b.y) + float(b.h) * 0.5f - my;
          d.step(i % 4 == 0, a);
        }
        check(!t->alive || t->hp < hp, "a shot aimed at its spot hits it");
      }
    }

    // Aiming behind turns the runner; the shot goes that way.
    {
      World w(room(""), 0, theme, art);
      Driver d{w};
      for (int i = 0; i < 3; ++i)
        d.step(false);
      check(w.player().facing == 1, "the runner faces right");
      d.step(true, way(-1.0f, -0.5f));
      const Projectile* s = lastShot(w);
      check(w.player().facing == -1 && s && s->vx < 0.0f && s->vy < 0.0f, "aimed up and left: turned left, shot up-left");
      d.step(false);
      d.step(true, way(0.0f, -1.0f));
      check(w.player().facing == -1, "straight up keeps the way the runner faces");
    }

    // The right stick held keeps firing; a plain held button fires once.
    {
      World w(room(""), 0, theme, art);
      Driver d{w};
      for (int i = 0; i < 3; ++i)
        d.step(false);
      int shots = 0;
      for (int i = 0; i < 12; ++i)
      {
        const std::size_t before = w.projectiles().size();
        d.step(true, way(0.7f, -0.7f), true);
        shots += w.projectiles().size() > before ? 1 : 0;
      }
      check(shots >= 3, "the right stick held keeps shooting (" + std::to_string(shots) + " shots)");
      for (int i = 0; i < 20; ++i)
        d.step(false);
      shots = 0;
      for (int i = 0; i < 12; ++i)
      {
        const std::size_t before = w.projectiles().size();
        d.step(true, way(0.7f, -0.7f));
        shots += w.projectiles().size() > before ? 1 : 0;
      }
      check(shots == 1, "the fire button held fires once (" + std::to_string(shots) + ")");
    }

    // Direction controls aim throughout a jump, including a double jump
    // somersault and the falling half. Exercise actual movement and shots.
    for (bool falling : {false, true})
    {
      for (int y = -1; y <= 1; ++y)
      {
        for (int x = -1; x <= 1; ++x)
        {
          World w(room(""), 0, theme, art);
          World::PlayOptions options;
          options.airAim = true;
          w.setPlayOptions(options);
          w.setDoubleJump(true);
          for (int i = 0; i < 3; ++i)
            w.update({});
          PlayerInput in;
          in.jump = {true, true};
          w.update(in);
          in.jump.triggered = false;
          w.update(in);
          if (falling)
          {
            for (int i = 0; i < 20 && w.player().state != PlayerState::Falling; ++i)
              w.update(in);
            check(w.player().state == PlayerState::Falling, "falling before the aimed shot");
          }
          else
          {
            // A second jump always somersaults, even with no movement.
            in.jump = {};
            w.update(in);
            in.jump = {true, true};
            w.update(in);
            in.jump.triggered = false;
          }
          in.left = x < 0;
          in.right = x > 0;
          in.up = y < 0;
          in.down = y > 0;
          in.fire = {true, true};
          w.update(in);
          const Projectile* shot = lastShot(w);
          const std::string label = std::string(falling ? "falling" : "double jumping") +
            " direction (" + std::to_string(x) + "," + std::to_string(y) + ")";
          check(w.player().state == (falling ? PlayerState::Falling : PlayerState::Jumping),
            label + " stays airborne");
          if (x == 0 && y == 0)
            check(shot && !shot->aimed && shot->dx == w.player().facing && shot->dy == 0,
              label + " shoots ahead without aim");
          else
          {
            const float len = std::hypot(float(x), float(y));
            check(shot && shot->aimed && std::fabs(shot->vx - float(x) / len) < 1e-4f &&
                std::fabs(shot->vy - float(y) / len) < 1e-4f,
              label + " shoots along the held direction");
          }
        }
      }
    }

    // Explicit aim wins over movement; releasing keys clears directional
    // aim, landing restores the ground stance, and bots keep old controls.
    for (bool enabled : {false, true})
    {
      World w(room(""), 0, theme, art);
      World::PlayOptions options;
      options.airAim = enabled;
      w.setPlayOptions(options);
      for (int i = 0; i < 3; ++i)
        w.update({});
      PlayerInput in;
      in.jump = {true, true};
      w.update(in);
      in.jump.triggered = false;
      in.up = true;
      in.fire = {true, true};
      w.update(in);
      const Projectile* shot = lastShot(w);
      check(shot && (enabled ? shot->aimed && shot->vy < -0.99f : !shot->aimed && shot->dy == 0),
        enabled ? "up shoots up during the first jump" : "bot controls retain the old airborne shot");
      in.right = true;
      in.aim = way(-0.6f, -0.8f);
      w.update(in);
      shot = lastShot(w);
      check(shot && shot->aimed && std::fabs(shot->vx + 0.6f) < 1e-4f &&
          std::fabs(shot->vy + 0.8f) < 1e-4f,
        "explicit aim overrides the held movement directions in the air");
      in = {};
      in.fire = {true, true};
      w.update(in);
      shot = lastShot(w);
      check(shot && !shot->aimed && shot->dy == 0, "releasing directions restores the ahead shot");
      in.left = in.right = in.up = in.down = true;
      w.update(in);
      shot = lastShot(w);
      check(shot && !shot->aimed && shot->dy == 0, "opposing directions cancel while aiming in the air");
      in = {};
      in.fire = {true, true};
      for (int i = 0; i < 30 && w.player().state != PlayerState::OnGround; ++i)
        w.update({});
      check(w.player().state == PlayerState::OnGround, "landed after aiming in the air");
      in.down = true;
      w.update(in);
      shot = lastShot(w);
      check(shot && !shot->aimed && shot->dy == 0 && w.player().visual == PlayerVisual::Crouching,
        "down on the ground crouches and shoots level again");
      in.down = false;
      in.up = true;
      in.right = true;
      w.update(in);
      shot = lastShot(w);
      check(shot && !shot->aimed && shot->dx == 0 && shot->dy == -1,
        "up and right on the ground retain the vertical stance shot");
    }

    // Bounce House bypasses ordinary movement's stance reset. Releasing
    // vertical prototype aim must still restore the ahead shot.
    for (bool right : {false, true})
    {
      World w(room("", "weapon=pulse_pistol\nrules=bounce\n"), 0, theme, art);
      World::PlayOptions options;
      options.airAim = true;
      w.setPlayOptions(options);
      for (int i = 0; i < 3; ++i)
        w.update({});
      check(w.cheat(Cheat::Ammo) && w.player().weapon == Weapon::Proto, "equip prototype in Bounce House");
      PlayerInput in;
      in.up = true;
      in.fire = {true, true};
      w.update(in);
      const Projectile* shot = lastShot(w);
      check(shot && shot->dy == -1, "prototype aims upward during a bounce");
      w.update({}); // release fire and aim, then clear the pistol cooldown
      w.cheat(Cheat::Ammo);
      in.up = false;
      in.right = right;
      w.update(in);
      shot = lastShot(w);
      check(shot && shot->dy == 0 && shot->dx == w.player().facing,
        right ? "horizontal input after upward prototype aim shoots ahead" :
                "releasing upward prototype aim during a bounce shoots ahead");
    }

    // A prototype keeps its own shots.
    {
      World w(room("", "weapon=pulse_pistol\n", 'W'), 0, theme, art);
      Driver d{w};
      for (int i = 0; i < 3; ++i)
        d.step(false);
      // The level's prototype is in the box: shot open crouched, walked over.
      for (int i = 0; i < 300 && w.player().weapon != Weapon::Proto; ++i)
        d.step(i % 2 == 0 && i < 8, {}, false, i >= 40, i < 40);
      for (int i = 0; i < 8; ++i)
        d.step(false);
      const bool proto = w.player().weapon == Weapon::Proto;
      check(proto, "the prototype is in hand");
      if (proto)
      {
        check(!w.aimsFreely(), "a prototype does not aim freely");
        d.step(true, way(0.6f, -0.8f));
        const Projectile* s = lastShot(w);
        check(!s || !s->aimed, "...its shot is not a free-aimed one");
        World::PlayOptions options;
        options.airAim = true;
        w.setPlayOptions(options);
        for (int y : {-1, 1})
        {
          for (int i = 0; i < 30; ++i)
            w.update({});
          PlayerInput in;
          in.jump = {true, true};
          w.update(in);
          in.jump.triggered = false;
          in.up = y < 0;
          in.down = y > 0;
          in.fire = {true, true};
          w.update(in);
          s = lastShot(w);
          check(w.player().state == PlayerState::Jumping && s && !s->aimed && s->dx == 0 && s->dy == y,
            y < 0 ? "prototype shoots up during a jump" : "prototype shoots down during a jump");
        }
      }
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf("%s\n", gFailures == 0 ? "all passed" : "FAILURES");
  return gFailures == 0 ? 0 : 1;
}
