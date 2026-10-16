// Direct touch in the menus. Each screen marks what it draws as tap
// targets; a tap picks the target under it, which sets the screen's cursor
// and, where a tap should act, presses OK for the next tick. So the menus
// keep a single path for the keyboard, the gamepad and a finger. A drag
// turns into up/down (or left/right) steps, and pans the map.

#include "frontend/game.hpp"

#include <algorithm>
#include <cmath>

namespace gr
{

namespace
{

// Drag distance per step: about a list row, or most of a carousel card.
constexpr float kRowDrag = 46.0f;
constexpr float kCardDrag = 150.0f;

} // namespace

void Game::clearTapTargets()
{
  mTapTargets.clear();
  // A drag left over from another screen does not step this one.
  if (mTapScreen != tapScreen())
  {
    mDragX = mDragY = 0.0f;
    mDragStepsX = mDragStepsY = 0;
  }
  mTapScreen = tapScreen();
}

void Game::tapTarget(float x, float y, float w, float h, std::function<void(float, float)> act)
{
  mTapTargets.push_back({x, y, w, h, std::move(act)});
}

void Game::tapPress(bool Input::*button)
{
  mTapInput.*button = true;
}

void Game::tapAt(float x, float y)
{
  // The targets are from the last frame drawn; a tap meant for a screen
  // that has gone since must not act on the next one.
  if (mTapScreen != tapScreen())
    return;
  for (auto it = mTapTargets.rbegin(); it != mTapTargets.rend(); ++it)
  {
    if (x >= it->x && x < it->x + it->w && y >= it->y && y < it->y + it->h)
    {
      const auto act = it->act;
      act(x, y);
      return;
    }
  }
}

void Game::dragBy(float dx, float dy)
{
  if (mMenu == Menu::Map)
  {
    // The map follows the finger.
    const float s = mapScale();
    mMapX -= dx / s;
    mMapY -= dy / s;
    return;
  }
  // Like scrolling a list on a phone: a finger moving up brings what is
  // below into view (the cursor steps down), one moving left the next card.
  const bool across = (mMenu == Menu::None && mMode == Mode::Select) || mMenu == Menu::Runner;
  const bool along = !across && !(mMenu == Menu::None && mMode == Mode::Editor && wantsText()) &&
    (mMenu != Menu::None || mMode == Mode::Title || mMode == Mode::List || mMode == Mode::Editor);
  auto steps = [](float& acc, float d, float per, int& out) {
    acc -= d;
    while (acc >= per)
    {
      acc -= per;
      ++out;
    }
    while (acc <= -per)
    {
      acc += per;
      --out;
    }
  };
  if (across)
    steps(mDragX, dx, mMenu == Menu::Runner ? kRowDrag * 2.0f : kCardDrag, mDragStepsX);
  if (along)
    steps(mDragY, dy, kRowDrag, mDragStepsY);
}

Input Game::takeTapInput()
{
  Input in = mTapInput;
  mTapInput = Input{};
  // One drag step a tick, so each one is a press of its own.
  if (mDragStepsY != 0)
  {
    (mDragStepsY > 0 ? in.down : in.up) = true;
    mDragStepsY += mDragStepsY > 0 ? -1 : 1;
  }
  if (mDragStepsX != 0)
  {
    (mDragStepsX > 0 ? in.right : in.left) = true;
    mDragStepsX += mDragStepsX > 0 ? -1 : 1;
  }
  // A tap is a fresh press even when the same button was down last tick.
  for (bool Input::*f : {&Input::left, &Input::right, &Input::up, &Input::down, &Input::confirm, &Input::back})
    if (in.*f)
      mPrev.*f = false;
  return in;
}

bool Game::runnerOnScreen(float& x, float& y) const
{
  if (!inLevel() || !mWorld)
    return false;
  const auto& p = mWorld->player();
  const auto& cam = mWorld->camera();
  const float cell = float(kCellSize) * kPixelScale;
  // The player's position is its bottom-left cell.
  x = (float(p.x) + float(Player::kWidth) * 0.5f) * cell - cam.renderX() * kPixelScale;
  y = (float(p.y) + 1.0f - float(p.height()) * 0.5f) * cell - cam.renderY() * kPixelScale;
  return x > -cell && x < float(kScreenW) + cell && y > -cell && y < float(kScreenH) + cell;
}

bool Game::menuPad() const
{
  auto it = mProfile.controls.find("touch.menupad");
  return it != mProfile.controls.end() && !it->second.empty() && it->second[0] != 0;
}

bool Game::touchThumbs() const
{
  auto it = mProfile.controls.find("touch.thumbs");
  return it == mProfile.controls.end() || it->second.empty() || it->second[0] != 0;
}

void Game::setMenuPad(bool on)
{
  if (menuPad() == on)
    return;
  mProfile.controls["touch.menupad"] = {on ? 1 : 0};
  saveProfile();
}

} // namespace gr
