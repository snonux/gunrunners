// Drawing for the vehicles (world_vehicle.cpp): the vehicle sprites with
// their moving parts (the tank's barrel, rotors, jets, the submarine's lamp),
// the runner at the controls, and the vehicle HUD and boarding prompt.

#include "game/world.hpp"

#include "assets/art.hpp"
#include "assets/enemy_art.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace gr
{

namespace
{

constexpr float kCellPx = float(kCellSize) * kPixelScale; // 32
constexpr Color kHudInk = rgb(10, 8, 20);

const char* spriteKey(VehicleKind k)
{
  switch (k)
  {
    case VehicleKind::Tank: return "veh_tank";
    case VehicleKind::Heli: return "veh_helicopter";
    case VehicleKind::Bike: return "veh_hoverbike";
    case VehicleKind::Sub: return "veh_submarine";
    case VehicleKind::Ship: return "veh_spaceship";
    case VehicleKind::Mech: return "veh_mech";
    default: return "veh_tank";
  }
}

// Where the runner sits, as fractions of the box (facing right), and how big.
struct Seat
{
  float fx, fy, scale;
  bool front; // drawn over the vehicle (the hoverbike), else behind its glass
};

Seat seatOf(VehicleKind k)
{
  switch (k)
  {
    case VehicleKind::Tank: return {0.5f, 0.4f, 0.6f, false};
    case VehicleKind::Heli: return {0.82f, 0.62f, 0.55f, false};
    case VehicleKind::Bike: return {0.5f, 0.42f, 0.85f, true};
    case VehicleKind::Sub: return {0.78f, 0.82f, 0.38f, false};
    case VehicleKind::Ship: return {0.66f, 0.5f, 0.4f, false};
    case VehicleKind::Mech: return {0.7f, 0.4f, 0.48f, false};
    default: return {0.5f, 0.5f, 0.5f, false};
  }
}

} // namespace

void World::drawVehicle(Renderer& r, const Vehicle& v, float camX, float camY, int frame, float alpha) const
{
  if (v.wreck > 0)
    return;
  const float bx = (float(v.prevX) + float(v.x - v.prevX) * alpha) * kCellPx - camX;
  const float bottom = (float(v.prevY) + float(v.y - v.prevY) * alpha + 1.0f) * kCellPx - camY;
  const float W = float(v.w) * kCellPx, H = float(v.h) * kCellPx;
  const float top = bottom - H;
  if (bx + W < -160.0f || bx > float(kScreenW) + 160.0f || bottom < -160.0f || top > float(kScreenH) + 160.0f)
    return;
  const int f = v.facing;
  auto at = [&](float fx, float fy) { return Vec2{bx + (f > 0 ? fx : 1.0f - fx) * W, top + fy * H}; };
  const bool running = v.occupied;

  // Animation frames for the baked sprite.
  int anim = 0;
  switch (v.kind)
  {
    case VehicleKind::Tank: anim = (v.step / 2) % 2; break;
    case VehicleKind::Heli: anim = running ? (frame / 2) % 2 : 0; break;
    case VehicleKind::Bike: anim = (frame / 4) % 2; break;
    case VehicleKind::Sub: anim = running ? (frame / 3) % 4 : 0; break;
    case VehicleKind::Ship: anim = running ? (frame / 3) % 2 : 0; break;
    case VehicleKind::Mech: anim = v.air >= 0 ? 0 : (v.step / 2) % 4; break;
    default: break;
  }
  const std::string key = spriteKey(v.kind);
  const Sprite& spr = styledEnemySprite(mArt, r, mTheme, key, 0, anim, v.w, v.h);

  // Behind: the tank's barrel, the submarine's lamp, jets.
  if (v.kind == VehicleKind::Tank)
  {
    const Vec2 pivot = at(0.5f, 0.32f);
    const float a = v.aim ? -0.785f : 0.0f;
    const float len = W * 0.5f;
    const float ex = pivot.x + std::cos(a) * len * float(f), ey = pivot.y + std::sin(a) * len;
    r.drawLine(pivot.x, pivot.y, ex, ey, 16.0f, rgb(26, 20, 38));
    r.drawLine(pivot.x, pivot.y, ex, ey, 10.0f, rgb(90, 112, 64));
    if (v.cool > 6)
      drawGlow(r, mArt, ex + std::cos(a) * 14.0f * float(f), ey + std::sin(a) * 14.0f, 46, rgb(255, 200, 90), 0.9f);
  }
  if (v.kind == VehicleKind::Sub && running)
  {
    const Vec2 lamp = at(0.98f, 0.58f);
    for (int k = 0; k < 4; ++k)
      r.drawLine(lamp.x, lamp.y, lamp.x + float(f) * (220.0f + float(k) * 40.0f), lamp.y - 40.0f + float(k) * 30.0f,
        40.0f, rgba(255, 250, 200, 12), Blend::Add);
    drawGlow(r, mArt, lamp.x, lamp.y, 40, rgb(255, 250, 210), 0.8f);
  }
  if (v.kind == VehicleKind::Ship && running && (v.vx != 0 || v.vy != 0))
  {
    const Vec2 tail = at(-0.02f, 0.55f);
    const float flick = 30.0f + float((frame * 7) % 20);
    r.drawLine(tail.x, tail.y, tail.x - float(f) * flick, tail.y, 18.0f, rgba(120, 200, 255, 200), Blend::Add);
    drawGlow(r, mArt, tail.x - float(f) * flick * 0.5f, tail.y, 50, rgb(120, 200, 255), 0.7f);
  }
  if (v.kind == VehicleKind::Mech && running && v.air >= 0 && v.air < 9)
  {
    const Vec2 jet = at(0.06f, 0.36f);
    r.drawLine(jet.x, jet.y, jet.x - float(f) * 8.0f, jet.y + 60.0f + float(frame % 3) * 8.0f, 20.0f,
      rgba(255, 170, 60, 210), Blend::Add);
    drawGlow(r, mArt, jet.x, jet.y + 40.0f, 50, rgb(255, 150, 50), 0.8f);
  }

  // The runner at the controls.
  const Seat seat = seatOf(v.kind);
  auto drawRider = [&]() {
    if (!running || mPlayer.state == PlayerState::Dying)
      return;
    const auto& ca = mArt.runner(mCharacter);
    const Vec2 s = at(seat.fx, seat.fy);
    DrawOpts o;
    o.scale = seat.scale;
    const Sprite& rs = v.kind == VehicleKind::Bike ? ca.crouch : ca.idle[std::size_t((frame / 30) % 2)];
    r.draw(rs.get(f), s.x, s.y, o);
  };
  if (!seat.front)
    drawRider();

  DrawOpts o;
  if (v.mercy > 0 && running)
    o.alpha = 0.75f + 0.25f * std::sin(float(frame) * 0.6f);
  // The hoverbike bobs on its pads.
  const float bob = v.kind == VehicleKind::Bike ? std::sin(float(frame) * 0.15f) * 3.0f : 0.0f;
  r.draw(spr.get(f), bx + W * 0.5f, bottom + bob, o);
  if (v.flash > 0)
  {
    DrawOpts fo;
    fo.blend = Blend::Add;
    fo.alpha = 0.6f;
    r.draw(spr.get(f), bx + W * 0.5f, bottom + bob, fo);
  }
  if (seat.front)
    drawRider();

  // In front: the main rotor.
  if (v.kind == VehicleKind::Heli)
  {
    const Vec2 hub = at(0.55f, 0.03f);
    const bool spinning = running || !mMap.onSolidGround(v.box());
    if (spinning)
    {
      for (int k = 0; k < 3; ++k)
      {
        const float half = W * 0.55f * std::abs(std::cos(float(frame) * 0.9f + float(k) * 1.05f));
        r.drawLine(hub.x - half, hub.y, hub.x + half, hub.y, 6.0f, rgba(40, 40, 50, 90));
      }
      r.fillRect(hub.x - W * 0.55f, hub.y - 2.0f, W * 1.1f, 4.0f, rgba(200, 200, 220, 40));
    }
    else
      r.drawLine(hub.x - W * 0.5f, hub.y, hub.x + W * 0.5f, hub.y, 7.0f, rgb(50, 50, 60));
    r.fillRect(hub.x - 7.0f, hub.y - 6.0f, 14.0f, 12.0f, rgb(60, 60, 70));
  }

  // Parked: a soft glow when the runner can climb in.
  if (!running && mState == WorldState::Playing && vehicleInReach() == int(&v - mVehicles.data()))
  {
    const float pulse = 0.35f + 0.25f * std::sin(float(frame) * 0.15f);
    drawGlow(r, mArt, bx + W * 0.5f, top + H * 0.5f, std::max(W, H) * 0.7f, vehicleDef(v.kind).color, pulse);
    r.drawText(mUseLabel, bx + W * 0.5f, top - 40.0f, {20.0f, rgb(255, 255, 255), kHudInk}, Align::Center);
  }
}

void World::drawVehicles(Renderer& r, float camX, float camY, int frame, float alpha) const
{
  // Parked ones first, so the one being driven is in front.
  for (const auto& v : mVehicles)
    if (!v.occupied)
      drawVehicle(r, v, camX, camY, frame, alpha);
  for (const auto& v : mVehicles)
    if (v.occupied)
      drawVehicle(r, v, camX, camY, frame, alpha);
}

void World::drawVehicleHud(Renderer& r, int frame) const
{
  if (mState != WorldState::Playing)
    return;
  const Vehicle* v = riding();
  if (!v)
  {
    const int i = vehicleInReach();
    if (i < 0)
      return;
    const VehicleDef& d = vehicleDef(mVehicles[std::size_t(i)].kind);
    const std::string text = (mUpBoards ? "UP OR " + mUseLabel : mUseLabel) + "  -  GET IN THE " + d.name;
    r.fillRect(float(kScreenW) * 0.5f - 300.0f, float(kScreenH) - 92.0f, 600.0f, 44.0f, rgba(10, 8, 20, 170));
    r.drawText(text, float(kScreenW) * 0.5f, float(kScreenH) - 84.0f, {22.0f, d.color, kHudInk}, Align::Center);
    return;
  }
  const VehicleDef& d = vehicleDef(v->kind);
  const float x = 12.0f, y = 134.0f; // under the HUD messages (y 92)
  const bool fuel = d.fuel > 0;
  r.fillRect(x, y, 430.0f, fuel ? 76.0f : 54.0f, rgba(10, 8, 20, 180));
  r.fillRect(x, y, 6.0f, fuel ? 76.0f : 54.0f, d.color);
  r.drawText(d.name, x + 18.0f, y + 6.0f, {17.0f, d.color, kHudInk});
  r.drawText("ARMOUR", x + 18.0f, y + 30.0f, {13.0f, rgb(190, 188, 214), kHudInk});
  const int pips = std::max(1, d.hp);
  const float pw = std::min(22.0f, 300.0f / float(pips));
  for (int i = 0; i < pips; ++i)
  {
    const bool full = i < v->hp;
    const bool low = v->hp <= 2 && full && (frame / 10) % 2 == 0;
    r.fillRect(x + 100.0f + float(i) * pw, y + 30.0f, pw - 4.0f, 14.0f,
      full ? (low ? rgb(255, 255, 255) : d.color) : rgba(80, 76, 100, 160));
  }
  if (fuel)
  {
    r.drawText("FUEL", x + 18.0f, y + 52.0f, {13.0f, rgb(190, 188, 214), kHudInk});
    const float frac = float(v->fuel) / float(d.fuel);
    r.fillRect(x + 100.0f, y + 54.0f, 300.0f, 12.0f, rgba(80, 76, 100, 160));
    const bool low = v->fuel < 150 && (frame / 10) % 2 == 0;
    r.fillRect(x + 100.0f, y + 54.0f, 300.0f * frac, 12.0f, low ? rgb(255, 80, 60) : rgb(120, 230, 120));
  }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%s GETS OUT", mUseLabel.c_str());
  r.drawText(buf, x + 420.0f, y + 6.0f, {13.0f, rgb(190, 188, 214), kHudInk}, Align::Right);
}

} // namespace gr
