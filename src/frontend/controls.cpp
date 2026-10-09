#include "frontend/controls.hpp"

#include <algorithm>
#include <cstdio>

namespace gr
{

namespace
{

// Half deflection, so a resting or slightly worn stick never walks the
// runner, and diagonals still count as up/down plus left/right.
constexpr Sint16 kStickThreshold = 16000;
constexpr Sint16 kTriggerThreshold = 12000;

} // namespace

Controls::Controls()
{
  if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0)
  {
    std::fprintf(stderr, "gamepads unavailable: %s\n", SDL_GetError());
    return;
  }
  // Pads that are already plugged in also arrive as
  // SDL_CONTROLLERDEVICEADDED events, which open them.
}

Controls::~Controls()
{
  for (auto* pad : mPads)
    SDL_GameControllerClose(pad);
  if (SDL_WasInit(SDL_INIT_GAMECONTROLLER))
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

void Controls::open(int deviceIndex)
{
  if (!SDL_IsGameController(deviceIndex))
    return;
  const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(deviceIndex);
  for (auto* pad : mPads)
    if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)) == id)
      return;
  if (SDL_GameController* pad = SDL_GameControllerOpen(deviceIndex))
  {
    mPads.push_back(pad);
    const char* name = SDL_GameControllerName(pad);
    std::fprintf(stderr, "gamepad connected: %s\n", name ? name : "unknown");
  }
}

void Controls::close(SDL_JoystickID id)
{
  auto it = std::find_if(mPads.begin(), mPads.end(), [id](SDL_GameController* pad) {
    return SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)) == id;
  });
  if (it == mPads.end())
    return;
  std::fprintf(stderr, "gamepad disconnected\n");
  SDL_GameControllerClose(*it);
  mPads.erase(it);
}

Controls::Action Controls::handleEvent(const SDL_Event& ev)
{
  switch (ev.type)
  {
    case SDL_QUIT:
      return Action::Quit;
    case SDL_KEYDOWN:
      if (ev.key.repeat)
        break;
      if (ev.key.keysym.sym == SDLK_t)
        return Action::CycleTheme;
      if (ev.key.keysym.sym == SDLK_F11 ||
          ((ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_KP_ENTER) && (ev.key.keysym.mod & KMOD_ALT)))
        return Action::ToggleFullscreen;
      break;
    case SDL_CONTROLLERDEVICEADDED:
      open(ev.cdevice.which);
      break;
    case SDL_CONTROLLERDEVICEREMOVED:
      close(ev.cdevice.which);
      break;
    case SDL_CONTROLLERBUTTONDOWN:
      if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_BACK)
        return Action::CycleTheme;
      break;
    default:
      break;
  }
  return Action::None;
}

Input Controls::read() const
{
  const Uint8* k = SDL_GetKeyboardState(nullptr);
  Input in;
  in.left = k[SDL_SCANCODE_LEFT] || k[SDL_SCANCODE_A];
  in.right = k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D];
  in.up = k[SDL_SCANCODE_UP] || k[SDL_SCANCODE_W];
  in.down = k[SDL_SCANCODE_DOWN] || k[SDL_SCANCODE_S];
  in.jump = k[SDL_SCANCODE_Z] || k[SDL_SCANCODE_SPACE];
  in.fire = k[SDL_SCANCODE_X] || k[SDL_SCANCODE_LCTRL] || k[SDL_SCANCODE_RCTRL];
  // Alt+Enter toggles fullscreen, so it must not also pick a menu item.
  in.confirm = (k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_KP_ENTER]) && !(SDL_GetModState() & KMOD_ALT);
  // AC_BACK is Android's back key: pause in a level, back out of a menu.
  in.pause = k[SDL_SCANCODE_ESCAPE] || k[SDL_SCANCODE_P] || k[SDL_SCANCODE_AC_BACK];
  in.back = k[SDL_SCANCODE_ESCAPE] || k[SDL_SCANCODE_BACKSPACE] || k[SDL_SCANCODE_AC_BACK];
  in.swap = k[SDL_SCANCODE_C];

  for (auto* pad : mPads)
  {
    auto button = [pad](SDL_GameControllerButton b) { return SDL_GameControllerGetButton(pad, b) != 0; };
    auto axis = [pad](SDL_GameControllerAxis a) { return SDL_GameControllerGetAxis(pad, a); };
    const Sint16 sx = axis(SDL_CONTROLLER_AXIS_LEFTX);
    const Sint16 sy = axis(SDL_CONTROLLER_AXIS_LEFTY);

    in.left = in.left || button(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || sx < -kStickThreshold;
    in.right = in.right || button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || sx > kStickThreshold;
    in.up = in.up || button(SDL_CONTROLLER_BUTTON_DPAD_UP) || sy < -kStickThreshold;
    in.down = in.down || button(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || sy > kStickThreshold;
    // South face button jumps; west, east or the right trigger fire.
    in.jump = in.jump || button(SDL_CONTROLLER_BUTTON_A);
    in.fire = in.fire || button(SDL_CONTROLLER_BUTTON_X) || button(SDL_CONTROLLER_BUTTON_B) ||
      button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) || axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > kTriggerThreshold;
    in.confirm = in.confirm || button(SDL_CONTROLLER_BUTTON_START) || button(SDL_CONTROLLER_BUTTON_A);
    in.pause = in.pause || button(SDL_CONTROLLER_BUTTON_START);
    in.back = in.back || button(SDL_CONTROLLER_BUTTON_B);
    in.swap = in.swap || button(SDL_CONTROLLER_BUTTON_Y);
  }
  return in;
}

} // namespace gr
