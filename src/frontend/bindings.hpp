#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

namespace gr
{

// What the player can rebind: the in-level actions, each with two
// keyboard keys and any number of gamepad buttons. Menus keep fixed keys
// on top (arrows, Enter, Esc; d-pad, A, B, Start), so no binding can lock
// anyone out of them, and Esc always pauses a level.
enum class Act
{
  Left,
  Right,
  Up,
  Down,
  Jump,
  Fire,
  Swap,
  Pause,
  QuickSave,
  QuickLoad,
  Map,
  Count,
};
constexpr int kActCount = int(Act::Count);

// Gamepad codes are SDL_GameControllerButton values, plus the two triggers.
constexpr int kPadLeftTrigger = 100;
constexpr int kPadRightTrigger = 101;

struct Bindings
{
  static constexpr int kKeySlots = 2;
  std::array<std::array<int, kKeySlots>, kActCount> keys{}; // SDL scancodes, 0 = none
  std::array<std::vector<int>, kActCount> pad;

  static Bindings defaults();

  // Saved in the profile's "control" lines.
  static Bindings fromProfile(const std::map<std::string, std::vector<int>>& controls);
  void toProfile(std::map<std::string, std::vector<int>>& controls) const;

  // Puts a key in one of an action's slots; the key leaves any other slot.
  void bindKey(Act a, int slot, int scancode);
  // Makes this pad button the action's only one; it leaves other actions.
  void bindPad(Act a, int code);

  bool keyBound(int scancode) const;
  bool padBound(int code) const;

  bool operator==(const Bindings& o) const { return keys == o.keys && pad == o.pad; }
  bool operator!=(const Bindings& o) const { return !(*this == o); }
};

const char* actName(Act a);
std::string keyName(int scancode); // "-" for none
std::string padName(int code);
std::string padNames(const std::vector<int>& codes); // "-" for none

} // namespace gr
