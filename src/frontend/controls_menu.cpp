// The CONTROLS menu: rebinding the keyboard and gamepad, the on-screen
// gamepad's size and layout, and a way back to the defaults. Reached from
// the title screen and the pause menu; everything is saved in the profile.

#include "frontend/game.hpp"

#include <SDL.h>

#include <algorithm>

namespace gr
{

namespace
{

constexpr Color kInk = rgb(16, 12, 26);
constexpr int kListenTicks = 6 * 60; // a listen nobody answers gives up
constexpr int kPadColumn = Bindings::kKeySlots;

enum class RowKind
{
  Action,
  TouchSize,
  TouchLayout,
  Reset,
};

} // namespace

struct Game::ControlRow
{
  RowKind kind;
  Act act;
};

std::vector<Game::ControlRow> Game::controlRows() const
{
  std::vector<ControlRow> rows;
  for (int a = 0; a < kActCount; ++a)
    rows.push_back({RowKind::Action, Act(a)});
  if (mOptions.touch)
  {
    rows.push_back({RowKind::TouchSize, Act::Count});
    rows.push_back({RowKind::TouchLayout, Act::Count});
  }
  rows.push_back({RowKind::Reset, Act::Count});
  return rows;
}

void Game::openControls()
{
  mMenu = Menu::Controls;
  mListening = false;
  mControlsCursor = std::clamp(mControlsCursor, 0, int(controlRows().size()) - 1);
}

const std::vector<int>& Game::touchLayout() const
{
  static const std::vector<int> kNone;
  auto it = mProfile.controls.find("touch.layout");
  return it == mProfile.controls.end() ? kNone : it->second;
}

void Game::setTouchLayout(const std::vector<int>& offsets)
{
  // Saved when the editor closes, not on every finger move.
  mProfile.controls["touch.layout"] = offsets;
}

void Game::saveBindings()
{
  mBindings.toProfile(mProfile.controls);
  mProfile.save(saveDir());
}

void Game::captureKey(int scancode)
{
  if (!mListening)
    return;
  const Act act = controlRows()[std::size_t(mControlsCursor)].act;
  if (scancode == SDL_SCANCODE_ESCAPE)
  {
    mListening = false;
    sound(Sfx::MenuMove);
  }
  else if (mControlsColumn < kPadColumn)
  {
    mBindings.bindKey(act, mControlsColumn, scancode);
    saveBindings();
    mListening = false;
    sound(Sfx::MenuSelect);
  }
  mMenuHold = !mListening;
}

void Game::capturePad(int code)
{
  if (!mListening || mControlsColumn != kPadColumn)
    return;
  mBindings.bindPad(controlRows()[std::size_t(mControlsCursor)].act, code);
  saveBindings();
  mListening = false;
  mMenuHold = true;
  sound(Sfx::MenuSelect);
}

void Game::tickControls(const Input& in, bool ok, bool cancel, int dir, int side)
{
  (void)in;
  if (mMenu == Menu::TouchEdit)
  {
    // DONE on the screen reads as a back press, like Esc or B.
    if (cancel)
    {
      mProfile.save(saveDir());
      mMenu = Menu::Controls;
      mMenuHold = true;
      sound(Sfx::MenuSelect);
    }
    return;
  }
  if (mListening)
  {
    // Keys and pad buttons arrive through captureKey / capturePad; a touch
    // BACK or a long wait gives up.
    if (cancel || ++mListenTicks > kListenTicks)
      mListening = false;
    return;
  }

  const auto rows = controlRows();
  const int n = int(rows.size());
  mControlsCursor = (mControlsCursor + dir + n) % n;
  mControlsColumn = (mControlsColumn + side + kPadColumn + 1) % (kPadColumn + 1);
  const ControlRow& row = rows[std::size_t(mControlsCursor)];
  if (cancel)
  {
    if (mControlsReturn == Menu::Pause)
      openMenu(Menu::Pause);
    else
      closeMenu();
    return;
  }
  if (!ok)
    return;
  sound(Sfx::MenuSelect);
  switch (row.kind)
  {
    case RowKind::Action:
      mListening = true;
      mListenTicks = 0;
      break;
    case RowKind::TouchSize:
      cycleTouchSize();
      break;
    case RowKind::TouchLayout:
      mMenu = Menu::TouchEdit;
      break;
    case RowKind::Reset:
      mBindings = Bindings::defaults();
      mProfile.controls.erase("touch.layout");
      mProfile.touchSize = 1;
      saveBindings();
      notice("CONTROLS RESET TO THE DEFAULTS");
      break;
  }
}

std::string Game::keysHint() const
{
  auto key = [this](Act a) { return keyName(mBindings.keys[std::size_t(a)][0]); };
  auto pad = [this](Act a) {
    std::string s;
    for (int c : mBindings.pad[std::size_t(a)])
      s += (s.empty() ? "" : "/") + padName(c);
    return s.empty() ? std::string("-") : s;
  };
  return "KEYS  arrows move  -  " + key(Act::Jump) + " jump  -  " + key(Act::Fire) + " fire  -  " + key(Act::Swap) +
    " runner  -  Esc menu      PAD  stick  -  " + pad(Act::Jump) + " jump  -  " + pad(Act::Fire) + " fire  -  " +
    pad(Act::Swap) + " runner  -  Start menu";
}

void Game::renderControls()
{
  auto& r = mRenderer;
  const auto& t = theme();
  const TextStyle hint{16.0f, rgb(190, 188, 214), kInk};
  if (mMenu == Menu::TouchEdit)
  {
    // The on-screen gamepad draws itself on top, with DONE and RESET.
    r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(0, 0, 0, 180));
    r.drawText("TOUCH LAYOUT", 640, 150, {40.0f, t.accentA, kInk, true}, Align::Center);
    r.drawText("Drag the stick and the buttons to where your thumbs want them.", 640, 214, hint, Align::Center);
    r.drawText("TOUCH PAD in CONTROLS sets their size.", 640, 240, hint, Align::Center);
    return;
  }

  // The panel is see-through, and a full table over the title's own menu
  // reads as clutter, so dim the screen further first.
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(0, 0, 0, 150));
  r.draw(mSlotPanel, 140, 100);
  r.drawText("CONTROLS", 640, 116, {40.0f, t.accentA, kInk, true}, Align::Center);
  const float colX[kPadColumn + 1] = {600.0f, 770.0f, 960.0f};
  const char* const heads[kPadColumn + 1] = {"KEY", "KEY 2", "GAMEPAD"};
  for (int c = 0; c <= kPadColumn; ++c)
    r.drawText(heads[c], colX[c], 168, {15.0f, t.accentB, kInk}, Align::Center);

  const auto rows = controlRows();
  const float step = 32.0f;
  for (int i = 0; i < int(rows.size()); ++i)
  {
    const ControlRow& row = rows[std::size_t(i)];
    const bool sel = i == mControlsCursor;
    const float y = 196.0f + float(i) * step + (row.kind == RowKind::Action ? 0.0f : 10.0f);
    const TextStyle plain{21.0f, t.hudText, kInk};
    const TextStyle chosen{21.0f, t.accentA, kInk, true};
    if (row.kind != RowKind::Action)
    {
      if (sel)
        r.fillRect(400, y - 3, 480, step - 4, withAlpha(t.accentA, 60));
      const std::string label = row.kind == RowKind::TouchSize ? touchSizeLabel()
        : row.kind == RowKind::TouchLayout                    ? "EDIT TOUCH LAYOUT"
                                                              : "RESET TO DEFAULTS";
      r.drawText(label, 640, y, sel ? chosen : plain, Align::Center);
      continue;
    }
    r.drawText(actName(row.act), 200, y, sel ? chosen : plain);
    for (int c = 0; c <= kPadColumn; ++c)
    {
      const bool cell = sel && c == mControlsColumn;
      if (cell)
        r.fillRect(colX[c] - 88, y - 3, 176, step - 4, withAlpha(t.accentA, mListening ? 140 : 60));
      std::string text = c < kPadColumn ? keyName(mBindings.keys[std::size_t(row.act)][std::size_t(c)])
                                        : padNames(mBindings.pad[std::size_t(row.act)]);
      if (cell && mListening)
        text = "...";
      r.drawText(text, colX[c], y, cell ? chosen : plain, Align::Center);
    }
  }

  std::string help = "UP/DOWN choose   LEFT/RIGHT key or gamepad   ENTER / A change   ESC / B back";
  if (mListening)
  {
    const std::string act = actName(rows[std::size_t(mControlsCursor)].act);
    help = mControlsColumn < kPadColumn ? "PRESS A KEY FOR " + act + "   -   ESC cancels"
                                        : "PRESS A GAMEPAD BUTTON FOR " + act + "   -   ESC cancels";
  }
  r.drawText(help, 640, 584, {mListening ? 20.0f : 16.0f, mListening ? t.accentA : rgb(190, 188, 214), kInk},
    Align::Center);
}

} // namespace gr
