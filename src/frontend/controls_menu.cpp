// The CONTROLS menu: rebinding the keyboard and gamepad, the on-screen
// gamepad's size and layout, and a way back to the defaults. Reached from
// the title screen and the pause menu; everything is saved in the profile.

#include "frontend/game.hpp"

#include <SDL.h>

#include <algorithm>
#include <cmath>

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
  TouchScheme,
  TouchSize,
  TouchLayout,
  Assists,
  Reset,
};

// ASSISTS AND OPTIONS: profile key, label, and what each one does.
struct AssistRow
{
  const char* key;
  const char* label;
  const char* help;
};
const AssistRow kAssistRows[] = {
  {"opt.holdfire", "HOLD TO FIRE", "Holding fire keeps shooting (rapid fire is still faster)."},
  {"opt.downjump", "DOWN + JUMP LEAVES A RIDE", "Off: only USE gets you out of a vehicle."},
  {"assist.jump", "JUMP HELP", "A jump pressed just before landing still jumps; more time at ledges."},
  {"assist.damage", "HALF DAMAGE", "Hits take half the hearts."},
  {"assist.slow", "GAME SPEED 75%", "Everything moves slower (not in levels that keep time with the music)."},
  {"assist.guide", "GUIDE ARROW", "A blinking arrow points to every item and bot left, then the exit (not in bonus levels)."},
  {"opt.calm", "REDUCED EFFECTS", "No screen shake and no flashes of light."},
  {"opt.swapdeath", "SWAP RUNNER ON DEATH", "A runner who dies is out for the level; lose them all and it starts over."},
  {"opt.voices", "RUNNER VOICES", "The runners talk about what they see. SAY A LINE (V) for more."},
  {"opt.subtitles", "VOICE SUBTITLES", "What the runners say, written at the bottom of the screen."},
};
const char* const kVoiceLevels[] = {"OFF", "QUIET", "NORMAL", "LOUD"};
constexpr int kVoiceDefault = 2;
constexpr int kAssistCount = int(sizeof(kAssistRows) / sizeof(kAssistRows[0]));

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
    rows.push_back({RowKind::TouchScheme, Act::Count});
    rows.push_back({RowKind::TouchSize, Act::Count});
    rows.push_back({RowKind::TouchLayout, Act::Count});
  }
  rows.push_back({RowKind::Assists, Act::Count});
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
    case RowKind::TouchScheme:
      mProfile.controls["touch.thumbs"] = {touchThumbs() ? 0 : 1};
      mProfile.save(saveDir());
      break;
    case RowKind::TouchSize:
      cycleTouchSize();
      break;
    case RowKind::TouchLayout:
      mMenu = Menu::TouchEdit;
      break;
    case RowKind::Assists:
      mMenu = Menu::Assists;
      mAssistCursor = 0;
      break;
    case RowKind::Reset:
      mBindings = Bindings::defaults();
      mProfile.controls.erase("touch.layout");
      mProfile.controls.erase("touch.thumbs");
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
  // The rows share the panel down to the help line, closer as more come.
  const float step = std::min(30.0f, std::floor(360.0f / float(std::max<std::size_t>(rows.size(), 2) - 1)));
  const float textSize = step < 22.0f ? 18.0f : 20.0f;
  // While it listens, a tap anywhere gives up (like BACK).
  if (mListening)
    tapTarget(0, 0, float(kScreenW), float(kScreenH), [this](float, float) { tapPress(&Input::back); });
  for (int i = 0; i < int(rows.size()); ++i)
  {
    const ControlRow& row = rows[std::size_t(i)];
    const bool sel = i == mControlsCursor;
    const float y = 196.0f + float(i) * step + (row.kind == RowKind::Action ? 0.0f : 10.0f);
    if (!mListening && row.kind != RowKind::Action)
    {
      tapTarget(400, y - 4, 480, step, [this, i](float, float) {
        mControlsCursor = i;
        tapPress(&Input::confirm);
      });
    }
    else if (!mListening)
    {
      // A key or button cell: the first tap picks it, a second one listens
      // for the new key (a tap on the action's name just picks the row).
      tapTarget(180, y - 4, 330, step, [this, i](float, float) {
        if (mControlsCursor != i)
          sound(Sfx::MenuMove);
        mControlsCursor = i;
      });
      for (int c = 0; c <= kPadColumn; ++c)
        tapTarget(colX[c] - 88, y - 4, 176, step, [this, i, c](float, float) {
          if (mControlsCursor == i && mControlsColumn == c)
          {
            tapPress(&Input::confirm);
            return;
          }
          mControlsCursor = i;
          mControlsColumn = c;
          sound(Sfx::MenuMove);
        });
    }
    const TextStyle plain{textSize, t.hudText, kInk};
    const TextStyle chosen{textSize, t.accentA, kInk, true};
    if (row.kind != RowKind::Action)
    {
      if (sel)
        r.fillRect(400, y - 3, 480, step - 4, withAlpha(t.accentA, 60));
      const std::string label = row.kind == RowKind::TouchScheme
        ? (touchThumbs() ? "TOUCH: THUMB ZONES" : "TOUCH: CLASSIC BUTTONS")
        : row.kind == RowKind::TouchSize                      ? touchSizeLabel()
        : row.kind == RowKind::TouchLayout                    ? "EDIT TOUCH LAYOUT"
        : row.kind == RowKind::Assists                        ? "ASSISTS AND OPTIONS"
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

  std::string help = mOptions.touch ? "TAP a key or button, TAP it again to change it   BACK to leave"
                                    : "UP/DOWN choose   LEFT/RIGHT key or gamepad   ENTER / A change   ESC / B back";
  if (mListening)
  {
    const std::string act = actName(rows[std::size_t(mControlsCursor)].act);
    help = mControlsColumn < kPadColumn ? "PRESS A KEY FOR " + act + "   -   ESC cancels"
                                        : "PRESS A GAMEPAD BUTTON FOR " + act + "   -   ESC cancels";
    if (mOptions.touch)
      help += " (or TAP)";
  }
  r.drawText(help, 640, rows.size() > 15 ? 601.0f : rows.size() > 13 ? 596.0f : 584.0f, {mListening ? 20.0f : 16.0f, mListening ? t.accentA : rgb(190, 188, 214), kInk},
    Align::Center);
}

bool Game::assistOn(const char* key) const
{
  // Subtitles and the guide arrow are on until switched off; everything
  // else starts off.
  return playOption(key, std::string(key) == "opt.subtitles" || std::string(key) == "assist.guide");
}

bool Game::playOption(const char* key, bool fallback) const
{
  const auto it = mProfile.controls.find(key);
  return it == mProfile.controls.end() || it->second.empty() ? fallback : it->second[0] != 0;
}

World::PlayOptions Game::playOptions() const
{
  World::PlayOptions o;
  o.holdFire = playOption("opt.holdfire");
  o.downJumpLeaves = playOption("opt.downjump");
  o.jumpHelp = playOption("assist.jump");
  o.lowDamage = playOption("assist.damage");
  o.quickRetry = !mOptions.autoplay;
  o.calm = playOption("opt.calm");
  o.swapOnDeath = playOption("opt.swapdeath");
  o.guide = assistOn("assist.guide");
  return o;
}

void Game::tickAssists(bool ok, bool cancel, int dir)
{
  const int n = kAssistCount + 1; // the rows, then BACK
  mAssistCursor = (mAssistCursor + dir + n) % n;
  if (cancel || (ok && mAssistCursor == kAssistCount))
  {
    mMenu = Menu::Controls;
    mMenuHold = true;
    sound(Sfx::MenuSelect);
    return;
  }
  if (!ok)
    return;
  sound(Sfx::MenuSelect);
  const char* key = kAssistRows[mAssistCursor].key;
  if (std::string(key) == "opt.voices")
  {
    // NORMAL, LOUD, OFF, QUIET, NORMAL...
    const auto it = mProfile.controls.find(key);
    const int v = it == mProfile.controls.end() || it->second.empty() ? kVoiceDefault : it->second[0];
    mProfile.controls[key] = {(std::clamp(v, 0, 3) + 1) % 4};
    if (voiceVolume() <= 0.0f)
      mVoice.stop(mAudio);
  }
  else
  {
    mProfile.controls[key] = {assistOn(key) ? 0 : 1};
  }
  mProfile.save(saveDir());
  // The level being played (and one parked behind a bonus level) changes now.
  for (World* w : {mWorld.get(), mMainWorld.get()})
    if (w)
      w->setPlayOptions(playOptions());
}

void Game::renderAssists()
{
  auto& r = mRenderer;
  const auto& t = theme();
  r.fillRect(0, 0, float(kScreenW), float(kScreenH), rgba(0, 0, 0, 150));
  r.draw(mSlotPanel, 140, 100);
  r.drawText("ASSISTS AND OPTIONS", 640, 116, {40.0f, t.accentA, kInk, true}, Align::Center);
  r.drawText("They keep records and bonus stars: none of them is a cheat.", 640, 168, {16.0f, t.accentB, kInk},
    Align::Center);
  // The rows close up as more options join, so BACK stays above the help line.
  const float step = std::min(42.0f, 360.0f / float(kAssistCount));
  for (int i = 0; i <= kAssistCount; ++i)
  {
    const bool sel = i == mAssistCursor;
    const float y = 196.0f + float(i) * step + (i == kAssistCount ? 6.0f : 0.0f);
    tapTarget(300, y - 6, 680, step - 6, [this, i](float, float) {
      if (mAssistCursor != i)
      {
        mAssistCursor = i;
        sound(Sfx::MenuMove);
        return;
      }
      tapPress(&Input::confirm);
    });
    if (sel)
      r.fillRect(300, y - 2, 680, step - 1, withAlpha(t.accentA, 60)); // the row and its help line
    const TextStyle plain{22.0f, t.hudText, kInk};
    const TextStyle chosen{22.0f, t.accentA, kInk, true};
    if (i == kAssistCount)
    {
      r.drawText("BACK", 640, y, sel ? chosen : plain, Align::Center);
      continue;
    }
    const AssistRow& row = kAssistRows[i];
    std::string value = assistOn(row.key) ? ": ON" : ": OFF";
    if (std::string(row.key) == "opt.voices")
    {
      const auto it = mProfile.controls.find(row.key);
      const int v = it == mProfile.controls.end() || it->second.empty() ? kVoiceDefault : it->second[0];
      value = std::string(": ") + kVoiceLevels[std::clamp(v, 0, 3)];
    }
    r.drawText(std::string(row.label) + value, 640, y, sel ? chosen : plain, Align::Center);
    // Closer rows: the help line tucks up under its label in smaller type.
    r.drawText(row.help, 640, y + std::min(24.0f, step - 14.0f), {step < 40.0f ? 13.0f : 14.0f, rgb(190, 188, 214), kInk},
      Align::Center);
  }
  r.drawText(mOptions.touch ? "TAP a row to pick it, TAP it again to switch it   BACK to leave"
                            : "UP/DOWN choose   ENTER / A switch   ESC / B back",
    640, kAssistCount > 8 ? 596.0f : 584.0f, {16.0f, rgb(190, 188, 214), kInk}, Align::Center);
}

} // namespace gr
