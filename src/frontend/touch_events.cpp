#include "frontend/touch_events.hpp"

#include <algorithm>
#include <iterator>

namespace gr
{

namespace
{

bool fingerEvent(Uint32 type)
{
  return type == SDL_FINGERDOWN || type == SDL_FINGERMOTION || type == SDL_FINGERUP;
}

} // namespace

TouchEvents::TouchEvents()
{
  SDL_AddEventWatch(watch, this);
}

TouchEvents::~TouchEvents()
{
  SDL_DelEventWatch(watch, this);
}

int SDLCALL TouchEvents::watch(void* userdata, SDL_Event* event)
{
  if (fingerEvent(event->type))
  {
    auto& self = *static_cast<TouchEvents*>(userdata);
    std::lock_guard<std::mutex> lock(self.mMutex);
    self.mEvents.push_back(event->tfinger);
  }
  return 1;
}

bool TouchEvents::restore(SDL_Event& event)
{
  if (!fingerEvent(event.type))
    return false;
  std::lock_guard<std::mutex> lock(mMutex);
  const auto found = std::find_if(mEvents.begin(), mEvents.end(), [&](const SDL_TouchFingerEvent& raw) {
    return raw.type == event.type && raw.timestamp == event.tfinger.timestamp &&
      raw.touchId == event.tfinger.touchId && raw.fingerId == event.tfinger.fingerId &&
      raw.windowID == event.tfinger.windowID;
  });
  if (found == mEvents.end())
    return false;
  event.tfinger = *found;
  // Skip older records too if their SDL events were removed from the queue.
  mEvents.erase(mEvents.begin(), std::next(found));
  return true;
}

} // namespace gr
