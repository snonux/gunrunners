#include "frontend/bindings.hpp"

#include <SDL.h>

#include <algorithm>
#include <cctype>

namespace gr
{

namespace
{

const char* const kActKeys[kActCount] = {"left", "right", "up", "down", "jump", "fire", "swap", "pause", "quicksave", "quickload"};

} // namespace

Bindings Bindings::defaults()
{
  Bindings b;
  auto keys = [&b](Act a, int k0, int k1) { b.keys[std::size_t(a)] = {k0, k1}; };
  keys(Act::Left, SDL_SCANCODE_LEFT, SDL_SCANCODE_A);
  keys(Act::Right, SDL_SCANCODE_RIGHT, SDL_SCANCODE_D);
  keys(Act::Up, SDL_SCANCODE_UP, SDL_SCANCODE_W);
  keys(Act::Down, SDL_SCANCODE_DOWN, SDL_SCANCODE_S);
  keys(Act::Jump, SDL_SCANCODE_X, SDL_SCANCODE_LCTRL);
  keys(Act::Fire, SDL_SCANCODE_Z, SDL_SCANCODE_SPACE);
  keys(Act::Swap, SDL_SCANCODE_C, 0);
  keys(Act::Pause, SDL_SCANCODE_P, 0);
  keys(Act::QuickSave, SDL_SCANCODE_F5, 0);
  keys(Act::QuickLoad, SDL_SCANCODE_F9, 0);
  auto pad = [&b](Act a, std::vector<int> codes) { b.pad[std::size_t(a)] = std::move(codes); };
  pad(Act::Left, {SDL_CONTROLLER_BUTTON_DPAD_LEFT});
  pad(Act::Right, {SDL_CONTROLLER_BUTTON_DPAD_RIGHT});
  pad(Act::Up, {SDL_CONTROLLER_BUTTON_DPAD_UP});
  pad(Act::Down, {SDL_CONTROLLER_BUTTON_DPAD_DOWN});
  pad(Act::Jump, {SDL_CONTROLLER_BUTTON_X});
  pad(Act::Fire, {SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
                   kPadRightTrigger});
  pad(Act::Swap, {SDL_CONTROLLER_BUTTON_Y});
  pad(Act::Pause, {SDL_CONTROLLER_BUTTON_START});
  // Quick save and load have no pad button until one is bound: a pad has
  // none to spare. The pause menu has both.
  return b;
}

Bindings Bindings::fromProfile(const std::map<std::string, std::vector<int>>& controls)
{
  Bindings b = defaults();
  for (int a = 0; a < kActCount; ++a)
  {
    const std::string name = kActKeys[a];
    auto k = controls.find("key." + name);
    if (k != controls.end())
      for (int s = 0; s < kKeySlots; ++s)
        b.keys[std::size_t(a)][std::size_t(s)] =
          s < int(k->second.size()) && k->second[std::size_t(s)] > 0 && k->second[std::size_t(s)] < SDL_NUM_SCANCODES
          ? k->second[std::size_t(s)]
          : 0;
    auto p = controls.find("pad." + name);
    if (p != controls.end())
    {
      b.pad[std::size_t(a)].clear();
      for (int c : p->second)
        if ((c >= 0 && c < SDL_CONTROLLER_BUTTON_MAX) || c == kPadLeftTrigger || c == kPadRightTrigger)
          b.pad[std::size_t(a)].push_back(c);
    }
  }
  return b;
}

void Bindings::toProfile(std::map<std::string, std::vector<int>>& controls) const
{
  for (int a = 0; a < kActCount; ++a)
  {
    const std::string name = kActKeys[a];
    controls["key." + name] = {keys[std::size_t(a)].begin(), keys[std::size_t(a)].end()};
    controls["pad." + name] = pad[std::size_t(a)];
  }
}

void Bindings::bindKey(Act a, int slot, int scancode)
{
  for (auto& slots : keys)
    for (auto& k : slots)
      if (k == scancode)
        k = 0;
  keys[std::size_t(a)][std::size_t(std::clamp(slot, 0, kKeySlots - 1))] = scancode;
}

void Bindings::bindPad(Act a, int code)
{
  for (auto& codes : pad)
    codes.erase(std::remove(codes.begin(), codes.end(), code), codes.end());
  pad[std::size_t(a)] = {code};
}

bool Bindings::keyBound(int scancode) const
{
  for (const auto& slots : keys)
    for (int k : slots)
      if (k == scancode)
        return true;
  return false;
}

bool Bindings::padBound(int code) const
{
  for (const auto& codes : pad)
    if (std::find(codes.begin(), codes.end(), code) != codes.end())
      return true;
  return false;
}

const char* actName(Act a)
{
  static const char* const kNames[kActCount] = {
    "MOVE LEFT", "MOVE RIGHT", "UP / CLIMB", "DOWN / DUCK", "JUMP", "FIRE", "SWITCH RUNNER", "PAUSE", "QUICK SAVE",
    "QUICK LOAD"};
  return kNames[std::clamp(int(a), 0, kActCount - 1)];
}

std::string keyName(int scancode)
{
  if (scancode <= 0)
    return "-";
  std::string n = SDL_GetScancodeName(SDL_Scancode(scancode));
  if (n.empty())
    return "KEY " + std::to_string(scancode);
  for (auto& c : n)
    c = char(std::toupper(static_cast<unsigned char>(c)));
  return n;
}

std::string padName(int code)
{
  switch (code)
  {
    case SDL_CONTROLLER_BUTTON_A: return "A";
    case SDL_CONTROLLER_BUTTON_B: return "B";
    case SDL_CONTROLLER_BUTTON_X: return "X";
    case SDL_CONTROLLER_BUTTON_Y: return "Y";
    case SDL_CONTROLLER_BUTTON_BACK: return "BACK";
    case SDL_CONTROLLER_BUTTON_GUIDE: return "GUIDE";
    case SDL_CONTROLLER_BUTTON_START: return "START";
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "LS";
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "RS";
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "LB";
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "RB";
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return "D-UP";
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return "D-DOWN";
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return "D-LEFT";
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "D-RIGHT";
    case kPadLeftTrigger: return "LT";
    case kPadRightTrigger: return "RT";
    default: return "BUTTON " + std::to_string(code);
  }
}

std::string padNames(const std::vector<int>& codes)
{
  if (codes.empty())
    return "-";
  std::string s;
  for (int c : codes)
    s += (s.empty() ? "" : " ") + padName(c);
  return s;
}

} // namespace gr
