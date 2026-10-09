#pragma once

#include "game/input.hpp"

#include <SDL.h>

#include <vector>

namespace gr
{

// Keyboard and gamepad input for the windowed game. Gamepads go through
// SDL's GameController API, so any pad SDL knows (Xbox, PlayStation,
// Switch Pro, 8BitDo, Steam Deck, most generic USB pads) uses the same
// layout. Pads can be plugged in and out while the game runs; extra
// mappings can be supplied with the SDL_GAMECONTROLLERCONFIG environment
// variable.
class Controls
{
public:
  Controls();
  ~Controls();
  Controls(const Controls&) = delete;
  Controls& operator=(const Controls&) = delete;

  enum class Action
  {
    None,
    Quit, // window closed
    CycleTheme,
    ToggleFullscreen, // F11 or Alt+Enter
  };

  // Feed every SDL event through here. Handles hotplugging and returns the
  // one-shot actions (window closed, theme switch) that are not part of
  // Input. Esc is part of Input: it opens the pause menu or backs out.
  Action handleEvent(const SDL_Event& ev);

  // Current state of the keyboard and all connected pads combined.
  Input read() const;

private:
  void open(int deviceIndex);
  void close(SDL_JoystickID id);

  std::vector<SDL_GameController*> mPads;
};

} // namespace gr
