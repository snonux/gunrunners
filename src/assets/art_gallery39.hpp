#pragma once

#include "data/theme.hpp"
#include "render/renderer.hpp"

namespace gr
{

// Level 39, The Gallery (theme key gallery, look "gallery",
// art_gallery39.cpp): the TV studio's control rooms and stages after
// hours, lit by the monitors. The masses are dark acoustic panels, slate
// fabric in bevelled frames, now and then a block of grey foam wedges or a
// wall box of microphone sockets. Where you stand, a black studio floor
// with a sheen on it, an aluminium nosing and a gaffer-taped cable channel.
// The one-way ledges are cable trays: galvanised ladder trays carrying
// bundles of black, red and blue cable. The spikes are broken screen glass
// standing up in a hazard-striped floor tray. Behind: the studio ceiling's
// lighting grid in the dark, lamps hanging from it, one or two lit through
// the haze; the control room's back wall of acoustic panels with a wall of
// screens across it (pictures, colour bars, a face, a clock); nearer,
// studio cameras on pedestals, light stands, flight cases, monitor carts
// and cable drums. The rooms' own furniture (the mixing desk, the tape
// shelves, the ON AIR sign, the forgotten set) is drawn by the level
// (world_gallery_draw.cpp).
bool isGallery(const Theme& t);
Texture bakeGallerySolid(const Renderer& r, const Theme& t, int variant);
Texture bakeGallerySolidTop(const Renderer& r, const Theme& t, float topOffset, int height);
Texture bakeGalleryPlatform(const Renderer& r, const Theme& t);
Texture bakeGallerySpikes(const Renderer& r, const Theme& t);
Texture bakeGallerySky(const Renderer& r, const Theme& t);
Texture bakeGalleryFar(const Renderer& r, const Theme& t, int layerW);
Texture bakeGalleryNear(const Renderer& r, const Theme& t, int layerW);
// The exit: CAM 1, the big studio monitor on its cart, its screen gone
// white with the live picture and ON AIR lit over it (96 x 144, the same
// frame as the family's teleporter).
Texture bakeGalleryExitBase(const Renderer& r, const Theme& t);

} // namespace gr
