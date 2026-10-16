#pragma once

// What world.hpp needs of the guide arrow's route search (game/guide.hpp):
// kept apart so that changing the search does not rebuild every file that
// includes the World.

#include <memory>

namespace gr
{

enum class GuideKind
{
  None,
  Item,
  Enemy,
  Exit,
};

struct GuideTarget;
struct GuideStatics;
struct GuideCache;

// The guide's kept grids and search scratch, held by the World between
// frames and made on first use. A copy of a World (the bot plans on copies)
// starts without one: it is only a cache, and copying megabytes of it would
// slow the planner down.
class GuideCacheSlot
{
public:
  GuideCacheSlot();
  ~GuideCacheSlot();
  GuideCacheSlot(const GuideCacheSlot&);
  GuideCacheSlot& operator=(const GuideCacheSlot&);
  GuideCacheSlot(GuideCacheSlot&&) noexcept;
  GuideCacheSlot& operator=(GuideCacheSlot&&) noexcept;

  GuideCache& get();

private:
  std::unique_ptr<GuideCache> mCache;
};

} // namespace gr
