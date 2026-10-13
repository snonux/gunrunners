// Level 37's mechanics, played by hand in small rooms: a booth swap and
// each runner's own hearts; heavy plates (Rocco opens a shutter, Dash
// can't); spring racks (Dash 14 cells, Rocco 4, Nova into the ceiling
// spikes and a tumble back without being thrown again); a cutting table
// coming down; the push rack and the vault door; a Mannequin frozen while
// watched; a decoy drawing a Mannequin; the carrier Powder Puffer's cloud;
// a Rack Runner's knockback; respawning as the checkpoint's runner; the
// OUT OF ORDER booth's air-run; the Gorgazilla suit; the C switch being
// off; a save; and the Totem's throw, switch, restack and heavy plate.
// Then the bot clears the Wardrobe and Totem with every runner.

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
  const std::string& extra = "weapon=decoy_launcher\n")
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
  std::string s = "name=WARDROBE TEST\nepisode=6\ntheme=wardrobe\n" + extra + "[map]\n";
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

// Up inside the booth the runner stands in, then out from behind the curtain.
void useBooth(World& w, Keys& prev)
{
  Keys up;
  up.up = true;
  step(w, up, prev);
  hold(w, Keys{}, kBoothCurtain + 2, prev);
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

    {
      // A booth swap: Dash steps into the ROCCO booth and comes out as
      // Rocco with Rocco's full hearts; back in a DASH booth, Dash has the
      // hearts Dash had.
      World w = make(room(4, 21, "@ booth 4 21 runner=rocco\n@ booth 12 21 runner=dash\n", {{2, 21, 2, 21, '^'}}));
      Keys prev;
      hold(w, left, 5, prev);
      hold(w, right, 5, prev);
      idle(w, 25);
      const int dashHp = w.player().hp;
      useBooth(w, prev);
      const bool rocco = w.characterIndex() == 1 && w.player().hp == characterByIndex(1).maxHp;
      hold(w, right, 16, prev);
      idle(w, 2);
      useBooth(w, prev);
      std::printf("     dash hp %d, back as dash %d (runner %d)\n", dashHp, w.player().hp, w.characterIndex());
      check(dashHp < characterByIndex(0).maxHp && rocco, "a booth turns Dash into Rocco, with Rocco's own full hearts");
      check(w.characterIndex() == 0 && w.player().hp == dashHp, "back in a DASH booth, Dash has the hearts Dash had");
      check(!w.switchCharacter(2) && w.characterIndex() == 0, "the C key's runner switch is off: runners change in booths");
    }
    {
      // A heavy plate: Dash is too light, Rocco opens the shutter (latched).
      const std::string ents = "@ shutter D0 rect=20,18,20,21\n@ switch PL0 12 21 kind=plate weight=2 latch=1 target=D0 w=2\n";
      World w = make(room(9, 21, ents));
      Keys prev;
      hold(w, right, 8, prev);
      idle(w, 4);
      check(w.map().block(20, 21) == Tile::Solid && !w.wardrobe().plates[0].down, "Dash on a heavy plate: nothing");
      World v = make(room(9, 21, ents), 1);
      hold(v, right, 8, prev);
      idle(v, 4);
      hold(v, left, 10, prev);
      check(v.map().block(20, 21) == Tile::Empty && v.wardrobe().plates[0].down, "Rocco presses it: the shutter rolls up and stays up");
      // A save with the shutter open loads with it open.
      const SaveGame s = v.snapshot();
      World u = make(room(9, 21, ents), 1);
      const bool ok = u.restore(s);
      check(ok && u.map().block(20, 21) == Tile::Empty && u.wardrobe().plates[0].down, "a save loads with the shutter open");
    }
    {
      // Spring racks: Dash 14 cells, Rocco 4; Nova hits the ceiling spikes,
      // loses a heart, tumbles back onto the rack and isn't thrown again.
      const std::string ents = "@ rack 10 21 w=2\n";
      const std::vector<Wall> spikes = {{8, 9, 13, 9, 'v'}};
      int rise[3] = {0, 0, 0};
      for (int c : {0, 1, 2})
      {
        World w = make(room(8, 21, ents, spikes), c);
        Keys prev;
        const int y0 = w.player().y;
        int top = y0;
        hold(w, right, 2, prev);
        for (int f = 0; f < 30; ++f)
        {
          step(w, none, prev);
          top = std::min(top, w.player().y);
        }
        rise[c] = y0 - top;
        if (c == 2)
        {
          idle(w, 30);
          std::printf("     nova: hp %d of %d, %s, on rack %d\n", w.player().hp, w.player().maxHp,
            w.player().state == PlayerState::OnGround ? "on the ground" : "in the air", w.wardrobe().onRack);
          check(w.player().hp == w.player().maxHp - 1 && w.player().state == PlayerState::OnGround && w.wardrobe().onRack == 0,
            "Nova hits the ceiling spikes, tumbles back onto the rack and stays there");
        }
      }
      std::printf("     rises: dash %d rocco %d nova %d\n", rise[0], rise[1], rise[2]);
      // (the top of a launch is a cell short of its count: the fall starts that frame)
      check(rise[0] >= kRackDash - 1 && rise[1] >= kRackRocco - 1 && rise[1] <= kRackRocco && rise[2] < kRackNova - 1,
        "a rack throws Dash 14 cells and Rocco 4");
    }
    {
      // A cutting table comes down flush with the floor once Rocco is on its plate.
      const std::string ents =
        "@ table TB1 rect=14,16,17,16 to=22\n@ switch PL1 10 21 kind=plate weight=2 target=TB1 w=2\n";
      World w = make(room(8, 21, ents, {{14, 22, 17, 23, '.'}}), 1);
      Keys prev;
      hold(w, right, 3, prev);
      idle(w, 40);
      check(w.map().block(14, 22) == Tile::Platform && w.map().block(14, 16) == Tile::Empty && w.wardrobe().tables[0].down(),
        "Rocco's plate brings the cutting table down flush with the floor");
    }
    {
      // The push rack: Dash can't move it; Rocco pushes it off the vault
      // door, then up at the door takes him through.
      const std::string ents = "@ pushrack PR1 12 19 w=2 h=3\n@ vaultdoor 12 21 to=30,21\n";
      World w = make(room(8, 21, ents));
      Keys prev;
      hold(w, right, 30, prev);
      check(w.wardrobe().pushRacks[0].x == 12, "Dash can't push the rack");
      World v = make(room(8, 21, ents), 1);
      hold(v, right, 40, prev);
      const int rx = v.wardrobe().pushRacks[0].x;
      while (v.player().x > 23)
        step(v, left, prev);
      step(v, none, prev);
      step(v, up, prev);
      step(v, none, prev);
      std::printf("     rack at %d, runner at %d,%d\n", rx, v.player().x, v.player().y);
      check(rx == 15 && v.map().block(15, 21) == Tile::Solid && v.map().block(12, 21) == Tile::Empty,
        "Rocco pushes it three blocks along");
      check(v.player().x == 60 && v.player().y == 43, "up at the vault door, through it");
    }
    {
      // A Mannequin holds still while the runner faces it and comes on once
      // they look away.
      World w = make(room(4, 21, "@ mannequin 20 21\n"));
      Keys prev;
      const int x0 = firstOf(w, EnemyKind::Mannequin)->x;
      idle(w, 30);
      const bool held = firstOf(w, EnemyKind::Mannequin)->x == x0;
      step(w, left, prev);
      idle(w, 30);
      const int x1 = firstOf(w, EnemyKind::Mannequin)->x;
      check(held && x1 < x0, "a Mannequin moves only while the runner looks away");
    }
    {
      // A decoy lands looking back at the runner; a Mannequin past it walks
      // to it and goes for it.
      World w = make(room(4, 21, "@ mannequin 18 21\n"));
      w.cheat(Cheat::Ammo);
      Keys prev;
      step(w, fire, prev);
      step(w, none, prev);
      idle(w, 12);
      const bool landed = !w.wardrobe().decoys.empty() && w.wardrobe().decoys[0].landed;
      step(w, left, prev); // the runner looks away: only the decoy watches
      idle(w, 50);
      const auto& ds = w.wardrobe().decoys;
      const Enemy* m = firstOf(w, EnemyKind::Mannequin);
      std::printf("     decoy %s, mannequin at %d\n", ds.empty() ? "gone" : "standing", m ? m->x : -1);
      check(landed && (ds.empty() || ds[0].hp < kDecoyHp), "a Mannequin goes for the decoy");
    }
    {
      // The carrier Powder Puffer's cloud infects the runner.
      World w = make(room(4, 21, "@ powder_puffer 16 21 carrier=1\n"));
      Keys prev;
      hold(w, right, 20, prev);
      idle(w, 10);
      check(w.player().virus > 0, "the carrier Powder Puffer's green cloud infects");
    }
    {
      // A Rack Runner rolls into the runner: a heart and 3 blocks back.
      World w = make(room(8, 21, "@ rackrunner 20 21 lane=4,36\n"));
      const int hp = w.player().hp, x0 = w.player().x;
      idle(w, 40);
      std::printf("     hp %d -> %d, x %d -> %d\n", hp, w.player().hp, x0, w.player().x);
      check(w.player().hp == hp - 1 && w.player().x <= x0 - 4, "a Rack Runner knocks the runner back");
    }
    {
      // Respawn: as the runner who touched the checkpoint, every pool full.
      World w = make(room(4, 21, "@ booth 4 21 runner=rocco\n", {{8, 21, 8, 21, 'c'}, {14, 22, 15, 23, '.'}}));
      Keys prev;
      useBooth(w, prev);
      hold(w, right, 30, prev);
      idle(w, 80);
      std::printf("     runner %d, deaths %d, hp %d\n", w.characterIndex(), w.stats().deaths, w.player().hp);
      check(w.stats().deaths == 1 && w.characterIndex() == 1 && w.player().hp == characterByIndex(1).maxHp &&
          w.wardrobe().pools[0] < 0,
        "back at the checkpoint as Rocco, who touched it, with every pool full");
    }
    {
      // The OUT OF ORDER booth: a cartoon runs on air over a 4-block gap.
      World w = make(room(4, 21, "@ booth 4 21 runner=toon\n", {{10, 22, 13, 23, '.'}}));
      Keys prev;
      useBooth(w, prev);
      hold(w, right, 16, prev);
      std::printf("     toon %d, runner at %d,%d\n", w.wardrobe().toon, w.player().x, w.player().y);
      check(w.wardrobe().toon && w.player().y == 43 && w.player().x >= 26, "the cartoon runs on air across the gap");
    }
    {
      // The Gorgazilla suit: touch it, use a booth, and a Mannequin backs off.
      World w = make(room(4, 21, "@ booth 4 21 runner=dash\n@ suit 7 21\n@ mannequin 18 21\n"));
      Keys prev;
      hold(w, right, 4, prev);
      hold(w, left, 4, prev);
      idle(w, 2);
      useBooth(w, prev);
      const int x0 = firstOf(w, EnemyKind::Mannequin)->x;
      step(w, left, prev);
      idle(w, 40);
      std::printf("     suit %d, mannequin %d -> %d\n", w.wardrobe().suit, x0, firstOf(w, EnemyKind::Mannequin)->x);
      check(w.wardrobe().suit && firstOf(w, EnemyKind::Mannequin)->x > x0, "in the Gorgazilla suit, Mannequins back off");
    }
    {
      // Totem: jump throws Nova off the top; down hands control back to
      // Rocco (with Dash); walking into Nova stacks all three again, and the
      // whole stack presses a weight-4 plate.
      const std::string ents = "@ switch PL 11 21 kind=plate weight=4 latch=1 target=G w=2\n@ shutter G rect=30,1,30,21\n";
      World w = make(room(5, 21, ents, {}, "rules=totem\n"));
      Keys prev;
      const bool stacked = w.characterIndex() == 1 && w.totemAbove(w.totemPlayer()) >= 0 &&
                           w.totemAbove(w.totemAbove(w.totemPlayer())) >= 0;
      step(w, jump, prev);
      step(w, none, prev);
      const bool thrown = w.characterIndex() == 2;
      idle(w, 30);
      const bool plateUp = !w.wardrobe().plates[0].down;
      hold(w, down, kTotemSwitch + 1, prev);
      step(w, none, prev);
      const bool rocco = w.characterIndex() == 1 && w.totemAbove(w.totemPlayer()) >= 0;
      hold(w, right, 20, prev);
      idle(w, 2);
      int n = 0;
      for (int i = w.totemPlayer(); i >= 0; i = w.totemAbove(i))
        ++n;
      std::printf("     stack %d, nova at %d, rocco at %d, plate %d\n", n, w.wardrobe().bodies[2].x, w.player().x,
        w.wardrobe().plates[0].down);
      check(stacked && thrown, "Totem: jump throws Nova off the top of the stack");
      check(plateUp, "Nova alone doesn't press a heavy plate");
      check(rocco, "down hands control back to Rocco, Dash still on his shoulders");
      check(n == 3 && w.characterIndex() == 1, "walking into Nova stacks all three again");
      check(w.wardrobe().plates[0].down && w.map().block(30, 10) == Tile::Empty, "the whole stack presses the weight-4 plate");
    }

    // The bot clears the Wardrobe and Totem with every runner.
    const auto level = std::make_shared<const Level>(Level::loadFile(argv[1]));
    const auto bonus = std::make_shared<const Level>(Level::loadFile(argv[2]));
    for (int character = 0; character < 6; ++character)
    {
      World w(level, character, theme, art);
      Bot bot;
      const int f = botPlay(w, 3000, bot);
      std::printf("     runner %d: %d frames, %d deaths\n", character, f, w.stats().deaths);
      check(w.state() != WorldState::Playing, "the bot clears the Wardrobe");
    }
    for (int character = 0; character < 6; ++character)
    {
      World w(bonus, character, theme, art);
      Bot bot;
      const int f = botPlay(w, 1350, bot);
      std::printf("     runner %d: %d frames\n", character, f);
      check(w.state() != WorldState::Playing && !w.bonusFailed(), "the bot clears Totem");
    }
  }
  SDL_DestroyRenderer(sdl);
  SDL_FreeSurface(surface);
  SDL_Quit();
  std::printf(gFailures == 0 ? "all wardrobe tests passed\n" : "%d wardrobe test(s) failed\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
