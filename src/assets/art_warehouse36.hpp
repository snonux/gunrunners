#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

namespace gr
{

// Level 36, Prop Warehouse (theme key prop_warehouse, look "warehouse",
// art_warehouse36.cpp): MaxTV's prop store at night. The masses are dark
// cinder block, two courses a block in running bond, the mortar sunk in
// shadow; now and then a conduit with a junction box, or a crack and a
// faded stencilled bay number. Where you stand, a concrete floor edge with
// a worn yellow safety line and a steel nosing. The one-way ledges are
// pallet-rack shelves: a chipboard deck on an orange step beam with its
// label strip; the ladders are fixed steel with safety-yellow rails. Behind:
// the dark of the roof space; far off, the corrugated back wall under its
// trusses, a row of high windows with the moon in one, bare work lights
// hanging on their flex over silhouettes of tall racks; nearer, the dark
// shelving itself, crammed with old props. The rack uprights, the curtains,
// the loading dock, the crane yard's steel and the freight lift shafts are
// drawn live by the level (world_warehouse_draw.cpp).
bool isPropWarehouse(const Theme& t);
Texture bakeWarehouseSolid(const Renderer& r, const Theme& t, int variant);
Texture bakeWarehouseSolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeWarehousePlatform(const Renderer& r, const Theme& t);
Texture bakeWarehouseLadder(const Renderer& r, const Theme& t);
Texture bakeWarehouseSky(const Renderer& r, const Theme& t);
// The far and near layers are taller than the screen (anchored
// kWarehouseFarRise / kWarehouseNearRise px down) so they still cover it
// after the climb up the freight lifts.
constexpr int kWarehouseFarRise = 240;
constexpr int kWarehouseNearRise = 520;
Texture bakeWarehouseFar(const Renderer& r, const Theme& t, int layerW);
Texture bakeWarehouseNear(const Renderer& r, const Theme& t, int layerW);
// The exit: the dressing-room door, a star on it and a bulb-lit sign over
// it (96 x 144, the same frame as the family's teleporter).
Texture bakeWarehouseExitBase(const Renderer& r, const Theme& t);

} // namespace gr
