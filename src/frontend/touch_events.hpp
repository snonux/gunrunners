#pragma once

#include <SDL.h>

#include <deque>
#include <mutex>

namespace gr
{

// Construct before the window renderer. SDL2's logical-size event watch
// rewrites finger coordinates (and clamps touches in the letterbox bars).
// Keep the original screen coordinates for the edge-hugging touch controls.
class TouchEvents
{
public:
  TouchEvents();
  ~TouchEvents();
  TouchEvents(const TouchEvents&) = delete;
  TouchEvents& operator=(const TouchEvents&) = delete;

  // After SDL_PollEvent, restore the corresponding event's coordinates.
  // Other events, or events not seen by the watch, are left alone.
  bool restore(SDL_Event& event);

private:
  static int SDLCALL watch(void* userdata, SDL_Event* event);
  std::mutex mMutex;
  std::deque<SDL_TouchFingerEvent> mEvents;
};

} // namespace gr
