// Level 42, The Live Finale: the set wagons' turntable rims, the gravity
// plates, the studio monitors, the backstage refill, the ON AIR door, the
// trapdoors, the decks and the switch's glass cover, the Censor Bars'
// black bars, the Ad Breaks' billboards, the Stunt Doubles' masks, Lance
// Marquee and the Encore Engine, the roll-in, the static wipe, CUT TO
// COMMERCIAL and the BLOOPER! card, the ratings ticker and the Encore's
// wheel on the HUD. Placeholder shapes until the art pass.

#include "game/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32

void box(Renderer& r, const CellBox& b, float camX, float camY, Color c, Blend bl = Blend::Alpha)
{
  r.fillRect(float(b.x) * kCellPx - camX, float(b.y) * kCellPx - camY, float(b.w) * kCellPx, float(b.h) * kCellPx, c, bl);
}

void frameBox(Renderer& r, const CellBox& b, float camX, float camY, Color c, float t = 6.0f)
{
  const float x = float(b.x) * kCellPx - camX, y = float(b.y) * kCellPx - camY;
  const float w = float(b.w) * kCellPx, h = float(b.h) * kCellPx;
  r.fillRect(x, y, w, t, c);
  r.fillRect(x, y + h - t, w, t, c);
  r.fillRect(x, y, t, h, c);
  r.fillRect(x + w - t, y, t, h, c);
}

CellBox layerBox(const Layer& l)
{
  return {l.x0 * kCellsPerTile, l.y0 * kCellsPerTile, (l.x1 - l.x0 + 1) * kCellsPerTile, (l.y1 - l.y0 + 1) * kCellsPerTile};
}

Color wagonColor(FnWagonState s)
{
  switch (s)
  {
    case FnWagonState::Running:
      return rgb(255, 215, 90);
    case FnWagonState::Rerun:
      return rgb(200, 120, 255);
    case FnWagonState::Rolling:
      return rgb(255, 120, 60);
    default:
      return rgb(90, 86, 120);
  }
}

const char* moveName(FnEngineMove m)
{
  static const char* const kNames[] = {"BEAT BARRAGE", "GOLD FIST", "CORE PULSE", "ARC STORM", "MAGNET GRAB",
    "ATOMIC BREATH"};
  return kNames[std::clamp(int(m), 0, 5)];
}

const char* attackName(FnAttack a)
{
  switch (a)
  {
    case FnAttack::Gold:
      return "GOLD";
    case FnAttack::Trapdoor:
      return "TRAPDOOR";
    case FnAttack::Laugh:
      return "LAUGH";
    case FnAttack::Light:
      return "LIGHTS";
    default:
      return "";
  }
}

} // namespace

void World::drawFinaleBack(Renderer& r, float camX, float camY, int frame, float /*alpha*/) const
{
  const auto& f = mFinale;
  const auto& b = f.boss;
  if (f.reel)
    return;
  // The turntable's rim under each wagon's slot, lit while its trick runs.
  for (const auto& w : f.wagons)
  {
    const CellBox rim{w.x0 * kCellsPerTile, 40 * kCellsPerTile - 1, (w.x1 - w.x0 + 1) * kCellsPerTile, 1};
    if (!isOnScreen(rim, 4))
      continue;
    const Color c = wagonColor(w.state);
    // A thin lit strip along the floor's edge, not a whole cell.
    r.fillRect(float(rim.x) * kCellPx - camX, float(rim.y + 1) * kCellPx - 8.0f - camY, float(rim.w) * kCellPx, 8.0f,
               withAlpha(c, w.state == FnWagonState::Running && (frame / 6) % 2 ? 230 : 140));
    const CellBox trig{w.trigger * kCellsPerTile, 40 * kCellsPerTile - 2, 1, 2};
    box(r, trig, camX, camY, withAlpha(rgb(255, 255, 255), 120));
  }
  // The pedestal under the first W: "Contestants, your arsenal!"
  if (f.pedX >= 0)
  {
    box(r, {f.pedX, f.pedY - 1, 2 * kCellsPerTile + 2, 2}, camX, camY, rgb(150, 120, 70));
    if (isOnScreen({f.pedX - 8, f.pedY - 12, 20, 12}, 0) && mPlayer.x < 16 * kCellsPerTile)
      r.drawText("CONTESTANTS, YOUR ARSENAL!", (float(f.pedX) + 2.0f) * kCellPx - camX, float(f.pedY - 8) * kCellPx - camY,
                 {20.0f, rgb(255, 215, 90), rgb(0, 0, 0)}, Align::Center);
  }
  // Gravity plates: the arrows flash before the flip.
  for (const auto& pl : f.plates)
  {
    const CellBox pb{pl.bx * kCellsPerTile, pl.by * kCellsPerTile + (pl.dir == Grav::Up ? 1 : 0), kCellsPerTile, 1};
    const bool flash = pl.flip > 0 && (frame / 2) % 2;
    box(r, pb, camX, camY, flash ? rgb(255, 255, 255) : rgb(120, 220, 255));
    const float ax = (float(pb.x) + 1.0f) * kCellPx - camX, ay = float(pb.y) * kCellPx - camY;
    const float dy = pl.dir == Grav::Up ? -24.0f : 24.0f;
    r.drawLine(ax, ay + dy * 2.0f, ax, ay + dy, 5.0f, flash ? rgb(255, 255, 255) : rgb(120, 220, 255));
  }
  // The studio monitors.
  bool studio = false;
  for (const auto& w : f.wagons)
    studio = studio || (w.kind == FnWagonKind::Studio && w.state == FnWagonState::Running);
  for (const auto& po : f.portals)
  {
    const CellBox m{po.bx * kCellsPerTile, po.by * kCellsPerTile, 2 * kCellsPerTile, 3 * kCellsPerTile};
    if (!isOnScreen(m, 2))
      continue;
    box(r, m, camX, camY, rgb(30, 30, 40));
    const CellBox scr{m.x + 1, m.y + 1, m.w - 2, m.h - 3};
    box(r, scr, camX, camY, studio ? withAlpha(rgb(90, 200, 255), 120 + 60 * ((frame / 3) % 2)) : rgb(16, 16, 22));
    r.drawText(po.label, (float(m.x) + 2.0f) * kCellPx - camX, float(m.y + 2) * kCellPx - camY,
               {18.0f, studio ? rgb(255, 60, 60) : rgb(110, 110, 120), 0}, Align::Center);
  }
  // The backstage refill.
  if (f.refillX >= 0)
  {
    const CellBox rb{f.refillX, f.refillY - 3, 2 * kCellsPerTile, 4};
    box(r, rb, camX, camY, f.refillUsed ? rgb(70, 60, 50) : rgb(200, 60, 80));
    if (!f.refillUsed)
      r.drawText("REFILL", (float(rb.x) + 2.0f) * kCellPx - camX, float(rb.y) * kCellPx - camY - 24.0f,
                 {16.0f, rgb(255, 240, 200), rgb(0, 0, 0)}, Align::Center);
  }
  // The ON AIR door, the trapdoors, the decks, the glass cover.
  if (f.door >= 0)
  {
    const Layer& l = mLayers[std::size_t(f.door)];
    const CellBox d = layerBox(l);
    if (l.solid)
      box(r, d, camX, camY, rgb(70, 20, 30));
    const bool lit = b.phase != FnPhase::Switched || (b.signDead < 30 && (frame / 2) % 2);
    r.drawText("ON AIR", (float(d.x) + 1.0f) * kCellPx - camX, float(d.y - 3) * kCellPx - camY,
               {22.0f, lit ? rgb(255, 50, 60) : rgb(70, 40, 40), rgb(0, 0, 0)}, Align::Center);
  }
  for (int k = 0; k < 3; ++k)
  {
    const int li = f.traps[std::size_t(k)];
    if (li < 0)
      continue;
    const Layer& l = mLayers[std::size_t(li)];
    const CellBox t = layerBox(l);
    if (l.solid)
      box(r, t, camX, camY, rgb(110, 80, 50));
    else
      box(r, t, camX, camY, withAlpha(rgb(200, 200, 220), 60 + 80 * ((frame / 2) % 2))); // the static flicker
    const bool warn = b.trap == k && b.openTrap != k &&
      ((b.phase == FnPhase::Lance && b.attack == FnAttack::Trapdoor) || b.phase == FnPhase::OffAir);
    if (warn && (frame / 2) % 2)
      frameBox(r, t, camX, camY, rgb(255, 230, 80), 5.0f);
  }
  for (int li : {f.deckStep, f.deckMid, f.deckHigh})
    if (li >= 0 && mLayers[std::size_t(li)].solid)
      box(r, layerBox(mLayers[std::size_t(li)]), camX, camY, rgb(150, 150, 170));
  if (f.cover >= 0 && b.coverHp > 0)
  {
    const CellBox c = layerBox(mLayers[std::size_t(f.cover)]);
    box(r, c, camX, camY, withAlpha(rgb(180, 230, 255), b.hitFlash > 0 ? 220 : 120));
  }
  if (!b.on || !isOnScreen({b.arenaX0, b.arenaY0, b.arenaX1 - b.arenaX0 + 1, b.floorY - b.arenaY0 + 1}, 4))
    return;
  // The master switch on its console.
  box(r, {b.deskX - 2, 70, 2, 2}, camX, camY, b.phase == FnPhase::Switched ? rgb(80, 255, 120) : rgb(255, 60, 60));
  // The Encore Engine stands at the back.
  if (b.phase == FnPhase::Engine || b.phase == FnPhase::Collapse)
  {
    CellBox eb = finaleEngine(b);
    if (b.phase == FnPhase::Collapse)
    {
      const int sink = std::min(eb.h - 2, b.t / 3);
      eb.y += sink;
      eb.h -= sink;
    }
    box(r, eb, camX, camY, b.hitFlash > 0 ? rgb(255, 255, 255) : rgb(110, 110, 140));
    box(r, {eb.x + 2, eb.y + 1, eb.w - 4, 3}, camX, camY, rgb(60, 60, 80)); // the head
    if (b.phase == FnPhase::Engine)
    {
      const bool open = b.core > 0 || b.stagger > 0;
      box(r, finaleCore(b), camX, camY, open ? ((frame / 2) % 2 ? rgb(255, 240, 120) : rgb(255, 160, 40)) : rgb(50, 50, 70));
      if (b.stagger > 0)
        r.drawText("STAGGERED x2", (float(eb.x) + 8.0f) * kCellPx - camX, float(eb.y - 3) * kCellPx - camY,
                   {20.0f, rgb(255, 240, 120), rgb(0, 0, 0)}, Align::Center);
    }
  }
  // Off the Air: Lance running along the back wall for the desk.
  if (b.phase == FnPhase::OffAir)
  {
    box(r, {b.runX, b.arenaY0 + 1, 3, 5}, camX, camY, rgb(230, 200, 60));
    box(r, {b.deskX - 4, b.arenaY0 + 4, 6, 2}, camX, camY, rgb(90, 60, 40)); // the broadcast desk
  }
}

void World::drawFinaleFront(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  const auto& f = mFinale;
  const auto& b = f.boss;
  // Encore shots glow in their prototype's colour.
  for (const auto& pr : mProjectiles)
    if (pr.alive && pr.encore >= 0)
    {
      const float x = (float(pr.x) + float(pr.w) * 0.5f) * kCellPx - camX, y = (float(pr.y) + 0.5f) * kCellPx - camY;
      r.fillRect(x - 10.0f, y - 10.0f, 20.0f, 20.0f, withAlpha(protoDef(pr.encore).color, 200), Blend::Add);
    }
  for (const auto& a : f.actors)
  {
    const Enemy& e = mEnemies[std::size_t(a.enemy)];
    // A Censor Bar's black bar, over everything below it until it is shot down.
    if (a.kind == FnActorKind::Censor && a.bar >= 0 && e.alive)
    {
      const int w = kFnBarW * std::min(a.bar, kFnBarUnroll) / kFnBarUnroll;
      box(r, {a.barX + (kFnBarW - w) / 2, a.barY, std::max(1, w), kFnBarH}, camX, camY, rgb(0, 0, 0));
      r.drawText("CENSORED", (float(a.barX) + float(kFnBarW) * 0.5f) * kCellPx - camX,
                 (float(a.barY) + 2.0f) * kCellPx - camY, {22.0f, rgb(60, 60, 60), 0}, Align::Center);
    }
    if (!e.alive || e.hidden || !isOnScreen(e.box(), 4))
      continue;
    const float ex = (float(e.x) + float(e.w) * 0.5f) * kCellPx - camX, top = float(e.y - e.h + 1) * kCellPx - camY;
    if (a.kind == FnActorKind::AdBreak)
    {
      static const Color kAd[] = {rgb(255, 90, 60), rgb(255, 210, 60), rgb(90, 200, 255), rgb(200, 120, 255)};
      const float drop = a.roll < kFnAdRoll ? float(kFnAdRoll - a.roll) * kCellPx : 0.0f;
      r.fillRect(ex - 90.0f, top - drop + 10.0f, 180.0f, 120.0f, kAd[(frame / 4) % 4]);
      r.drawText("HALCYON ARMS", ex, top - drop + 50.0f, {18.0f, rgb(20, 20, 20), 0}, Align::Center);
      if (e.tell > 0)
        r.drawText("JINGLE!", ex, top - drop - 30.0f, {22.0f, rgb(255, 230, 80), rgb(0, 0, 0)}, Align::Center);
    }
    if (a.kind == FnActorKind::Double)
    {
      static const Color kSuits[] = {rgb(60, 140, 255), rgb(255, 140, 40), rgb(255, 90, 190)};
      r.fillRect(ex - 40.0f, top + 20.0f, 80.0f, 18.0f, rgb(10, 10, 10)); // the mask
      r.fillRect(ex - 44.0f, top + 60.0f, 88.0f, 12.0f, kSuits[a.runner]);
      if (e.tell > 0)
        r.fillRect(ex + float(e.dir) * 60.0f - 12.0f, top + 50.0f, 24.0f, 24.0f, rgb(255, 240, 160), Blend::Add);
    }
  }
  // The rubber duck's sequined host jacket.
  for (const auto& it : mItems)
    if (!it.taken && it.kind == ItemKind::Duck && !f.reel)
    {
      const float x = (float(it.x) + 1.0f) * kCellPx - camX, y = (float(it.y) - 0.3f) * kCellPx - camY;
      r.fillRect(x - 18.0f, y - 4.0f, 36.0f, 14.0f, (frame / 5) % 2 ? rgb(255, 215, 90) : rgb(230, 60, 140));
      r.fillRect(x - 6.0f, y - 18.0f, 12.0f, 4.0f, rgb(230, 230, 240)); // chrome teeth
    }
  if (!b.on || b.phase == FnPhase::Waiting || b.phase == FnPhase::Switched)
    return;
  const float tall = float(b.floorY - b.arenaY0 + 1);
  if (b.phase == FnPhase::Lance || b.phase == FnPhase::Intro)
  {
    // Lance on his hover podium; the prize wheel over it.
    const float px = (float(b.prevX) + float(b.x - b.prevX) * alpha) * kCellPx - camX;
    const float py = (float(b.prevY) + float(b.y - b.prevY) * alpha) * kCellPx - camY;
    const bool cue = b.cue > 0;
    r.fillRect(px, py, float(kFnPodiumW) * kCellPx, float(kFnPodiumH) * kCellPx,
               b.hitFlash > 0 ? rgb(255, 255, 255) : rgb(200, 170, 80));
    r.fillRect(px + 16.0f, py + 16.0f, float(kFnPodiumW) * kCellPx - 32.0f, 48.0f,
               cue ? ((frame / 3) % 2 ? rgb(255, 90, 60) : rgb(255, 200, 90)) : rgb(120, 90, 40));
    r.fillRect(px + 88.0f, py - 150.0f, 80.0f, 150.0f, rgb(230, 230, 240)); // Lance, in his white tux
    r.fillRect(px + 100.0f, py - 190.0f, 56.0f, 44.0f, rgb(240, 200, 160));
    r.fillRect(px + 104.0f, py - 160.0f, 48.0f, 6.0f, rgb(250, 250, 255)); // the chrome smile
    if (cue)
      r.fillRect(px + 150.0f, py - 120.0f, 70.0f, 50.0f, rgb(250, 250, 240));
    if (b.wheel >= 0)
    {
      const float wx = px + float(kFnPodiumW) * kCellPx * 0.5f, wy = py - 260.0f;
      const float ang = b.wheel >= kFnWheelFrames - kFnWheelHold ? 0.0f : float(b.wheel) * 0.9f;
      r.fillRect(wx - 50.0f, wy - 50.0f, 100.0f, 100.0f, rgb(60, 30, 80));
      r.drawLine(wx, wy, wx + std::sin(ang) * 46.0f, wy - std::cos(ang) * 46.0f, 6.0f, rgb(255, 215, 90));
      if (b.wheel >= kFnWheelFrames - kFnWheelHold)
        r.drawText(attackName(b.pick), wx, wy - 90.0f, {22.0f, rgb(255, 230, 120), rgb(0, 0, 0)}, Align::Center);
    }
    if (b.attack == FnAttack::Laugh && b.waveR < 0)
      r.drawText("LAUGH", px + 128.0f, py - 240.0f, {30.0f, (frame / 2) % 2 ? rgb(255, 60, 60) : rgb(255, 230, 80), rgb(0, 0, 0)},
                 Align::Center);
  }
  // Rain of gold: shadows, then the bars.
  if (b.phase == FnPhase::Lance && b.attack == FnAttack::Gold)
    for (int g : b.gold)
    {
      if (b.attackT < kFnGoldTell)
      {
        const int w = 2 + 4 * b.attackT / kFnGoldTell;
        box(r, {g + (6 - w) / 2, b.floorY, w, 1}, camX, camY, withAlpha(rgb(0, 0, 0), 160));
      }
      else
      {
        const int fall = std::min(b.attackT - kFnGoldTell, 10);
        box(r, {g, b.arenaY0 + int(tall) * fall / 10 - 4, 6, 4}, camX, camY, rgb(255, 215, 60));
      }
    }
  // The laugh-track shockwave.
  for (int wx : {b.waveL, b.waveR})
    if (wx >= 0 && b.waveR >= 0)
      box(r, {wx, b.floorY - 1, 2, 2}, camX, camY, withAlpha(rgb(255, 230, 120), 200), Blend::Add);
  // A studio lamp: its shadow grows, then it falls.
  if (b.lampX >= 0)
  {
    if (b.attackT < kFnLampTell)
    {
      const int w = 1 + 5 * b.attackT / kFnLampTell;
      box(r, {b.lampX - w / 2, b.floorY, w, 1}, camX, camY, withAlpha(rgb(0, 0, 0), 170));
    }
    else
      box(r, {b.lampX - 3, b.floorY - 4, 6, 4}, camX, camY, rgb(90, 90, 100));
  }
  if (b.phase == FnPhase::Engine)
  {
    // The icon over the Engine: its next move, the counter blinking.
    const CellBox eb = finaleEngine(b);
    const float ix = (float(eb.x) + 8.0f) * kCellPx - camX, iy = float(eb.y - 6) * kCellPx - camY;
    if (b.moveT >= 0)
    {
      const int c = finaleCounter(b.move);
      const bool logged = std::find(f.wheel.begin(), f.wheel.end(), c) != f.wheel.end();
      r.drawText(moveName(b.move), ix, iy, {22.0f, rgb(255, 120, 80), rgb(0, 0, 0)}, Align::Center);
      if ((frame / 4) % 2)
        r.drawText(logged ? protoDef(c).name : "BLASTER", ix, iy + 28.0f, {18.0f, rgb(120, 255, 160), rgb(0, 0, 0)},
                   Align::Center);
    }
    for (int cx : {b.coinL, b.coinR})
      if (cx >= 0)
        box(r, {cx, b.floorY - 1, 2, 2}, camX, camY, rgb(255, 215, 60));
    if (b.ring > 0)
      box(r, {b.ex - b.ring, b.ey - 8, b.ring, 5}, camX, camY, withAlpha(rgb(120, 220, 255), 110), Blend::Add);
    if (b.move == FnEngineMove::Arc && b.moveT >= 0)
      for (int a : b.arcs)
      {
        const bool strike = b.moveT > 22;
        box(r, {a, strike ? b.arenaY0 : b.floorY, 4, strike ? int(tall) : 1}, camX, camY,
            strike ? withAlpha(rgb(200, 220, 255), 200) : withAlpha(rgb(120, 160, 255), 120 + 80 * ((frame / 2) % 2)),
            Blend::Add);
      }
    if (b.breath > 0)
      box(r, {b.ex - kFnBreathReach, eb.y, kFnBreathReach, kFnEngineH}, camX, camY,
          withAlpha(rgb(90, 255, 200), 90 + 40 * ((frame / 2) % 2)), Blend::Add);
    if (b.move == FnEngineMove::Magnet && b.moveT > 15)
      r.drawLine(float(b.ex) * kCellPx - camX, float(b.ey - 6) * kCellPx - camY,
                 (float(mPlayer.x) + 1.5f) * kCellPx - camX, float(mPlayer.y - 2) * kCellPx - camY, 4.0f,
                 withAlpha(rgb(255, 60, 60), 160));
  }
}

void World::drawFinaleTop(Renderer& r, float /*camX*/, float /*camY*/, int frame) const
{
  const auto& f = mFinale;
  const float W = float(kScreenW), H = float(kScreenH);
  bool rolling = false;
  const char* name = "";
  for (const auto& w : f.wagons)
    if (w.state == FnWagonState::Rolling)
    {
      rolling = true;
      name = finaleWagonName(w.kind);
    }
  if (rolling)
  {
    // The turntable turns: the stage floor scrolls under a painted turn.
    for (int k = 0; k < 8; ++k)
    {
      const float x = std::fmod(float(frame * 24 + k * 180), W + 180.0f) - 180.0f;
      r.fillRect(x, H * 0.62f, 90.0f, 20.0f, withAlpha(rgb(255, 215, 90), 140));
    }
    r.drawText(std::string("PREVIOUSLY ON: ") + name, W * 0.5f, H * 0.3f, {40.0f, rgb(255, 215, 90), rgb(0, 0, 0)},
               Align::Center);
  }
  if (f.wipe > 0)
  {
    // A monitor's static wipe.
    for (int k = 0; k < 40; ++k)
    {
      const float y = float((k * 37 + frame * 53) % 720);
      r.fillRect(0.0f, y, W, 8.0f, withAlpha(rgb(220, 220, 230), 140));
    }
    r.fillRect(0.0f, 0.0f, W, H, withAlpha(rgb(30, 30, 40), 140));
  }
  if (f.boss.commercial > 0)
  {
    r.fillRect(0.0f, 0.0f, W, H, rgb(20, 16, 40));
    r.drawText("CUT TO COMMERCIAL", W * 0.5f, H * 0.4f, {56.0f, rgb(255, 215, 90), rgb(0, 0, 0)}, Align::Center);
    r.drawText("HALCYON ARMS - WE'LL BE RIGHT BACK", W * 0.5f, H * 0.55f, {26.0f, rgb(255, 255, 255), 0}, Align::Center);
  }
  if (f.reel && f.card > 0)
  {
    // BLOOPER! and a VHS warp.
    for (int k = 0; k < 12; ++k)
      r.fillRect(0.0f, float((k * 61 + frame * 29) % 720), W, 6.0f, withAlpha(rgb(255, 255, 255), 90));
    r.drawText("BLOOPER!", W * 0.5f + float((frame % 3) - 1) * 6.0f, H * 0.35f, {64.0f, rgb(255, 60, 90), rgb(0, 0, 0)},
               Align::Center);
    r.drawText(finaleReelName(f.rule), W * 0.5f, H * 0.5f, {30.0f, rgb(255, 255, 255), rgb(0, 0, 0)}, Align::Center);
  }
}

void World::drawFinaleHud(Renderer& r, int frame) const
{
  const auto& f = mFinale;
  const auto& b = f.boss;
  const auto& p = mPlayer;
  char buf[96];
  float y = 100.0f;
  if (f.reel)
  {
    std::snprintf(buf, sizeof(buf), "THE BLOOPER REEL: %s - NEXT IN %d", finaleReelName(f.rule),
                  std::max(0, (kFnReelEvery - f.reelT + 14) / 15));
    // Under the bonus clock and the messages.
    r.drawText(buf, float(kScreenW) * 0.5f, 150.0f, {22.0f, rgb(255, 120, 150), rgb(0, 0, 0)}, Align::Center);
    return;
  }
  // The ratings ticker: each wagon's trick counting down, RERUN.
  for (const auto& w : f.wagons)
  {
    if (w.state == FnWagonState::Running)
      std::snprintf(buf, sizeof(buf), "%s  %d", finaleWagonName(w.kind), std::max(0, (kFnTrick - w.t + 14) / 15));
    else if (w.state == FnWagonState::Rerun)
      std::snprintf(buf, sizeof(buf), "%s  RERUN", finaleWagonName(w.kind));
    else
      continue;
    r.fillRect(16.0f, y - 4.0f, 300.0f, 32.0f, withAlpha(rgb(10, 10, 24), 190));
    r.drawText(buf, 26.0f, y, {20.0f, w.state == FnWagonState::Rerun && (frame / 6) % 2 ? rgb(200, 120, 255) : rgb(255, 215, 90), 0});
    y += 36.0f;
  }
  // The Encore's wheel.
  if (p.weapon == Weapon::Proto && p.proto == int(ProtoId::Encore))
  {
    const int id = f.wheel[std::size_t(std::clamp(f.sel, 0, int(f.wheel.size()) - 1))];
    std::snprintf(buf, sizeof(buf), "ENCORE: %s  POOL %d", id < 0 ? "BLASTER" : protoDef(id).name, p.ammo);
    r.drawText(buf, float(kScreenW) - 24.0f, 124.0f, // under the gravity badge and the messages
               {20.0f, f.spin > 0 && (frame / 2) % 2 ? rgb(255, 255, 255) : (id < 0 ? rgb(220, 220, 230) : protoDef(id).color),
                rgb(0, 0, 0)},
               Align::Right);
  }
  if (!finaleFight())
    return;
  // The boss: its health, Off the Air's race.
  const float bx = float(kScreenW) * 0.5f - 250.0f, by = float(kScreenH) - 60.0f;
  int hp = 0, max = 1;
  const char* who = "";
  switch (b.phase)
  {
    case FnPhase::Lance:
    case FnPhase::Intro:
      hp = b.lanceHp;
      max = kFnLanceHp;
      who = "LANCE MARQUEE";
      break;
    case FnPhase::Engine:
    case FnPhase::Collapse:
      hp = b.engineHp;
      max = kFnEngineHp;
      who = "THE ENCORE ENGINE";
      break;
    case FnPhase::OffAir:
      hp = b.coverHp;
      max = kFnCoverHp;
      who = "THE GLASS COVER";
      break;
    default:
      break;
  }
  r.fillRect(bx - 6.0f, by - 6.0f, 512.0f, 40.0f, withAlpha(rgb(10, 10, 20), 200));
  r.fillRect(bx, by, 500.0f * float(std::max(0, hp)) / float(max), 28.0f, rgb(255, 60, 80));
  r.drawText(who, bx + 250.0f, by + 2.0f, {18.0f, rgb(255, 255, 255), rgb(0, 0, 0)}, Align::Center);
  if (b.phase == FnPhase::OffAir)
  {
    const float t = float(b.runX - b.runStartX) / float(std::max(1, b.deskX - kFnPodiumW / 2 - b.runStartX));
    r.fillRect(bx, by - 30.0f, 500.0f, 10.0f, rgb(40, 40, 60));
    r.fillRect(bx, by - 30.0f, 500.0f * std::clamp(t, 0.0f, 1.0f), 10.0f, rgb(255, 215, 90));
    r.drawText("LANCE TO THE DESK", bx + 250.0f, by - 58.0f, {16.0f, rgb(255, 215, 90), rgb(0, 0, 0)}, Align::Center);
  }
}

} // namespace gr
