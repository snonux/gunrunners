#include "frontend/controls.hpp"

#include <algorithm>

namespace gr
{

namespace
{

// Half deflection, so a resting or slightly worn stick never walks the
// runner, and diagonals still count as up/down plus left/right.
constexpr Sint16 kStickThreshold = 16000;
constexpr Sint16 kTriggerThreshold = 12000;

// Android reports a pad's Back/Select (and some pads' B) as the system back
// key, so there it does what the phone's back key does: pause in a level,
// back out of a menu. On a desktop it opens the map (its default binding),
// or cycles the theme if no action is bound to it.
#ifdef __ANDROID__
constexpr bool kBackIsBackKey = true;
#else
constexpr bool kBackIsBackKey = false;
#endif

} // namespace

Controls::Controls()
{
  if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0)
  {
    SDL_Log("gamepads unavailable: %s", SDL_GetError());
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
    SDL_Log("gamepad connected: %s", name ? name : "unknown");
  }
}

void Controls::close(SDL_JoystickID id)
{
  auto it = std::find_if(mPads.begin(), mPads.end(), [id](SDL_GameController* pad) {
    return SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)) == id;
  });
  if (it == mPads.end())
    return;
  SDL_Log("gamepad disconnected");
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
      if (ev.key.keysym.scancode == SDL_SCANCODE_T && !mBindings.keyBound(SDL_SCANCODE_T))
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
      if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_BACK && !kBackIsBackKey &&
          !mBindings.padBound(SDL_CONTROLLER_BUTTON_BACK))
        return Action::CycleTheme;
      break;
    default:
      break;
  }
  return Action::None;
}

Input Controls::read(bool menus, bool text) const
{
  Input in = readKeys(SDL_GetKeyboardState(nullptr), menus, text);
  for (auto* pad : mPads)
    in = in | readPad(pad, menus);
  return in;
}

Input Controls::readKeys(const Uint8* k, bool menus, bool text) const
{
  if (text)
  {
    Input in;
    in.left = k[SDL_SCANCODE_LEFT];
    in.right = k[SDL_SCANCODE_RIGHT];
    in.up = k[SDL_SCANCODE_UP];
    in.down = k[SDL_SCANCODE_DOWN];
    in.back = k[SDL_SCANCODE_ESCAPE] || k[SDL_SCANCODE_AC_BACK];
    return in;
  }
  auto act = [&](Act a) {
    for (int key : mBindings.keys[std::size_t(a)])
      if (key > 0 && k[key])
        return true;
    return false;
  };
  Input in;
  in.left = act(Act::Left);
  in.right = act(Act::Right);
  in.up = act(Act::Up);
  in.down = act(Act::Down);
  in.jump = act(Act::Jump);
  in.fire = act(Act::Fire);
  in.swap = act(Act::Swap);
  in.quickSave = act(Act::QuickSave);
  in.quickLoad = act(Act::QuickLoad);
  in.map = act(Act::Map);
  // Fixed keys on top of the bindings: Esc always pauses or backs out, and
  // menus always answer the arrows and Enter. AC_BACK is Android's back key.
  in.pause = act(Act::Pause) || k[SDL_SCANCODE_ESCAPE] || k[SDL_SCANCODE_AC_BACK];
  in.back = k[SDL_SCANCODE_ESCAPE] || k[SDL_SCANCODE_BACKSPACE] || k[SDL_SCANCODE_AC_BACK];
  // Alt+Enter toggles fullscreen, so it must not also pick a menu item.
  in.confirm = (k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_KP_ENTER]) && !(SDL_GetModState() & KMOD_ALT);
  if (menus)
  {
    in.left = in.left || k[SDL_SCANCODE_LEFT];
    in.right = in.right || k[SDL_SCANCODE_RIGHT];
    in.up = in.up || k[SDL_SCANCODE_UP];
    in.down = in.down || k[SDL_SCANCODE_DOWN];
  }
  return in;
}

Input Controls::readPad(SDL_GameController* pad, bool menus) const
{
  auto button = [pad](int b) {
    if (b == kPadLeftTrigger)
      return SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > kTriggerThreshold;
    if (b == kPadRightTrigger)
      return SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > kTriggerThreshold;
    return SDL_GameControllerGetButton(pad, SDL_GameControllerButton(b)) != 0;
  };
  auto act = [&](Act a) {
    for (int b : mBindings.pad[std::size_t(a)])
      if (button(b))
        return true;
    return false;
  };
  const Sint16 sx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
  const Sint16 sy = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
  Input in;
  // The left stick always steers; the d-pad by its bindings.
  in.left = act(Act::Left) || sx < -kStickThreshold;
  in.right = act(Act::Right) || sx > kStickThreshold;
  in.up = act(Act::Up) || sy < -kStickThreshold;
  in.down = act(Act::Down) || sy > kStickThreshold;
  in.jump = act(Act::Jump);
  in.fire = act(Act::Fire);
  in.swap = act(Act::Swap);
  in.quickSave = act(Act::QuickSave);
  in.quickLoad = act(Act::QuickLoad);
  in.map = act(Act::Map);
  // Fixed: Start always pauses and confirms, A confirms, B backs out.
  in.pause = act(Act::Pause) || button(SDL_CONTROLLER_BUTTON_START);
  in.confirm = button(SDL_CONTROLLER_BUTTON_START) || button(SDL_CONTROLLER_BUTTON_A);
  in.back = button(SDL_CONTROLLER_BUTTON_B);
  if (kBackIsBackKey && button(SDL_CONTROLLER_BUTTON_BACK))
    in.pause = in.back = true;
  if (menus)
  {
    in.left = in.left || button(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    in.right = in.right || button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    in.up = in.up || button(SDL_CONTROLLER_BUTTON_DPAD_UP);
    in.down = in.down || button(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
  }
  return in;
}

} // namespace gr
